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

#include "bit_stream_parser_h265.h"

#include <vector>
#include <map>
#include <algorithm>

//sizeId = 0
extern int scaling_list_default_0[1][6][16];
//sizeId = 1, 2
extern int scaling_list_default_1_2[2][6][64];
//sizeId = 3
extern int scaling_list_default_3[1][2][64];

class HevcParser : public BitStreamParser {
public:
    HevcParser(DataStream *stream, int nSize, int64_t pts);
    virtual ~HevcParser();

    virtual int                     GetOffsetX() const;
    virtual int                     GetOffsetY() const;
    virtual int                     GetPictureWidth() const;
    virtual int                     GetPictureHeight() const;
    virtual int                     GetAlignedWidth() const;
    virtual int                     GetAlignedHeight() const;

    virtual void                    SetMaxFramesNumber(size_t num) { m_maxFramesNumber_ = num; }

    virtual const unsigned char*    GetExtraData() const;
    virtual size_t                  GetExtraDataSize() const;
    virtual void                    SetUseStartCodes(bool bUse);
    virtual void                    SetFrameRate(double fps);
    virtual double                  GetFrameRate()  const;
    virtual PARSER_RESULT           ReInit();
    virtual void                    GetFrameRate(ParserRate *frameRate) const;
    // TODO: Find equivalent input for AMF
    
    //virtual rocDecStatus              QueryOutput(amf::AMFData** ppData);

protected:
    // ISO-IEC 14496-15-2004.pdf, page 14, table 1 " NAL unit types in elementary streams.
    enum NalUnitType {
        NAL_UNIT_CODED_SLICE_TRAIL_N = 0, // 0
        NAL_UNIT_CODED_SLICE_TRAIL_R,     // 1
    
        NAL_UNIT_CODED_SLICE_TSA_N,       // 2
        NAL_UNIT_CODED_SLICE_TLA_R,       // 3
    
        NAL_UNIT_CODED_SLICE_STSA_N,      // 4
        NAL_UNIT_CODED_SLICE_STSA_R,      // 5

        NAL_UNIT_CODED_SLICE_RADL_N,      // 6
        NAL_UNIT_CODED_SLICE_RADL_R,      // 7
    
        NAL_UNIT_CODED_SLICE_RASL_N,      // 8
        NAL_UNIT_CODED_SLICE_RASL_R,      // 9

        NAL_UNIT_RESERVED_VCL_N10,
        NAL_UNIT_RESERVED_VCL_R11,
        NAL_UNIT_RESERVED_VCL_N12,
        NAL_UNIT_RESERVED_VCL_R13,
        NAL_UNIT_RESERVED_VCL_N14,
        NAL_UNIT_RESERVED_VCL_R15,

        NAL_UNIT_CODED_SLICE_BLA_W_LP,    // 16
        NAL_UNIT_CODED_SLICE_BLA_W_RADL,  // 17
        NAL_UNIT_CODED_SLICE_BLA_N_LP,    // 18
        NAL_UNIT_CODED_SLICE_IDR_W_RADL,  // 19
        NAL_UNIT_CODED_SLICE_IDR_N_LP,    // 20
        NAL_UNIT_CODED_SLICE_CRA,         // 21
        NAL_UNIT_RESERVED_IRAP_VCL22,
        NAL_UNIT_RESERVED_IRAP_VCL23,

        NAL_UNIT_RESERVED_VCL24,
        NAL_UNIT_RESERVED_VCL25,
        NAL_UNIT_RESERVED_VCL26,
        NAL_UNIT_RESERVED_VCL27,
        NAL_UNIT_RESERVED_VCL28,
        NAL_UNIT_RESERVED_VCL29,
        NAL_UNIT_RESERVED_VCL30,
        NAL_UNIT_RESERVED_VCL31,

        NAL_UNIT_VPS,                     // 32
        NAL_UNIT_SPS,                     // 33
        NAL_UNIT_PPS,                     // 34
        NAL_UNIT_ACCESS_UNIT_DELIMITER,   // 35
        NAL_UNIT_EOS,                     // 36
        NAL_UNIT_EOB,                     // 37
        NAL_UNIT_FILLER_DATA,             // 38
        NAL_UNIT_PREFIX_SEI,              // 39
        NAL_UNIT_SUFFIX_SEI,              // 40
        NAL_UNIT_RESERVED_NVCL41,
        NAL_UNIT_RESERVED_NVCL42,
        NAL_UNIT_RESERVED_NVCL43,
        NAL_UNIT_RESERVED_NVCL44,
        NAL_UNIT_RESERVED_NVCL45,
        NAL_UNIT_RESERVED_NVCL46,
        NAL_UNIT_RESERVED_NVCL47,
        NAL_UNIT_UNSPECIFIED_48,
        NAL_UNIT_UNSPECIFIED_49,
        NAL_UNIT_UNSPECIFIED_50,
        NAL_UNIT_UNSPECIFIED_51,
        NAL_UNIT_UNSPECIFIED_52,
        NAL_UNIT_UNSPECIFIED_53,
        NAL_UNIT_UNSPECIFIED_54,
        NAL_UNIT_UNSPECIFIED_55,
        NAL_UNIT_UNSPECIFIED_56,
        NAL_UNIT_UNSPECIFIED_57,
        NAL_UNIT_UNSPECIFIED_58,
        NAL_UNIT_UNSPECIFIED_59,
        NAL_UNIT_UNSPECIFIED_60,
        NAL_UNIT_UNSPECIFIED_61,
        NAL_UNIT_UNSPECIFIED_62,
        NAL_UNIT_UNSPECIFIED_63,
        NAL_UNIT_INVALID,
    };

    struct NalUnitHeader {
        uint32_t forbidden_zero_bit;
        uint32_t nal_unit_type;
        uint32_t nuh_layer_id;
        uint32_t nuh_temporal_id_plus1;
        uint32_t num_emu_byte_removed;
    };

    enum H265_ScalingListSize {
        H265_SCALING_LIST_4x4 = 0,
        H265_SCALING_LIST_8x8,
        H265_SCALING_LIST_16x16,
        H265_SCALING_LIST_32x32,
        H265_SCALING_LIST_SIZE_NUM
    };

    typedef struct {
        uint32_t general_profile_space;                      //u(2)
        bool general_tier_flag;                              //u(1)
        uint32_t general_profile_idc;                        //u(5)
        bool general_profile_compatibility_flag[32];         //u(1)
        bool general_progressive_source_flag;                //u(1)
        bool general_interlaced_source_flag;                 //u(1)
        bool general_non_packed_constraint_flag;             //u(1)
        bool general_frame_only_constraint_flag;             //u(1)
        uint64_t general_reserved_zero_44bits;               //u(44)
        uint32_t general_level_idc;                          //u(8)
        //maxNumSubLayersMinus1 max is 7 - 1 = 6
        bool sub_layer_profile_present_flag[6];              //u(1)
        bool sub_layer_level_present_flag[6];                //u(1)

        uint32_t reserved_zero_2bits[8];                     //u(2)

        uint32_t sub_layer_profile_space[6];                 //u(2)
        bool sub_layer_tier_flag[6];                         //u(1)
        uint32_t sub_layer_profile_idc[6];                   //u(5)
        bool sub_layer_profile_compatibility_flag[6][32];    //u(1)
        bool sub_layer_progressive_source_flag[6];           //u(1)
        bool sub_layer_interlaced_source_flag[6];            //u(1)
        bool sub_layer_non_packed_constraint_flag[6];        //u(1)
        bool sub_layer_frame_only_constraint_flag[6];        //u(1)
        uint64_t sub_layer_reserved_zero_44bits[6];          //u(44)
        uint32_t sub_layer_level_idc[6];                     //u(8)
    } H265_profile_tier_level_t;

#define H265_SCALING_LIST_NUM 6         ///< list number for quantization matrix
#define H265_SCALING_LIST_MAX_I 64

    typedef struct {
        bool scaling_list_pred_mode_flag[4][6];              //u(1)
        uint32_t scaling_list_pred_matrix_id_delta[4][6];    //ue(v)
        int32_t scaling_list_dc_coef_minus8[4][6];           //se(v)
        int32_t scaling_list_delta_coef;                     //se(v)         could have issues......
        int32_t ScalingList[H265_SCALING_LIST_SIZE_NUM][H265_SCALING_LIST_NUM][H265_SCALING_LIST_MAX_I];
    } H265_scaling_list_data_t;

    typedef struct {
        int32_t num_negative_pics;
        int32_t num_positive_pics;
        int32_t num_of_pics;
        int32_t num_of_delta_poc;
        int32_t deltaPOC[16];
        bool used_by_curr_pic[16];
    } H265_short_term_RPS_t;

    typedef struct {
        int32_t num_of_pics;
        int32_t POCs[32];
        bool used_by_curr_pic[32];
    } H265_long_term_RPS_t;

