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
 * \brief The rocDecode Parser.
 *
 * \defgroup group_rocdecode_parser Parser definitions.
 * \brief Bit Stream Parser for rocDecode Library.
 */

#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <queue>
#include <iomanip>
#include <memory>
#include <chrono>
#include <ctime>
#include <hip/hip_runtime.h>
#include "../commons.h"
#include "result.h"
#include "parser_buffer.h"

enum BitStreamType {
    BitStreamH264AnnexB = 0,
    BitStreamH264AvcC,
    BitStreamMpeg2,
    BitStreamMpeg4part2,
    BitStreamVC1,
    BitStream265AnnexB,
    BitStreamIVF,
    BitStreamUnknown
};

enum ParserSeekOrigin {
    PARSER_SEEK_BEGIN          = 0,
    PARSER_SEEK_CURRENT        = 1,
    PARSER_SEEK_END            = 2,
};

class BitStreamParser;
typedef std::shared_ptr<BitStreamParser> BitStreamParserPtr;

//  Common Parser Class
class BitStreamParser {
public:
    virtual ~BitStreamParser();

    virtual void                    SetFrameRate(double fps) = 0;
    virtual double                  GetFrameRate() const = 0;
    virtual ParserResult            ReInit() = 0;
    
    virtual ParserResult            QueryOutput(ParserBuffer** pp_buffer) = 0;
    static BitStreamParserPtr       Create(BitStreamType type);
    virtual void                    FindFirstFrameSPSandPPS() = 0;
    virtual bool                    CheckDataStreamEof(int n_video_bytes) = 0;

    virtual ParserResult            Close() = 0;
    virtual ParserResult            Read(void* p_data, size_t size, size_t* p_read) = 0;
    virtual ParserResult            Write(const void* p_data, size_t size, size_t* p_written) = 0;
    virtual ParserResult            Seek(ParserSeekOrigin e_origin, int64_t i_position, int64_t* p_new_position) = 0;
    virtual ParserResult            GetSize(int64_t* p_size) = 0;
    ParserResult                    Realloc(size_t size);
};

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

#endif /* PARSER_H */