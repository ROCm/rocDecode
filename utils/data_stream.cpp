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

#include "data_stream.h"

DataStream::DataStream() {
    m_pMemory_ = new uint8_t [DATA_STREAM_SIZE];
    m_uiAllocatedSize_ = DATA_STREAM_SIZE;
    m_uiMemorySize_ = sizeof(m_pMemory_);
    m_pos_ = 0;
}

PARSER_RESULT DataStream::OpenDataStream(DataStream** str) {
    DataStream *ptr;
    PARSER_RESULT res;
    ptr = new DataStream();
    res = ptr->Open();
    if (res != PARSER_OK) {
        return res;
    }
    //ptr->m_pMemory_ = pData;
    //ptr->m_uiMemorySize_ = pSize;
    *str = ptr;
    ptr = NULL;
    return PARSER_OK;
}

DataStream::~DataStream() {
    Close();
}

PARSER_RESULT DataStream::Close() {
    /*if(m_pMemory_ != NULL) {
        delete m_pMemory_;
    }*/
    m_pMemory_ = NULL,
    m_uiMemorySize_ = 0,
    m_uiAllocatedSize_ = 0,
    m_pos_ = 0;
    return PARSER_OK;
}

PARSER_RESULT DataStream::Realloc(size_t iSize) {
    if(iSize > m_uiMemorySize_) {
        uint8_t* pNewMemory = new uint8_t [iSize];
        if(pNewMemory == NULL) {
            return PARSER_OUT_OF_MEMORY;
        }
        m_uiAllocatedSize_ = iSize;
        if(m_pMemory_ != NULL) {
            memcpy(pNewMemory, m_pMemory_, m_uiMemorySize_);
            delete m_pMemory_;
        }

        m_pMemory_ = pNewMemory;
    }
    m_uiMemorySize_ = iSize;
    if(m_pos_ > m_uiMemorySize_) {
        m_pos_ = m_uiMemorySize_;
    }
    return PARSER_OK;
}

PARSER_RESULT DataStream::Read(void* pData, size_t iSize, size_t* pRead) {
    if (pData == NULL) {
        return PARSER_INVALID_POINTER;
    }
    if (m_pMemory_ == NULL) {
        return PARSER_NOT_INITIALIZED;
    }
    size_t toRead = std::min(iSize, m_uiMemorySize_ - m_pos_);
    memcpy(pData, m_pMemory_ + m_pos_, toRead);
    m_pos_ += toRead;
    if(pRead != NULL)
    {
        *pRead = toRead;
    }
    return PARSER_OK;
}

PARSER_RESULT DataStream::Write(const void* pData, size_t iSize, size_t* pWritten)
{
    if (pData == NULL) {
        return PARSER_INVALID_POINTER;
    }
    if (Realloc(m_pos_ + iSize)) {
        return PARSER_STREAM_NOT_ALLOCATED;
    }

    size_t toWrite = std::min(iSize, m_uiMemorySize_ - m_pos_);
    memcpy(m_pMemory_ + m_pos_, pData, toWrite);
    m_pos_ += toWrite;
    if(pWritten != NULL)
    {
        *pWritten = toWrite;
    }
    return PARSER_OK;
}

PARSER_RESULT DataStream::Seek(PARSER_SEEK_ORIGIN eOrigin, int64_t iPosition, int64_t* pNewPosition) {
    switch(eOrigin) {
    case PARSER_SEEK_BEGIN:
        m_pos_ = (size_t)iPosition;
        break;

    case PARSER_SEEK_CURRENT:
        m_pos_ += (size_t)iPosition;
        break;

    case PARSER_SEEK_END:
        m_pos_ = m_uiMemorySize_ - (size_t)iPosition;
        break;
    }

    if(m_pos_ > m_uiMemorySize_) {
        m_pos_ = m_uiMemorySize_;
    }
    if(pNewPosition != NULL) {
        *pNewPosition = m_pos_;
    }
    return PARSER_OK;
}

PARSER_RESULT DataStream::GetPosition(int64_t* pPosition) {
    if (pPosition != NULL) {
        return PARSER_INVALID_POINTER;
    }
    *pPosition = m_pos_;
    return PARSER_OK;
}

PARSER_RESULT DataStream::GetSize(int64_t* pSize) {
    if (pSize != NULL) {
        return PARSER_INVALID_POINTER;
    }
    *pSize = m_uiMemorySize_;
    return PARSER_OK;
}

bool DataStream::IsSeekable() {
    return true;
}