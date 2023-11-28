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
    RocVideoParser();    // default constructor
    RocVideoParser(RocdecParserParams *pParams) : parser_params_(*pParams) {};
    virtual ~RocVideoParser() = default ;
    virtual void SetParserParams(RocdecParserParams *pParams) { parser_params_ = *pParams; };
    RocdecParserParams *GetParserParams() {return &parser_params_;};
    virtual rocDecStatus Initialize(RocdecParserParams *pParams);
    virtual rocDecStatus ParseVideoData(RocdecSourceDataPacket *pData) = 0;     // pure virtual: implemented by derived class
    virtual rocDecStatus UnInitialize() = 0;     // pure virtual: implemented by derived class

protected:
    RocdecParserParams parser_params_ = {};
    /**
     * @brief callback function pointers for the parser
     * 
     */
    PFNVIDSEQUENCECALLBACK pfn_sequece_cb_;             /**< Called before decoding frames and/or whenever there is a fmt change */
    PFNVIDDECODECALLBACK pfn_decode_picture_cb_;        /**< Called when a picture is ready to be decoded (decode order)         */
    PFNVIDDISPLAYCALLBACK pfn_display_picture_cb_;      /**< Called whenever a picture is ready to be displayed (display order)  */
    PFNVIDSEIMSGCALLBACK pfn_get_sei_message_cb_;       /**< Called when all SEI messages are parsed for particular frame        */

    uint32_t pic_width_;
    uint32_t pic_height_;
    bool new_sps_activated_;

    RocdecVideoFormat video_format_params_;
    RocdecSeiMessageInfo sei_message_info_params_;
    RocdecPicParams dec_pic_params_;
};

enum ParserSeekOrigin {
    PARSER_SEEK_BEGIN          = 0,
    PARSER_SEEK_CURRENT        = 1,
    PARSER_SEEK_END            = 2,
};

typedef enum ParserResult {
    PARSER_OK                                   = 0,
    PARSER_FAIL                                    ,

// common errors
    PARSER_UNEXPECTED                              ,

    PARSER_ACCESS_DENIED                           ,
    PARSER_INVALID_ARG                             ,
    PARSER_OUT_OF_RANGE                            ,

    PARSER_OUT_OF_MEMORY                           ,
    PARSER_INVALID_POINTER                         ,

    PARSER_NO_INTERFACE                            ,
    PARSER_NOT_IMPLEMENTED                         ,
    PARSER_NOT_SUPPORTED                           ,
    PARSER_NOT_FOUND                               ,

    PARSER_ALREADY_INITIALIZED                     ,
    PARSER_NOT_INITIALIZED                         ,

    PARSER_INVALID_FORMAT                          ,// invalid data format

    PARSER_WRONG_STATE                             ,
    PARSER_FILE_NOT_OPEN                           ,// cannot open file
    PARSER_STREAM_NOT_ALLOCATED                    ,

// device common codes
    PARSER_NO_DEVICE                               ,

    //result codes
    PARSER_EOF                                     ,
    PARSER_REPEAT                                  ,

    //error codes
    PARSER_INVALID_DATA_TYPE                       ,//invalid data type
    PARSER_INVALID_RESOLUTION                      ,//invalid resolution (width or height)
    PARSER_CODEC_NOT_SUPPORTED                     ,//codec not supported
} ParserResult;

// helpers
namespace Parser {
    inline char GetLowByte(uint16_t data) {
        return (data >> 8);
    }

    inline char GetHiByte(uint16_t data) {
        return (data & 0xFF);
    }

    inline bool GetBit(const uint8_t *data, size_t &bit_idx) {
        bool ret = (data[bit_idx / 8] >> (7 - bit_idx % 8) & 1);
        bit_idx++;
        return ret;
    }
    inline uint32_t GetBitToUint32(const uint8_t *data, size_t &bit_idx) {
        uint32_t ret = (data[bit_idx / 8] >> (7 - bit_idx % 8) & 1);
        bit_idx++;
        return ret;
    }

    inline uint32_t ReadBits(const uint8_t *data, size_t &start_bit_idx, size_t bits_to_read) {
        if (bits_to_read > 32) {
            return 0; // assert(0);
        }
        uint32_t result = 0;
        for (size_t i = 0; i < bits_to_read; i++) {
            result = result << 1;
            result |= GetBitToUint32(data, start_bit_idx); // start_bit_idx incremented inside
        }
        return result;
    }

    inline size_t CountContiniusZeroBits(const uint8_t *data, size_t &start_bit_idx) {
        size_t start_bit_idx_org = start_bit_idx;
        while (GetBit(data, start_bit_idx) == false) {} // start_bit_idx incremented inside
        start_bit_idx--; // remove non zero
        return start_bit_idx - start_bit_idx_org;
    }

    namespace ExpGolomb {
        inline uint32_t ReadUe(const uint8_t *data, size_t &start_bit_idx) {
            size_t zero_bits_count = CountContiniusZeroBits(data, start_bit_idx); // start_bit_idx incremented inside
            if (zero_bits_count > 30) {
                return 0; // assert(0)
            }

            uint32_t left_part = (0x1 << zero_bits_count) - 1;
            start_bit_idx++;
            uint32_t rightPart = ReadBits(data, start_bit_idx, zero_bits_count);
            return left_part + rightPart;
        }

        inline uint32_t ReadSe(const uint8_t *data, size_t &start_bit_idx) {
            uint32_t ue = ReadUe(data, start_bit_idx);
            // se From Ue 
            uint32_t mod2 = ue % 2;
            uint32_t r = ue / 2 + mod2;

            if (mod2 == 0) {
                return r * -1;
            }
            return r;
        }
    }
}