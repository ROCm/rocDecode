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
#pragma once

#include <memory>
#include <string>
#include "rocparser.h"
#include "roc_video_parser.h"
#include "h264_parser.h"
#include "hevc_parser.h"

class RocParserHandle {
public:    
    explicit RocParserHandle(RocdecParserParams *pParams) { create_parser(pParams); };   // default constructor
    ~RocParserHandle() { clear_errors(); }
    bool no_error() { return error.empty(); }
    const char* error_msg() { return error.c_str(); }
    void capture_error(const std::string& err_msg) { error = err_msg; }
    rocDecStatus ParseVideoData(RocdecSourceDataPacket *pPacket) { return roc_parser_->ParseVideoData(pPacket); }
    rocDecStatus DestroyParser() { return destroy_parser(); };

private:
    std::shared_ptr<RocVideoParser> roc_parser_ = nullptr;    // class instantiation
    void clear_errors() { error = ""; }
    void create_parser(RocdecParserParams *pParams) {
        switch(pParams->CodecType) {
            case rocDecVideoCodec_H264:
                roc_parser_ = std::make_shared<H264VideoParser>();
                break;
            case rocDecVideoCodec_HEVC:
                roc_parser_ = std::make_shared<HEVCVideoParser>();
                break;
            default:
                THROW("Unsupported parser type "+ TOSTR(pParams->CodecType));
                break;
        }

        if (roc_parser_ ) {
            rocDecStatus ret = roc_parser_->Initialize(pParams);
            if (ret != ROCDEC_SUCCESS)
                THROW("rocParser Initialization failed with error: "+ TOSTR(ret));
        }
    }
    rocDecStatus destroy_parser() {
      rocDecStatus ret = ROCDEC_NOT_INITIALIZED;
        if (roc_parser_ ) {
            ret = roc_parser_->UnInitialize();
            if (ret != ROCDEC_SUCCESS)
                THROW("rocParser UnInitialization failed with error: "+ TOSTR(ret));
        }
        return ret;
    }

    std::string error;
};