    typedef struct {
        //CpbCnt = cpb_cnt_minus1
        uint32_t bit_rate_value_minus1[32];                  //ue(v)
        uint32_t cpb_size_value_minus1[32];                  //ue(v)
        uint32_t cpb_size_du_value_minus1[32];               //ue(v)
        uint32_t bit_rate_du_value_minus1[32];               //ue(v)
        bool cbr_flag[32];                                   //u(1)
    } H265_sub_layer_hrd_parameters;

    typedef struct {
        bool nal_hrd_parameters_present_flag;                //u(1)
        bool vcl_hrd_parameters_present_flag;                //u(1)
        bool sub_pic_hrd_params_present_flag;                //u(1)
        uint32_t tick_divisor_minus2;                        //u(8)
        uint32_t du_cpb_removal_delay_increment_length_minus1;  //u(5)
        bool sub_pic_cpb_params_in_pic_timing_sei_flag;      //u(1)
        uint32_t dpb_output_delay_du_length_minus1;          //u(5)
        uint32_t bit_rate_scale;                             //u(4)
        uint32_t cpb_size_scale;                             //u(4)
        uint32_t cpb_size_du_scale;                          //u(4)
        uint32_t initial_cpb_removal_delay_length_minus1;    //u(5)
        uint32_t au_cpb_removal_delay_length_minus1;         //u(5)
        uint32_t dpb_output_delay_length_minus1;             //u(5)
        bool fixed_pic_rate_general_flag[7];                 //u(1)
        bool fixed_pic_rate_within_cvs_flag[7];              //u(1)
        uint32_t elemental_duration_in_tc_minus1[7];         //ue(v)
        bool low_delay_hrd_flag[7];                          //u(1)
        uint32_t cpb_cnt_minus1[7];                          //ue(v)
        //sub_layer_hrd_parameters()
        H265_sub_layer_hrd_parameters sub_layer_hrd_parameters_0[7];
        //sub_layer_hrd_parameters()
        H265_sub_layer_hrd_parameters sub_layer_hrd_parameters_1[7];
    } H265_hrd_parameters_t;

    typedef struct {
        bool aspect_ratio_info_present_flag;                 //u(1)
        uint32_t aspect_ratio_idc;                           //u(8)
        uint32_t sar_width;                                  //u(16)
        uint32_t sar_height;                                 //u(16)
        bool overscan_info_present_flag;                     //u(1)
        bool overscan_appropriate_flag;                      //u(1)
        bool video_signal_type_present_flag;                 //u(1)
        uint32_t video_format;                               //u(3)
        bool video_full_range_flag;                          //u(1)
        bool colour_description_present_flag;                //u(1)
        uint32_t colour_primaries;                           //u(8)
        uint32_t transfer_characteristics;                   //u(8)
        uint32_t matrix_coeffs;                              //u(8)
        bool chroma_loc_info_present_flag;                   //u(1)
        uint32_t chroma_sample_loc_type_top_field;           //ue(v)
        uint32_t chroma_sample_loc_type_bottom_field;        //ue(v)
        bool neutral_chroma_indication_flag;                 //u(1)
        bool field_seq_flag;                                 //u(1)
        bool frame_field_info_present_flag;                  //u(1)
        bool default_display_window_flag;                    //u(1)
        uint32_t def_disp_win_left_offset;                   //ue(v)
        uint32_t def_disp_win_right_offset;                  //ue(v)
        uint32_t def_disp_win_top_offset;                    //ue(v)
        uint32_t def_disp_win_bottom_offset;                 //ue(v)
        bool vui_timing_info_present_flag;                   //u(1)
        uint32_t vui_num_units_in_tick;                      //u(32)
        uint32_t vui_time_scale;                             //u(32)
        bool vui_poc_proportional_to_timing_flag;            //u(1)
        uint32_t vui_num_ticks_poc_diff_one_minus1;          //ue(v)
        bool vui_hrd_parameters_present_flag;                //u(1)
        //hrd_parameters()
        H265_hrd_parameters_t hrd_parameters;
        bool bitstream_restriction_flag;                     //u(1)
        bool tiles_fixed_structure_flag;                     //u(1)
        bool motion_vectors_over_pic_boundaries_flag;        //u(1)
        bool restricted_ref_pic_lists_flag;                  //u(1)
        uint32_t min_spatial_segmentation_idc;               //ue(v)
        uint32_t max_bytes_per_pic_denom;                    //ue(v)
        uint32_t max_bits_per_min_cu_denom;                  //ue(v)
        uint32_t log2_max_mv_length_horizontal;              //ue(v)
        uint32_t log2_max_mv_length_vertical;                //ue(v)
    } H265_vui_parameters_t;

    typedef struct {
        uint32_t rbsp_stop_one_bit; /* equal to 1 */
        uint32_t rbsp_alignment_zero_bit; /* equal to 0 */
    } H265_rbsp_trailing_bits_t;

    struct SpsData {
        uint32_t sps_video_parameter_set_id;                 //u(4)
        uint32_t sps_max_sub_layers_minus1;                  //u(3)
        bool sps_temporal_id_nesting_flag;                   //u(1)
        //profile_tier_level( sps_max_sub_layers_minus1 )
        H265_profile_tier_level_t profile_tier_level;
        uint32_t sps_seq_parameter_set_id;                   //ue(v)
        uint32_t chroma_format_idc;                          //ue(v)
        bool separate_colour_plane_flag;                     //u(1)
        uint32_t pic_width_in_luma_samples;                  //ue(v)
        uint32_t pic_height_in_luma_samples;                 //ue(v)
        uint32_t max_cu_width;
        uint32_t max_cu_height;
        uint32_t max_cu_depth;
        bool conformance_window_flag;                        //u(1)
        uint32_t conf_win_left_offset;                       //ue(v)
        uint32_t conf_win_right_offset;                      //ue(v)
        uint32_t conf_win_top_offset;                        //ue(v)
        uint32_t conf_win_bottom_offset;                     //ue(v)
        uint32_t bit_depth_luma_minus8;                      //ue(v)
        uint32_t bit_depth_chroma_minus8;                    //ue(v)
        uint32_t log2_max_pic_order_cnt_lsb_minus4;          //ue(v)
        bool sps_sub_layer_ordering_info_present_flag;       //u(1)
        uint32_t sps_max_dec_pic_buffering_minus1[6];        //ue(v)
        uint32_t sps_max_num_reorder_pics[6];                //ue(v)
        uint32_t sps_max_latency_increase_plus1[6];          //ue(v)
        uint32_t log2_min_luma_coding_block_size_minus3;     //ue(v)
        uint32_t log2_diff_max_min_luma_coding_block_size;   //ue(v)
        uint32_t log2_min_transform_block_size_minus2;       //ue(v)
        uint32_t log2_diff_max_min_transform_block_size;     //ue(v)
        uint32_t max_transform_hierarchy_depth_inter;        //ue(v)
        uint32_t max_transform_hierarchy_depth_intra;        //ue(v)
        bool scaling_list_enabled_flag;                      //u(1)
        bool sps_scaling_list_data_present_flag;             //u(1)
        //scaling_list_data()
        H265_scaling_list_data_t scaling_list_data;
        bool amp_enabled_flag;                               //u(1)
        bool sample_adaptive_offset_enabled_flag;            //u(1)
        bool pcm_enabled_flag;                               //u(1)
        uint32_t pcm_sample_bit_depth_luma_minus1;           //u(4)
        uint32_t pcm_sample_bit_depth_chroma_minus1;         //u(4)
        uint32_t log2_min_pcm_luma_coding_block_size_minus3; //ue(v)
        uint32_t log2_diff_max_min_pcm_luma_coding_block_size;  //ue(v)
        bool pcm_loop_filter_disabled_flag;                  //u(1)
        uint32_t num_short_term_ref_pic_sets;                //ue(v)
        //short_term_ref_pic_set(i) max is 64
        H265_short_term_RPS_t stRPS[64];
        H265_long_term_RPS_t ltRPS;
        //H265_short_term_ref_pic_set_t short_term_ref_pic_set[64];
        bool long_term_ref_pics_present_flag;                //u(1)
        uint32_t num_long_term_ref_pics_sps;                 //ue(v)
        //max is 32
        uint32_t lt_ref_pic_poc_lsb_sps[32];                 //u(v)
        bool used_by_curr_pic_lt_sps_flag[32];               //u(1)
        bool sps_temporal_mvp_enabled_flag;                  //u(1)
        bool strong_intra_smoothing_enabled_flag;            //u(1)
        bool vui_parameters_present_flag;                    //u(1)
        //vui_parameters()
        H265_vui_parameters_t vui_parameters;
        bool sps_extension_flag;                             //u(1)
        bool sps_extension_data_flag;                        //u(1)
        //rbsp_trailing_bits( )
        H265_rbsp_trailing_bits_t rbsp_trailing_bits;

        SpsData(void) {
            memset(this, 0, sizeof(*this));
        }

