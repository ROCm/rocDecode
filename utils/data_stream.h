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

/*!
 * \file
 * \brief The rocDecode Data Stream.
 *
 * \defgroup group_rocdecode_parser Parser definitions.
 * \brief The data stream for Bit Stream Parser.
 */

#ifndef DATASTREAM_H
#define DATASTREAM_H
#pragma once

#include "result.h"
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <memory>
#include <algorithm>

enum PARSER_SEEK_ORIGIN {
    PARSER_SEEK_BEGIN          = 0,
    PARSER_SEEK_CURRENT        = 1,
    PARSER_SEEK_END            = 2,
};


class DataStream {
public:
    DataStream(uint8_t *pData);
    virtual ~DataStream();
    // interface
    virtual PARSER_RESULT           Open() { return PARSER_OK; };
    virtual PARSER_RESULT           Close();
    virtual PARSER_RESULT           Read(void* pData, size_t iSize, size_t* pRead);
    virtual PARSER_RESULT           Write(const void* pData, size_t iSize, size_t* pWritten);
    virtual PARSER_RESULT           Seek(PARSER_SEEK_ORIGIN eOrigin, int64_t iPosition, int64_t* pNewPosition);
    virtual PARSER_RESULT           GetPosition(int64_t* pPosition);
    virtual PARSER_RESULT           GetSize(int64_t* pSize);
    virtual bool                    IsSeekable();

protected:
    PARSER_RESULT Realloc(size_t iSize);

    uint8_t* m_pMemory_;
    size_t m_uiMemorySize_;
    size_t m_uiAllocatedSize_;
    size_t m_pos_;
};
//----------------------------------------------------------------------------------------------
// smart pointer
//----------------------------------------------------------------------------------------------
typedef std::shared_ptr<DataStream> DataStreamPtr;
//----------------------------------------------------------------------------------------------
#endif // DATASTREAM_H