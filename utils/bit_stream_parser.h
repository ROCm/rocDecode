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
#include "log.h"
#include "result.h"
#include "data_stream.h"
#include "byte_array.h"
#include "platform.h"
#include "parser_data.h"
#include "parser_buffer.h"
#include "context.h"

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

class BitStreamParser;
typedef std::shared_ptr<BitStreamParser> BitStreamParserPtr;

//  Common Parser Class
class BitStreamParser
{
public:
    virtual ~BitStreamParser();

    virtual int                     GetOffsetX() const = 0;
    virtual int                     GetOffsetY() const = 0;
    virtual int                     GetPictureWidth() const = 0;
    virtual int                     GetPictureHeight() const = 0;
    virtual int                     GetAlignedWidth() const = 0;
    virtual int                     GetAlignedHeight() const = 0;

    virtual void                    SetMaxFramesNumber(size_t num) = 0;

    virtual const unsigned char*    GetExtraData() const = 0;
    virtual size_t                  GetExtraDataSize() const = 0;
    virtual void                    SetUseStartCodes(bool bUse) = 0;
    virtual void                    SetFrameRate(double fps) = 0;
    virtual double                  GetFrameRate() const = 0;
    virtual PARSER_RESULT           ReInit() = 0;
    virtual void                    GetFrameRate(ParserRate *frameRate) const = 0;
    
    virtual PARSER_RESULT           QueryOutput(ParserData** ppData) = 0;
    static BitStreamParserPtr       Create(DataStream* pStream, BitStreamType type, ParserContext* pContext);
    virtual void                    FindFirstFrameSPSandPPS() = 0;
    virtual bool                    CheckDataStreamEof(int nVideoBytes) = 0;
};

// helpers
namespace Parser
{
    inline char getLowByte(uint16_t data)
    {
        return (data >> 8);
    }

    inline char getHiByte(uint16_t data)
    {
        return (data & 0xFF);
    }

    inline bool getBit(const uint8_t *data, size_t &bitIdx)
    {
        bool ret = (data[bitIdx / 8] >> (7 - bitIdx % 8) & 1);
        bitIdx++;
        return ret;
    }
    inline uint32_t getBitToUint32(const uint8_t *data, size_t &bitIdx)
    {
        uint32_t ret = (data[bitIdx / 8] >> (7 - bitIdx % 8) & 1);
        bitIdx++;
        return ret;
    }

    inline uint32_t readBits(const uint8_t *data, size_t &startBitIdx, size_t bitsToRead)
    {
        if (bitsToRead > 32)
        {
            return 0; // assert(0);
        }
        uint32_t result = 0;
        for (size_t i = 0; i < bitsToRead; i++)
        {
            result = result << 1;
            result |= getBitToUint32(data, startBitIdx); // startBitIdx incremented inside
        }
        return result;
    }

    inline size_t countContiniusZeroBits(const uint8_t *data, size_t &startBitIdx)
    {
        size_t startBitIdxOrg = startBitIdx;
        while (getBit(data, startBitIdx) == false) // startBitIdx incremented inside
        {
        }
        startBitIdx--; // remove non zero
        return startBitIdx - startBitIdxOrg;
    }

    namespace ExpGolomb
    {
        inline uint32_t readUe(const uint8_t *data, size_t &startBitIdx)
        {
            size_t zeroBitsCount = countContiniusZeroBits(data, startBitIdx); // startBitIdx incremented inside
            if (zeroBitsCount > 30)
            {
                return 0; // assert(0)
            }

            uint32_t leftPart = (0x1 << zeroBitsCount) - 1;
            startBitIdx++;
            uint32_t rightPart = readBits(data, startBitIdx, zeroBitsCount);
            return leftPart + rightPart;
        }

        inline uint32_t readSe(const uint8_t *data, size_t &startBitIdx)
        {
            uint32_t ue = readUe(data, startBitIdx);
            // se From Ue 
            uint32_t mod2 = ue % 2;
            uint32_t r = ue / 2 + mod2;

            if (mod2 == 0)
            {
                return r * -1;
            }
            return r;
        }
    }
}

#endif /* PARSER_H */