        bool Parse(uint8_t *data, size_t size);
        void ParsePTL(H265_profile_tier_level_t *ptl, bool profilePresentFlag, uint32_t maxNumSubLayersMinus1, uint8_t *nalu, size_t size, size_t &offset);
        void ParseSubLayerHrdParameters(H265_sub_layer_hrd_parameters *sub_hrd, uint32_t CpbCnt, bool sub_pic_hrd_params_present_flag, uint8_t *nalu, size_t size, size_t &offset);
        void ParseHrdParameters(H265_hrd_parameters_t *hrd, bool commonInfPresentFlag, uint32_t maxNumSubLayersMinus1, uint8_t *nalu, size_t size, size_t &offset);
        static void ParseScalingList(H265_scaling_list_data_t * s_data, uint8_t *data, size_t size,size_t &offset);
        void ParseVUI(H265_vui_parameters_t *vui, uint32_t maxNumSubLayersMinus1, uint8_t *data, size_t size,size_t &offset);
        void ParseShortTermRefPicSet(H265_short_term_RPS_t *rps, int32_t stRpsIdx, uint32_t num_short_term_ref_pic_sets, H265_short_term_RPS_t rps_ref[], uint8_t *data, size_t size,size_t &offset);
    };

    struct PpsData {
        uint32_t pps_pic_parameter_set_id;                   //ue(v)
        uint32_t pps_seq_parameter_set_id;                   //ue(v)
        bool dependent_slice_segments_enabled_flag;          //u(1)
        bool output_flag_present_flag;                       //u(1)
        uint32_t num_extra_slice_header_bits;                //u(3)
        bool sign_data_hiding_enabled_flag;                  //u(1)
        bool cabac_init_present_flag;                        //u(1)
        uint32_t num_ref_idx_l0_default_active_minus1;       //ue(v)
        uint32_t num_ref_idx_l1_default_active_minus1;       //ue(v)
        int32_t init_qp_minus26;                             //se(v)
        bool constrained_intra_pred_flag;                    //u(1)
        bool transform_skip_enabled_flag;                    //u(1)
        bool cu_qp_delta_enabled_flag;                       //u(1)
        uint32_t diff_cu_qp_delta_depth;                     //ue(v)
        int32_t pps_cb_qp_offset;                            //se(v)
        int32_t pps_cr_qp_offset;                            //se(v)
        bool pps_slice_chroma_qp_offsets_present_flag;       //u(1)
        bool weighted_pred_flag;                             //u(1)
        bool weighted_bipred_flag;                           //u(1)
        bool transquant_bypass_enabled_flag;                 //u(1)
        bool tiles_enabled_flag;                             //u(1)
        bool entropy_coding_sync_enabled_flag;               //u(1)
        uint32_t num_tile_columns_minus1;                    //ue(v)
        uint32_t num_tile_rows_minus1;                       //ue(v)
        bool uniform_spacing_flag;                           //u(1)
        //PicWidthInCtbsY = Ceil( pic_width_in_luma_samples / CtbSizeY )  = 256 assume max width is 4096
        //CtbSizeY = 1<<CtbLog2SizeY   so min is 16
        // 4 <= CtbLog2SizeY <= 6
        uint32_t column_width_minus1[265];                   //ue(v)
        //2304/16=144 assume max height is 2304
        uint32_t row_height_minus1[144];                     //ue(v)
        bool loop_filter_across_tiles_enabled_flag;          //u(1)
        bool pps_loop_filter_across_slices_enabled_flag;     //u(1)
        bool deblocking_filter_control_present_flag;         //u(1)
        bool deblocking_filter_override_enabled_flag;        //u(1)
        bool pps_deblocking_filter_disabled_flag;            //u(1)
        int32_t pps_beta_offset_div2;                        //se(v)
        int32_t pps_tc_offset_div2;                          //se(v)
        bool pps_scaling_list_data_present_flag;             //u(1)
        //scaling_list_data( )
        H265_scaling_list_data_t scaling_list_data;
        bool lists_modification_present_flag;                //u(1)
        uint32_t log2_parallel_merge_level_minus2;           //ue(v)
        bool slice_segment_header_extension_present_flag;    //u(1)
        bool pps_extension_flag;                             //u(1)
        bool pps_extension_data_flag;                        //u(1)
        //rbsp_trailing_bits( )
        H265_rbsp_trailing_bits_t rbsp_trailing_bits;
        PpsData(void) {
            memset(this, 0, sizeof(*this));
        }
        bool Parse(uint8_t *data, size_t size);
    };

    // See ITU-T Rec. H.264 (04/2013) Advanced video coding for generic audiovisual services, page 28, 91.
    struct AccessUnitSigns {
        bool bNewPicture;
        AccessUnitSigns() : bNewPicture(false) {}
        bool Parse(uint8_t *data, size_t size, std::map<uint32_t,SpsData> &spsMap, std::map<uint32_t,PpsData> &ppsMap);
        bool IsNewPicture();
    };

    class ExtraDataBuilder {
    public:
        ExtraDataBuilder() : m_SPSCount_(0), m_PPSCount_(0) {}

        void AddSPS(uint8_t *sps, size_t size);
        void AddPPS(uint8_t *pps, size_t size);
        bool GetExtradata(ByteArray   &extradata);

    private:
        ByteArray    m_SPSs_;
        ByteArray    m_PPSs_;
        int32_t      m_SPSCount_;
        int32_t      m_PPSCount_;
    };

    friend struct AccessUnitSigns;

    static const uint32_t MacroblocSize = 16;
    static const uint8_t NalUnitTypeMask = 0x1F; // b00011111
    static const uint8_t NalRefIdcMask = 0x60;   // b01100000
    static const uint8_t NalUnitLengthSize = 4U;

    static const size_t m_ReadSize_ = 1024*4;

    static const uint16_t maxSpsSize = 0xFFFF;
    static const uint16_t minSpsSize = 5;
    static const uint16_t maxPpsSize = 0xFFFF;

    NalUnitHeader ReadNextNaluUnit(size_t *offset, size_t *nalu, size_t *size);
    void          FindSPSandPPS();
    static inline NalUnitHeader GetNaluUnitType(uint8_t *nalUnit) {
        NalUnitHeader nalu_header;
        nalu_header.num_emu_byte_removed = 0;
        //read nalu header
        nalu_header.forbidden_zero_bit = (uint32_t) ((nalUnit[0] >> 7)&1);
        nalu_header.nal_unit_type = (uint32_t) ((nalUnit[0] >> 1)&63);
        nalu_header.nuh_layer_id = (uint32_t) (((nalUnit[0]&1) << 6) | ((nalUnit[1] & 248) >> 3));
        nalu_header.nuh_temporal_id_plus1 = (uint32_t) (nalUnit[1] & 7);

        return nalu_header;
    }
    size_t EBSPtoRBSP(uint8_t *streamBuffer,size_t begin_bytepos, size_t end_bytepos);
    ParserRect GetCropRect() const;

typedef     int64_t           pts;     // in 100 nanosecs

    ByteArray   m_ReadData_;
    ByteArray   m_Extradata_;
    
    ByteArray   m_EBSPtoRBSPData_;

    bool           m_bUseStartCodes_;
    pts            m_currentFrameTimestamp_;
    DataStreamPtr  m_pStream_;
    std::map<uint32_t,SpsData> m_SpsMap_;
    std::map<uint32_t,PpsData> m_PpsMap_;
    size_t         m_PacketCount_;
    bool           m_bEof_;
    double         m_fps_;
    size_t         m_maxFramesNumber_;
};

BitStreamParser* CreateHEVCParser(DataStream* pStream, int nSize, int64_t pts) {
    return new HevcParser(pStream, nSize, pts);
}

HevcParser::HevcParser(DataStream *stream, int nSize, int64_t pts) :
    m_bUseStartCodes_(false),
    m_currentFrameTimestamp_(0),
    m_pStream_(stream),
    m_PacketCount_(0),
    m_bEof_(false),
    m_fps_(0),
    m_maxFramesNumber_(0) {
    stream->Seek(PARSER_SEEK_BEGIN, 0, NULL);
    FindSPSandPPS();
}

HevcParser::~HevcParser() {}

PARSER_RESULT HevcParser::ReInit()
{
    m_currentFrameTimestamp_ = 0;
    m_pStream_->Seek(PARSER_SEEK_BEGIN, 0, NULL);
    m_PacketCount_ = 0;
    m_bEof_ = false;
    return PARSER_OK;
}

static const int s_winUnitX[]={1,2,2,1};
static const int s_winUnitY[]={1,2,1,1};

static int getWinUnitX (int chromaFormatIdc) { return s_winUnitX[chromaFormatIdc];      }
// static int getWinUnitY (int chromaFormatIdc) { return s_winUnitY[chromaFormatIdc];      }
static const int MacroblockSize = 16;


