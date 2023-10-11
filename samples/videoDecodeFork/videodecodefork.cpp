/*
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
*/

#include <iostream>
#include <unistd.h>
#include <vector>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <libgen.h>
#include <filesystem>
#include "videoDemuxer.hpp"
#include "vcndecode.hpp"

static int *nTotal;

void DecProc(VideoDecode *pDec, VideoDemuxer *demuxer, int *pnFrame) {
    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL, *pFrame = NULL;

    do {
        demuxer->demux(&pVideo, &nVideoBytes);
        nFrameReturned = pDec->decode(pVideo, nVideoBytes);

        nFrame += nFrameReturned;
    } while (nVideoBytes);
    *pnFrame = nFrame;
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-t Number of forks ( >= 1) - optional; default:4" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {
    std::string inputFilePath;
    int nForks = 4;
    int deviceId = 0;

    // Parse command-line arguments
    if(argc < 1) {
        ShowHelpAndExit();
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            ShowHelpAndExit();
        }
        if (!strcmp(argv[i], "-i")) {
            if (++i == argc) {
                ShowHelpAndExit("-i");
            }
            inputFilePath = argv[i];
            continue;
        }
        if (!strcmp(argv[i], "-t")) {
            if (++i == argc) {
                ShowHelpAndExit("-t");
            }
            nForks = atoi(argv[i]);
            if (nForks <= 0) {
                ShowHelpAndExit(argv[i]);
            }
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    std::vector<std::unique_ptr<VideoDemuxer>> vDemuxer;
    std::vector<std::unique_ptr<VideoDecode>> vViddec;
    std::vector<int> vDeviceId(nForks);
    int64_t pts = 0;
    bool bDecodeOutSemiPlanar = false;

    int numDevices = 0;
    hipError_t hipStatus = hipSuccess;
    hipStatus = hipGetDeviceCount(&numDevices);
    if (hipStatus != hipSuccess) {
        std::cout << "ERROR: hipGetDeviceCount failed! (" << hipStatus << ")" << std::endl;
        return 1;
    }

    int sd = (numDevices >= 2) ? 1 : 0;

    for (int i = 0; i < nForks; i++) {
        std::unique_ptr<VideoDemuxer> demuxer(new VideoDemuxer(inputFilePath.c_str()));
        vDeviceId[i] = (i % 2 == 0) ? 0 : sd;
        std::unique_ptr<VideoDecode> dec(new VideoDecode(vDeviceId[i]));
        vDemuxer.push_back(std::move(demuxer));
        vViddec.push_back(std::move(dec));
    }

    float totalFPS = 0;
    std::vector<int> vFrame;
    vFrame.resize(nForks, 0);

    std::string deviceName, gcnArchName, drmNode;
    int pciBusID, pciDomainID, pciDeviceID;

    for (int i = 0; i < nForks; i++) {
        vViddec[i]->getDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID, drmNode);
        std::cout << "info: stream " << i <<  " using GPU device " << vDeviceId[i] << ": (drm node: " << drmNode << ") " << deviceName << "[" << gcnArchName << "] on PCI bus "         << std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
    }

    int pid_status;
    pid_t pids[nForks];
    nTotal = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *nTotal = 0;

    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < nForks; i++) {
        if ((pids[i] = fork()) < 0) {
            std::cout << "ERROR: failed to create fork" << std::endl;
            abort();
        }
        else if (pids[i] == 0) {
            DecProc(vViddec[i].get(), vDemuxer[i].get(), &vFrame[i]);
            *nTotal += vFrame[i];
            _exit(0);
        }
    }

    for(int i = 0; i < nForks; i++) {	
        waitpid(pids[i], &pid_status, 0);
        if (!WIFEXITED(pid_status)) {
            std::cout << "child with " << pids[i] << " exited with status " << WEXITSTATUS(pid_status) << std::endl;
        }
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Calculate average decoding time
    double totalDecodingTime = 0.0;
    totalDecodingTime = duration;
    double averageDecodingTime = totalDecodingTime / *nTotal;

    std::cout << "info: Total Frames Decoded: " << *nTotal << std::endl;
    std::cout << "info: avg decoding time per frame (ms): " << averageDecodingTime  << std::endl;
    std::cout << "info: avg FPS: " << 1000 / averageDecodingTime << std::endl;
    munmap(nTotal, sizeof(int));
    return 0;
}
