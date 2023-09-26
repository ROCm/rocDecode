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
#include "../commons.h"

/**
 * @brief Base class for video parsing
 * 
 */
class RocVideoParser {
public:
    RocVideoParser() {};    // default constructor
    RocVideoParser(RocdecParserParams *pParams) : parser_params_(pParams) {};
    virtual ~RocVideoParser() = default ;
    virtual void SetParserParams(RocdecParserParams *pParams) { parser_params_ = pParams; };
    RocdecParserParams *GetParserParams() {return parser_params_;};
    virtual rocDecStatus Initialize(RocdecParserParams *pParams);
    virtual rocDecStatus ParseVideoData(RocdecSourceDataPacket *pData) = 0;     // pure virtual: implemented by derived class

protected:
    RocdecParserParams *parser_params_ = nullptr;
    /**
     * @brief callback function pointers for the parser
     * 
     */
    PFNVIDSEQUENCECALLBACK pfn_sequece_cb_;             /**< Called before decoding frames and/or whenever there is a fmt change */
    PFNVIDDECODECALLBACK pfn_decode_picture_cb_;        /**< Called when a picture is ready to be decoded (decode order)         */
    PFNVIDDISPLAYCALLBACK pfn_display_picture_cb_;      /**< Called whenever a picture is ready to be displayed (display order)  */
    PFNVIDSEIMSGCALLBACK pfn_get_sei_message_cb_;       /**< Called when all SEI messages are parsed for particular frame        */
};