ParserRect HevcParser::GetCropRect() const {
    ParserRect rect ={0};
    if(m_SpsMap_.size() == 0) {
        return rect;
    }
    const SpsData &sps = m_SpsMap_.cbegin()->second;

    rect.right = int32_t(sps.pic_width_in_luma_samples);
    rect.bottom = int32_t(sps.pic_height_in_luma_samples);

    if (sps.conformance_window_flag)
    {
        rect.left += getWinUnitX(sps.chroma_format_idc) * sps.conf_win_left_offset;
        rect.right -= getWinUnitX(sps.chroma_format_idc) * sps.conf_win_right_offset;
        rect.top += getWinUnitX(sps.chroma_format_idc) * sps.conf_win_top_offset;
        rect.bottom -= getWinUnitX(sps.chroma_format_idc) * sps.conf_win_bottom_offset;
    }
    return rect;
}

int  HevcParser::GetOffsetX() const {
    return GetCropRect().left;
}

int  HevcParser::GetOffsetY() const {
    return GetCropRect().top;
}

int HevcParser::GetPictureWidth() const {
    return GetCropRect().Width();
}

int HevcParser::GetPictureHeight() const {
    return GetCropRect().Height();
}

int HevcParser::GetAlignedWidth() const {
    if(m_SpsMap_.size() == 0) {
        return 0;
    }
    const SpsData &sps = m_SpsMap_.cbegin()->second;

    int32_t blocksize = sps.log2_min_luma_coding_block_size_minus3+3;
    int width =int(sps.pic_width_in_luma_samples / (1<<blocksize) * (1<<blocksize));
    return width;
}

int HevcParser::GetAlignedHeight() const {
    if(m_SpsMap_.size() == 0) {
        return 0;
    }
    const SpsData &sps = m_SpsMap_.cbegin()->second;

    int32_t blocksize = sps.log2_min_luma_coding_block_size_minus3+3;
    int height = int(sps.pic_height_in_luma_samples / (1<<blocksize) * (1<<blocksize));
    return height;
}

const unsigned char* HevcParser::GetExtraData() const {
    return m_Extradata_.GetData();
}

size_t HevcParser::GetExtraDataSize() const {
    return m_Extradata_.GetSize();
};

void HevcParser::SetUseStartCodes(bool bUse) {
    m_bUseStartCodes_ = bUse;
}

void HevcParser::SetFrameRate(double fps) {
    m_fps_ = fps;
}

double HevcParser::GetFrameRate() const {
    if(m_fps_ != 0) {
        return m_fps_;
    }
    if(m_SpsMap_.size() > 0) {
        const SpsData &sps = m_SpsMap_.cbegin()->second;
        if(sps.vui_parameters_present_flag && sps.vui_parameters.vui_timing_info_present_flag && sps.vui_parameters.vui_num_units_in_tick) {
            // according to the latest h264 standard nuit_field_based_flag is always = 1 and therefore this must be divided by two 
            // some old clips may get wrong FPS. This is just a sample. Use container information
            return (double)sps.vui_parameters.vui_time_scale / sps.vui_parameters.vui_num_units_in_tick / 2;
        }
    }
    return 25.0;
}

void HevcParser::GetFrameRate(ParserRate *frameRate) const {
    if(m_SpsMap_.size() > 0) {
        const SpsData &sps = m_SpsMap_.cbegin()->second;
        if(sps.vui_parameters_present_flag && sps.vui_parameters.vui_timing_info_present_flag && sps.vui_parameters.vui_num_units_in_tick) {
            // according to the latest h264 standard nuit_field_based_flag is always = 1 and therefore this must be divided by two 
            // some old clips may get wrong FPS. This is just a sample. Use container information
            frameRate->num = sps.vui_parameters.vui_time_scale / 2;
            frameRate->den = sps.vui_parameters.vui_num_units_in_tick;
            return;
        }
    }
    frameRate->num = 0;
    frameRate->den = 0;
}

HevcParser::NalUnitHeader HevcParser::ReadNextNaluUnit(size_t *offset, size_t *nalu, size_t *size) {
    *size = 0;
    size_t startOffset = *offset;

    bool newNalFound = false;
    size_t zerosCount = 0;

    while(!newNalFound) {
        // read next portion if needed
        size_t ready = m_ReadData_.GetSize() - *offset;
        //printf("ReadNextNaluUnit: remaining data size for read: %d\n", ready);
        if (ready == 0) {
            if (m_bEof_ == false) {
                m_ReadData_.SetSize(m_ReadData_.GetSize() + m_ReadSize_);
                ready = 0;
                m_pStream_->Read(m_ReadData_.GetData() + *offset, m_ReadSize_, &ready);
            }
            if (ready != m_ReadSize_ && ready != 0) {
                m_ReadData_.SetSize(m_ReadData_.GetSize() - (m_ReadSize_ - ready));
            }
            if(ready == 0 ) {
                if (m_bEof_ == false)
                m_ReadData_.SetSize(m_ReadData_.GetSize() - m_ReadSize_);

                m_bEof_ = true;
                newNalFound = startOffset != *offset; 
                *offset = m_ReadData_.GetSize();
                break; // EOF
            }
        }

        uint8_t* data = m_ReadData_.GetData();
        if (data == nullptr) { // check data before adding the offset
            NalUnitHeader header_nalu;
            header_nalu.nal_unit_type = NAL_UNIT_INVALID;
            return header_nalu; // no data read
        }
        data += *offset; // don't forget the offset!

        for(size_t i = 0; i < ready; i++) {
            uint8_t ch = *data++;
            if (0 == ch) {
                zerosCount++;
            }
            else {
                if (1 == ch && zerosCount >=2) { // We found a start code in Annex B stream
                    if(*offset + (i - zerosCount) > startOffset) {
                        ready = i - zerosCount;
                        newNalFound = true; // new NAL
                        break; 
                    }
                    else {
                        *nalu = *offset + zerosCount + 1;
                    }
                }
                zerosCount = 0;
            }
        }
        // if zeros found but not a new NAL - continue with zerosCount on the next iteration
        *offset += ready;
    }
    if(!newNalFound) {
        NalUnitHeader header_nalu;
        header_nalu.nal_unit_type = NAL_UNIT_INVALID;
        return header_nalu; // EOF
    }
    *size = *offset - *nalu;
    // get NAL type
    return GetNaluUnitType(m_ReadData_.GetData() + *nalu);
}

void HevcParser::FindSPSandPPS() {
    ExtraDataBuilder extraDataBuilder;

    size_t dataOffset = 0;
    do {
        
        size_t naluSize = 0;
        size_t naluOffset = 0;
        NalUnitHeader naluHeader = ReadNextNaluUnit(&dataOffset, &naluOffset, &naluSize);

        if (naluHeader.nal_unit_type == NAL_UNIT_INVALID ) {
            break; // EOF
        }

        if (naluHeader.nal_unit_type == NAL_UNIT_SPS) {
            m_EBSPtoRBSPData_.SetSize(naluSize);
            memcpy(m_EBSPtoRBSPData_.GetData(), m_ReadData_.GetData() + naluOffset, naluSize);
            size_t newNaluSize = EBSPtoRBSP(m_EBSPtoRBSPData_.GetData(),0, naluSize);

            SpsData sps;
            sps.Parse(m_EBSPtoRBSPData_.GetData(), newNaluSize);
            m_SpsMap_[sps.sps_video_parameter_set_id] = sps;
            extraDataBuilder.AddSPS(m_ReadData_.GetData()+naluOffset, naluSize);
        }
        else if (naluHeader.nal_unit_type == NAL_UNIT_PPS) {
            m_EBSPtoRBSPData_.SetSize(naluSize);
            memcpy(m_EBSPtoRBSPData_.GetData(), m_ReadData_.GetData() + naluOffset, naluSize);
            size_t newNaluSize = EBSPtoRBSP(m_EBSPtoRBSPData_.GetData(),0, naluSize);

            PpsData pps;
            pps.Parse(m_EBSPtoRBSPData_.GetData(), newNaluSize);
            m_PpsMap_[pps.pps_pic_parameter_set_id] = pps;
            extraDataBuilder.AddPPS(m_ReadData_.GetData()+naluOffset, naluSize);
        }
        else if (
        NAL_UNIT_CODED_SLICE_TRAIL_R == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TRAIL_N == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TLA_R == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TSA_N == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_STSA_R == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_STSA_N == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_W_LP == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_W_RADL == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_N_LP == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_IDR_W_RADL == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_IDR_N_LP == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_CRA == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RADL_N == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RADL_R == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RASL_N == naluHeader.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RASL_R == naluHeader.nal_unit_type
        ) {
            break; // frame data
        }
    } while (true);

    m_pStream_->Seek(PARSER_SEEK_BEGIN, 0, NULL);
    m_ReadData_.SetSize(0);
    // It will fail if SPS or PPS are absent
    extraDataBuilder.GetExtradata(m_Extradata_);
}

