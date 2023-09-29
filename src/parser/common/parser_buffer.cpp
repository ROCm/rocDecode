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

#include "parser_buffer.h"

ParserBuffer::ParserBuffer() :
            m_buffer_(NULL),
            m_packet_size_(0),
            m_duration_(0),
            m_current_timestamp_(0) {}

ParserBuffer::~ParserBuffer () {
    if (m_buffer_) {
        delete[] m_buffer_;
    }
    m_buffer_ = NULL;
    m_packet_size_ = 0;
    m_duration_ = 0;
    m_current_timestamp_ = 0;
}

int64_t ParserBuffer::GetPts() const { 
    return m_current_timestamp_; 
}

void ParserBuffer::SetPts(int64_t pts) {
    if (pts == m_current_timestamp_) {
        return;
    }
    m_current_timestamp_ = pts;
}

int64_t ParserBuffer::GetDuration() const { 
    return m_duration_; 
}

void ParserBuffer::SetDuration(int64_t duration) { 
    m_duration_ = duration; 
}

bool ParserBuffer::IsReusable() { return PARSER_NOT_IMPLEMENTED; }

PARSER_RESULT ParserBuffer::SetSize(size_t new_size) {
    m_packet_size_ = new_size;
    return PARSER_OK;
}

size_t ParserBuffer::GetSize() { 
    return m_packet_size_; 
}

void* ParserBuffer::GetNative() { 
    return m_buffer_; 
}

void ParserBuffer::SetNative(size_t size) { 
    m_buffer_ = new uint8_t[size];
}

PARSER_RESULT ParserBuffer::AllocBuffer(PARSER_MEMORY_TYPE type, size_t size, ParserBuffer** pp_buffer) {
    PARSER_RESULT res = PARSER_OK;
    switch(type) { 
        case PARSER_MEMORY_HOST: {
            ParserBuffer* p_new_buffer = new ParserBuffer;
            if (p_new_buffer != NULL) {
                p_new_buffer->SetNative(size);
                res = p_new_buffer->SetSize(size);
                if (res != PARSER_OK) {
                    return res;
                }
                *pp_buffer = p_new_buffer;
            }
        }
        break;
        case PARSER_MEMORY_HIP: {
            res = PARSER_NOT_IMPLEMENTED;
        }
        break;
        case PARSER_MEMORY_UNKNOWN:{
            res = PARSER_NOT_IMPLEMENTED;
        }
        break;
        default: {
            res = PARSER_INVALID_ARG;
        }
        break;       
    }
    return res;
}