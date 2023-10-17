/*
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

#ifndef PARSERBUFFER_H
#define PARSERBUFFER_H
#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <ctime>
#include <chrono>
#include "roc_video_parser.h"

typedef enum ParserMemoryType {
    PARSER_MEMORY_UNKNOWN          = 0,
    PARSER_MEMORY_HOST             = 1,
    PARSER_MEMORY_HIP              = 2,
} ParserMemoryType;

class ParserBuffer  {
public:
    ParserBuffer();
    virtual ~ParserBuffer();

    virtual ParserResult      SetSize(size_t new_size);
    virtual size_t            GetSize();
    virtual void*             GetNative();
    virtual void              SetNative(size_t size);

    //parser data functions
    virtual bool              IsReusable();
    virtual void              SetPts(int64_t pts);
    virtual int64_t           GetPts() const;
    virtual void              SetDuration(int64_t duration);
    virtual int64_t           GetDuration() const;

    static ParserResult       AllocBuffer(ParserMemoryType type, size_t size, ParserBuffer** pp_buffer);

private:
    int64_t     m_current_timestamp_;
    int64_t     m_duration_;
    size_t      m_packet_size_;
    uint8_t*    m_buffer_;
};

// smart pointer
typedef std::shared_ptr<ParserBuffer> ParserBufferPtr;

#endif // PARSERBUFFER_H