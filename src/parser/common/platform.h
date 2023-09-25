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
 * \brief The rocDecode Platform.
 *
 * \defgroup group_rocdecode_parser Parser definitions.
 * \brief The extra structs/classes for Bit Stream Parser platform.
 */

#ifndef PLATFORM_H
#define PLATFORM_H
#pragma once

#include <cstdint>
#define PARSER_SECOND          10000000L    // 1 second in 100 nanoseconds
typedef struct ParserRect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
#if defined(__cplusplus)
    bool operator==(const ParserRect& other) const {
         return left == other.left && top == other.top && right == other.right && bottom == other.bottom; 
    }
    __inline__ bool operator!=(const ParserRect& other) const { return !operator==(other); }
    int32_t Width() const { return right - left; }
    int32_t Height() const { return bottom - top; }
#endif
} ParserRect;

typedef struct ParserRate {
    uint32_t num;
    uint32_t den;
#if defined(__cplusplus)
    bool operator==(const ParserRate& other) const {
         return num == other.num && den == other.den; 
    }
    __inline__ bool operator!=(const ParserRate& other) const { return !operator==(other); }
#endif
} ParserRate;

#endif // PLATFORM_H