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

#include "h264_parser.h"

/**
 * @brief Construct a new HEVCParser object
 * 
 */
H264VideoParser::H264VideoParser() {

}

/**
 * @brief Function to initialize any h264 parser related members
 * 
 * @return rocDecStatus 
 */
rocDecStatus H264VideoParser::Initialize(RocdecParserParams *pParams) {
    RocVideoParser::Initialize(pParams);
    //todo::
    return ROCDEC_NOT_IMPLEMENTED;
}

/**
 * @brief Function to Parse video data: Typically called from application when a demuxed picture is ready to be parsed
 * 
 * @param pData: IN: pointer to demuxed data packet
 * @return rocDecStatus 
 */
rocDecStatus H264VideoParser::ParseVideoData(RocdecSourceDataPacket *pData) {
    return ROCDEC_NOT_IMPLEMENTED;
}