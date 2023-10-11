/*
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
*/

#include <iostream>
#include <unistd.h>
#include <vector>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include <libgen.h>
#include <filesystem>
#include "videoDemuxer.hpp"
#include "vcndecode.hpp"

void DecProc(VideoDecode *pDec, VideoDemuxer *demuxer, int *pnFrame, double *pnFPS) {
    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL, *pFrame = NULL;
    auto startTime = std::chrono::high_resolution_clock::now();
    do {
        demuxer->demux(&pVideo, &nVideoBytes);
        nFrameReturned = pDec->decode(pVideo, nVideoBytes);

        nFrame += nFrameReturned;
    } while (nVideoBytes);
    *pnFrame = nFrame;
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Calculate average decoding time
    double totalDecodingTime = 0.0;
    totalDecodingTime = duration;
    double averageDecodingTime = totalDecodingTime / nFrame;
    double nFPS = 1000 / averageDecodingTime;
    *pnFPS = nFPS;
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-t Number of threads (>=1) - optional; default:4" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string inputFilePath;
    int nThread = 4;
    int deviceId = 0;

    // Parse command-line arguments
    if (argc < 1) {
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
            nThread = atoi(argv[i]);
            if (nThread <= 0) {
                ShowHelpAndExit(argv[i]);
            }
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    std::vector<std::unique_ptr<VideoDemuxer>> vDemuxer;
    std::vector<std::unique_ptr<VideoDecode>> vViddec;
    std::vector<int> vDeviceId(nThread);
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

    for (int i = 0; i < nThread; i++) {
        std::unique_ptr<VideoDemuxer> demuxer(new VideoDemuxer(inputFilePath.c_str()));
        vDeviceId[i] = (i % 2 == 0) ? 0 : sd;
        std::unique_ptr<VideoDecode> dec(new VideoDecode(vDeviceId[i]));
        vDemuxer.push_back(std::move(demuxer));
        vViddec.push_back(std::move(dec));
    }

    float totalFPS = 0;
    std::vector<std::thread> vThread;
    std::vector<double> vFPS;
    std::vector<int> vFrame;
    vFPS.resize(nThread, 0);
    vFrame.resize(nThread, 0);
    int nTotal = 0;

    std::string deviceName, gcnArchName, drmNode;
    int pciBusID, pciDomainID, pciDeviceID;

    for (int i = 0; i < nThread; i++) {
        vViddec[i]->getDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID, drmNode);
        std::cout << "info: stream " << i <<  " using GPU device " << vDeviceId[i] << ": (drm node: " << drmNode << ") " << deviceName << "[" << gcnArchName << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
    }

    for (int i = 0; i < nThread; i++) {
        vThread.push_back(std::thread(DecProc, vViddec[i].get(), vDemuxer[i].get(), &vFrame[i], &vFPS[i]));
    }

    for (int i = 0; i < nThread; i++) {
        vThread[i].join();
        totalFPS += vFPS[i];
        nTotal += vFrame[i];
    }

    std::cout << "info: Video codec format: " << vViddec[0]->getCodecFmtName(vViddec[0]->getVcnVideoCodecId()) << std::endl;
    std::cout << "info: Video size: [ " << vViddec[0]->getWidth() << ", " << vViddec[0]->getHeight() << " ]" << std::endl;
    std::cout << "info: Video pix format: " << vViddec[0]->getPixFmtName(vViddec[0]->getSubsampling()) << std::endl;
    std::cout << "info: Total Frames Decoded: " << nTotal << std::endl;
    std::cout << "info: avg decoding time per frame (ms): " << 1000 / totalFPS  << std::endl;
    std::cout << "info: avg FPS: " << totalFPS << std::endl;
    return 0;
}
