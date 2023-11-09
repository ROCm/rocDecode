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
    << "-of Output Format - (0: native, 1: bgr 2: bgr48, 3: rgb, 4: rgb48; 5: bgra, 6: bgra64; converts native YUV frame to RGB image format; optional; default: 0" << std::endl
    << "-crop crop rectangle for output (not used when using interopped decoded frame); optional; default: 0" << std::endl;

    exit(0);
}

void dumpRGBImage(std::string outputfileName, void* pdevMem, OutputSurfaceInfo *pSurfInfo, int rgbImageSize)
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

void colorConvertYUV2RGB(uint8_t *pSrc, OutputSurfaceInfo *pSurfInfo, uint8_t *pRGBdevMem, OutputFormatEnum eOutputFormat) {
    
    int  nRgbWidth = (pSurfInfo->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
    // todo:: get color standard from the decoder
    if (pSurfInfo->surface_format == rocDecVideoSurfaceFormat_YUV444) {
        if (eOutputFormat == bgr)
          YUV444ToColor24<BGR24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == bgra)
          YUV444ToColor32<BGRA32>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgb)
          YUV444ToColor24<RGB24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgba)
          YUV444ToColor32<RGBA32>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
    }
    else if (pSurfInfo->surface_format == rocDecVideoSurfaceFormat_NV12) {
        if (eOutputFormat == bgr)
          Nv12ToColor24<BGR24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == bgra)
          Nv12ToColor32<BGRA32>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgb)
          Nv12ToColor24<RGB24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgba)
          Nv12ToColor32<RGBA32>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 4 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
    }
    if (pSurfInfo->surface_format == rocDecVideoSurfaceFormat_YUV444_16Bit) {
        if (eOutputFormat == bgr)
          YUV444P16ToColor24<BGR24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgb)
          YUV444P16ToColor24<RGB24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == bgr48)
          YUV444P16ToColor48<BGR48>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgb48)
          YUV444P16ToColor48<RGB48>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == bgra64)
          YUV444P16ToColor64<BGRA64>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgba64)
          YUV444P16ToColor64<RGBA64>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pSurfInfo->output_width, 
                                pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
    }
    else if (pSurfInfo->surface_format == rocDecVideoSurfaceFormat_P016) {
        if (eOutputFormat == bgr)
          P016ToColor24<BGR24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgb)
          P016ToColor24<RGB24>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 3 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == bgr48)
          P016ToColor48<BGR48>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgb48)
          P016ToColor48<RGB48>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 6 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == bgra64)
          P016ToColor64<BGRA64>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
        else if (eOutputFormat == rgba64)
          P016ToColor64<RGBA64>(pSrc, pSurfInfo->output_pitch, (uint8_t *)pRGBdevMem, 8 * nRgbWidth, pSurfInfo->output_width, 
                              pSurfInfo->output_height, pSurfInfo->output_vstride, 0);
    }

}

