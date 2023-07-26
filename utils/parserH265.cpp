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

#include "parserH265.h"

class HevcParser : public BitStreamParser {
public:
    HevcParser(amf::AMFDataStream* stream, amf::AMFContext* pContext);
    virtual ~HevcParser();

    virtual int                     GetOffsetX() const;
    virtual int                     GetOffsetY() const;
    virtual int                     GetPictureWidth() const;
    virtual int                     GetPictureHeight() const;
    virtual int                     GetAlignedWidth() const;
    virtual int                     GetAlignedHeight() const;

    virtual void                    SetMaxFramesNumber(size_t num) { m_maxFramesNumber = num; }

    virtual const unsigned char*    GetExtraData() const;
    virtual size_t                  GetExtraDataSize() const;
    virtual void                    SetUseStartCodes(bool bUse);
    virtual void                    SetFrameRate(double fps);
    virtual double                  GetFrameRate()  const;
    virtual rocDecStatus            ReInit();

    // TODO: Find equivalent input for AMF
    //virtual void                    GetFrameRate(AMFRate *frameRate) const;
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
        ExtraDataBuilder() : m_SPSCount(0), m_PPSCount(0) {}

        void AddSPS(uint8_t *sps, size_t size);
        void AddPPS(uint8_t *pps, size_t size);
        bool GetExtradata(AMFByteArray   &extradata);

    private:
        AMFByteArray    m_SPSs;
        AMFByteArray    m_PPSs;
        int32_t         m_SPSCount;
        int32_t         m_PPSCount;
    };

    friend struct AccessUnitSigns;

    static const uint32_t MacroblocSize = 16;
    static const uint8_t NalUnitTypeMask = 0x1F; // b00011111
    static const uint8_t NalRefIdcMask = 0x60;   // b01100000
    static const uint8_t NalUnitLengthSize = 4U;

    static const size_t m_ReadSize = 1024*4;

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
    AMFRect GetCropRect() const;

typedef     int64_t           pts;     // in 100 nanosecs

    AMFByteArray   m_ReadData;
    AMFByteArray   m_Extradata;
    
    AMFByteArray   m_EBSPtoRBSPData;

    bool           m_bUseStartCodes;
    pts        m_currentFrameTimestamp;
    amf::AMFDataStreamPtr m_pStream;
    std::map<uint32_t,SpsData> m_SpsMap;
    std::map<uint32_t,PpsData> m_PpsMap;
    size_t       m_PacketCount;
    bool            m_bEof;
    double          m_fps;
    size_t        m_maxFramesNumber;
    amf::AMFContext* m_pContext;
};

//-------------------------------------------------------------------------------------------------
BitStreamParser* CreateHEVCParser(uint8_t* pStream, int nSize, int64_t pts) {
    return new HevcParser(pStream, nSize, pts);
}

//-------------------------------------------------------------------------------------------------
HevcParser::HevcParser(pStream, nSize, pts) :
    m_bUseStartCodes(false),
    m_currentFrameTimestamp(0),
    m_pStream(stream),
    m_PacketCount(0),
    m_bEof(false),
    m_fps(0),
    m_maxFramesNumber(0),
    m_pContext(pContext)
{
    stream->Seek(amf::AMF_SEEK_BEGIN, 0, NULL);
    FindSPSandPPS();
}