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
#include "parser_handle.h"
#include "../commons.h"


/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocParserStatus ROCDECAPI rocDecCreateVideoParser(RocdecVideoParser *parser_handle, RocdecParserParams *parser_params)
//! Create video parser object and initialize
/************************************************************************************************/
rocDecStatus ROCDECAPI 
rocDecCreateVideoParser(RocdecVideoParser *parser_handle, RocdecParserParams *parser_params) {
    if (parser_handle == nullptr || parser_params == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    RocdecVideoParser handle = nullptr;
    try {
        handle = new RocParserHandle(parser_params);
    } 
    catch(const std::exception& e) {
        ERR( STR("Failed to init the rocDecode handle, ") + STR(e.what()))
        return ROCDEC_RUNTIME_ERROR;
    }
    *parser_handle = handle;
    return rocDecStatus::ROCDEC_SUCCESS;
}

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocParserStatus ROCDECAPI rocDecParseVideoData(RocdecVideoParser parser_handle, RocdecSourceDataPacket *packet)
//! Parse the video data from source data packet in packet
//! Extracts parameter sets like SPS, PPS, bitstream etc. from packet and
//! calls back pfnDecodePicture with RocdecPicParams data for kicking of HW decoding
//! calls back pfnSequenceCallback with RocdecVideoFormat data for initial sequence header or when
//! the decoder encounters a video format change
//! calls back pfnDisplayPicture with ROCDECPARSERDISPINFO data to display a video frame
/************************************************************************************************/
rocDecStatus ROCDECAPI
rocDecParseVideoData(RocdecVideoParser parser_handle, RocdecSourceDataPacket *packet) {
    if (parser_handle == nullptr || packet == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto roc_parser_handle = static_cast<RocParserHandle *>(parser_handle);
    rocDecStatus ret;
    try {
        ret = roc_parser_handle->ParseVideoData(packet);
    }
    catch(const std::exception& e) {
        roc_parser_handle->CaptureError(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;  
}

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocDecStatus ROCDECAPI rocDecDestroyVideoParser(RocdecVideoParser parser_handle)
//! Destroy the video parser object
/************************************************************************************************/
extern rocDecStatus ROCDECAPI
rocDecDestroyVideoParser(RocdecVideoParser parser_handle) {
    if (parser_handle == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    auto roc_parser_handle = static_cast<RocParserHandle *>(parser_handle);
    rocDecStatus ret;
    try {
        ret = roc_parser_handle->DestroyParser();
    }
    catch(const std::exception& e) {
        roc_parser_handle->CaptureError(e.what());
        ERR(e.what())
        return ROCDEC_RUNTIME_ERROR;
    }
    return ret;
}
