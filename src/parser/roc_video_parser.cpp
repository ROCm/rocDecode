/*
Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.

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
    pic_count_ = 0;
    pic_width_ = 0;
    pic_height_ = 0;
    new_sps_activated_ = false;
    frame_rate_.numerator = 0;
    frame_rate_.denominator = 0;
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
    pfn_sequece_cb_         = pParams->pfn_sequence_callback;             /**< Called before decoding frames and/or whenever there is a fmt change */
    pfn_decode_picture_cb_  = pParams->pfn_decode_picture;        /**< Called when a picture is ready to be decoded (decode order)         */
    pfn_display_picture_cb_ = pParams->pfn_display_picture;      /**< Called whenever a picture is ready to be displayed (display order)  */
    pfn_get_sei_message_cb_ = pParams->pfn_get_sei_msg;       /**< Called when all SEI messages are parsed for particular frame        */

    parser_params_ = *pParams;

    return ROCDEC_SUCCESS;
}

ParserResult RocVideoParser::GetNalUnit() {
    bool start_code_found = false;

    nal_unit_size_ = 0;
    curr_start_code_offset_ = next_start_code_offset_;  // save the current start code offset

    // Search for the next start code
    while (curr_byte_offset_ < pic_data_size_ - 2) {
        if (pic_data_buffer_ptr_[curr_byte_offset_] == 0 && pic_data_buffer_ptr_[curr_byte_offset_ + 1] == 0 && pic_data_buffer_ptr_[curr_byte_offset_ + 2] == 0x01) {
            curr_start_code_offset_ = next_start_code_offset_;  // save the current start code offset

            start_code_found = true;
            start_code_num_++;
            next_start_code_offset_ = curr_byte_offset_;
            // Move the pointer 3 bytes forward
            curr_byte_offset_ += 3;

            // For the very first NAL unit, search for the next start code (or reach the end of frame)
            if (start_code_num_ == 1) {
                start_code_found = false;
                curr_start_code_offset_ = next_start_code_offset_;
                continue;
            } else {
                break;
            }
        }
        curr_byte_offset_++;
    }    
    if (start_code_num_ == 0) {
        // No NAL unit in the frame data
        return PARSER_NOT_FOUND;
    }
    if (start_code_found) {
        nal_unit_size_ = next_start_code_offset_ - curr_start_code_offset_;
        return PARSER_OK;
    } else {
        nal_unit_size_ = pic_data_size_ - curr_start_code_offset_;
        return PARSER_EOF;
    }        
}

size_t RocVideoParser::EbspToRbsp(uint8_t *streamBuffer,size_t begin_bytepos, size_t end_bytepos) {
    int count = 0;
    if (end_bytepos < begin_bytepos) {
        return end_bytepos;
    }
    uint8_t *streamBuffer_i = streamBuffer + begin_bytepos;
    uint8_t *streamBuffer_end = streamBuffer + end_bytepos;
    int reduce_count = 0;
    for (; streamBuffer_i != streamBuffer_end; ) { 
        //starting from begin_bytepos to avoid header information
        //in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any uint8_t-aligned position
        uint8_t tmp =* streamBuffer_i;
        if (count == ZEROBYTES_SHORTSTARTCODE) {
            if (tmp == 0x03) {
                //check the 4th uint8_t after 0x000003, except when cabac_zero_word is used, in which case the last three bytes of this NAL unit must be 0x000003
                if ((streamBuffer_i + 1 != streamBuffer_end) && (streamBuffer_i[1] > 0x03)) {
                    return static_cast<size_t>(-1);
                }
                //if cabac_zero_word is used, the final uint8_t of this NAL unit(0x03) is discarded, and the last two bytes of RBSP must be 0x0000
                if (streamBuffer_i + 1 == streamBuffer_end) {
                    break;
                }
                memmove(streamBuffer_i, streamBuffer_i + 1, streamBuffer_end-streamBuffer_i - 1);
                streamBuffer_end--;
                reduce_count++;
                count = 0;
                tmp = *streamBuffer_i;
            } else if (tmp < 0x03) {
            }
        }
        if (tmp == 0x00) {
            count++;
        } else {
            count = 0;
        }
        streamBuffer_i++;
    }
    return end_bytepos - begin_bytepos + reduce_count;
}