bool HevcParser::SpsData::Parse(uint8_t *nalu, size_t size) {
    size_t offset = 16; // 2 bytes NALU header + 
    uint32_t activeVPS = Parser::readBits(nalu, offset,4);
    uint32_t max_sub_layer_minus1 = Parser::readBits(nalu, offset,3);
    sps_temporal_id_nesting_flag = Parser::getBit(nalu, offset);
    H265_profile_tier_level_t ptl;
    memset (&ptl,0,sizeof(ptl));
    ParsePTL(&ptl, true, max_sub_layer_minus1, nalu, size, offset);
    uint32_t SPS_ID = Parser::ExpGolomb::readUe(nalu, offset);

    sps_video_parameter_set_id = activeVPS;
    sps_max_sub_layers_minus1 = max_sub_layer_minus1;
    memcpy (&profile_tier_level,&ptl,sizeof(ptl));
    sps_seq_parameter_set_id = SPS_ID;

    chroma_format_idc = Parser::ExpGolomb::readUe(nalu, offset);
    if (chroma_format_idc == 3) {
        separate_colour_plane_flag = Parser::getBit(nalu, offset);
    }
    pic_width_in_luma_samples = Parser::ExpGolomb::readUe(nalu, offset);
    pic_height_in_luma_samples = Parser::ExpGolomb::readUe(nalu, offset);
    conformance_window_flag = Parser::getBit(nalu, offset);
    if (conformance_window_flag) {
        conf_win_left_offset = Parser::ExpGolomb::readUe(nalu, offset);
        conf_win_right_offset = Parser::ExpGolomb::readUe(nalu, offset);
        conf_win_top_offset = Parser::ExpGolomb::readUe(nalu, offset);
        conf_win_bottom_offset = Parser::ExpGolomb::readUe(nalu, offset);
    }
    bit_depth_luma_minus8 = Parser::ExpGolomb::readUe(nalu, offset);
    bit_depth_chroma_minus8 = Parser::ExpGolomb::readUe(nalu, offset);
    log2_max_pic_order_cnt_lsb_minus4 = Parser::ExpGolomb::readUe(nalu, offset);
    sps_sub_layer_ordering_info_present_flag = Parser::getBit(nalu, offset);
    for (uint32_t i=(sps_sub_layer_ordering_info_present_flag?0:sps_max_sub_layers_minus1); i<=sps_max_sub_layers_minus1; i++) {
        sps_max_dec_pic_buffering_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
        sps_max_num_reorder_pics[i] = Parser::ExpGolomb::readUe(nalu, offset);
        sps_max_latency_increase_plus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
    }
    log2_min_luma_coding_block_size_minus3 = Parser::ExpGolomb::readUe(nalu, offset);

    int log2MinCUSize = log2_min_luma_coding_block_size_minus3 +3;

    log2_diff_max_min_luma_coding_block_size = Parser::ExpGolomb::readUe(nalu, offset);

    int maxCUDepthDelta = log2_diff_max_min_luma_coding_block_size;
    max_cu_width = ( 1<<(log2MinCUSize + maxCUDepthDelta) );
    max_cu_height = ( 1<<(log2MinCUSize + maxCUDepthDelta) );

    log2_min_transform_block_size_minus2 = Parser::ExpGolomb::readUe(nalu, offset);

    uint32_t QuadtreeTULog2MinSize = log2_min_transform_block_size_minus2 + 2;
    int addCuDepth = std::max (0, log2MinCUSize - (int)QuadtreeTULog2MinSize );
    max_cu_depth = (maxCUDepthDelta + addCuDepth);

    log2_diff_max_min_transform_block_size = Parser::ExpGolomb::readUe(nalu, offset);
    max_transform_hierarchy_depth_inter = Parser::ExpGolomb::readUe(nalu, offset);
    max_transform_hierarchy_depth_intra = Parser::ExpGolomb::readUe(nalu, offset);
    scaling_list_enabled_flag = Parser::getBit(nalu, offset);
    if (scaling_list_enabled_flag) {
        sps_scaling_list_data_present_flag = Parser::getBit(nalu, offset);
        if (sps_scaling_list_data_present_flag) {
            ParseScalingList(&scaling_list_data, nalu, size, offset);
        }
    }
    amp_enabled_flag = Parser::getBit(nalu, offset);
    sample_adaptive_offset_enabled_flag = Parser::getBit(nalu, offset);
    pcm_enabled_flag = Parser::getBit(nalu, offset);
    if (pcm_enabled_flag) {
        pcm_sample_bit_depth_luma_minus1 = Parser::readBits(nalu, offset,4);
        pcm_sample_bit_depth_chroma_minus1 = Parser::readBits(nalu, offset,4);
        log2_min_pcm_luma_coding_block_size_minus3 = Parser::ExpGolomb::readUe(nalu, offset);
        log2_diff_max_min_pcm_luma_coding_block_size = Parser::ExpGolomb::readUe(nalu, offset);
        pcm_loop_filter_disabled_flag = Parser::getBit(nalu, offset);
    }
    num_short_term_ref_pic_sets = Parser::ExpGolomb::readUe(nalu, offset);
    for (uint32_t i=0; i<num_short_term_ref_pic_sets; i++) {
        //short_term_ref_pic_set( i )
        ParseShortTermRefPicSet(&stRPS[i], i, num_short_term_ref_pic_sets, stRPS, nalu, size, offset);
    }
    long_term_ref_pics_present_flag = Parser::getBit(nalu, offset);
    if (long_term_ref_pics_present_flag) {
        num_long_term_ref_pics_sps = Parser::ExpGolomb::readUe(nalu, offset);
        ltRPS.num_of_pics = num_long_term_ref_pics_sps;
        for (uint32_t i=0; i<num_long_term_ref_pics_sps; i++) {
            //The number of bits used to represent lt_ref_pic_poc_lsb_sps[ i ] is equal to log2_max_pic_order_cnt_lsb_minus4 + 4.
            lt_ref_pic_poc_lsb_sps[i] = Parser::readBits(nalu, offset,(log2_max_pic_order_cnt_lsb_minus4 + 4));
            used_by_curr_pic_lt_sps_flag[i] = Parser::getBit(nalu, offset);
            ltRPS.POCs[i]=lt_ref_pic_poc_lsb_sps[i];
            ltRPS.used_by_curr_pic[i] = used_by_curr_pic_lt_sps_flag[i];            
        }
    }
    sps_temporal_mvp_enabled_flag = Parser::getBit(nalu, offset);
    strong_intra_smoothing_enabled_flag = Parser::getBit(nalu, offset);
    vui_parameters_present_flag = Parser::getBit(nalu, offset);
    if (vui_parameters_present_flag) {
        //vui_parameters()
        ParseVUI(&vui_parameters, sps_max_sub_layers_minus1, nalu, size, offset);
    }
    sps_extension_flag = Parser::getBit(nalu, offset);
    if( sps_extension_flag ) {
        //while( more_rbsp_data( ) )
            //sps_extension_data_flag u(1)
    }
    return true;
}

