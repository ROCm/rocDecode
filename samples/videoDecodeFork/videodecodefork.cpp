/*
Copyright (c) 2023 - 2023 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <vector>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <libgen.h>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
#else
    #include <experimental/filesystem>
#endif
#include "video_demuxer.hpp"
#include "roc_video_dec.h"

static int *nTotal;

void DecProc(RocVideoDecoder *pDec, VideoDemuxer *demuxer, int *pnFrame) {
    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = nullptr;
    uint8_t *pFrame = nullptr;
    int64_t pts = 0;
    double totalDecTime = 0.0;
    auto startTime = std::chrono::high_resolution_clock::now();

    do {
        demuxer->Demux(&pVideo, &nVideoBytes, &pts);
        nFrameReturned = pDec->DecodeFrame(pVideo, nVideoBytes, 0, pts);
        nFrame += nFrameReturned;
    } while (nVideoBytes);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto timePerFrame = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Calculate average decoding time
    totalDecTime = timePerFrame;
    double averageDecodingTime = totalDecTime / nFrame;
    double nFPS = 1000 / averageDecodingTime;
}

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-t Number of threads (>= 1) - optional; default: 4" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string inputFilePath;
    int isOutputRGB = 0;
    int deviceId = 0;
    int n_fork = 4;
    Rect *p_crop_rect = nullptr;
    OUTPUT_SURF_MEMORY_TYPE mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;        // set to internal

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
            n_fork = atoi(argv[i]);
            if (n_fork <= 0) {
                ShowHelpAndExit(argv[i]);
            }
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    std::vector<std::unique_ptr<VideoDemuxer>> vDemuxer;
    std::vector<std::unique_ptr<RocVideoDecoder>> vViddec;
    std::vector<int> vDeviceId(n_fork);

    // TODO: Change this block to use VCN query API 
    int numDevices = 0;
    hipError_t hipStatus = hipSuccess;
    hipStatus = hipGetDeviceCount(&numDevices);
    if (hipStatus != hipSuccess) {
        std::cout << "ERROR: hipGetDeviceCount failed! (" << hipStatus << ")" << std::endl;
        return 1;
    }

    int sd = (numDevices >= 2) ? 0 : 0;

    for (int i = 0; i < n_fork; i++) {
        std::unique_ptr<VideoDemuxer> demuxer(new VideoDemuxer(inputFilePath.c_str()));
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer->GetCodecID());
        vDeviceId[i] = (i % 2 == 0) ? 0 : sd;
        std::unique_ptr<RocVideoDecoder> dec(new RocVideoDecoder(vDeviceId[i], mem_type, rocdec_codec_id, false, true, p_crop_rect));
        vDemuxer.push_back(std::move(demuxer));
        vViddec.push_back(std::move(dec));
    }

    float totalFPS = 0;
    std::vector<int> vFrame;
    vFrame.resize(n_fork, 0);

    std::string deviceName, gcnArchName, drmNode;
    int pciBusID, pciDomainID, pciDeviceID;

    for (int i = 0; i < n_fork; i++) {
        vViddec[i]->GetDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID);
        std::cout << "info: stream " << i << " using GPU device " << vDeviceId[i] << " - " << deviceName << "[" << gcnArchName << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
        std::cout << "info: decoding started for thread " << i << " ,please wait!" << std::endl;
    }

    int pid_status;
    pid_t pids[n_fork];
    nTotal = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *nTotal = 0;

    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n_fork; i++) {
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

    for(int i = 0; i < n_fork; i++) {	
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
