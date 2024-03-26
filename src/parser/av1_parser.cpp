/*
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

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

#include <algorithm>
#include "av1_parser.h"

Av1VideoParser::Av1VideoParser() {
    //todo: add constructor code
}

Av1VideoParser::~Av1VideoParser() {
}

rocDecStatus Av1VideoParser::Initialize(RocdecParserParams *p_params) {
    return RocVideoParser::Initialize(p_params);
}

rocDecStatus Av1VideoParser::UnInitialize() {
    return ROCDEC_SUCCESS;
}

rocDecStatus Av1VideoParser::ParseVideoData(RocdecSourceDataPacket *p_data) { 
    //to be implemented
    return ROCDEC_NOT_IMPLEMENTED;
}