bool HevcParser::PpsData::Parse(uint8_t *nalu, size_t size) {
    size_t offset = 16; // 2 bytes NALU header

    uint32_t PPS_ID = Parser::ExpGolomb::readUe(nalu, offset);
    
    pps_pic_parameter_set_id = PPS_ID;
    uint32_t activeSPS = Parser::ExpGolomb::readUe(nalu, offset);

    pps_seq_parameter_set_id = activeSPS;
    dependent_slice_segments_enabled_flag = Parser::getBit(nalu, offset);
    output_flag_present_flag = Parser::getBit(nalu, offset);
    num_extra_slice_header_bits = Parser::readBits(nalu, offset,3);
    sign_data_hiding_enabled_flag = Parser::getBit(nalu, offset);
    cabac_init_present_flag = Parser::getBit(nalu, offset);
    num_ref_idx_l0_default_active_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
    num_ref_idx_l1_default_active_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
    init_qp_minus26 = Parser::ExpGolomb::readSe(nalu, offset);
    constrained_intra_pred_flag = Parser::getBit(nalu, offset);
    transform_skip_enabled_flag = Parser::getBit(nalu, offset);
    cu_qp_delta_enabled_flag = Parser::getBit(nalu, offset);
    if (cu_qp_delta_enabled_flag) {
        diff_cu_qp_delta_depth = Parser::ExpGolomb::readUe(nalu, offset);
    }
    pps_cb_qp_offset = Parser::ExpGolomb::readSe(nalu, offset);
    pps_cr_qp_offset = Parser::ExpGolomb::readSe(nalu, offset);
    pps_slice_chroma_qp_offsets_present_flag = Parser::getBit(nalu, offset);
    weighted_pred_flag = Parser::getBit(nalu, offset);
    weighted_bipred_flag = Parser::getBit(nalu, offset);
    transquant_bypass_enabled_flag = Parser::getBit(nalu, offset);
    tiles_enabled_flag = Parser::getBit(nalu, offset);
    entropy_coding_sync_enabled_flag = Parser::getBit(nalu, offset);
    if (tiles_enabled_flag) {
        num_tile_columns_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
        num_tile_rows_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
        uniform_spacing_flag = Parser::getBit(nalu, offset);
        if (!uniform_spacing_flag) {
            for (uint32_t i=0; i<num_tile_columns_minus1; i++) {
                column_width_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
            }
            for (uint32_t i=0; i<num_tile_rows_minus1; i++) {
                row_height_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
            }
        }
        loop_filter_across_tiles_enabled_flag = Parser::getBit(nalu, offset);
    }
    else {
         loop_filter_across_tiles_enabled_flag = 1;
    }
    pps_loop_filter_across_slices_enabled_flag = Parser::getBit(nalu, offset);
    deblocking_filter_control_present_flag = Parser::getBit(nalu, offset);
    if (deblocking_filter_control_present_flag) {
        deblocking_filter_override_enabled_flag = Parser::getBit(nalu, offset);
        pps_deblocking_filter_disabled_flag = Parser::getBit(nalu, offset);
        if (!pps_deblocking_filter_disabled_flag) {
            pps_beta_offset_div2 = Parser::ExpGolomb::readSe(nalu, offset);
            pps_tc_offset_div2 = Parser::ExpGolomb::readSe(nalu, offset);
        }
    }
    pps_scaling_list_data_present_flag = Parser::getBit(nalu, offset);
    if (pps_scaling_list_data_present_flag) {
        SpsData::ParseScalingList(&scaling_list_data, nalu, size, offset);
    }
    lists_modification_present_flag = Parser::getBit(nalu, offset);
    log2_parallel_merge_level_minus2 = Parser::ExpGolomb::readUe(nalu, offset);
    slice_segment_header_extension_present_flag = Parser::getBit(nalu, offset);
    pps_extension_flag = Parser::getBit(nalu, offset);
    if (pps_extension_flag) {
        //while( more_rbsp_data( ) )
            //pps_extension_data_flag u(1)
        //rbsp_trailing_bits( )
    }
    return true;
}

void HevcParser::SpsData::ParsePTL(H265_profile_tier_level_t *ptl, bool profilePresentFlag, uint32_t maxNumSubLayersMinus1, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    if(profilePresentFlag) {
        ptl->general_profile_space = Parser::readBits(nalu, offset,2);
        ptl->general_tier_flag = Parser::getBit(nalu, offset);
        ptl->general_profile_idc = Parser::readBits(nalu, offset,5);
        for (int i=0; i < 32; i++) {
            ptl->general_profile_compatibility_flag[i] = Parser::getBit(nalu, offset);
        }
        ptl->general_progressive_source_flag = Parser::getBit(nalu, offset);
        ptl->general_interlaced_source_flag = Parser::getBit(nalu, offset);
        ptl->general_non_packed_constraint_flag = Parser::getBit(nalu, offset);
        ptl->general_frame_only_constraint_flag = Parser::getBit(nalu, offset);
        //readBits is limited to 32 
        //ptl->general_reserved_zero_44bits = Parser::readBits(nalu, offset,44);
        offset+=44;
    }

    ptl->general_level_idc = Parser::readBits(nalu, offset,8);
    for(uint32_t i=0; i < maxNumSubLayersMinus1; i++) {
        ptl->sub_layer_profile_present_flag[i] = Parser::getBit(nalu, offset);
        ptl->sub_layer_level_present_flag[i] = Parser::getBit(nalu, offset);
    }
    if (maxNumSubLayersMinus1 > 0) {
        for(uint32_t i=maxNumSubLayersMinus1; i<8; i++) {               
            ptl->reserved_zero_2bits[i] = Parser::readBits(nalu, offset,2);
        }
    }
    for(uint32_t i=0; i<maxNumSubLayersMinus1; i++) {
        if(ptl->sub_layer_profile_present_flag[i]) {
            ptl->sub_layer_profile_space[i] = Parser::readBits(nalu, offset,2);
            ptl->sub_layer_tier_flag[i] = Parser::getBit(nalu, offset);
            ptl->sub_layer_profile_idc[i] = Parser::readBits(nalu, offset,5);
            for(int j = 0; j<32; j++) {
                ptl->sub_layer_profile_compatibility_flag[i][j] = Parser::getBit(nalu, offset);
            }
            ptl->sub_layer_progressive_source_flag[i] = Parser::getBit(nalu, offset);
            ptl->sub_layer_interlaced_source_flag[i] = Parser::getBit(nalu, offset);
            ptl->sub_layer_non_packed_constraint_flag[i] = Parser::getBit(nalu, offset);
            ptl->sub_layer_frame_only_constraint_flag[i] = Parser::getBit(nalu, offset);
            ptl->sub_layer_reserved_zero_44bits[i] = Parser::readBits(nalu, offset,44);
        }
        if(ptl->sub_layer_level_present_flag[i]) {
            ptl->sub_layer_level_idc[i] = Parser::readBits(nalu, offset,8);
        }
    }
}

void HevcParser::SpsData::ParseSubLayerHrdParameters(H265_sub_layer_hrd_parameters *sub_hrd, uint32_t CpbCnt, bool sub_pic_hrd_params_present_flag, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    for (uint32_t i=0; i<=CpbCnt; i++) {
        sub_hrd->bit_rate_value_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
        sub_hrd->cpb_size_value_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
        if(sub_pic_hrd_params_present_flag) {
            sub_hrd->cpb_size_du_value_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
            sub_hrd->bit_rate_du_value_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
        }
        sub_hrd->cbr_flag[i] = Parser::getBit(nalu, offset);
    }
}

void HevcParser::SpsData::ParseHrdParameters(H265_hrd_parameters_t *hrd, bool commonInfPresentFlag, uint32_t maxNumSubLayersMinus1, uint8_t *nalu, size_t size,size_t &offset) {
    if (commonInfPresentFlag) {
        hrd->nal_hrd_parameters_present_flag = Parser::getBit(nalu, offset);
        hrd->vcl_hrd_parameters_present_flag = Parser::getBit(nalu, offset);
        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag) {
            hrd->sub_pic_hrd_params_present_flag = Parser::getBit(nalu, offset);
            if (hrd->sub_pic_hrd_params_present_flag) {
                hrd->tick_divisor_minus2 = Parser::readBits(nalu, offset,8);
                hrd->du_cpb_removal_delay_increment_length_minus1 = Parser::readBits(nalu, offset,5);
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = Parser::getBit(nalu, offset);
                hrd->dpb_output_delay_du_length_minus1 = Parser::readBits(nalu, offset,5);
            }
            hrd->bit_rate_scale = Parser::readBits(nalu, offset,4);
            hrd->cpb_size_scale = Parser::readBits(nalu, offset,4);
            if (hrd->sub_pic_hrd_params_present_flag) {
                hrd->cpb_size_du_scale = Parser::readBits(nalu, offset,4);
            }
            hrd->initial_cpb_removal_delay_length_minus1 = Parser::readBits(nalu, offset,5);
            hrd->au_cpb_removal_delay_length_minus1 = Parser::readBits(nalu, offset,5);
            hrd->dpb_output_delay_length_minus1 = Parser::readBits(nalu, offset,5);
        }
    }
    for (uint32_t i=0; i<= maxNumSubLayersMinus1; i++) {
        hrd->fixed_pic_rate_general_flag[i] = Parser::getBit(nalu, offset);
        if (!hrd->fixed_pic_rate_general_flag[i]) {
            hrd->fixed_pic_rate_within_cvs_flag[i] = Parser::getBit(nalu, offset);
        }
        else {
            hrd->fixed_pic_rate_within_cvs_flag[i] = hrd->fixed_pic_rate_general_flag[i];
        }

        if (hrd->fixed_pic_rate_within_cvs_flag[i]) {
            hrd->elemental_duration_in_tc_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
        }
        else {
            hrd->low_delay_hrd_flag[i] = Parser::getBit(nalu, offset);
        }
        if (!hrd->low_delay_hrd_flag[i]) {
            hrd->cpb_cnt_minus1[i] = Parser::ExpGolomb::readUe(nalu, offset);
        }
        if (hrd->nal_hrd_parameters_present_flag) {
            //sub_layer_hrd_parameters( i )
            ParseSubLayerHrdParameters(&hrd->sub_layer_hrd_parameters_0[i], hrd->cpb_cnt_minus1[i], hrd->sub_pic_hrd_params_present_flag, nalu, size, offset);
        }
        if (hrd->vcl_hrd_parameters_present_flag) {
            //sub_layer_hrd_parameters( i )
            ParseSubLayerHrdParameters(&hrd->sub_layer_hrd_parameters_1[i], hrd->cpb_cnt_minus1[i], hrd->sub_pic_hrd_params_present_flag, nalu, size, offset);
        }
    }
}

