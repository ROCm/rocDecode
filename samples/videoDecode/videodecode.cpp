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
#include <libgen.h>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
#else
    #include <experimental/filesystem>
#endif
#include "video_demuxer.hpp"
#include "roc_video_dec.h"

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-crop crop rectangle for output (not used when using interopped decoded frame); optional; default: 0" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string inputFilePath, outputFilePath;
    int dumpOutputFrames = 0;
    int isOutputRGB = 0;
    int deviceId = 0;
    Rect crop_rect = {};
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
        if (!strcmp(argv[i], "-o")) {
            if (++i == argc) {
                ShowHelpAndExit("-o");
            }
            outputFilePath = argv[i];
            dumpOutputFrames = 1;
            continue;
        }
        if (!strcmp(argv[i], "-d")) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            deviceId = atoi(argv[i]);
            continue;
        }
        if (!strcmp(argv[i], "-crop")) {
            if (++i == argc || 4 != sscanf(argv[i], "%d,%d,%d,%d", &crop_rect.l, &crop_rect.t, &crop_rect.r, &crop_rect.b)) {
                ShowHelpAndExit("-crop");
            }
            if ((crop_rect.r - crop_rect.l) % 2 == 1 || (crop_rect.b - crop_rect.t) % 2 == 1) {
                std::cout << "output crop rectangle must have width and height of even numbers" << std::endl;
                exit(1);
            }
            p_crop_rect = &crop_rect;
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
    try {
        VideoDemuxer demuxer(inputFilePath.c_str());
        rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
        RocVideoDecoder viddec(deviceId, mem_type, rocdec_codec_id, false, true, p_crop_rect);

        std::string deviceName, gcnArchName, drmNode;
        int pciBusID, pciDomainID, pciDeviceID;

        viddec.GetDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID);
        std::cout << "info: Using GPU device " << deviceId << " - " << deviceName << "[" << gcnArchName << "] on PCI bus " <<
        std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
        std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
        std::cout << "info: decoding started, please wait!" << std::endl;

        int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
        uint8_t *pVideo = nullptr;
        uint8_t *pFrame = nullptr;
        int64_t pts = 0;
        OutputSurfaceInfo *pSurfInfo;
        bool bDecodeOutSemiPlanar = false;
        uint32_t width, height;
        double totalDecTime = 0;

        do {
            auto startTime = std::chrono::high_resolution_clock::now();
            demuxer.Demux(&pVideo, &nVideoBytes, &pts);
            nFrameReturned = viddec.DecodeFrame(pVideo, nVideoBytes, 0, pts);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto timePerFrame = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            totalDecTime += timePerFrame;
            if (!nFrame && !viddec.GetOutputSurfaceInfo(&pSurfInfo)) {
                std::cerr << "Error: Failed to get Output Surface Info!" << std::endl;
                break;
            }

            if (dumpOutputFrames) {
                for (int i = 0; i < nFrameReturned; i++) {
                    pFrame = viddec.GetFrame(&pts);
                    viddec.SaveSurfToFile(outputFilePath, pFrame, pSurfInfo);
                    // release frame
                    viddec.ReleaseFrame(pts);
                }
            }
            nFrame += nFrameReturned;
        } while (nVideoBytes);
    #if 0   // is flushing required?
        // Flush last frames from the decoder if any
        do {
            // send null packet to decoder to flush out
            pVideo = nullptr; nVideoBytes = 0;
            int64_t pts = 0;
            //nFrameReturned = viddec.decode(pVideo, nVideoBytes, pts);
        } while (nFrameReturned);
    #endif
        std::cout << "info: Video codec format: " << viddec.GetCodecFmtName(viddec.GetCodecId()) << std::endl;
        std::cout << "info: Video size: [ " << pSurfInfo->output_width << ", " << pSurfInfo->output_height << " ]" << std::endl;
        std::cout << "info: Video surface format: " << viddec.GetSurfaceFmtName(pSurfInfo->surface_format) << std::endl;
        std::cout << "info: Video Bit depth: " << pSurfInfo->bit_depth << std::endl;
        std::cout << "info: Total frame decoded: " << nFrame << std::endl;
        if (!dumpOutputFrames) {
            std::cout << "info: avg decoding time per frame (ms): " << totalDecTime / nFrame << std::endl;
            std::cout << "info: avg FPS: " << (nFrame / totalDecTime) * 1000 << std::endl;
        }
    }catch (const std::exception &ex) {
      std::cout << ex.what();
      exit(1);
    }

    return 0;
}
