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
#include "colorspace_kernels.hpp"

FILE *fpOut = nullptr;
enum OutputFormatEnum
{
    native = 0, bgr, bgr48, rgb, rgb48, bgra, bgra64, rgba, rgba64
};
std::vector<std::string> stOutputForamtName = {"native", "bgr", "bgr48", "rgb", "rgb48", "bgra", "bgra64", "rgba", "rgba64"};

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-of Output Format - (0: native, 1: bgr 2: bgr48, 3: rgb, 4: rgb48; 5: bgra, 6: bgra64; converts native YUV frame to RGB image format; optional; default: 0" << std::endl;
    exit(0);
}

void dumpRGBImage(std::string outputfileName, void* pdevMem, outputImageInfo *pImageInfo, int rgbImageSize)
{
    if (fpOut == nullptr) {
        fpOut = fopen(outputfileName.c_str(), "wb");
    }
    uint8_t *hstPtr = nullptr;
    hstPtr = new uint8_t [rgbImageSize];
    hipError_t hipStatus = hipSuccess;
    hipStatus = hipMemcpyDtoH((void *)hstPtr, pdevMem, rgbImageSize);
    if (hipStatus != hipSuccess) {
        std::cout << "ERROR: hipMemcpyDtoH failed! (" << hipStatus << ")" << std::endl;
        delete [] hstPtr;
        return;
    }
    if (fpOut) {
        fwrite(hstPtr, 1, rgbImageSize, fpOut);
    }

    if (hstPtr != nullptr) {
        delete [] hstPtr;
        hstPtr = nullptr;
    }
}