void HevcParser::SpsData::ParseScalingList(H265_scaling_list_data_t * s_data, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    for (int sizeId=0; sizeId < 4; sizeId++) {
        for (int matrixId=0; matrixId < ((sizeId == 3)? 2:6); matrixId++) {
            s_data->scaling_list_pred_mode_flag[sizeId][matrixId] = Parser::getBit(nalu, offset);
            if(!s_data->scaling_list_pred_mode_flag[sizeId][matrixId]) {
                s_data->scaling_list_pred_matrix_id_delta[sizeId][matrixId] = Parser::ExpGolomb::readUe(nalu, offset);

                int refMatrixId = matrixId - s_data->scaling_list_pred_matrix_id_delta[sizeId][matrixId];
                int coefNum = std::min(64, (1<< (4 + (sizeId<<1))));

                //fill in scaling_list_dc_coef_minus8
                if (!s_data->scaling_list_pred_matrix_id_delta[sizeId][matrixId]) {
                    if (sizeId>1)
                    {
                        s_data->scaling_list_dc_coef_minus8[sizeId-2][matrixId] = 8;
                    }
                }
                else {
                    if (sizeId>1) {
                        s_data->scaling_list_dc_coef_minus8[sizeId-2][matrixId] = s_data->scaling_list_dc_coef_minus8[sizeId-2][refMatrixId];
                    }
                }

                for (int i=0; i<coefNum; i++) {
                    if (s_data->scaling_list_pred_matrix_id_delta[sizeId][matrixId] == 0) {
                        if (sizeId == 0) {
                            s_data->ScalingList[sizeId][matrixId][i] = scaling_list_default_0[sizeId][matrixId][i];
                        }
                        else if(sizeId == 1 || sizeId == 2) {
                            s_data->ScalingList[sizeId][matrixId][i] = scaling_list_default_1_2[sizeId][matrixId][i];
                        }
                        else if(sizeId == 3) {
                            s_data->ScalingList[sizeId][matrixId][i] = scaling_list_default_3[sizeId][matrixId][i];
                        }
                    }
                    else {
                        s_data->ScalingList[sizeId][matrixId][i] = s_data->ScalingList[sizeId][refMatrixId][i];
                    }
                }
            }
            else {
                int nextCoef = 8;
                int coefNum = std::min(64, (1<< (4 + (sizeId<<1))));
                if (sizeId > 1) {
                    s_data->scaling_list_dc_coef_minus8[sizeId-2][matrixId] = Parser::ExpGolomb::readSe(nalu, offset);
                    nextCoef = s_data->scaling_list_dc_coef_minus8[sizeId-2][matrixId] + 8;
                }
                for (int i=0; i < coefNum; i++) {
                    s_data->scaling_list_delta_coef = Parser::ExpGolomb::readSe(nalu, offset);
                    nextCoef = (nextCoef + s_data->scaling_list_delta_coef +256)%256;
                    s_data->ScalingList[sizeId][matrixId][i] = nextCoef;
                }
            }
        }
    }
}

void HevcParser::SpsData::ParseShortTermRefPicSet(H265_short_term_RPS_t *rps, int32_t stRpsIdx, uint32_t number_short_term_ref_pic_sets, H265_short_term_RPS_t rps_ref[], uint8_t *nalu, size_t /*size*/, size_t& offset) {
    uint32_t interRPSPred = 0;
    uint32_t delta_idx_minus1 = 0;
    int32_t i=0;

    if (stRpsIdx != 0) {
        interRPSPred = Parser::getBit(nalu, offset);
    }
    if (interRPSPred) {
        uint32_t delta_rps_sign, abs_delta_rps_minus1;
        bool used_by_curr_pic_flag[16] = {0};
        bool use_delta_flag[16] = {0};
        if (unsigned(stRpsIdx) == number_short_term_ref_pic_sets) {
            delta_idx_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
        }
        delta_rps_sign = Parser::getBit(nalu, offset);
        abs_delta_rps_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
        int32_t delta_rps = (int32_t) (1 - 2*delta_rps_sign) * (abs_delta_rps_minus1 + 1);
        int32_t ref_idx = stRpsIdx - delta_idx_minus1 - 1;
        for (int j=0; j<= (rps_ref[ref_idx].num_negative_pics + rps_ref[ref_idx].num_positive_pics); j++) {
            used_by_curr_pic_flag[j] = Parser::getBit(nalu, offset);
            if (!used_by_curr_pic_flag[j]) {
                use_delta_flag[j] = Parser::getBit(nalu, offset);
            }
            else {
                use_delta_flag[j] = 1;
            }
        }

        for (int j=rps_ref[ref_idx].num_positive_pics - 1; j>= 0; j--) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].deltaPOC[rps_ref[ref_idx].num_negative_pics + j];  //positive deltaPOC from ref_rps
            if (delta_poc<0 && use_delta_flag[rps_ref[ref_idx].num_negative_pics + j]) {
                rps->deltaPOC[i] = delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_negative_pics + j];
            }
        }
        if (delta_rps < 0 && use_delta_flag[rps_ref[ref_idx].num_of_pics]) {
            rps->deltaPOC[i] = delta_rps;
            rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_of_pics];
        }
        for (int j=0; j<rps_ref[ref_idx].num_negative_pics; j++) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].deltaPOC[j];
            if (delta_poc < 0 && use_delta_flag[j]) {
                rps->deltaPOC[i]=delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[j];
            }
        }
        rps->num_negative_pics = i;
        
        
        for (int j=rps_ref[ref_idx].num_negative_pics - 1; j>= 0; j--) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].deltaPOC[j];  //positive deltaPOC from ref_rps
            if (delta_poc>0 && use_delta_flag[j]) {
                rps->deltaPOC[i] = delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[j];
            }
        }
        if (delta_rps > 0 && use_delta_flag[rps_ref[ref_idx].num_of_pics]) {
            rps->deltaPOC[i] = delta_rps;
            rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_of_pics];
        }
        for (int j=0; j<rps_ref[ref_idx].num_positive_pics; j++) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].deltaPOC[rps_ref[ref_idx].num_negative_pics+j];
            if (delta_poc > 0 && use_delta_flag[rps_ref[ref_idx].num_negative_pics+j]) {
                rps->deltaPOC[i]=delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_negative_pics+j];
            }
        }
        rps->num_positive_pics = i -rps->num_negative_pics ;
        rps->num_of_delta_poc = rps_ref[ref_idx].num_negative_pics + rps_ref[ref_idx].num_positive_pics;
        rps->num_of_pics = i;
    }
    else {
        rps->num_negative_pics = Parser::ExpGolomb::readUe(nalu, offset);
        rps->num_positive_pics = Parser::ExpGolomb::readUe(nalu, offset);
        int32_t prev = 0;
        int32_t poc;
        uint32_t delta_poc_s0_minus1,delta_poc_s1_minus1;
        for (int j=0; j < rps->num_negative_pics; j++) {
            delta_poc_s0_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
            poc = prev - delta_poc_s0_minus1 - 1;
            prev = poc;
            rps->deltaPOC[j] = poc;
            rps->used_by_curr_pic[j] = Parser::getBit(nalu, offset);
        }
        prev = 0;
        for (int j=rps->num_negative_pics; j < rps->num_negative_pics + rps->num_positive_pics; j++) {
            delta_poc_s1_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
            poc = prev + delta_poc_s1_minus1 + 1;
            prev = poc;
            rps->deltaPOC[j] = poc;
            rps->used_by_curr_pic[j] = Parser::getBit(nalu, offset);
        }
        rps->num_of_pics = rps->num_negative_pics + rps->num_positive_pics;
        rps->num_of_delta_poc = rps->num_negative_pics + rps->num_positive_pics;
    }
}

