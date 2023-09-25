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

#ifndef PARSERDATA_H
#define PARSERDATA_H
#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <ctime>
#include <chrono>
#include "result.h"

typedef enum PARSER_MEMORY_TYPE {
    PARSER_MEMORY_UNKNOWN          = 0,
    PARSER_MEMORY_HOST             = 1,
    PARSER_MEMORY_HIP              = 2,
} PARSER_MEMORY_TYPE;

class ParserData {
public:
    virtual bool           IsReusable() = 0;
    virtual void           SetPts(int64_t pts) = 0;
    virtual int64_t        GetPts() const = 0;
    virtual void           SetDuration(int64_t duration) = 0;
    virtual int64_t        GetDuration() const = 0;
};

// smart pointer
typedef std::shared_ptr<ParserData> ParserDataPtr;

#endif // PARSERDATA_H