int main(int argc, char **argv) {

    std::string inputFilePath, outputFilePath;
    int dumpOutputFrames = 0;
    int convertToRGB = 0;
    int deviceId = 0;
    Rect crop_rect = {};
    Rect *p_crop_rect = nullptr;
    size_t rgbImageSize;
    uint32_t rgbImageStride;
    hipError_t hipStatus = hipSuccess;
    uint8_t *pRGBdevMem = nullptr;
    OUTPUT_SURF_MEMORY_TYPE mem_type = OUT_SURFACE_MEM_DEV_INTERNAL;     // set to internal
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
    rocDecVideoCodec rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer.GetCodecID());
    RocVideoDecoder viddec(deviceId, mem_type, rocdec_codec_id, false, true, p_crop_rect);

    std::string deviceName, gcnArchName;
    int pciBusID, pciDomainID, pciDeviceID;

    viddec.GetDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID);
    std::cout << "info: Using GPU device " << deviceId << deviceName << "[" << gcnArchName << "] on PCI bus " <<
    std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
    std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
    std::cout << "info: decoding started, please wait!" << std::endl;

    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = nullptr;
    uint8_t *pFrame = nullptr;
    int64_t pts = 0;
    OutputSurfaceInfo *pSurfInfo;
    uint32_t width, height;
    double totalDecTime = 0;
    convertToRGB = eOutputFormat != native;

    do {
        auto startTime = std::chrono::high_resolution_clock::now();
        demuxer.Demux(&pVideo, &nVideoBytes, &pts);
        nFrameReturned = viddec.DecodeFrame(pVideo, nVideoBytes, 0, pts);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto timePerFrame = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        totalDecTime += timePerFrame;
        if (!nFrame && !viddec.GetOutputSurfaceInfo(&pSurfInfo)){
            std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
            break;
        }

        for (int i = 0; i < nFrameReturned; i++) {
            pFrame = viddec.GetFrame(&pts);
            if (convertToRGB) {
                if (pSurfInfo->bit_depth == 8) {
                    nRgbWidth = (pSurfInfo->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                    rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pSurfInfo->output_height * 3 : nRgbWidth * pSurfInfo->output_height * 4; 
                } else {    // 16bit
                    nRgbWidth = (pSurfInfo->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                    rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pSurfInfo->output_height * 3 : ((eOutputFormat == bgr48) || (eOutputFormat == rgb48)) ? 
                                                          nRgbWidth * pSurfInfo->output_height * 6 : nRgbWidth * pSurfInfo->output_height * 8; 
                }
                if (pRGBdevMem == nullptr) {
                    hipStatus = hipMalloc(&pRGBdevMem, rgbImageSize);
                    if (hipStatus != hipSuccess) {
                        std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hipStatus << std::endl;
                        return -1;
                    }
                }
                colorConvertYUV2RGB(pFrame, pSurfInfo, pRGBdevMem, eOutputFormat);
            }
            if (dumpOutputFrames) {
                if (convertToRGB)
                    dumpRGBImage(outputFilePath, pRGBdevMem, pSurfInfo, rgbImageSize);
                else
                    viddec.SaveSurfToFile(outputFilePath, pFrame, pSurfInfo);
            }
            // release frame
            viddec.ReleaseFrame(pts);
        }
        nFrame += nFrameReturned;
    } while (nVideoBytes);

#if 0   // flushing is not required in rocDecode. Parser is supposed to flush frames after it decodes the last frames
     // Flush last frames from the decoder if any
    do {
        // send null packet to decoder to flush out
        pVideo = nullptr; nVideoBytes = 0;
        int64_t pts = 0;
        nFrameReturned = viddec.decode(pVideo, nVideoBytes, pts);
        if (!nFrame && !viddec.getOutputImageInfo(&pSurfInfo)){
            std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
            break;
        }

        for (int i = 0; i < nFrameReturned; i++) {
            pFrame = viddec.getFrame(&pts);
            if (convertToRGB) {
                if (pSurfInfo->bit_depth == 8) {
                    nRgbWidth = (pSurfInfo->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                    rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pSurfInfo->output_height * 3 : nRgbWidth * pSurfInfo->output_height * 4; 
                } else {    // 16bit
                    nRgbWidth = (pSurfInfo->output_width + 1) & ~1;    // has to be a multiple of 2 for hip colorconvert kernels
                    rgbImageSize = ((eOutputFormat == bgr) || (eOutputFormat == rgb)) ? nRgbWidth * pSurfInfo->output_height * 3 : ((eOutputFormat == bgr48) || (eOutputFormat == rgb48)) ? 
                                                          nRgbWidth * pSurfInfo->output_height * 6 : nRgbWidth * pSurfInfo->output_height * 8; 
                }
                if (pRGBdevMem == nullptr) {
                    hipStatus = hipMalloc(&pRGBdevMem, rgbImageSize);
                    if (hipStatus != hipSuccess) {
                        std::cerr << "ERROR: hipMalloc failed to allocate the device memory for the output!" << hipStatus << std::endl;
                        return -1;
                    }
                }
                colorConvertYUV2RGB(pFrame, pSurfInfo, pRGBdevMem, eOutputFormat);
            }
            if (dumpOutputFrames) {
                if (convertToRGB)
                    dumpRGBImage(outputFilePath, pRGBdevMem, pSurfInfo, rgbImageSize);
                else
                    viddec.saveImage(outputFilePath, pFrame, pSurfInfo, convertToRGB);
            }
            // release frame
            viddec.releaseFrame(pts);
        }
        nFrame += nFrameReturned;
    } while (nFrameReturned);
#endif
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

    std::cout << "info: Video codec format: " << viddec.GetCodecFmtName(viddec.GetCodecId()) << std::endl;
    std::cout << "info: Video size: [ " << pSurfInfo->output_width << ", " << pSurfInfo->output_height << " ]" << std::endl;
    std::cout << "info: Video surface format: " << viddec.GetSurfaceFmtName(pSurfInfo->surface_format) << std::endl;
    std::cout << "info: Video Bit depth: " << pSurfInfo->bit_depth << std::endl;
    std::cout << "info: Total frame decoded: " << nFrame << std::endl;
    if (!dumpOutputFrames) {
        std::cout << "info: avg decoding time per frame (ms): " << totalDecTime / nFrame << std::endl;
        std::cout << "info: avg FPS: " << (nFrame / totalDecTime) * 1000 << std::endl;
    }

    return 0;
}
