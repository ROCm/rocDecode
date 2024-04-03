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
#pragma once

#include "av1_defines.h"
#include "roc_video_parser.h"

#define OBU_HEADER_SIZE 1
#define OBU_EXTENSION_SIZE 1

class Av1VideoParser : public RocVideoParser {
public:
    /*! \brief Av1VideoParser constructor
     */
    Av1VideoParser();

    /*! \brief Av1VideoParser destructor
     */
    virtual ~Av1VideoParser();

    /*! \brief Function to Initialize the parser
     * \param [in] p_params Input of <tt>RocdecParserParams</tt> with codec type to initialize parser.
     * \return <tt>rocDecStatus</tt> Returns success on completion, else error code for failure
     */
    virtual rocDecStatus Initialize(RocdecParserParams *p_params);

    /*! \brief Function to Parse video data: Typically called from application when a demuxed picture is ready to be parsed
     * \param [in] p_data Pointer to picture data of type <tt>RocdecSourceDataPacket</tt>
     * @return <tt>rocDecStatus</tt> Returns success on completion, else error_code for failure
     */
    virtual rocDecStatus ParseVideoData(RocdecSourceDataPacket *p_data);

    /*! \brief function to uninitialize AV1 parser
     * @return rocDecStatus 
     */
    virtual rocDecStatus UnInitialize();     // derived method

protected:
    Av1SequenceHeader seq_header_;

    /*! \brief Function to parse a sequence header OBU
     * \param p_stream Pointer to the bit stream
     * \param size Byte size of the stream
     * \return None
     */
    void ParseSequenceHeader(uint8_t *p_stream, size_t size);

    /*! \brief Function to parse color config in sequence header
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_seq_header Pointer to sequence header struct
     * \return None
     */
    void ParseColorConfig(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header);

    /*! \brief Function to calculate the floor of the base 2 logarithm of the input x
     * \param [in] x A 32-bit unsigned integer
     * \return the location of the most significant bit in x
     */
    inline uint32_t FloorLog2(uint32_t x) {
        uint32_t s = 0;
        while (x != 0) {
            x = x >> 1;
            s++;
        }
        return x ? s - 1 : 0;
    }

    /*! \brief Function to read variable length unsigned n-bit number appearing directly in the bitstream. 4.10.3. uvlc().
     * \param [in] p_stream Bit stream pointer
     * \param [in] bit_offset Starting bit offset
     * \param [out] bit_offset Updated bit offset
     * \return The unsigned value
     */
    inline uint32_t ReadUVLC(const uint8_t *p_stream, size_t &bit_offset) {
        int leading_zeros = 0;
        while (!Parser::GetBit(p_stream, bit_offset)) {
            ++leading_zeros;
        }
        // Maximum 32 bits.
        if (leading_zeros >= 32) {
            return 0xFFFFFFFF;
        }
        uint32_t base = (1u << leading_zeros) - 1;
        uint32_t value = Parser::ReadBits(p_stream, bit_offset, leading_zeros);
        return base + value;
    }

    /*! \brief Function to read unsigned little-endian num_bytes-byte number appearing directly in the bitstream. 4.10.4. le(n).
     * \param [in] p_stream Bit stream pointer
     * \param [in] num_bytes Number of bytes to read
     * \return The unsigned value
     */
    inline uint32_t ReadLeBytes(const uint8_t *p_stream, int num_bytes) {
        uint32_t t = 0;
        for (int i = 0; i < num_bytes; i++) {
            t += (p_stream[i] << ( i * 8 ) );
        }
        return t;
    }

    /*! \brief Function to read unsigned integer represented by a variable number of little-endian bytes, which
     *         is less than or equal to (1 << 32) - 1. 4.10.5. leb128().
     * \param [in] p_stream Bit stream pointer
     * \param [out] p_num_bytes_read Number of bytes read
     * \return The unsigned value
     */
    inline uint32_t ReadLeb128(const uint8_t *p_stream, uint32_t *p_num_bytes_read) {
        uint32_t value = 0;
        *p_num_bytes_read = 0;
        uint32_t len;
        for (len = 0; len < 4; ++len) {
            value |= (p_stream[len] & 0x7F) << (len * 7);
            if ((p_stream[len] & 0x80) == 0) {
                ++len;
                *p_num_bytes_read = len;
                break;
            }
        }
        return value;
    }

    /*! \brief Function to read signed integer converted from an n bits unsigned integer in the bitstream. 4.10.6. su(n).
     * \param [in] p_stream Bit stream pointer
     * \param [in] bit_offset Starting bit offset
     * \param [out] bit_offset Updated bit offset
     * \param [in] num_bits Number of bits to read
     * \return The signed value
     */
    inline int32_t ReadSigned(const uint8_t *p_stream, size_t &bit_offset, int num_bits) {
        int32_t value;
        uint32_t u_value = Parser::ReadBits(p_stream, bit_offset, num_bits);
        uint32_t sign_mask = 1 << (num_bits - 1);
        if ( u_value & sign_mask ) {
            value = u_value - 2 * sign_mask;
        } else {
            value = u_value;
        }
        return value;
    }

    /*! \brief Function to read unsigned encoded (non-symmetric) integer with maximum number of values num_bits 
     *         (i.e. output in range 0..num_bits-1). This encoding is non-symmetric because the values are not all 
     *         coded with the same number of bits. 4.10.7. ns(n).
     * \param [in] p_stream Bit stream pointer
     * \param [in] bit_offset Starting bit offset
     * \param [out] bit_offset Updated bit offset
     * \param [in] num_bits Number of bits to read
     * \return The unsigned value
     */
    inline uint32_t ReadUnsignedNonSymmetic(const uint8_t *p_stream, size_t &bit_offset, int num_bits) {
        uint32_t w = FloorLog2(num_bits) + 1;
        uint32_t m = (1 << w) - num_bits;
        uint32_t v = Parser::ReadBits(p_stream, bit_offset, w - 1);
        if (v < m) {
            return v;
        }
        uint32_t extra_bit = Parser::GetBit(p_stream, bit_offset);
        return (v << 1) - m + extra_bit;
    }
};