void HevcParser::SpsData::ParseVUI(H265_vui_parameters_t *vui, uint32_t maxNumSubLayersMinus1, uint8_t *nalu, size_t size, size_t &offset) {
    vui->aspect_ratio_info_present_flag = Parser::getBit(nalu, offset);
    if (vui->aspect_ratio_info_present_flag) {
        vui->aspect_ratio_idc = Parser::readBits(nalu, offset,8);
        if (vui->aspect_ratio_idc == 255) {
            vui->sar_width = Parser::readBits(nalu, offset,16);
            vui->sar_height = Parser::readBits(nalu, offset,16);
        }
    }
    vui->overscan_info_present_flag = Parser::getBit(nalu, offset);
    if (vui->overscan_info_present_flag) {
        vui->overscan_appropriate_flag = Parser::getBit(nalu, offset);
    }
    vui->video_signal_type_present_flag = Parser::getBit(nalu, offset);
    if(vui->video_signal_type_present_flag) {
        vui->video_format = Parser::readBits(nalu, offset,3);
        vui->video_full_range_flag = Parser::getBit(nalu, offset);
        vui->colour_description_present_flag = Parser::getBit(nalu, offset);
        if (vui->colour_description_present_flag) {
            vui->colour_primaries = Parser::readBits(nalu, offset,8);
            vui->transfer_characteristics = Parser::readBits(nalu, offset,8);
            vui->matrix_coeffs = Parser::readBits(nalu, offset,8);
        }
    }
    vui->chroma_loc_info_present_flag = Parser::getBit(nalu, offset);
    if (vui->chroma_loc_info_present_flag) {
        vui->chroma_sample_loc_type_top_field = Parser::ExpGolomb::readUe(nalu, offset);
        vui->chroma_sample_loc_type_bottom_field = Parser::ExpGolomb::readUe(nalu, offset);
    }
    vui->neutral_chroma_indication_flag = Parser::getBit(nalu, offset);
    vui->field_seq_flag = Parser::getBit(nalu, offset);
    vui->frame_field_info_present_flag = Parser::getBit(nalu, offset);
    vui->default_display_window_flag = Parser::getBit(nalu, offset);
    if (vui->default_display_window_flag) {
        vui->def_disp_win_left_offset = Parser::ExpGolomb::readUe(nalu, offset);
        vui->def_disp_win_right_offset = Parser::ExpGolomb::readUe(nalu, offset);
        vui->def_disp_win_top_offset = Parser::ExpGolomb::readUe(nalu, offset);
        vui->def_disp_win_bottom_offset = Parser::ExpGolomb::readUe(nalu, offset);
    }
    vui->vui_timing_info_present_flag = Parser::getBit(nalu, offset);
    if (vui->vui_timing_info_present_flag) {
        vui->vui_num_units_in_tick = Parser::readBits(nalu, offset,32);
        vui->vui_time_scale = Parser::readBits(nalu, offset,32);
        vui->vui_poc_proportional_to_timing_flag = Parser::getBit(nalu, offset);
        if (vui->vui_poc_proportional_to_timing_flag) {
            vui->vui_num_ticks_poc_diff_one_minus1 = Parser::ExpGolomb::readUe(nalu, offset);
        }
        vui->vui_hrd_parameters_present_flag = Parser::getBit(nalu, offset);
        if (vui->vui_hrd_parameters_present_flag) {
            ParseHrdParameters(&vui->hrd_parameters, 1, maxNumSubLayersMinus1, nalu, size, offset);
        }
    }
    vui->bitstream_restriction_flag = Parser::getBit(nalu, offset);
    if (vui->bitstream_restriction_flag) {
        vui->tiles_fixed_structure_flag = Parser::getBit(nalu, offset);
        vui->motion_vectors_over_pic_boundaries_flag = Parser::getBit(nalu, offset);
        vui->restricted_ref_pic_lists_flag = Parser::getBit(nalu, offset);
        vui->min_spatial_segmentation_idc = Parser::ExpGolomb::readUe(nalu, offset);
        vui->max_bytes_per_pic_denom = Parser::ExpGolomb::readUe(nalu, offset);
        vui->max_bits_per_min_cu_denom = Parser::ExpGolomb::readUe(nalu, offset);
        vui->log2_max_mv_length_horizontal = Parser::ExpGolomb::readUe(nalu, offset);
        vui->log2_max_mv_length_vertical = Parser::ExpGolomb::readUe(nalu, offset);
    }
}

bool HevcParser::AccessUnitSigns::Parse(uint8_t *nalu, size_t /*size*/, std::map<uint32_t, SpsData>&/*spsMap*/, std::map<uint32_t, PpsData>& /*ppsMap*/) {
    size_t offset = 16; // 2 bytes NALU header
    bNewPicture = Parser::getBit(nalu, offset);
    return true;
}

bool HevcParser::AccessUnitSigns::IsNewPicture() {
    return bNewPicture;
}

void HevcParser::ExtraDataBuilder::AddSPS(uint8_t *sps, size_t size) {
    m_SPSCount_++;
    size_t pos = m_SPSs_.GetSize();
    uint16_t spsSize = size & maxSpsSize;
    m_SPSs_.SetSize(pos + spsSize +2);
    uint8_t *data = m_SPSs_.GetData() + pos;
    *data++ = Parser::getLowByte(spsSize);
    *data++ = Parser::getHiByte(spsSize);
    memcpy(data , sps, (size_t)spsSize);
}

void HevcParser::ExtraDataBuilder::AddPPS(uint8_t *pps, size_t size) {
    m_PPSCount_++;
    size_t pos = m_PPSs_.GetSize();
    uint16_t ppsSize = size & maxPpsSize;
    m_PPSs_.SetSize(pos + ppsSize +2);
    uint8_t *data = m_PPSs_.GetData() + pos;
    *data++ = Parser::getLowByte(ppsSize);
    *data++ = Parser::getHiByte(ppsSize);
    memcpy(data , pps, (size_t)ppsSize);
}

bool HevcParser::ExtraDataBuilder::GetExtradata(ByteArray &extradata) {
    if( m_SPSs_.GetSize() == 0  || m_PPSs_ .GetSize() ==0 ) {
        return false;
    }
    if (m_SPSCount_ > 0x1F) {
        return false;
    }
    if (m_SPSs_.GetSize() < minSpsSize) {
        return false;
    }
    extradata.SetSize(
        21 +                // reserved
        1 +                 // length size
        1 +                 // array size
        3 +                 // SPS type + SPS count (2)
        m_SPSs_.GetSize() +
        3 +                 // PPS type + PPS count (2)
        m_PPSs_.GetSize()
        );

    uint8_t *data = extradata.GetData();
    
    memset(data, 0, extradata.GetSize());

    *data = 0x01; // configurationVersion
    data+=21;
    *data++ = (0xFC | (NalUnitLengthSize - 1));   // reserved(11111100) + lengthSizeMinusOne

    *data++ = static_cast<uint8_t>(2); // reserved(11100000) + numOfSequenceParameterSets


    *data++ = NAL_UNIT_SPS;
    *data++ = Parser::getLowByte(static_cast<int16_t>(m_SPSCount_));
    *data++ = Parser::getHiByte(static_cast<int16_t>(m_SPSCount_));

    memcpy(data, m_SPSs_.GetData(), m_SPSs_.GetSize());
    data += m_SPSs_.GetSize();


    *data++ = NAL_UNIT_PPS;
    *data++ = Parser::getLowByte(static_cast<int16_t>(m_PPSCount_));
    *data++ = Parser::getHiByte(static_cast<int16_t>(m_PPSCount_));
    memcpy(data, m_PPSs_.GetData(), m_PPSs_.GetSize());
    data += m_PPSs_.GetSize();
    return true;
}

#define ZEROBYTES_SHORTSTARTCODE 2 //indicates the number of zero bytes in the short start-code prefix

size_t HevcParser::EBSPtoRBSP(uint8_t *streamBuffer,size_t begin_bytepos, size_t end_bytepos) {
    int count = 0;
    if(end_bytepos < begin_bytepos) {
        return end_bytepos;
    }
    uint8_t *streamBuffer_i=streamBuffer+begin_bytepos;
    uint8_t *streamBuffer_end=streamBuffer+end_bytepos;
    int iReduceCount=0;
    for(; streamBuffer_i!=streamBuffer_end; ) { 
        //starting from begin_bytepos to avoid header information
        //in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any uint8_t-aligned position
        uint8_t tmp=*streamBuffer_i;
        if(count == ZEROBYTES_SHORTSTARTCODE) {
            if(tmp == 0x03) {
                //check the 4th uint8_t after 0x000003, except when cabac_zero_word is used, in which case the last three bytes of this NAL unit must be 0x000003
                if((streamBuffer_i+1 != streamBuffer_end) && (streamBuffer_i[1] > 0x03)) {
                    return static_cast<size_t>(-1);
                }
                //if cabac_zero_word is used, the final uint8_t of this NAL unit(0x03) is discarded, and the last two bytes of RBSP must be 0x0000
                if(streamBuffer_i+1 == streamBuffer_end) {
                    break;
                }
                memmove(streamBuffer_i,streamBuffer_i+1,streamBuffer_end-streamBuffer_i-1);
                streamBuffer_end--;
                iReduceCount++;
                count = 0;
                tmp = *streamBuffer_i;
            }
            else if(tmp < 0x03) {
            }
        }
        if(tmp == 0x00) {
            count++;
        }
        else {
            count = 0;
        }
        streamBuffer_i++;
    }
    return end_bytepos - begin_bytepos + iReduceCount;
}

//sizeId = 0
int scaling_list_default_0 [1][6][16] =  {{{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}}};
//sizeId = 1, 2
int scaling_list_default_1_2 [2][6][64] = {{{16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                            {16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                            {16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                            {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91},
                                            {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91},
                                            {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91}},
                                           {{16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                            {16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                            {16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                            {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91},
                                            {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91},
                                            {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91}}};
//sizeId = 3
int scaling_list_default_3 [1][2][64] = {{{16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                          {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91}}};