int main(int argc, char **argv) {

    std::string inputFilePath, outputFilePath;
    int dumpOutputFrames = 0;
    int convertToRGB = 0;
    int deviceId = 0;
    size_t rgbImageSize;
    uint32_t rgbImageStride;
    hipError_t hipStatus = hipSuccess;
    uint8_t *pRGBdevMem = nullptr;
    OutputFormatEnum eOutputFormat = native; 
    int nRgbWidth;

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
        if (!strcmp(argv[i], "-of")) {
            if (++i == argc) {
                ShowHelpAndExit("-of");
            }
            auto it = find(stOutputForamtName.begin(), stOutputForamtName.end(), argv[i]);
            if (it == stOutputForamtName.end()) {
                ShowHelpAndExit("-of");
            }
            eOutputFormat = (OutputFormatEnum)(it-stOutputForamtName.begin());
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    VideoDemuxer demuxer(inputFilePath.c_str());
    VideoDecode viddec(deviceId);

    std::string deviceName, gcnArchName, drmNode;
    int pciBusID, pciDomainID, pciDeviceID;

    viddec.getDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID, drmNode);
    std::cout << "info: Using GPU device " << deviceId << ": (drm node: " << drmNode << ") " << deviceName << "[" << gcnArchName << "] on PCI bus " <<
    std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
    std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
    std::cout << "info: decoding started, please wait!" << std::endl;

    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = nullptr;
    uint8_t *pFrame = nullptr;
    int64_t pts = 0;
    outputImageInfo *pImageInfo;

    uint32_t width, height;
    vcnImageFormat_t subsampling;
    double totalDecTime = 0;
    convertToRGB = eOutputFormat != native;

    do {
        auto startTime = std::chrono::high_resolution_clock::now();
        demuxer.demux(&pVideo, &nVideoBytes, &pts);
        nFrameReturned = viddec.decode(pVideo, nVideoBytes, pts);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto timePerFrame = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        totalDecTime += timePerFrame;
        if (!nFrame && !viddec.getOutputImageInfo(&pImageInfo)){
            std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
            break;
        }

        for (int i = 0; i < nFrameReturned; i++) {
            pFrame = viddec.getFrame(&pts);
            if (convertToRGB) { 
                  if (pImageInfo->nBitDepth == 8) {
                    nRgbWidth = (pImageInfo->nOutputWidth + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                    rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pImageInfo->nOutputHeight * 3 : nRgbWidth * pImageInfo->nOutputHeight * 4; 
                    if (pRGBdevMem == nullptr) {
                        hipStatus = hipMalloc(&pRGBdevMem, rgbImageSize);
                        if (hipStatus != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hipStatus << std::endl;
                            return -1;
                        }
                    }
                    // todo:: get color standard from the decoder
                    if (pImageInfo->chromaFormat == VCN_FMT_YUV444) {
                        if (eOutputFormat == bgr)
                          YUV444ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra)
                          YUV444ToColor32<BGRA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          YUV444ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba)
                          YUV444ToColor32<RGBA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                    else if (pImageInfo->chromaFormat == VCN_FMT_YUV420) {
                        if (eOutputFormat == bgr)
                          Nv12ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra)
                          Nv12ToColor32<BGRA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          Nv12ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba)
                          Nv12ToColor32<RGBA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                }
                else {
                    if (eOutputFormat != native) {
                        nRgbWidth = (pImageInfo->nOutputWidth + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                        rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pImageInfo->nOutputHeight * 3 : ((eOutputFormat == bgr48) || (eOutputFormat == rgb48)) ? 
                                                               nRgbWidth * pImageInfo->nOutputHeight * 6 : nRgbWidth * pImageInfo->nOutputHeight * 8; 
                    }
                    if (pRGBdevMem == nullptr && eOutputFormat != native) {
                        hipStatus = hipMalloc(&pRGBdevMem, rgbImageSize);
                        if (hipStatus != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hipStatus << std::endl;
                            return -1;
                        }
                    }
                    // todo:: get color standard from the decoder
                    if (pImageInfo->chromaFormat == VCN_FMT_YUV444) {
                        if (eOutputFormat == bgr)
                          YUV444P16ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          YUV444P16ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgr48)
                          YUV444P16ToColor48<BGR48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb48)
                          YUV444P16ToColor48<RGB48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra64)
                          YUV444P16ToColor64<BGRA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba64)
                          YUV444P16ToColor64<RGBA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                    else if (pImageInfo->chromaFormat == VCN_FMT_YUV420P10 || pImageInfo->chromaFormat == VCN_FMT_YUV420P12) {
                        if (eOutputFormat == bgr)
                          P016ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          P016ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgr48)
                          P016ToColor48<BGR48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb48)
                          P016ToColor48<RGB48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra64)
                          P016ToColor64<BGRA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba64)
                          P016ToColor64<RGBA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                }
            }
            if (dumpOutputFrames) {
                if (convertToRGB)
                    dumpRGBImage(outputFilePath, pRGBdevMem, pImageInfo, rgbImageSize);
                else
                    viddec.saveImage(outputFilePath, pFrame, pImageInfo, convertToRGB);
            }
            // release frame
            viddec.releaseFrame(pts);
        }
        nFrame += nFrameReturned;
    } while (nVideoBytes);

     // Flush last frames from the decoder if any
    do {
        // send null packet to decoder to flush out
        pVideo = nullptr; nVideoBytes = 0;
        int64_t pts = 0;
        nFrameReturned = viddec.decode(pVideo, nVideoBytes, pts);
        if (!nFrame && !viddec.getOutputImageInfo(&pImageInfo)){
            std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
            break;
        }

        for (int i = 0; i < nFrameReturned; i++) {
            pFrame = viddec.getFrame(&pts);
            if (convertToRGB) { 
                  if (pImageInfo->nBitDepth == 8) {
                    nRgbWidth = (pImageInfo->nOutputWidth + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                    rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pImageInfo->nOutputHeight * 3 : nRgbWidth * pImageInfo->nOutputHeight * 4; 
                    if (pRGBdevMem == nullptr) {
                        hipStatus = hipMalloc(&pRGBdevMem, rgbImageSize);
                        if (hipStatus != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hipStatus << std::endl;
                            return -1;
                        }
                    }
                    // todo:: get color standard from the decoder
                    if (pImageInfo->chromaFormat == VCN_FMT_YUV444) {
                        if (eOutputFormat == bgr)
                          YUV444ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra)
                          YUV444ToColor32<BGRA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          YUV444ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba)
                          YUV444ToColor32<RGBA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                    else if (pImageInfo->chromaFormat == VCN_FMT_YUV420) {
                        if (eOutputFormat == bgr)
                          Nv12ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra)
                          Nv12ToColor32<BGRA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          Nv12ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba)
                          Nv12ToColor32<RGBA32>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                }
                else {
                    if (eOutputFormat != native) {
                        nRgbWidth = (pImageInfo->nOutputWidth + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                        rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pImageInfo->nOutputHeight * 3 : ((eOutputFormat == bgr48) || (eOutputFormat == rgb48)) ? 
                                                               nRgbWidth * pImageInfo->nOutputHeight * 6 : nRgbWidth * pImageInfo->nOutputHeight * 8; 
                    }
                    if (pRGBdevMem == nullptr && eOutputFormat != native) {
                        hipStatus = hipMalloc(&pRGBdevMem, rgbImageSize);
                        if (hipStatus != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hipStatus << std::endl;
                            return -1;
                        }
                    }
                    // todo:: get color standard from the decoder
                    if (pImageInfo->chromaFormat == VCN_FMT_YUV444) {
                        if (eOutputFormat == bgr)
                          YUV444P16ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          YUV444P16ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgr48)
                          YUV444P16ToColor48<BGR48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb48)
                          YUV444P16ToColor48<RGB48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra64)
                          YUV444P16ToColor64<BGRA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba64)
                          YUV444P16ToColor64<RGBA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                                pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                    else if (pImageInfo->chromaFormat == VCN_FMT_YUV420P10 || pImageInfo->chromaFormat == VCN_FMT_YUV420P12) {
                        if (eOutputFormat == bgr)
                          P016ToColor24<BGR24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb)
                          P016ToColor24<RGB24>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgr48)
                          P016ToColor48<BGR48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgb48)
                          P016ToColor48<RGB48>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == bgra64)
                          P016ToColor64<BGRA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                        else if (eOutputFormat == rgba64)
                          P016ToColor64<RGBA64>(pFrame, pImageInfo->nOutputHStride, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pImageInfo->nOutputWidth, 
                                              pImageInfo->nOutputHeight, pImageInfo->nOutputVStride, 0);
                    }
                }
            }
            if (dumpOutputFrames) {
                if (convertToRGB)
                    dumpRGBImage(outputFilePath, pRGBdevMem, pImageInfo, rgbImageSize);
                else
                    viddec.saveImage(outputFilePath, pFrame, pImageInfo, convertToRGB);
            }
            // release frame
            viddec.releaseFrame(pts);
        }
        nFrame += nFrameReturned;
    } while (nFrameReturned);

    if (pRGBdevMem != nullptr) {
        hipStatus = hipFree(pRGBdevMem);
        if (hipStatus != hipSuccess) {
            std::cout << "ERROR: hipFree failed! (" << hipStatus << ")" << std::endl;
            return -1;
        }
    }
    if (fpOut) {
      fclose(fpOut);
      fpOut = nullptr;
    }

    std::cout << "info: Video codec format: " << viddec.getCodecFmtName(viddec.getVcnVideoCodecId()) << std::endl;
    std::cout << "info: Video size: [ " << pImageInfo->nOutputWidth << ", " << pImageInfo->nOutputHeight << " ]" << std::endl;
    std::cout << "info: Video pix format: " << viddec.getPixFmtName(pImageInfo->chromaFormat) << std::endl;
    std::cout << "info: Video Bit depth: " << pImageInfo->nBitDepth << std::endl;
    std::cout << "info: Total frame decoded: " << nFrame << std::endl;
    if (!dumpOutputFrames) {
        std::cout << "info: avg decoding time per frame (ms): " << totalDecTime / nFrame << std::endl;
        std::cout << "info: avg FPS: " << (nFrame / totalDecTime) * 1000 << std::endl;
    }

    return 0;
}
