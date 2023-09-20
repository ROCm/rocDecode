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
 * \brief The rocDecode Data Byte Array.
 *
 * \defgroup group_rocdecode_parser Parser definitions.
 * \brief The byte array for Bit Stream Parser data.
 */


#ifndef BYTEARRAY_H
#define BYTEARRAY_H


#pragma once

#include <cstdint>
#include <cstring>

#define    INIT_ARRAY_SIZE 1024
#define    ARRAY_MAX_SIZE (1LL << 60LL) // extremely large maximum size

class ByteArray {
protected:
    uint8_t        *m_pData_;
    size_t         m_iSize_;
    size_t         m_iMaxSize_;
public:
    ByteArray() : m_pData_(0), m_iSize_(0), m_iMaxSize_(0) {}
    ByteArray(const ByteArray &other) : m_pData_(0), m_iSize_(0), m_iMaxSize_(0) {
        *this = other;
    }
    ByteArray(size_t num) : m_pData_(0), m_iSize_(0), m_iMaxSize_(0) {
        SetSize(num);
    }
    virtual ~ByteArray() {
        if (m_pData_ != 0) {
            delete[] m_pData_;
        }
    }
    void  SetSize(size_t num) {
        if (num == m_iSize_) {
            return;
        }
        if (num < m_iSize_) {
            memset(m_pData_ + num, 0, m_iMaxSize_ - num);
        }
        else if (num > m_iMaxSize_) {
            // This is done to prevent the following error from surfacing
            // for the pNewData allocation on some compilers:
            //     -Werror=alloc-size-larger-than=
            size_t newSize = (num / INIT_ARRAY_SIZE) * INIT_ARRAY_SIZE + INIT_ARRAY_SIZE;
            if (newSize > ARRAY_MAX_SIZE) {
                return;
            }
            m_iMaxSize_ = newSize;

            uint8_t *pNewData = new uint8_t[m_iMaxSize_];
            memset(pNewData, 0, m_iMaxSize_);
            if (m_pData_ != NULL) {
                memcpy(pNewData, m_pData_, m_iSize_);
                delete[] m_pData_;
            }
            m_pData_ = pNewData;
        }
        m_iSize_ = num;
    }
    void Copy(const ByteArray &old) {
        if (m_iMaxSize_ < old.m_iSize_) {
            m_iMaxSize_ = old.m_iMaxSize_;
            if (m_pData_ != NULL) {
                delete[] m_pData_;
            }
            m_pData_ = new uint8_t[m_iMaxSize_];
            memset(m_pData_, 0, m_iMaxSize_);
        }
        memcpy(m_pData_, old.m_pData_, old.m_iSize_);
        m_iSize_ = old.m_iSize_;
    }
    uint8_t    operator[] (size_t iPos) const {
        return m_pData_[iPos];
    }
    uint8_t&    operator[] (size_t iPos) {
        return m_pData_[iPos];
    }
    ByteArray&    operator=(const ByteArray &other) {
        SetSize(other.GetSize());
        if (GetSize() > 0) {
            memcpy(GetData(), other.GetData(), GetSize());
        }
        return *this;
    }
    uint8_t *GetData() const { return m_pData_; }
    size_t GetSize() const { return m_iSize_; }
};
#endif // BYTEARRAY_H