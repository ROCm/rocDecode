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
#define INVALID_INDEX -1  // Invalid buffer index.

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

    typedef struct {
        int index;
    } Av1Picture;

protected:
    Av1SequenceHeader seq_header_;
    Av1FrameHeader frame_header_;

    int temporal_id_; //  temporal level of the data contained in the OBU
    int spatial_id_;  // spatial level of the data contained in the OBU

    // Frame header syntax elements
    int ref_frame_type_[NUM_REF_FRAMES];
    int ref_frame_id_[NUM_REF_FRAMES];
    int ref_order_hint_[NUM_REF_FRAMES];
    int ref_valid_[NUM_REF_FRAMES];

    // A list of all frame buffers that may be used for reference of the current picture or any
    // subsequent pictures. The value is the index of a frame in DPB buffer pool. If an entry is
    // not used as reference, the value should be -1.
    int ref_pic_map_[NUM_REF_FRAMES];
    int ref_pic_map_next_[NUM_REF_FRAMES];  // for next picture

    // The reference list for the current picture
    Av1Picture ref_pictures_[REFS_PER_FRAME];
    // The free frame buffer in DPB pool that the current picutre is decoded into
    int new_fb_index_;

    /*! \brief Function to parse a sequence header OBU
     * \param p_stream Pointer to the bit stream
     * \param size Byte size of the stream
     * \return None
     */
    void ParseSequenceHeader(uint8_t *p_stream, size_t size);

    /*! \brief Function to parse a frame header OBU
     * \param p_stream Pointer to the bit stream
     * \param size Byte size of the stream
     * \return <tt>ParserResult</tt>
     */
    ParserResult ParseUncompressedHeader(uint8_t *p_stream, size_t size);

    /*! \brief Function to parse color config in sequence header
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_seq_header Pointer to sequence header struct
     * \return None
     */
    void ParseColorConfig(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header);

    /*! \brief Function to mark reference frames
     * \param [in] p_seq_header Pointer to sequence header
     * \param [in] p_frame_header Ponter to frame header
     * \param [in] id_len Current frame id length
     */
    void MarkRefFrames(Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header, uint32_t id_len);

    /*! \brief Function to parse frame size
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void FrameSize(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse super res parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void SuperResParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    void ComputeImageSize(Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse render size info
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void RenderSize(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to compute the distance between two order hints by sign extending the result of subtracting the values.
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [in] a One order hint
     * \param [in] b The other order hint
     * \return The order hint distance
     */
    int GetRelativeDist(Av1SequenceHeader *p_seq_header, int a, int b);

    /*! \brief Function to compute the elements in the ref_frame_idx array. 7.8. Set frame refs process.
     * \param [in] p_seq_header Pinter to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void SetFrameRefs(Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to find the latest backward reference
     * \param [in] shifted_order_hints An array containing the expected output order shifted such that the current
     *             frame has hint equal to curr_frame_hint
     * \param [in] used_frame An array marking which reference frames have been used
     * \param [in] curr_frame_hint A variable set equal to 1 << (OrderHintBits - 1)
     */
    int FindLatestBackward(int *shifted_order_hints, int *used_frame, int curr_frame_hint);

    /*! \brief Function to find the earliest backward reference
     * \param [in] shifted_order_hints An array containing the expected output order shifted such that the current
     *             frame has hint equal to curr_frame_hint
     * \param [in] used_frame An array marking which reference frames have been used
     * \param [in] curr_frame_hint A variable set equal to 1 << (OrderHintBits - 1)
     */
    int FindEarliestBackward(int *shifted_order_hints, int *used_frame, int curr_frame_hint);

    /*! \brief Function to find the latest forward reference
     * \param [in] shifted_order_hints An array containing the expected output order shifted such that the current
     *             frame has hint equal to curr_frame_hint
     * \param [in] used_frame An array marking which reference frames have been used
     * \param [in] curr_frame_hint A variable set equal to 1 << (OrderHintBits - 1)
     */
    int FindLatestForward(int *shifted_order_hints, int *used_frame, int curr_frame_hint);

    /*! \brief Function to parse frame size with refs info
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void FrameSizeWithRefs(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to indicates that this frame can be decoded without dependence on previous coded frames. setup_past_independence().
     * \param [out] p_frame_header Pointer to frame header struct
     */
    void SetupPastIndependence(Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse tile info
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void TileInfo(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to calculate the smallest value for k such that blk_size << k is greater than or equal to target.
     * \param [in] blk_size Block size
     * \param [in] target target value
     * \return 32-bit unsigned
     */
    uint32_t TileLog2(uint32_t blk_size, uint32_t target);

    /*! \brief Function to parse quantization parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void QuantizationParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to read delta quantizer
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    uint32_t ReadDeltaQ(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to segmentation parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void SegmentationParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse quantizer index delta parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void DeltaQParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse loop filter delta parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void DeltaLFParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse loop filter parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void LoopFilterParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse CDEF parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void CdefParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to loop restoration parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void LrParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse TX mode
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void ReadTxMode(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to skip mode parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void SkipModeParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

    /*! \brief Function to parse global motion parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void GlobalMotionParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header);

    /*! \brief Function to calculate global motion parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [out] p_frame_header Pointer to frame header struct
     * \param [in] type Motion type
     * \param [in] ref Reference frame
     * \param [in] idx Parameter index
     * \return None
     */
    void ReadGlobalParam(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header, int type, int ref, int idx);

    /*! \brief Function to decode signed subexp with ref. 5.9.26. decode_signed_subexp_with_ref()
     */
    int DecodeSignedSubexpWithRef(const uint8_t *p_stream, size_t &offset, int low, int high, int r);

    /*! \brief Function to decode unsigned subexp with ref. 5.9.27. decode_unsigned_subexp_with_ref()
     */
    int DecodeUnsignedSubexpWithRef(const uint8_t *p_stream, size_t &offset, int mx, int r);

    /*! \brief Function to decode subexp. 5.9.28. decode_subexp()
     */
    int DecodeSubexp(const uint8_t *p_stream, size_t &offset, int num_syms);

    /*! \brief Function to inverse recenter. 5.9.29. inverse_recenter()
     */
    int InverseRecenter(int r, int v);

    /*! \brief Function to parse film grain parameters
     * \param p_stream Pointer to the bit stream
     * \param [in] offset Starting bit offset
     * \param [out] offset Updated bit offset
     * \param [in] p_seq_header Pointer to sequence header struct
     * \param [out] p_frame_header Pointer to frame header struct
     * \return None
     */
    void FilmGrainParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header);

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