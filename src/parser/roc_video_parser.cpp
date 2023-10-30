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

#include "roc_video_parser.h"

RocVideoParser::RocVideoParser() {
    pic_width_ = 0;
    pic_height_ = 0;
    new_sps_activated_ = false;
}

/**
 * @brief Initializes any parser related stuff for all parsers
 * 
 * @return rocDecStatus : ROCDEC_SUCCESS on success
 */
rocDecStatus RocVideoParser::Initialize(RocdecParserParams *pParams) {
    if(pParams == nullptr) {
        ERR(STR("Parser parameters are not set for the parser"));
        return ROCDEC_NOT_INITIALIZED;
    }
    // Initialize callback function pointers
    pfn_sequece_cb_         = pParams->pfnSequenceCallback;             /**< Called before decoding frames and/or whenever there is a fmt change */
    pfn_decode_picture_cb_  = pParams->pfnDecodePicture;        /**< Called when a picture is ready to be decoded (decode order)         */
    pfn_display_picture_cb_ = pParams->pfnDisplayPicture;      /**< Called whenever a picture is ready to be displayed (display order)  */
    pfn_get_sei_message_cb_ = pParams->pfnGetSEIMsg;       /**< Called when all SEI messages are parsed for particular frame        */

    parser_params_ = pParams;

    return ROCDEC_SUCCESS;
}
