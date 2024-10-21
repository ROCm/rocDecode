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

#include "vp9_defines.h"
#include "roc_video_parser.h"

class Vp9VideoParser : public RocVideoParser {
public:
    /*! \brief Vp9VideoParser constructor
     */
    Vp9VideoParser();

    /*! \brief Vp9VideoParser destructor
     */
    virtual ~Vp9VideoParser();

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
        int      pic_idx;
        int      dec_buf_idx;  // frame index in decode/display buffer pool
        uint32_t use_status;    // refer to FrameBufUseStatus
    } Vp9Picture;

    /*! \brief Decoded picture buffer
     */
    typedef struct {
        Vp9Picture frame_store[VP9_NUM_REF_FRAMES]; // BufferPool
        uint32_t ref_frame_width[VP9_NUM_REF_FRAMES]; // RefFrameWidth
        uint32_t ref_frame_height[VP9_NUM_REF_FRAMES]; // RefFrameHeight
    } DecodedPictureBuffer;

    Vp9UncompressedHeader uncompressed_header_;
    uint32_t header_size_in_bytes_;
    uint32_t loop_filter_level_;
    uint8_t last_frame_type_; // LastFrameType
    uint8_t frame_is_intra_;

protected:
    DecodedPictureBuffer dpb_buffer_;
    Vp9Picture curr_pic_;

    /*! \brief Function to parse one picture bit stream received from the demuxer.
     * \param [in] p_stream A pointer of <tt>uint8_t</tt> for the input stream to be parsed
     * \param [in] pic_data_size Size of the input stream
     * \return <tt>ParserResult</tt>
     */
    ParserResult ParsePictureData(const uint8_t *p_stream, uint32_t pic_data_size);

    /*! \brief Function to notify decoder about new sequence format through callback
     * \return <tt>ParserResult</tt>
     */
    ParserResult NotifyNewSequence();

    /*! \brief Function to fill the decode parameters and call back decoder to decode a picture
     * \return <tt>ParserResult</tt>
     */
    ParserResult SendPicForDecode();

    /*! Function to initialize the local DPB (BufferPool)
     *  \return None
     */
    void InitDpb();

    /*! \brief Function to send out the remaining pictures that need for output in decode frame buffer.
     * \return <tt>ParserResult</tt>
     */
    ParserResult FlushDpb();

    /*! \brief Function to find a free buffer in the decode buffer pool
     *  \return <tt>ParserResult</tt>
     */
    ParserResult FindFreeInDecBufPool();

    /*! \brief Function to find a free buffer in DPB for the current picture and mark it.
     * \return <tt>ParserResult</tt>
     */
    ParserResult FindFreeInDpbAndMark();

    /*! \brief Function to parse an uncompressed header (uncompressed_header(), 6.2)
     * \param [in] p_stream Pointer to the bit stream
     * \param [in] size Byte size of the stream
     * \param [out] p_bytes_parsed Number of bytes that have been parsed
     * \return <tt>ParserResult</tt>
     */
    ParserResult ParseUncompressedHeader(uint8_t *p_stream, size_t size, int *p_bytes_parsed);

    /*! \brief Function to parse frame sync syntax (frame_sync_code(), 6.2.1)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     * \return <tt>ParserResult</tt>
     */
    ParserResult FrameSyncCode(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse color config syntax (color_config(), 6.2.2)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     * \return <tt>ParserResult</tt>
     */
    ParserResult ColorConfig(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse frame size syntax (frame_size(), 6.2.3)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void FrameSize(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse render size syntax (frame_size(), 6.2.4)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void RenderSize(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse frame size with refs syntax (frame_size_with_refs(), 6.2.5)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void FrameSizeWithRefs(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to calculate 8x8 and 64x64 block columns and rows of the frame (compute_image_size(), 6.2.6)
     * \param [inout] p_uncomp_header Pointer to uncompressed header struct
     * \return None
     */
    void ComputeImageSize(Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse loop filter params syntax (loop_filter_params(), 6.2.8)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void LoopFilterParams(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse quantization params syntax (quantization_params(), 6.2.9)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void QuantizationParams(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse delta quantizer syntax (read_delta_q(), 6.2.10)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \return <tt>int8_t</tt>
     */
    int8_t ReadDeltaQ(const uint8_t *p_stream, size_t &offset);

    /*! \brief Function to parse segmentation params syntax (segmentation_params(), 6.2.11)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void SegmentationParams(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to parse probability syntax (read_delta_q(), 6.2.12)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \return <tt>uint8_t</tt>
     */
    uint8_t ReadProb(const uint8_t *p_stream, size_t &offset);

    /*! \brief Function to parse tile info syntax (tile_info(), 6.2.13)
     * \param [in] p_stream Pointer to the bit stream
     * \param [inout] offset Bit offset
     * \param [out] p_uncomp_header Pointer to uncompressed header struct
     *  \return None
     */
    void TileInfo(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header);

    /*! \brief Function to read igned integer using n bits for the value and 1 bit for a sign flag 4.10.6. su(n).
     * \param [in] p_stream Bit stream pointer
     * \param [in] bit_offset Starting bit offset
     * \param [out] bit_offset Updated bit offset
     * \param [in] num_bits Number of bits to read
     * \return The signed value
     */
    inline int32_t ReadSigned(const uint8_t *p_stream, size_t &bit_offset, int num_bits) {
        uint32_t u_value = Parser::ReadBits(p_stream, bit_offset, num_bits);
        uint8_t sign = Parser::GetBit(p_stream, bit_offset);
        return sign ? -u_value : u_value;
    }
};