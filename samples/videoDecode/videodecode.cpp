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
#include "video_demuxer.hpp"
#include "rocdecode.h"

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl
    << "-c convert YUV to floting point Tensor - 1: converts the decoded YUV frame to a floating point tensor); optional; default: 0" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string inputFilePath, outputFilePath;
    int dumpOutputFrames = 0;
    int deviceId = 0;
    int convertToTensor = 0;
    size_t rgbImageSize;
    uint32_t rgbImageStride;
    hipError_t hipStatus = hipSuccess;
    uint8_t *pRGBdevMem = nullptr;
    uint8_t *pTensor = nullptr;
    size_t tensorSize = 0;
    uint4 tensor_stride;
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
        if (!strcmp(argv[i], "-c")) {
            if (++i == argc) {
                ShowHelpAndExit("-c");
            }
            int temp = std::stoi(argv[i]);
            if (temp == 1) {
                convertToTensor = 1;
            }
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
    outputImageInfo imageInfoRGB;
    bool bDecodeOutSemiPlanar = false;

    uint32_t width, height;
    vcnImageFormat_t subsampling;
    double totalDecTime = 0;

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

        if (convertToTensor || dumpOutputFrames) {
            for (int i = 0; i < nFrameReturned; i++) {
                pFrame = viddec.getFrame(&pts);
                if (convertToTensor) {
                    size_t element_size = sizeof(float);
                    uint32_t num_channels = 1;
                    tensorSize = pImageInfo->nOutputWidth * pImageInfo->nOutputHeight * num_channels * element_size;
                    tensor_stride.x = element_size;
                    tensor_stride.y = tensor_stride.x * pImageInfo->nOutputWidth;
                    tensor_stride.z = tensor_stride.y * pImageInfo->nOutputHeight;
                    tensor_stride.w = tensor_stride.z * num_channels;
                    if (pTensor == nullptr) {
                        hipStatus = hipMalloc(&pTensor, tensorSize);
                        if (hipStatus != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the tensor!" << hipStatus << std::endl;
                            return -1;
                        }
                    }
                    //converts an U8 or U16 image to a floating-point tensor
                    if (!viddec.convertImageToTensor(pFrame, pImageInfo->nOutputWidth, pImageInfo->nOutputHeight, 1, pImageInfo->nBitDepth == 8 ? COLOR_U8 : COLOR_U16, TYPE_FLOAT32, 0,
                        pImageInfo->nOutputHStride, pTensor, 0, tensor_stride, 1, 0, false)) {
                            std::cerr << "ERROR: RGB to tensor conversion failed!" << hipStatus << std::endl;
                            viddec.releaseFrame(pts);
                            hipStatus = hipFree(pRGBdevMem);
                            hipStatus = hipFree(pTensor);
                            return -1;
                    }
                }
                if (dumpOutputFrames) {
                    viddec.saveImage(outputFilePath, pFrame, pImageInfo, false);
                }
                // release frame
                viddec.releaseFrame(pts);
            }
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

        if (convertToTensor || dumpOutputFrames) {
            for (int i = 0; i < nFrameReturned; i++) {
                pFrame = viddec.getFrame(&pts);
                if (convertToTensor) {
                    size_t element_size = sizeof(float);
                    uint32_t num_channels = 1;
                    tensorSize = pImageInfo->nOutputWidth * pImageInfo->nOutputHeight * num_channels * element_size;
                    tensor_stride.x = element_size;
                    tensor_stride.y = tensor_stride.x * pImageInfo->nOutputWidth;
                    tensor_stride.z = tensor_stride.y * pImageInfo->nOutputHeight;
                    tensor_stride.w = tensor_stride.z * num_channels;
                    if (pTensor == nullptr) {
                        hipStatus = hipMalloc(&pTensor, tensorSize);
                        if (hipStatus != hipSuccess) {
                            std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the tensor!" << hipStatus << std::endl;
                            return -1;
                        }
                    }
                    //converts an U8 or U16 image to a floating-point tensor
                    if (!viddec.convertImageToTensor(pFrame, pImageInfo->nOutputWidth, pImageInfo->nOutputHeight, 1, pImageInfo->nBitDepth == 8 ? COLOR_U8 : COLOR_U16, TYPE_FLOAT32, 0,
                        pImageInfo->nOutputHStride, pTensor, 0, tensor_stride, 1, 0, false)) {
                            std::cerr << "ERROR: RGB to tensor conversion failed!" << hipStatus << std::endl;
                            viddec.releaseFrame(pts);
                            hipStatus = hipFree(pRGBdevMem);
                            hipStatus = hipFree(pTensor);
                            return -1;
                    }
                }
                if (dumpOutputFrames) {
                    viddec.saveImage(outputFilePath, pFrame, pImageInfo, false);
                }
                // release frame
                viddec.releaseFrame(pts);
            }
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

    if (pTensor != nullptr) {
        hipStatus = hipFree(pTensor);
        if (hipStatus != hipSuccess) {
            std::cout << "ERROR: hipFree failed! (" << hipStatus << ")" << std::endl;
            return -1;
        }
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
