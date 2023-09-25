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
#include <filesystem>
#include <fstream>
#include "video_demuxer.hpp"
#include "../../src/parser/common/bit_stream_parser.h"
#include "rocdecode.h"

void ShowHelpAndExit(const char *option = NULL) {
    std::cout << "Options:" << std::endl
    << "-i Input File Path - required" << std::endl
    << "-o Output File Path - dumps output if requested; optional" << std::endl
    << "-d GPU device ID (0 for the first device, 1 for the second, etc.); optional; default: 0" << std::endl;
    exit(0);
}

int main(int argc, char **argv) {

    std::string inputFilePath, outputFilePath;
    int dumpOutputFrames = 0;
    int isOutputRGB = 0;
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
        ShowHelpAndExit(argv[i]);
    }
   
    ParserContext context;
    BitStreamParserPtr parser;
    DataStream *datastream;
    PARSER_RESULT res;
    VideoDemuxer demuxer(inputFilePath.c_str());

    // initializing parser
    res = DataStream::OpenDataStream(&datastream);
    parser = BitStreamParser::Create(datastream, BitStream265AnnexB, &context);
    //VideoDecode viddec(deviceId);

    std::string deviceName, gcnArchName, drmNode;
    int pciBusID, pciDomainID, pciDeviceID;

    /*viddec.getDeviceinfo(deviceName, gcnArchName, pciBusID, pciDomainID, pciDeviceID, drmNode);
    std::cout << "info: Using GPU device " << deviceId << ": (drm node: " << drmNode << ") " << deviceName << "[" << gcnArchName << "] on PCI bus " <<
    std::setfill('0') << std::setw(2) << std::right << std::hex << pciBusID << ":" << std::setfill('0') << std::setw(2) <<
    std::right << std::hex << pciDomainID << "." << pciDeviceID << std::dec << std::endl;
    std::cout << "info: decoding started, please wait!" << std::endl;*/

    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = nullptr;
    uint8_t *pFrame = nullptr;
    int64_t pts = 0;
    //outputImageInfo *pImageInfo;
    bool bDecodeOutSemiPlanar = false;

    uint32_t width, height;
    //vcnImageFormat_t subsampling;
    double totalDecTime = 0;
    int tempCount = 0;
    ParserData* data;
    ParserBuffer* outputBuffer;
    bool bNeedNewInput = true;
    std::ofstream fp;
    if (dumpOutputFrames) {
        fp.open(outputFilePath.c_str(), std::ofstream::out | std::ofstream::app | std::ofstream::binary);
    }

    do {
        auto startTime = std::chrono::high_resolution_clock::now();
        demuxer.Demux(&pVideo, &nVideoBytes, &pts);
        if (parser->CheckDataStreamEof(nVideoBytes)) {
            break;
        }
        res = datastream->Write(pVideo, nVideoBytes, 0);
        
        if (res != PARSER_OK) {
            std::cerr << "ERROR: Write to datastream failed" << res << std::endl;
            return res; 
        }
        parser->FindFirstFrameSPSandPPS();
        if (bNeedNewInput) {
            data = NULL;
            res = parser->QueryOutput(&data); // read compressed frame into buffer
            outputBuffer = static_cast<ParserBuffer*>(data);
            if(res == PARSER_EOF || data == NULL) {
                break;// end of file
            }
        }

        //nFrameReturned = viddec.decode(pVideo, nVideoBytes, pts);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto timePerFrame = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        totalDecTime += timePerFrame;
        /*if (!nFrame && !viddec.getOutputImageInfo(&pImageInfo)){
            std::cerr << "Error: Failed to get Output Image Info!" << std::endl;
            break;
        }*/

        if (dumpOutputFrames) {
            std::cout << "size of buffer to write = " << outputBuffer->GetSize() << std::endl;
            if (fp.is_open()) {
                fp.write((char*)outputBuffer->GetNative(), outputBuffer->GetSize());
            }
            else {
                std::cout << "file not open" << std::endl;
                exit(0);
            }
            /*for (int i = 0; i < nFrameReturned; i++) {
                pFrame = viddec.getFrame(&pts);
                viddec.saveImage(outputFilePath, pFrame, pImageInfo, false);
                // release frame
                viddec.releaseFrame(pts);
            }*/
        }
        nFrame++;
        //nFrame += nFrameReturned;
    } while (nVideoBytes);
     // Flush last frames from the decoder if any
    do {
        // send null packet to decoder to flush out
        pVideo = nullptr; nVideoBytes = 0;
        int64_t pts = 0;
        //nFrameReturned = viddec.decode(pVideo, nVideoBytes, pts);
    } while (nFrameReturned);

    /*std::cout << "info: Video codec format: " << viddec.getCodecFmtName(viddec.getVcnVideoCodecId()) << std::endl;
    std::cout << "info: Video size: [ " << pImageInfo->nOutputWidth << ", " << pImageInfo->nOutputHeight << " ]" << std::endl;
    std::cout << "info: Video pix format: " << viddec.getPixFmtName(pImageInfo->chromaFormat) << std::endl;
    std::cout << "info: Video Bit depth: " << pImageInfo->nBitDepth << std::endl;
    std::cout << "info: Total frame decoded: " << nFrame << std::endl;
    if (!dumpOutputFrames) {
        std::cout << "info: avg decoding time per frame (ms): " << totalDecTime / nFrame << std::endl;
        std::cout << "info: avg FPS: " << (nFrame / totalDecTime) * 1000 << std::endl;
    }*/

    return 0;
}
