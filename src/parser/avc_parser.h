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

#include "avc_defines.h"
#include "roc_video_parser.h"

class AvcVideoParser : public RocVideoParser {

public:
    /*! \brief AvcVideoParser constructor
     */
    AvcVideoParser();

    /*! \brief AvcVideoParser destructor
     */
    virtual ~AvcVideoParser();

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

    /*! \brief function to uninitialize AVC parser
     * @return rocDecStatus 
     */
    virtual rocDecStatus UnInitialize();     // derived method


protected:    
    enum PictureStructure {
        kFrame,
        kTopField,
        kBottomField
    };

    enum AvcRefMarking {
        kUnusedForReference = 0,
        kUsedForShortTerm = 1,
        kUsedForLongTerm = 2
    };

    typedef struct {
        int      pic_idx;  // picture index or id
        PictureStructure pic_structure;

        int32_t  pic_order_cnt;
        int32_t  top_field_order_cnt;
        int32_t  bottom_field_order_cnt;
        int32_t  frame_num;
        int32_t  frame_num_wrap; // FrameNumWrap
        int32_t  pic_num; // PicNum
        int32_t  long_term_pic_num; // LongTermPicNum
        uint32_t long_term_frame_idx; // LongTermFrameIdx: long term reference frame/field identifier

        uint32_t is_reference;
    } AvcPicture;

    /*! \brief Decoded picture buffer
     */
    typedef struct{
        uint32_t dpb_size;  // DPB buffer size in number of frames
        uint32_t num_short_term; // numShortTerm;
        uint32_t num_long_term; // numLongTerm;
        AvcPicture frame_buffer_list[AVC_MAX_DPB_FRAMES];
    } DecodedPictureBuffer;

    AvcNalUnitHeader nal_unit_header_;
    AvcSeqParameterSet sps_list_[AVC_MAX_SPS_NUM];
    int32_t active_sps_id_;
    AvcPicParameterSet pps_list_[AVC_MAX_PPS_NUM];
    int32_t active_pps_id_;

    AvcNalUnitHeader   slice_nal_unit_header_;
    AvcSliceHeader     slice_header_0_;

    int prev_pic_order_cnt_msb_; // prevPicOrderCntMsb
    int prev_pic_order_cnt_lsb_; // prevPicOrderCntLsb
    int prev_top_field_order_cnt_;
    int prev_frame_num_offset_t; // prevFrameNumOffset
    int prev_frame_num_; // prevFrameNum
    int prev_has_mmco_5_;
    int curr_has_mmco_5_;
    int prev_ref_pic_bottom_field_;
    int curr_ref_pic_bottom_field_;

    // DPB
    AvcPicture curr_pic_;
    DecodedPictureBuffer dpb_buffer_;
    AvcPicture ref_list_0_[AVC_MAX_REF_FRAME_NUM];
    AvcPicture ref_list_1_[AVC_MAX_REF_FRAME_NUM];

    /*! \brief Function to notify decoder about video format change (new SPS) through callback
     * \param [in] p_sps Pointer to the current active SPS
     * \return <tt>ParserResult</tt>
     */
    ParserResult NotifyNewSps(AvcSeqParameterSet *p_sps);

    /*! \brief Function to fill the decode parameters and call back decoder to decode a picture
     * \return <tt>ParserResult</tt>
     */
    ParserResult SendPicForDecode();

    /*! \brief Function to parse one picture bit stream received from the demuxer.
     * \param [in] p_stream A pointer of <tt>uint8_t</tt> for the input stream to be parsed
     * \param [in] pic_data_size Size of the input stream
     * \return <tt>ParserResult</tt>
     */
    ParserResult ParsePictureData(const uint8_t *p_stream, uint32_t pic_data_size);

    /*! \brief Function to parse the NAL unit header
     * \param [in] header_byte The AVC NAL unit header byte
     * \return <tt>AvcNalUnitHeader</tt> Parsed nal header
     */
    AvcNalUnitHeader ParseNalUnitHeader(uint8_t header_byte);

    /*! \brief Function to parse Sequence Parameter Set 
     * \param [in] p_stream A pointer of <tt>uint8_t</tt> for the input stream to be parsed
     * \param [in] size Size of the input stream
     * \return No return value
     */
    void ParseSps(uint8_t *p_stream, size_t size);

    /*! \brief Function to parse Picture Parameter Set 
     * \param [in] p_stream A pointer of <tt>uint8_t</tt> for the input stream to be parsed
     * \param [in] size Size of the input stream
     * \return No return value
     */
    void ParsePps(uint8_t *p_stream, size_t stream_size_in_byte);

    /*! \brief Function to parse slice header
     * \param p_stream The pointer to the input bit stream
     * \param [in] stream_size_in_byte The byte size of the stream
     * \param [out] p_slice_header The pointer to the slice header strucutre
     * \return <tt>ParserResult</tt>
     */
    ParserResult ParseSliceHeader(uint8_t *p_stream, size_t stream_size_in_byte, AvcSliceHeader *p_slice_header);

    /*! \brief Function to parse a scaling list
     * \param [in] p_stream A pointer of <tt>uint8_t</tt> for the input stream to be parsed
     * \param [in/out] offset Current bit offset
     * \param [out] scaling_list Pointer to the output scaling list
     * \param [in] list_size Scaling list size
     * \param [out] use_default_scaling_matrix_flag Array of flags that indicate whether to use default values
     */
    void GetScalingList(uint8_t *p_stream, size_t &offset, uint32_t *scaling_list, uint32_t list_size, uint32_t *use_default_scaling_matrix_flag);

    /*! \brief Function to parse vidio usability information (VUI) parameters
     * \param [in] p_stream The pointer to the input bit stream
     * \param [in/out] offset Current bit offset
     * \param [out] p_vui_params The pointer to VUI structure
     * \return No return value
     */
    void GetVuiParameters(uint8_t *p_stream, size_t &offset, AvcVuiSeqParameters *p_vui_params);

    /*! \brief Function to check if there is more data in RBSP
     * \param [in] p_stream The pointer to the input bit stream
     * \param [in] stream_size_in_byte The byte size of the stream
     * \param [in] bit_offset The current bit offset
     * \return true/false
    */
    bool MoreRbspData(uint8_t *p_stream, size_t stream_size_in_byte, size_t bit_offset);

    /*! \brief Function to calculate picture order count of the current slice
     */
    void CalculateCurrPoc();

    /*! \brief Function to set up the reference picutre lists.
     */
    void SetupReflist();

#if DBGINFO
    /*! \brief Function to log out parsed SPS content for debug.
    */
    void PrintSps(AvcSeqParameterSet *p_sps);

    /*! \brief Function to log out parsed PPS content for debug.
    */
    void PrintPps(AvcPicParameterSet *p_pps);

    /*! \brief Function to log out parsed slice header content for debug.
    */
    void PrintSliceHeader(AvcSliceHeader *p_slice_header);
#endif // DBGINFO
};