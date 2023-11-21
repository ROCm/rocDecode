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

#include "hevc_parser.h"

template <typename T>
inline T *AllocStruct(const int max_cnt) {
    T *p = nullptr;
    try {
        p = (max_cnt == 1) ? new T : new T [max_cnt];
    }
    catch(const std::exception& e) {
        ERR(STR("Failed to alloc HEVC header struct Data, ") + STR(e.what()))
    }
    memset(p, 0, sizeof(T) * max_cnt);
    return p;
}

HEVCVideoParser::HEVCVideoParser() {
    pic_count_ = 0;
    first_pic_after_eos_nal_unit_ = 0;
    m_active_vps_id_ = -1; 
    m_active_sps_id_ = -1;
    m_active_pps_id_ = -1;
    b_new_picture_ = false;
    // allocate all fixed size structors here
    m_vps_ = AllocStruct<VpsData>(MAX_VPS_COUNT);
    m_sps_ = AllocStruct<SpsData>(MAX_SPS_COUNT);
    m_pps_ = AllocStruct<PpsData>(MAX_PPS_COUNT);
    m_sh_ = AllocStruct<SliceHeaderData>(1);
    m_sh_copy_ = AllocStruct<SliceHeaderData>(1);

    sei_rbsp_buf_ = NULL;
    sei_rbsp_buf_size_ = 0;
    sei_payload_buf_ = NULL;
    sei_payload_buf_size_ = 0;
    sei_message_list_.assign(INIT_SEI_MESSAGE_COUNT, {0});

    memset(&curr_pic_info_, 0, sizeof(HevcPicInfo));
    memset(&dpb_buffer_, 0, sizeof(DecodedPictureBuffer));
    for (int i = 0; i < HVC_MAX_DPB_FRAMES; i++) {
        dpb_buffer_.frame_buffer_list[i].pic_idx = i;
    }
}

rocDecStatus HEVCVideoParser::Initialize(RocdecParserParams *p_params) {
    ParserResult status = Init();
    if (status)
        return ROCDEC_RUNTIME_ERROR;
    RocVideoParser::Initialize(p_params);
    return ROCDEC_SUCCESS;
}

/**
 * @brief function to uninitialize hevc parser
 * 
 * @return rocDecStatus 
 */
rocDecStatus HEVCVideoParser::UnInitialize() {
    //todo:: do any uninitialization here
    return ROCDEC_SUCCESS;
}


rocDecStatus HEVCVideoParser::ParseVideoData(RocdecSourceDataPacket *p_data) {
    bool status = ParseFrameData(p_data->payload, p_data->payload_size);
    if (!status) {
        ERR(STR("Parser failed!"));
        return ROCDEC_RUNTIME_ERROR;
    }

    // Init Roc decoder for the first time or reconfigure the existing decoder
    if (new_sps_activated_) {
        FillSeqCallbackFn(&m_sps_[m_active_sps_id_]);
        new_sps_activated_ = false;
    }

    // Whenever new sei message found
    if (pfn_get_sei_message_cb_ && sei_message_count_ > 0) {
        FillSeiMessageCallbackFn();
    }

    // Decode the picture
    if (SendPicForDecode() != PARSER_OK) {
        ERR(STR("Failed to decode!"));
        return ROCDEC_RUNTIME_ERROR;
    }

    pic_count_++;

    return ROCDEC_SUCCESS;
}

HEVCVideoParser::~HEVCVideoParser() {
    if (m_vps_) {
        delete [] m_vps_;
    }
    if (m_sps_) {
        delete [] m_sps_;
    }
    if (m_pps_) {
        delete [] m_pps_;
    }
    if (m_sh_) {
        delete m_sh_;
    }
    if (m_sh_copy_) {
        delete m_sh_copy_;
    }
    if (sei_rbsp_buf_) {
        free(sei_rbsp_buf_);
    }
    if (sei_payload_buf_) {
        free(sei_payload_buf_);
    }
}

ParserResult HEVCVideoParser::Init() {
    b_new_picture_ = false;
    return PARSER_OK;
}

void HEVCVideoParser::FillSeqCallbackFn(SpsData* sps_data) {
    video_format_params_.codec = rocDecVideoCodec_HEVC;
    video_format_params_.frame_rate.numerator = 0;
    video_format_params_.frame_rate.denominator = 0;
    video_format_params_.bit_depth_luma_minus8 = sps_data->bit_depth_luma_minus8;
    video_format_params_.bit_depth_chroma_minus8 = sps_data->bit_depth_chroma_minus8;
    if (sps_data->profile_tier_level.general_progressive_source_flag && !sps_data->profile_tier_level.general_interlaced_source_flag)
        video_format_params_.progressive_sequence = 1;
    else if (!sps_data->profile_tier_level.general_progressive_source_flag && sps_data->profile_tier_level.general_interlaced_source_flag)
        video_format_params_.progressive_sequence = 0;
    else // default value
        video_format_params_.progressive_sequence = 1;
    video_format_params_.min_num_decode_surfaces = sps_data->sps_max_dec_pic_buffering_minus1[sps_data->sps_max_sub_layers_minus1] + 1;
    video_format_params_.coded_width = sps_data->pic_width_in_luma_samples;
    video_format_params_.coded_height = sps_data->pic_height_in_luma_samples;
    video_format_params_.chroma_format = static_cast<rocDecVideoChromaFormat>(sps_data->chroma_format_idc);
    int sub_width_c, sub_height_c;
    switch (video_format_params_.chroma_format) {
        case rocDecVideoChromaFormat_Monochrome: {
            sub_width_c = 1;
            sub_height_c = 1;
            break;
        }
        case rocDecVideoChromaFormat_420: {
            sub_width_c = 2;
            sub_height_c = 2;
            break;
        }
        case rocDecVideoChromaFormat_422: {
            sub_width_c = 2;
            sub_height_c = 1;
            break;
        }
        case rocDecVideoChromaFormat_444: {
            sub_width_c = 1;
            sub_height_c = 1;
            break;
        }
        default:
            ERR(STR("Error: Sequence Callback function - Chroma Format is not supported"));
            return;  
    }
    if(sps_data->conformance_window_flag) {
        video_format_params_.display_area.left = sub_width_c * sps_data->conf_win_left_offset;
        video_format_params_.display_area.top = sub_height_c * sps_data->conf_win_top_offset;
        video_format_params_.display_area.right = sps_data->pic_width_in_luma_samples - (sub_width_c * sps_data->conf_win_right_offset);
        video_format_params_.display_area.bottom = sps_data->pic_height_in_luma_samples - (sub_height_c * sps_data->conf_win_bottom_offset);
    } 
    else { // default values
        video_format_params_.display_area.left = 0;
        video_format_params_.display_area.top = 0;
        video_format_params_.display_area.right = video_format_params_.coded_width;
        video_format_params_.display_area.bottom = video_format_params_.coded_height;
    }
    
    video_format_params_.bitrate = 0;
    if (sps_data->vui_parameters_present_flag) {
        if (sps_data->vui_parameters.aspect_ratio_info_present_flag) {
            video_format_params_.display_aspect_ratio.x = sps_data->vui_parameters.sar_width;
            video_format_params_.display_aspect_ratio.y = sps_data->vui_parameters.sar_height;
        }
        else { // default values
            video_format_params_.display_aspect_ratio.x = 0;
            video_format_params_.display_aspect_ratio.y = 0;
        }
    }
    if (sps_data->vui_parameters_present_flag) {
        video_format_params_.video_signal_description.video_format = sps_data->vui_parameters.video_format;
        video_format_params_.video_signal_description.video_full_range_flag = sps_data->vui_parameters.video_full_range_flag;
        video_format_params_.video_signal_description.color_primaries = sps_data->vui_parameters.colour_primaries;
        video_format_params_.video_signal_description.transfer_characteristics = sps_data->vui_parameters.transfer_characteristics;
        video_format_params_.video_signal_description.matrix_coefficients = sps_data->vui_parameters.matrix_coeffs;
        video_format_params_.video_signal_description.reserved_zero_bits = 0;
    }
    video_format_params_.seqhdr_data_length = 0;

    // callback function with RocdecVideoFormat params filled out
    pfn_sequece_cb_(parser_params_.pUserData, &video_format_params_);
}

void HEVCVideoParser::FillSeiMessageCallbackFn() {
    sei_message_info_params_.sei_message_count = sei_message_count_;
    sei_message_info_params_.pSEIMessage = sei_message_list_.data();
    sei_message_info_params_.pSEIData = (void*)sei_payload_buf_;
    sei_message_info_params_.picIdx = curr_pic_info_.pic_idx;

    // callback function with RocdecSeiMessageInfo params filled out
    if (pfn_get_sei_message_cb_) pfn_get_sei_message_cb_(parser_params_.pUserData, &sei_message_info_params_);
}

int HEVCVideoParser::SendPicForDecode() {
    int i, j, ref_idx, buf_idx;
    SpsData *sps_ptr = &m_sps_[m_active_sps_id_];
    PpsData *pps_ptr = &m_pps_[m_active_pps_id_];
    dec_pic_params_ = {0};

    dec_pic_params_.PicWidth = sps_ptr->pic_width_in_luma_samples;
    dec_pic_params_.PicHeight = sps_ptr->pic_height_in_luma_samples;
    dec_pic_params_.CurrPicIdx = curr_pic_info_.pic_idx;
    dec_pic_params_.field_pic_flag = sps_ptr->profile_tier_level.general_interlaced_source_flag;
    dec_pic_params_.bottom_field_flag = 0; // For now. Need to parse VUI/SEI pic_timing()
    dec_pic_params_.second_field = 0; // For now. Need to parse VUI/SEI pic_timing()

    dec_pic_params_.nBitstreamDataLen = pic_stream_data_size_;
    dec_pic_params_.pBitstreamData = pic_stream_data_ptr_;
    dec_pic_params_.nNumSlices = slice_num_;
    dec_pic_params_.pSliceDataOffsets = nullptr; // Todo: do we need this? Remove if not.

    dec_pic_params_.ref_pic_flag = 1;  // HEVC decoded picture is always marked as short term at first.
    dec_pic_params_.intra_pic_flag = m_sh_->slice_type == HEVC_SLICE_TYPE_I ? 1 : 0;

    // Todo: field_pic_flag, bottom_field_flag, second_field, ref_pic_flag, and intra_pic_flag seems to be associated with AVC/H.264.
    // Do we need them for general purpose? Reomve if not.

    // Fill picture parameters
    RocdecHevcPicParams *pic_param_ptr = &dec_pic_params_.pic_params.hevc;

    // Current picture
    pic_param_ptr->cur_pic.PicIdx = curr_pic_info_.pic_idx;
    pic_param_ptr->cur_pic.POC = curr_pic_info_.pic_order_cnt;

    // Reference pictures
    ref_idx = 0;
    for (i = 0; i < num_poc_st_curr_before_; i++) {
        buf_idx = ref_pic_set_st_curr_before_[i];  // buffer index in DPB
        pic_param_ptr->ref_frames[ref_idx].PicIdx = dpb_buffer_.frame_buffer_list[buf_idx].pic_idx;
        pic_param_ptr->ref_frames[ref_idx].POC = dpb_buffer_.frame_buffer_list[buf_idx].pic_order_cnt;
        pic_param_ptr->ref_frames[ref_idx].Flags = 0; // assume frame picture for now
        pic_param_ptr->ref_frames[ref_idx].Flags |= RocdecHEVCPicture_RPS_ST_CURR_BEFORE;
        ref_idx++;
    }

    for (i = 0; i < num_poc_st_curr_after_; i++) {
        buf_idx = ref_pic_set_st_curr_after_[i]; // buffer index in DPB
        pic_param_ptr->ref_frames[ref_idx].PicIdx = dpb_buffer_.frame_buffer_list[buf_idx].pic_idx;
        pic_param_ptr->ref_frames[ref_idx].POC = dpb_buffer_.frame_buffer_list[buf_idx].pic_order_cnt;
        pic_param_ptr->ref_frames[ref_idx].Flags = 0; // assume frame picture for now
        pic_param_ptr->ref_frames[ref_idx].Flags |= RocdecHEVCPicture_RPS_ST_CURR_AFTER;
        ref_idx++;
    }

    for (i = 0; i < num_poc_lt_curr_; i++) {
        buf_idx = ref_pic_set_lt_curr_[i]; // buffer index in DPB
        pic_param_ptr->ref_frames[ref_idx].PicIdx = dpb_buffer_.frame_buffer_list[buf_idx].pic_idx;
        pic_param_ptr->ref_frames[ref_idx].POC = dpb_buffer_.frame_buffer_list[buf_idx].pic_order_cnt;
        pic_param_ptr->ref_frames[ref_idx].Flags = 0; // assume frame picture for now
        pic_param_ptr->ref_frames[ref_idx].Flags |= RocdecHEVCPicture_LONG_TERM_REFERENCE | RocdecHEVCPicture_RPS_LT_CURR;
        ref_idx++;
    }

    for (i = ref_idx; i < 15; i++) {
        pic_param_ptr->ref_frames[i].PicIdx = 0xFF;
    }

    pic_param_ptr->picture_width_in_luma_samples = sps_ptr->pic_width_in_luma_samples;
    pic_param_ptr->picture_height_in_luma_samples = sps_ptr->pic_height_in_luma_samples;

    pic_param_ptr->pic_fields.bits.chroma_format_idc = sps_ptr->chroma_format_idc;
    pic_param_ptr->pic_fields.bits.separate_colour_plane_flag = sps_ptr->separate_colour_plane_flag;
    pic_param_ptr->pic_fields.bits.pcm_enabled_flag = sps_ptr->pcm_enabled_flag;
    pic_param_ptr->pic_fields.bits.scaling_list_enabled_flag = sps_ptr->scaling_list_enabled_flag;
    pic_param_ptr->pic_fields.bits.transform_skip_enabled_flag = pps_ptr->transform_skip_enabled_flag;
    pic_param_ptr->pic_fields.bits.amp_enabled_flag = sps_ptr->amp_enabled_flag;
    pic_param_ptr->pic_fields.bits.strong_intra_smoothing_enabled_flag = sps_ptr->strong_intra_smoothing_enabled_flag;
    pic_param_ptr->pic_fields.bits.sign_data_hiding_enabled_flag = pps_ptr->sign_data_hiding_enabled_flag;
    pic_param_ptr->pic_fields.bits.constrained_intra_pred_flag = pps_ptr->constrained_intra_pred_flag;
    pic_param_ptr->pic_fields.bits.cu_qp_delta_enabled_flag = pps_ptr->cu_qp_delta_enabled_flag;
    pic_param_ptr->pic_fields.bits.weighted_pred_flag = pps_ptr->weighted_pred_flag;
    pic_param_ptr->pic_fields.bits.weighted_bipred_flag = pps_ptr->weighted_bipred_flag;
    pic_param_ptr->pic_fields.bits.transquant_bypass_enabled_flag = pps_ptr->transquant_bypass_enabled_flag;
    pic_param_ptr->pic_fields.bits.tiles_enabled_flag = pps_ptr->tiles_enabled_flag;
    pic_param_ptr->pic_fields.bits.entropy_coding_sync_enabled_flag = pps_ptr->entropy_coding_sync_enabled_flag;
    pic_param_ptr->pic_fields.bits.pps_loop_filter_across_slices_enabled_flag = pps_ptr->pps_loop_filter_across_slices_enabled_flag;
    pic_param_ptr->pic_fields.bits.loop_filter_across_tiles_enabled_flag = pps_ptr->loop_filter_across_tiles_enabled_flag;
    pic_param_ptr->pic_fields.bits.pcm_loop_filter_disabled_flag = sps_ptr->pcm_loop_filter_disabled_flag;
    pic_param_ptr->pic_fields.bits.NoPicReorderingFlag = sps_ptr->sps_max_num_reorder_pics[0] ? 0 : 1;
    pic_param_ptr->pic_fields.bits.NoBiPredFlag = m_sh_->slice_type == HEVC_SLICE_TYPE_B ? 0 : 1;

    pic_param_ptr->sps_max_dec_pic_buffering_minus1 = sps_ptr->sps_max_dec_pic_buffering_minus1[sps_ptr->sps_max_sub_layers_minus1];  // HighestTid
    pic_param_ptr->bit_depth_luma_minus8 = sps_ptr->bit_depth_luma_minus8;
    pic_param_ptr->bit_depth_chroma_minus8 = sps_ptr->bit_depth_chroma_minus8;
    pic_param_ptr->pcm_sample_bit_depth_luma_minus1 = sps_ptr->pcm_sample_bit_depth_luma_minus1;
    pic_param_ptr->pcm_sample_bit_depth_chroma_minus1 = sps_ptr->pcm_sample_bit_depth_chroma_minus1;
    pic_param_ptr->log2_min_luma_coding_block_size_minus3 = sps_ptr->log2_min_luma_coding_block_size_minus3;
    pic_param_ptr->log2_diff_max_min_luma_coding_block_size = sps_ptr->log2_diff_max_min_luma_coding_block_size;
    pic_param_ptr->log2_min_transform_block_size_minus2 = sps_ptr->log2_min_transform_block_size_minus2;
    pic_param_ptr->log2_diff_max_min_transform_block_size = sps_ptr->log2_diff_max_min_transform_block_size;
    pic_param_ptr->log2_min_pcm_luma_coding_block_size_minus3 = sps_ptr->log2_min_pcm_luma_coding_block_size_minus3;
    pic_param_ptr->log2_diff_max_min_pcm_luma_coding_block_size = sps_ptr->log2_diff_max_min_pcm_luma_coding_block_size;
    pic_param_ptr->max_transform_hierarchy_depth_intra = sps_ptr->max_transform_hierarchy_depth_intra;
    pic_param_ptr->max_transform_hierarchy_depth_inter = sps_ptr->max_transform_hierarchy_depth_inter;
    pic_param_ptr->init_qp_minus26 = pps_ptr->init_qp_minus26;
    pic_param_ptr->diff_cu_qp_delta_depth = pps_ptr->diff_cu_qp_delta_depth;
    pic_param_ptr->pps_cb_qp_offset = pps_ptr->pps_cb_qp_offset;
    pic_param_ptr->pps_cr_qp_offset = pps_ptr->pps_cr_qp_offset;
    pic_param_ptr->log2_parallel_merge_level_minus2 = pps_ptr->log2_parallel_merge_level_minus2;

    if (pps_ptr->tiles_enabled_flag) {
        pic_param_ptr->num_tile_columns_minus1 = pps_ptr->num_tile_columns_minus1;
        pic_param_ptr->num_tile_rows_minus1 = pps_ptr->num_tile_rows_minus1;
        for (i = 0; i <= pps_ptr->num_tile_columns_minus1; i++) {
            pic_param_ptr->column_width_minus1[i] = pps_ptr->column_width_minus1[i];
        }
        for (i = 0; i <= pps_ptr->num_tile_rows_minus1; i++) {
            pic_param_ptr->row_height_minus1[i] = pps_ptr->row_height_minus1[i];
        }
    }

    pic_param_ptr->slice_parsing_fields.bits.lists_modification_present_flag = pps_ptr->lists_modification_present_flag;
    pic_param_ptr->slice_parsing_fields.bits.long_term_ref_pics_present_flag = sps_ptr->long_term_ref_pics_present_flag;
    pic_param_ptr->slice_parsing_fields.bits.sps_temporal_mvp_enabled_flag = sps_ptr->sps_temporal_mvp_enabled_flag;
    pic_param_ptr->slice_parsing_fields.bits.cabac_init_present_flag = pps_ptr->cabac_init_present_flag;
    pic_param_ptr->slice_parsing_fields.bits.output_flag_present_flag = pps_ptr->output_flag_present_flag;
    pic_param_ptr->slice_parsing_fields.bits.dependent_slice_segments_enabled_flag = pps_ptr->dependent_slice_segments_enabled_flag;
    pic_param_ptr->slice_parsing_fields.bits.pps_slice_chroma_qp_offsets_present_flag = pps_ptr->pps_slice_chroma_qp_offsets_present_flag;
    pic_param_ptr->slice_parsing_fields.bits.sample_adaptive_offset_enabled_flag = sps_ptr->sample_adaptive_offset_enabled_flag;
    pic_param_ptr->slice_parsing_fields.bits.deblocking_filter_override_enabled_flag = pps_ptr->deblocking_filter_override_enabled_flag;
    pic_param_ptr->slice_parsing_fields.bits.pps_disable_deblocking_filter_flag = pps_ptr->pps_deblocking_filter_disabled_flag;
    pic_param_ptr->slice_parsing_fields.bits.slice_segment_header_extension_present_flag = pps_ptr->slice_segment_header_extension_present_flag;
    pic_param_ptr->slice_parsing_fields.bits.RapPicFlag = IsIrapPic(&slice_nal_unit_header_) ? 1 : 0;
    pic_param_ptr->slice_parsing_fields.bits.IdrPicFlag = IsIdrPic(&slice_nal_unit_header_) ? 1 : 0;
    pic_param_ptr->slice_parsing_fields.bits.IntraPicFlag = m_sh_->slice_type == HEVC_SLICE_TYPE_I ? 1 : 0;

    pic_param_ptr->log2_max_pic_order_cnt_lsb_minus4 = sps_ptr->log2_max_pic_order_cnt_lsb_minus4;
    pic_param_ptr->num_short_term_ref_pic_sets = sps_ptr->num_short_term_ref_pic_sets;
    pic_param_ptr->num_long_term_ref_pic_sps = sps_ptr->num_long_term_ref_pics_sps;
    pic_param_ptr->num_ref_idx_l0_default_active_minus1 = pps_ptr->num_ref_idx_l0_default_active_minus1;
    pic_param_ptr->num_ref_idx_l1_default_active_minus1 = pps_ptr->num_ref_idx_l1_default_active_minus1;
    pic_param_ptr->pps_beta_offset_div2 = pps_ptr->pps_beta_offset_div2;
    pic_param_ptr->pps_tc_offset_div2 = pps_ptr->pps_tc_offset_div2;
    pic_param_ptr->num_extra_slice_header_bits = pps_ptr->num_extra_slice_header_bits;

    pic_param_ptr->st_rps_bits = m_sh_->short_term_ref_pic_set_size;

    /// Fill slice parameters
    RocdecHevcSliceParams *slice_params_ptr = &dec_pic_params_.slice_params.hevc;

    // We put all slices into one slice data buffer.
    slice_params_ptr->slice_data_size = pic_stream_data_size_;
    slice_params_ptr->slice_data_offset = 0; // point to the start code
    slice_params_ptr->slice_data_flag = 0x00; // VA_SLICE_DATA_FLAG_ALL;
    slice_params_ptr->slice_data_byte_offset = 0;  // VCN consumes from the start code
    slice_params_ptr->slice_segment_address = m_sh_->slice_segment_address;

    // Ref lists
    memset(slice_params_ptr->RefPicList, 0xFF, sizeof(slice_params_ptr->RefPicList));
    if (m_sh_->slice_type != HEVC_SLICE_TYPE_I) {
        for (i = 0; i <= m_sh_->num_ref_idx_l0_active_minus1; i++) {
            int idx = ref_pic_list_0_[i]; // pic_idx of the ref pic
            for (j = 0; j < 15; j++) {
                if (pic_param_ptr->ref_frames[j].PicIdx == idx) {
                    break;
                }
            }
            if (j == 15) {
                ERR("Could not find matching pic in ref_frames list. The slice type is P/B, and the idx from the ref_pic_list_0_ is: " + TOSTR(idx));
                return PARSER_FAIL;
            }
            else {
                slice_params_ptr->RefPicList[0][i] = j;
            }
        }

        if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
            for (i = 0; i <= m_sh_->num_ref_idx_l1_active_minus1; i++) {
                int idx = ref_pic_list_1_[i]; // pic_idx of the ref pic
                for (j = 0; j < 15; j++) {
                    if (pic_param_ptr->ref_frames[j].PicIdx == idx) {
                        break;
                    }
                }
                if (j == 15) {
                    ERR("Could not find matching pic in ref_frames list. The slice type is B, and the idx from the ref_pic_list_1_ is: " + TOSTR(idx));
                    return PARSER_FAIL;
                }
                else {
                    slice_params_ptr->RefPicList[1][i] = j;
                }
            }
        }
    }

    slice_params_ptr->LongSliceFlags.fields.LastSliceOfPic = 1;
    slice_params_ptr->LongSliceFlags.fields.dependent_slice_segment_flag = m_sh_->dependent_slice_segment_flag;
    slice_params_ptr->LongSliceFlags.fields.slice_type = m_sh_->slice_type;
    slice_params_ptr->LongSliceFlags.fields.color_plane_id = m_sh_->colour_plane_id;
    slice_params_ptr->LongSliceFlags.fields.slice_sao_luma_flag = m_sh_->slice_sao_luma_flag;
    slice_params_ptr->LongSliceFlags.fields.slice_sao_chroma_flag = m_sh_->slice_sao_chroma_flag;
    slice_params_ptr->LongSliceFlags.fields.mvd_l1_zero_flag = m_sh_->mvd_l1_zero_flag;
    slice_params_ptr->LongSliceFlags.fields.cabac_init_flag = m_sh_->cabac_init_flag;
    slice_params_ptr->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag = m_sh_->slice_temporal_mvp_enabled_flag;
    slice_params_ptr->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag = m_sh_->slice_deblocking_filter_disabled_flag;
    slice_params_ptr->LongSliceFlags.fields.collocated_from_l0_flag = m_sh_->collocated_from_l0_flag;
    slice_params_ptr->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag = m_sh_->slice_loop_filter_across_slices_enabled_flag;

    slice_params_ptr->collocated_ref_idx = m_sh_->collocated_ref_idx;
    slice_params_ptr->num_ref_idx_l0_active_minus1 = m_sh_->num_ref_idx_l0_active_minus1;
    slice_params_ptr->num_ref_idx_l1_active_minus1 = m_sh_->num_ref_idx_l1_active_minus1;
    slice_params_ptr->slice_qp_delta = m_sh_->slice_qp_delta;
    slice_params_ptr->slice_cb_qp_offset = m_sh_->slice_cb_qp_offset;
    slice_params_ptr->slice_cr_qp_offset = m_sh_->slice_cr_qp_offset;
    slice_params_ptr->slice_beta_offset_div2 = m_sh_->slice_beta_offset_div2;
    slice_params_ptr->slice_tc_offset_div2 = m_sh_->slice_tc_offset_div2;

    if ((pps_ptr->weighted_pred_flag && m_sh_->slice_type == HEVC_SLICE_TYPE_P) || (pps_ptr->weighted_bipred_flag && m_sh_->slice_type == HEVC_SLICE_TYPE_B)) {
        slice_params_ptr->luma_log2_weight_denom = m_sh_->pred_weight_table.luma_log2_weight_denom;
        slice_params_ptr->delta_chroma_log2_weight_denom = m_sh_->pred_weight_table.delta_chroma_log2_weight_denom;
        for (i = 0; i < m_sh_->num_ref_idx_l0_active_minus1; i++) {
            slice_params_ptr->delta_luma_weight_l0[i] = m_sh_->pred_weight_table.delta_luma_weight_l0[i];
            slice_params_ptr->luma_offset_l0[i] = m_sh_->pred_weight_table.luma_offset_l0[i];
            slice_params_ptr->delta_chroma_weight_l0[i][0] = m_sh_->pred_weight_table.delta_chroma_weight_l0[i][0];
            slice_params_ptr->delta_chroma_weight_l0[i][1] = m_sh_->pred_weight_table.delta_chroma_weight_l0[i][1];
            slice_params_ptr->ChromaOffsetL0[i][0] = m_sh_->pred_weight_table.chroma_offset_l0[i][0];
            slice_params_ptr->ChromaOffsetL0[i][1] = m_sh_->pred_weight_table.chroma_offset_l0[i][1];
        }

        if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
            for (i = 0; i < m_sh_->num_ref_idx_l1_active_minus1; i++) {
                slice_params_ptr->delta_luma_weight_l1[i] = m_sh_->pred_weight_table.delta_luma_weight_l1[i];
                slice_params_ptr->luma_offset_l1[i] = m_sh_->pred_weight_table.luma_offset_l1[i];
                slice_params_ptr->delta_chroma_weight_l1[i][0] = m_sh_->pred_weight_table.delta_chroma_weight_l1[i][0];
                slice_params_ptr->delta_chroma_weight_l1[i][1] = m_sh_->pred_weight_table.delta_chroma_weight_l1[i][1];
                slice_params_ptr->ChromaOffsetL1[i][0] = m_sh_->pred_weight_table.chroma_offset_l1[i][0];
                slice_params_ptr->ChromaOffsetL1[i][1] = m_sh_->pred_weight_table.chroma_offset_l1[i][1];
            }
        }
    }

    slice_params_ptr->five_minus_max_num_merge_cand = m_sh_->five_minus_max_num_merge_cand;
    slice_params_ptr->num_entry_point_offsets = m_sh_->num_entry_point_offsets;
    slice_params_ptr->entry_offset_to_subset_array = 0; // don't care
    slice_params_ptr->slice_data_num_emu_prevn_bytes = 0; // don't care

    /// Fill scaling lists
    if (sps_ptr->scaling_list_enabled_flag) {
        RocdecHevcIQMatrix *iq_matrix_ptr = &dec_pic_params_.iq_matrix.hevc;
        H265ScalingListData *scaling_list_data_ptr = &pps_ptr->scaling_list_data;
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 16; j++) {
                    iq_matrix_ptr->ScalingList4x4[i][j] = scaling_list_data_ptr->scaling_list[0][i][j];
            }

            for (j = 0; j < 64; j++) {
                iq_matrix_ptr->ScalingList8x8[i][j] = scaling_list_data_ptr->scaling_list[1][i][j];
                iq_matrix_ptr->ScalingList16x16[i][j] = scaling_list_data_ptr->scaling_list[2][i][j];
                if (i < 2) {
                    iq_matrix_ptr->ScalingList32x32[i][j] = scaling_list_data_ptr->scaling_list[3][i * 3][j];
                }
            }

            iq_matrix_ptr->ScalingListDC16x16[i] = scaling_list_data_ptr->scaling_list_dc_coef[0][i];
            if (i < 2) {
                iq_matrix_ptr->ScalingListDC32x32[i] = scaling_list_data_ptr->scaling_list_dc_coef[1][i * 3];
            }
        }
    }

    if (pfn_decode_picture_cb_(parser_params_.pUserData, &dec_pic_params_) == 0) {
        ERR("Decode error occurred.");
        return PARSER_FAIL;
    }
    else {
        return PARSER_OK;
    }
}

bool HEVCVideoParser::ParseFrameData(const uint8_t* p_stream, uint32_t frame_data_size) {
    int ret = PARSER_OK;

    frame_data_buffer_ptr_ = (uint8_t*)p_stream;
    frame_data_size_ = frame_data_size;
    curr_byte_offset_ = 0;
    start_code_num_ = 0;
    curr_start_code_offset_ = 0;
    next_start_code_offset_ = 0;

    slice_num_ = 0;
    sei_message_count_ = 0;
    sei_payload_size_ = 0;

    do {
        ret = GetNalUnit();

        if (ret == PARSER_NOT_FOUND) {
            ERR(STR("Error: no start code found in the frame data."));
            return false;
        }

        // Parse the NAL unit
        if (nal_unit_size_) {
            // start code + NAL unit header = 5 bytes
            int ebsp_size = nal_unit_size_ - 5 > RBSP_BUF_SIZE ? RBSP_BUF_SIZE : nal_unit_size_ - 5; // only copy enough bytes for header parsing

            nal_unit_header_ = ParseNalUnitHeader(&frame_data_buffer_ptr_[curr_start_code_offset_ + 3]);
            switch (nal_unit_header_.nal_unit_type) {
                case NAL_UNIT_VPS: {
                    memcpy(m_rbsp_buf_, (frame_data_buffer_ptr_ + curr_start_code_offset_ + 5), ebsp_size);
                    m_rbsp_size_ = EBSPtoRBSP(m_rbsp_buf_, 0, ebsp_size);
                    ParseVps(m_rbsp_buf_, m_rbsp_size_);
                    break;
                }

                case NAL_UNIT_SPS: {
                    memcpy(m_rbsp_buf_, (frame_data_buffer_ptr_ + curr_start_code_offset_ + 5), ebsp_size);
                    m_rbsp_size_ = EBSPtoRBSP(m_rbsp_buf_, 0, ebsp_size);
                    ParseSps(m_rbsp_buf_, m_rbsp_size_);
                    break;
                }

                case NAL_UNIT_PPS: {
                    memcpy(m_rbsp_buf_, (frame_data_buffer_ptr_ + curr_start_code_offset_ + 5), ebsp_size);
                    m_rbsp_size_ = EBSPtoRBSP(m_rbsp_buf_, 0, ebsp_size);
                    ParsePps(m_rbsp_buf_, m_rbsp_size_);
                    break;
                }
                
                case NAL_UNIT_CODED_SLICE_TRAIL_R:
                case NAL_UNIT_CODED_SLICE_TRAIL_N:
                case NAL_UNIT_CODED_SLICE_TLA_R:
                case NAL_UNIT_CODED_SLICE_TSA_N:
                case NAL_UNIT_CODED_SLICE_STSA_R:
                case NAL_UNIT_CODED_SLICE_STSA_N:
                case NAL_UNIT_CODED_SLICE_BLA_W_LP:
                case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
                case NAL_UNIT_CODED_SLICE_BLA_N_LP:
                case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
                case NAL_UNIT_CODED_SLICE_IDR_N_LP:
                case NAL_UNIT_CODED_SLICE_CRA_NUT:
                case NAL_UNIT_CODED_SLICE_RADL_N:
                case NAL_UNIT_CODED_SLICE_RADL_R:
                case NAL_UNIT_CODED_SLICE_RASL_N:
                case NAL_UNIT_CODED_SLICE_RASL_R: {
                    memcpy(m_rbsp_buf_, (frame_data_buffer_ptr_ + curr_start_code_offset_ + 5), ebsp_size);
                    m_rbsp_size_ = EBSPtoRBSP(m_rbsp_buf_, 0, ebsp_size);
                    // For each picture, only parse the first slice header
                    if (slice_num_ == 0) {
                        // Save slice NAL unit header
                        slice_nal_unit_header_ = nal_unit_header_;

                        // Use the data directly from demuxer without copying
                        pic_stream_data_ptr_ = frame_data_buffer_ptr_ + curr_start_code_offset_;
                        // Picture stream data size is calculated as the diff between the frame end and the first slice offset.
                        // This is to consider the possibility of non-slice NAL units between slices.
                        pic_stream_data_size_ = frame_data_size - curr_start_code_offset_;

                        ParseSliceHeader(m_rbsp_buf_, m_rbsp_size_);

                        // Start decode process
                        if (IsIrapPic(&slice_nal_unit_header_)) {
                            if (IsIdrPic(&slice_nal_unit_header_) || IsBlaPic(&slice_nal_unit_header_) || pic_count_ == 0 || first_pic_after_eos_nal_unit_) {
                                no_rasl_output_flag_ = 1;
                            }
                            else {
                                no_rasl_output_flag_ = 0;
                            }
                        }

                        if (first_pic_after_eos_nal_unit_) {
                            first_pic_after_eos_nal_unit_ = 0;  // clear the flag
                        }

                        // Get POC
                        CalculateCurrPOC();

                        // Decode RPS
                        DeocdeRps();

                        // Construct ref lists
                        if(m_sh_->slice_type != HEVC_SLICE_TYPE_I) {
                            ConstructRefPicLists();
                        }

                        // Find a free buffer in DPM and mark as used
                        FindFreeBufAndMark();
                    }
                    slice_num_++;
                    break;
                }
                
                case NAL_UNIT_PREFIX_SEI:
                case NAL_UNIT_SUFFIX_SEI: {
                    if (pfn_get_sei_message_cb_) {
                        int sei_ebsp_size = nal_unit_size_ - 5; // copy the entire NAL unit
                        if (sei_rbsp_buf_) {
                            if (sei_ebsp_size > sei_rbsp_buf_size_) {
                                free(sei_rbsp_buf_);
                                sei_rbsp_buf_ = (uint8_t*)malloc(sei_ebsp_size);
                                sei_rbsp_buf_size_ = sei_ebsp_size;
                            }
                        }
                        else {
                            sei_rbsp_buf_size_ = sei_ebsp_size > INIT_SEI_PAYLOAD_BUF_SIZE ? sei_ebsp_size : INIT_SEI_PAYLOAD_BUF_SIZE;
                            sei_rbsp_buf_ = (uint8_t*)malloc(sei_rbsp_buf_size_);
                        }
                        memcpy(sei_rbsp_buf_, (frame_data_buffer_ptr_ + curr_start_code_offset_ + 5), sei_ebsp_size);
                        m_rbsp_size_ = EBSPtoRBSP(sei_rbsp_buf_, 0, sei_ebsp_size);
                        ParseSeiMessage(sei_rbsp_buf_, m_rbsp_size_);
                    }
                    break;
                }

                case NAL_UNIT_EOS: {
                    first_pic_after_eos_nal_unit_ = 1;
                    break;
                }

                case NAL_UNIT_EOB: {
                    pic_count_ = 0;
                    break;
                }

                default:
                    break;
            }
        }

        // Break if this is the last NAL unit
        if (ret == PARSER_EOF) {
            break;
        }
    } while (1);

    return true;
}

int HEVCVideoParser::GetNalUnit() {
    bool start_code_found = false;

    nal_unit_size_ = 0;
    curr_start_code_offset_ = next_start_code_offset_;  // save the current start code offset

    // Search for the next start code
    while (curr_byte_offset_ < frame_data_size_ - 2) {
        if (frame_data_buffer_ptr_[curr_byte_offset_] == 0 && frame_data_buffer_ptr_[curr_byte_offset_ + 1] == 0 && frame_data_buffer_ptr_[curr_byte_offset_ + 2] == 0x01) {
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
            }
            else {
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
    }
    else {
        nal_unit_size_ = frame_data_size_ - curr_start_code_offset_;
        return PARSER_EOF;
    }        
}

void HEVCVideoParser::ParsePtl(H265ProfileTierLevel *ptl, bool profile_present_flag, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t size, size_t& offset) {
    if (profile_present_flag) {
        ptl->general_profile_space = Parser::ReadBits(nalu, offset, 2);
        ptl->general_tier_flag = Parser::GetBit(nalu, offset);
        ptl->general_profile_idc = Parser::ReadBits(nalu, offset, 5);
        for (int i = 0; i < 32; i++) {
            ptl->general_profile_compatibility_flag[i] = Parser::GetBit(nalu, offset);
        }
        ptl->general_progressive_source_flag = Parser::GetBit(nalu, offset);
        ptl->general_interlaced_source_flag = Parser::GetBit(nalu, offset);
        ptl->general_non_packed_constraint_flag = Parser::GetBit(nalu, offset);
        ptl->general_frame_only_constraint_flag = Parser::GetBit(nalu, offset);
        // ReadBits is limited to 32
        offset += 44; // skip 44 bits
        // Todo: add constrant flags parsing for higher profiles when needed
    }

    ptl->general_level_idc = Parser::ReadBits(nalu, offset, 8);
    for(uint32_t i = 0; i < max_num_sub_layers_minus1; i++) {
        ptl->sub_layer_profile_present_flag[i] = Parser::GetBit(nalu, offset);
        ptl->sub_layer_level_present_flag[i] = Parser::GetBit(nalu, offset);
    }
    if (max_num_sub_layers_minus1 > 0) {
        for(uint32_t i = max_num_sub_layers_minus1; i < 8; i++) {               
            ptl->reserved_zero_2bits[i] = Parser::ReadBits(nalu, offset, 2);
        }
    }
    for (uint32_t i = 0; i < max_num_sub_layers_minus1; i++) {
        if (ptl->sub_layer_profile_present_flag[i]) {
            ptl->sub_layer_profile_space[i] = Parser::ReadBits(nalu, offset, 2);
            ptl->sub_layer_tier_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_profile_idc[i] = Parser::ReadBits(nalu, offset, 5);
            for (int j = 0; j < 32; j++) {
                ptl->sub_layer_profile_compatibility_flag[i][j] = Parser::GetBit(nalu, offset);
            }
            ptl->sub_layer_progressive_source_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_interlaced_source_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_non_packed_constraint_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_frame_only_constraint_flag[i] = Parser::GetBit(nalu, offset);
            // ReadBits is limited to 32
            offset += 44;  // skip 44 bits
            // Todo: add constrant flags parsing for higher profiles when needed
        }
        if (ptl->sub_layer_level_present_flag[i]) {
            ptl->sub_layer_level_idc[i] = Parser::ReadBits(nalu, offset, 8);
        }
    }
}

void HEVCVideoParser::ParseSubLayerHrdParameters(H265SubLayerHrdParameters *sub_hrd, uint32_t cpb_cnt, bool sub_pic_hrd_params_present_flag, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    for (uint32_t i = 0; i <= cpb_cnt; i++) {
        sub_hrd->bit_rate_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        sub_hrd->cpb_size_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        if(sub_pic_hrd_params_present_flag) {
            sub_hrd->cpb_size_du_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            sub_hrd->bit_rate_du_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        sub_hrd->cbr_flag[i] = Parser::GetBit(nalu, offset);
    }
}

void HEVCVideoParser::ParseHrdParameters(H265HrdParameters *hrd, bool common_inf_present_flag, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t size,size_t &offset) {
    if (common_inf_present_flag) {
        hrd->nal_hrd_parameters_present_flag = Parser::GetBit(nalu, offset);
        hrd->vcl_hrd_parameters_present_flag = Parser::GetBit(nalu, offset);
        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag) {
            hrd->sub_pic_hrd_params_present_flag = Parser::GetBit(nalu, offset);
            if (hrd->sub_pic_hrd_params_present_flag) {
                hrd->tick_divisor_minus2 = Parser::ReadBits(nalu, offset, 8);
                hrd->du_cpb_removal_delay_increment_length_minus1 = Parser::ReadBits(nalu, offset, 5);
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = Parser::GetBit(nalu, offset);
                hrd->dpb_output_delay_du_length_minus1 = Parser::ReadBits(nalu, offset, 5);
            }
            hrd->bit_rate_scale = Parser::ReadBits(nalu, offset, 4);
            hrd->cpb_size_scale = Parser::ReadBits(nalu, offset, 4);
            if (hrd->sub_pic_hrd_params_present_flag) {
                hrd->cpb_size_du_scale = Parser::ReadBits(nalu, offset, 4);
            }
            hrd->initial_cpb_removal_delay_length_minus1 = Parser::ReadBits(nalu, offset, 5);
            hrd->au_cpb_removal_delay_length_minus1 = Parser::ReadBits(nalu, offset, 5);
            hrd->dpb_output_delay_length_minus1 = Parser::ReadBits(nalu, offset, 5);
        }
    }
    for (uint32_t i = 0; i <= max_num_sub_layers_minus1; i++) {
        hrd->fixed_pic_rate_general_flag[i] = Parser::GetBit(nalu, offset);
        if (!hrd->fixed_pic_rate_general_flag[i]) {
            hrd->fixed_pic_rate_within_cvs_flag[i] = Parser::GetBit(nalu, offset);
        }
        else {
            hrd->fixed_pic_rate_within_cvs_flag[i] = hrd->fixed_pic_rate_general_flag[i];
        }

        if (hrd->fixed_pic_rate_within_cvs_flag[i]) {
            hrd->elemental_duration_in_tc_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        else {
            hrd->low_delay_hrd_flag[i] = Parser::GetBit(nalu, offset);
        }
        if (!hrd->low_delay_hrd_flag[i]) {
            hrd->cpb_cnt_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
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

// Table 7-5. Default values of ScalingList[ 0 ][ matrixId ][ i ] with i = 0..15.
static const uint8_t default_scaling_list_side_id_0[] = {
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16
};

// Table 7-6. Default values of ScalingList[ 1..3 ][ 0..2 ][ i ] with i = 0..63.
static const uint8_t default_scaling_list_intra[] = {
    16, 16, 16, 16, 17, 18, 21, 24,
    16, 16, 16, 16, 17, 19, 22, 25,
    16, 16, 17, 18, 20, 22, 25, 29,
    16, 16, 18, 21, 24, 27, 31, 36,
    17, 17, 20, 24, 30, 35, 41, 47,
    18, 19, 22, 27, 35, 44, 54, 65,
    21, 22, 25, 31, 41, 54, 70, 88,
    24, 25, 29, 36, 47, 65, 88, 115
};

// Table 7-6. Default values of ScalingList[ 1..3 ][ 3..5 ][ i ] with i = 0..63.
static const uint8_t default_scaling_list_inter[] = {
    16, 16, 16, 16, 17, 18, 20, 24,
    16, 16, 16, 17, 18, 20, 24, 25,
    16, 16, 17, 18, 20, 24, 25, 28,
    16, 17, 18, 20, 24, 25, 28, 33,
    17, 18, 20, 24, 25, 28, 33, 41,
    18, 20, 24, 25, 28, 33, 41, 54,
    20, 24, 25, 28, 33, 41, 54, 71,
    24, 25, 28, 33, 41, 54, 71, 91
};

static const int diag_scan_4x4[16] = {
    0, 4, 1, 8,
    5, 2,12, 9,
    6, 3,13,10,
    7,14,11,15
};

static const int diag_scan_8x8[64] = {
    0, 8, 1, 16, 9, 2,24,17,
    10, 3,32,25,18,11, 4,40,
    33,26,19,12, 5,48,41,34,
    27,20,13, 6,56,49,42,35,
    28,21,14, 7,57,50,43,36,
    29,22,15,58,51,44,37,30,
    23,59,52,45,38,31,60,53,
    46,39,61,54,47,62,55,63
};

void HEVCVideoParser::SetDefaultScalingList(H265ScalingListData *sl_ptr) {
    int size_id, matrix_id, i;

    // DC coefficient for 16x16 and 32x32
    for (matrix_id = 0; matrix_id < 6; matrix_id++) {
        sl_ptr->scaling_list_dc_coef[0][matrix_id] = 16;
        sl_ptr->scaling_list_dc_coef[1][matrix_id] = 16;
    }

    // sizeId 0
    for (matrix_id = 0; matrix_id < 6; matrix_id++) {
        for (i = 0; i < 16; i++) {
            sl_ptr->scaling_list[0][matrix_id][i] = default_scaling_list_side_id_0[i];
        }
    }

    // sizeId 1..3, matrixId 0..2
    for (size_id = 1; size_id <= 3; size_id++) {
        for (matrix_id = 0; matrix_id <= 2; matrix_id++) {
            for (i = 0; i < 64; i++) {
            sl_ptr->scaling_list[size_id][matrix_id][i] = default_scaling_list_intra[i];
            }
        }
    }

    // sizeId 1..3, matrixId 3..5
    for (size_id = 1; size_id <= 3; size_id++) {
        for (matrix_id = 3; matrix_id <= 5; matrix_id++) {
            for (i = 0; i < 64; i++) {
            sl_ptr->scaling_list[size_id][matrix_id][i] = default_scaling_list_inter[i];
            }
        }
    }
}

void HEVCVideoParser::ParseScalingList(H265ScalingListData * sl_ptr, uint8_t *nalu, size_t size, size_t& offset, SpsData *sps_ptr) {
    for (int size_id = 0; size_id < 4; size_id++) {
        for (int matrix_id = 0; matrix_id < 6; matrix_id += (size_id == 3) ? 3 : 1) {
            sl_ptr->scaling_list_pred_mode_flag[size_id][matrix_id] = Parser::GetBit(nalu, offset);
            if(!sl_ptr->scaling_list_pred_mode_flag[size_id][matrix_id]) {
                sl_ptr->scaling_list_pred_matrix_id_delta[size_id][matrix_id] = Parser::ExpGolomb::ReadUe(nalu, offset);
                // If scaling_list_pred_matrix_id_delta is 0, infer from default scaling list. We have filled the scaling
                // list with default values earlier.
                if (sl_ptr->scaling_list_pred_matrix_id_delta[size_id][matrix_id]) {
                    // Infer from the reference scaling list
                    int ref_matrix_id = matrix_id - sl_ptr->scaling_list_pred_matrix_id_delta[size_id][matrix_id] * (size_id == 3 ? 3 : 1);
                    int coef_num = std::min(64, (1 << (4 + (size_id << 1))));
                    for (int i = 0; i < coef_num; i++) {
                        sl_ptr->scaling_list[size_id][matrix_id][i] = sl_ptr->scaling_list[size_id][ref_matrix_id][i];
                    }

                    // Copy to DC coefficient for 16x16 or 32x32
                    if (size_id > 1) {
                        sl_ptr->scaling_list_dc_coef[size_id - 2][matrix_id] = sl_ptr->scaling_list_dc_coef[size_id - 2][ref_matrix_id];
                    }
                }                
            }
            else {
                int next_coef = 8;
                int coef_num = std::min(64, (1 << (4 + (size_id << 1))));
                if (size_id > 1) {
                    sl_ptr->scaling_list_dc_coef_minus8[size_id - 2][matrix_id] = Parser::ExpGolomb::ReadSe(nalu, offset);
                    next_coef = sl_ptr->scaling_list_dc_coef_minus8[size_id - 2][matrix_id] + 8;
                    // Record DC coefficient for 16x16 or 32x32
                    sl_ptr->scaling_list_dc_coef[size_id - 2][matrix_id] = next_coef;
                }
                for (int i = 0; i < coef_num; i++) {
                    sl_ptr->scaling_list_delta_coef = Parser::ExpGolomb::ReadSe(nalu, offset);
                    next_coef = (next_coef + sl_ptr->scaling_list_delta_coef + 256) % 256;
                    if (size_id == 0) {
                        sl_ptr->scaling_list[size_id][matrix_id][diag_scan_4x4[i]] = next_coef;
                    }
                    else {
                        sl_ptr->scaling_list[size_id][matrix_id][diag_scan_8x8[i]] = next_coef;
                    }
                }
            }
        }
    }

    if (sps_ptr->chroma_format_idc == 3) {
        for (int i = 0; i < 64; i++) {
            sl_ptr->scaling_list[3][1][i] = sl_ptr->scaling_list[2][1][i];
            sl_ptr->scaling_list[3][2][i] = sl_ptr->scaling_list[2][2][i];
            sl_ptr->scaling_list[3][4][i] = sl_ptr->scaling_list[2][4][i];
            sl_ptr->scaling_list[3][5][i] = sl_ptr->scaling_list[2][5][i];
        }
        sl_ptr->scaling_list_dc_coef[1][1] = sl_ptr->scaling_list_dc_coef[0][1];
        sl_ptr->scaling_list_dc_coef[1][2] = sl_ptr->scaling_list_dc_coef[0][2];
        sl_ptr->scaling_list_dc_coef[1][4] = sl_ptr->scaling_list_dc_coef[0][4];
        sl_ptr->scaling_list_dc_coef[1][5] = sl_ptr->scaling_list_dc_coef[0][5];
    }
}

void HEVCVideoParser::ParseShortTermRefPicSet(H265ShortTermRPS *rps, int32_t st_rps_idx, uint32_t number_short_term_ref_pic_sets, H265ShortTermRPS rps_ref[], uint8_t *nalu, size_t /*size*/, size_t& offset) {
    int32_t i = 0;

    if (st_rps_idx != 0) {
        rps->inter_ref_pic_set_prediction_flag = Parser::GetBit(nalu, offset);
    }
    else {
        rps->inter_ref_pic_set_prediction_flag = 0;
    }
    if (rps->inter_ref_pic_set_prediction_flag) {
        if (unsigned(st_rps_idx) == number_short_term_ref_pic_sets) {
            rps->delta_idx_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        else {
            rps->delta_idx_minus1 = 0;
        }
        rps->delta_rps_sign = Parser::GetBit(nalu, offset);
        rps->abs_delta_rps_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        int32_t delta_rps = (int32_t) (1 - 2 * rps->delta_rps_sign) * (rps->abs_delta_rps_minus1 + 1);
        int32_t ref_idx = st_rps_idx - rps->delta_idx_minus1 - 1;
        for (int j = 0; j <= (rps_ref[ref_idx].num_of_delta_poc); j++) {
            rps->used_by_curr_pic_flag[j] = Parser::GetBit(nalu, offset);
            if (!rps->used_by_curr_pic_flag[j]) {
                rps->use_delta_flag[j] = Parser::GetBit(nalu, offset);
            }
            else {
                rps->use_delta_flag[j] = 1;
            }
        }

        for (int j = rps_ref[ref_idx].num_positive_pics - 1; j >= 0; j--) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[rps_ref[ref_idx].num_negative_pics + j];  //positive delta_poc from ref_rps
            if (delta_poc < 0 && rps->use_delta_flag[rps_ref[ref_idx].num_negative_pics + j]) {
                rps->delta_poc[i] = delta_poc;
                rps->used_by_curr_pic[i++] = rps->used_by_curr_pic_flag[rps_ref[ref_idx].num_negative_pics + j];
            }
        }
        if (delta_rps < 0 && rps->use_delta_flag[rps_ref[ref_idx].num_of_pics]) {
            rps->delta_poc[i] = delta_rps;
            rps->used_by_curr_pic[i++] = rps->used_by_curr_pic_flag[rps_ref[ref_idx].num_of_pics];
        }
        for (int j = 0; j < rps_ref[ref_idx].num_negative_pics; j++) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[j];
            if (delta_poc < 0 && rps->use_delta_flag[j]) {
                rps->delta_poc[i]=delta_poc;
                rps->used_by_curr_pic[i++] = rps->used_by_curr_pic_flag[j];
            }
        }
        rps->num_negative_pics = i;
        
        for (int j = rps_ref[ref_idx].num_negative_pics - 1; j >= 0; j--) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[j];  //positive delta_poc from ref_rps
            if (delta_poc > 0 && rps->use_delta_flag[j]) {
                rps->delta_poc[i] = delta_poc;
                rps->used_by_curr_pic[i++] = rps->used_by_curr_pic_flag[j];
            }
        }
        if (delta_rps > 0 && rps->use_delta_flag[rps_ref[ref_idx].num_of_pics]) {
            rps->delta_poc[i] = delta_rps;
            rps->used_by_curr_pic[i++] = rps->used_by_curr_pic_flag[rps_ref[ref_idx].num_of_pics];
        }
        for (int j = 0; j < rps_ref[ref_idx].num_positive_pics; j++) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[rps_ref[ref_idx].num_negative_pics+j];
            if (delta_poc > 0 && rps->use_delta_flag[rps_ref[ref_idx].num_negative_pics+j]) {
                rps->delta_poc[i] = delta_poc;
                rps->used_by_curr_pic[i++] = rps->used_by_curr_pic_flag[rps_ref[ref_idx].num_negative_pics+j];
            }
        }
        rps->num_positive_pics = i - rps->num_negative_pics ;
        rps->num_of_delta_poc = rps_ref[ref_idx].num_negative_pics + rps_ref[ref_idx].num_positive_pics;
        rps->num_of_pics = i;
    }
    else {
        rps->num_negative_pics = Parser::ExpGolomb::ReadUe(nalu, offset);
        rps->num_positive_pics = Parser::ExpGolomb::ReadUe(nalu, offset);
        int32_t prev = 0;
        int32_t poc;
        // DeltaPocS0, UsedByCurrPicS0
        for (int j = 0; j < rps->num_negative_pics; j++) {
            rps->delta_poc_s0_minus1[j] = Parser::ExpGolomb::ReadUe(nalu, offset);
            poc = prev - rps->delta_poc_s0_minus1[j] - 1;
            prev = poc;
            rps->delta_poc[j] = poc;  // DeltaPocS0
            rps->used_by_curr_pic_s0_flag[j] = Parser::GetBit(nalu, offset);
            rps->used_by_curr_pic[j] = rps->used_by_curr_pic_s0_flag[j];  // UsedByCurrPicS0
        }
        prev = 0;
        // DeltaPocS1, UsedByCurrPicS1
        for (int j = 0; j < rps->num_positive_pics; j++) {
            rps->delta_poc_s1_minus1[j] = Parser::ExpGolomb::ReadUe(nalu, offset);
            poc = prev + rps->delta_poc_s1_minus1[j] + 1;
            prev = poc;
            rps->delta_poc[j + rps->num_negative_pics] = poc;  // DeltaPocS1
            rps->used_by_curr_pic_s1_flag[j] = Parser::GetBit(nalu, offset);
            rps->used_by_curr_pic[j + rps->num_negative_pics] = rps->used_by_curr_pic_s1_flag[j];  // UsedByCurrPicS1
        }
        rps->num_of_pics = rps->num_negative_pics + rps->num_positive_pics;
        rps->num_of_delta_poc = rps->num_negative_pics + rps->num_positive_pics;
    }
}

void HEVCVideoParser::ParsePredWeightTable(HEVCVideoParser::SliceHeaderData *slice_header_ptr, int chroma_array_type, uint8_t *stream_ptr, size_t &offset) {
    HevcPredWeightTable *pred_weight_table_ptr = &slice_header_ptr->pred_weight_table;
    int chroma_log2_weight_denom; // ChromaLog2WeightDenom
    int i, j;

    pred_weight_table_ptr->luma_log2_weight_denom = Parser::ExpGolomb::ReadUe(stream_ptr, offset);
    if (chroma_array_type) {
        pred_weight_table_ptr->delta_chroma_log2_weight_denom = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
    }
    chroma_log2_weight_denom = pred_weight_table_ptr->luma_log2_weight_denom + pred_weight_table_ptr->delta_chroma_log2_weight_denom;

    for (i = 0; i <= slice_header_ptr->num_ref_idx_l0_active_minus1; i++) {
        pred_weight_table_ptr->luma_weight_l0_flag[i] = Parser::GetBit(stream_ptr, offset);
    }
    if (chroma_array_type) {
        for (i = 0; i <= slice_header_ptr->num_ref_idx_l0_active_minus1; i++) {
            pred_weight_table_ptr->chroma_weight_l0_flag[i] = Parser::GetBit(stream_ptr, offset);
        }
    }
    for (i = 0; i <= slice_header_ptr->num_ref_idx_l0_active_minus1; i++) {
        if (pred_weight_table_ptr->luma_weight_l0_flag[i]) {
            pred_weight_table_ptr->delta_luma_weight_l0[i] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
            pred_weight_table_ptr->luma_offset_l0[i] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
        }
        if (pred_weight_table_ptr->chroma_weight_l0_flag[i]) {
            for (j = 0; j < 2; j++) {
                pred_weight_table_ptr->delta_chroma_weight_l0[i][j] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
                pred_weight_table_ptr->delta_chroma_offset_l0[i][j] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
                pred_weight_table_ptr->chroma_weight_l0[i][j] = (1 << chroma_log2_weight_denom) + pred_weight_table_ptr->delta_chroma_weight_l0[i][j];
                pred_weight_table_ptr->chroma_offset_l0[i][j] = std::clamp((pred_weight_table_ptr->delta_chroma_offset_l0[i][j] - ((128 * pred_weight_table_ptr->chroma_weight_l0[i][j]) >> chroma_log2_weight_denom) + 128), -128, 127);
            }
        }
        else {
            pred_weight_table_ptr->chroma_weight_l0[i][0] = 1 << chroma_log2_weight_denom;
            pred_weight_table_ptr->chroma_offset_l0[i][0] = 0;
            pred_weight_table_ptr->chroma_weight_l0[i][1] = 1 << chroma_log2_weight_denom;
            pred_weight_table_ptr->chroma_offset_l0[i][1] = 0;
        }
    }

    if (slice_header_ptr->slice_type == HEVC_SLICE_TYPE_B) {
        for (i = 0; i <= slice_header_ptr->num_ref_idx_l1_active_minus1; i++) {
            pred_weight_table_ptr->luma_weight_l1_flag[i] = Parser::GetBit(stream_ptr, offset);
        }
        if (chroma_array_type) {
            for (i = 0; i <= slice_header_ptr->num_ref_idx_l1_active_minus1; i++) {
                pred_weight_table_ptr->chroma_weight_l1_flag[i] = Parser::GetBit(stream_ptr, offset);
            }
        }
        for (i = 0; i <= slice_header_ptr->num_ref_idx_l1_active_minus1; i++) {
            if (pred_weight_table_ptr->luma_weight_l1_flag[i]) {
                pred_weight_table_ptr->delta_luma_weight_l1[i] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
                pred_weight_table_ptr->luma_offset_l1[i] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
            }
            if (pred_weight_table_ptr->chroma_weight_l1_flag[i]) {
                for (j = 0; j < 2; j++) {
                    pred_weight_table_ptr->delta_chroma_weight_l1[i][j] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
                    pred_weight_table_ptr->delta_chroma_offset_l1[i][j] = Parser::ExpGolomb::ReadSe(stream_ptr, offset);
                    pred_weight_table_ptr->chroma_weight_l1[i][j] = (1 << chroma_log2_weight_denom) + pred_weight_table_ptr->delta_chroma_weight_l1[i][j];
                    pred_weight_table_ptr->chroma_offset_l1[i][j] = std::clamp((pred_weight_table_ptr->delta_chroma_offset_l1[i][j] - ((128 * pred_weight_table_ptr->chroma_weight_l1[i][j]) >> chroma_log2_weight_denom) + 128), -128, 127);
                }
            }
            else {
                pred_weight_table_ptr->chroma_weight_l1[i][0] = 1 << chroma_log2_weight_denom;
                pred_weight_table_ptr->chroma_offset_l1[i][0] = 0;
                pred_weight_table_ptr->chroma_weight_l1[i][1] = 1 << chroma_log2_weight_denom;
                pred_weight_table_ptr->chroma_offset_l1[i][1] = 0;
            }
        }
    }
}

void HEVCVideoParser::ParseVui(H265VuiParameters *vui, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t size, size_t &offset) {
    vui->aspect_ratio_info_present_flag = Parser::GetBit(nalu, offset);
    if (vui->aspect_ratio_info_present_flag) {
        vui->aspect_ratio_idc = Parser::ReadBits(nalu, offset, 8);
        if (vui->aspect_ratio_idc == 255) {
            vui->sar_width = Parser::ReadBits(nalu, offset, 16);
            vui->sar_height = Parser::ReadBits(nalu, offset, 16);
        }
    }
    vui->overscan_info_present_flag = Parser::GetBit(nalu, offset);
    if (vui->overscan_info_present_flag) {
        vui->overscan_appropriate_flag = Parser::GetBit(nalu, offset);
    }
    vui->video_signal_type_present_flag = Parser::GetBit(nalu, offset);
    if (vui->video_signal_type_present_flag) {
        vui->video_format = Parser::ReadBits(nalu, offset, 3);
        vui->video_full_range_flag = Parser::GetBit(nalu, offset);
        vui->colour_description_present_flag = Parser::GetBit(nalu, offset);
        if (vui->colour_description_present_flag) {
            vui->colour_primaries = Parser::ReadBits(nalu, offset, 8);
            vui->transfer_characteristics = Parser::ReadBits(nalu, offset, 8);
            vui->matrix_coeffs = Parser::ReadBits(nalu, offset, 8);
        }
    }
    vui->chroma_loc_info_present_flag = Parser::GetBit(nalu, offset);
    if (vui->chroma_loc_info_present_flag) {
        vui->chroma_sample_loc_type_top_field = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->chroma_sample_loc_type_bottom_field = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    vui->neutral_chroma_indication_flag = Parser::GetBit(nalu, offset);
    vui->field_seq_flag = Parser::GetBit(nalu, offset);
    vui->frame_field_info_present_flag = Parser::GetBit(nalu, offset);
    vui->default_display_window_flag = Parser::GetBit(nalu, offset);
    if (vui->default_display_window_flag) {
        vui->def_disp_win_left_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->def_disp_win_right_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->def_disp_win_top_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->def_disp_win_bottom_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    vui->vui_timing_info_present_flag = Parser::GetBit(nalu, offset);
    if (vui->vui_timing_info_present_flag) {
        vui->vui_num_units_in_tick = Parser::ReadBits(nalu, offset, 32);
        vui->vui_time_scale = Parser::ReadBits(nalu, offset, 32);
        vui->vui_poc_proportional_to_timing_flag = Parser::GetBit(nalu, offset);
        if (vui->vui_poc_proportional_to_timing_flag) {
            vui->vui_num_ticks_poc_diff_one_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        vui->vui_hrd_parameters_present_flag = Parser::GetBit(nalu, offset);
        if (vui->vui_hrd_parameters_present_flag) {
            ParseHrdParameters(&vui->hrd_parameters, 1, max_num_sub_layers_minus1, nalu, size, offset);
        }
    }
    vui->bitstream_restriction_flag = Parser::GetBit(nalu, offset);
    if (vui->bitstream_restriction_flag) {
        vui->tiles_fixed_structure_flag = Parser::GetBit(nalu, offset);
        vui->motion_vectors_over_pic_boundaries_flag = Parser::GetBit(nalu, offset);
        vui->restricted_ref_pic_lists_flag = Parser::GetBit(nalu, offset);
        vui->min_spatial_segmentation_idc = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->max_bytes_per_pic_denom = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->max_bits_per_min_cu_denom = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->log2_max_mv_length_horizontal = Parser::ExpGolomb::ReadUe(nalu, offset);
        vui->log2_max_mv_length_vertical = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
}

void HEVCVideoParser::ParseVps(uint8_t *nalu, size_t size) {
    size_t offset = 0; // current bit offset
    uint32_t vps_id = Parser::ReadBits(nalu, offset, 4);
    memset(&m_vps_[vps_id], 0, sizeof(m_vps_[vps_id]));

    m_vps_[vps_id].vps_video_parameter_set_id = vps_id;
    m_vps_[vps_id].vps_base_layer_internal_flag = Parser::GetBit(nalu, offset);
    m_vps_[vps_id].vps_base_layer_available_flag = Parser::GetBit(nalu, offset);
    m_vps_[vps_id].vps_max_layers_minus1 = Parser::ReadBits(nalu, offset, 6);
    m_vps_[vps_id].vps_max_sub_layers_minus1 = Parser::ReadBits(nalu, offset, 3);
    m_vps_[vps_id].vps_temporal_id_nesting_flag = Parser::GetBit(nalu, offset);
    m_vps_[vps_id].vps_reserved_0xffff_16bits = Parser::ReadBits(nalu, offset, 16);
    ParsePtl(&m_vps_[vps_id].profile_tier_level, true, m_vps_[vps_id].vps_max_sub_layers_minus1, nalu, size, offset);
    m_vps_[vps_id].vps_sub_layer_ordering_info_present_flag = Parser::GetBit(nalu, offset);

    for (int i = 0; i <= m_vps_[vps_id].vps_max_sub_layers_minus1; i++) {
        if (m_vps_[vps_id].vps_sub_layer_ordering_info_present_flag || (i == 0)) {
            m_vps_[vps_id].vps_max_dec_pic_buffering_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            m_vps_[vps_id].vps_max_num_reorder_pics[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            m_vps_[vps_id].vps_max_latency_increase_plus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        else {
            m_vps_[vps_id].vps_max_dec_pic_buffering_minus1[i] = m_vps_[vps_id].vps_max_dec_pic_buffering_minus1[0];
            m_vps_[vps_id].vps_max_num_reorder_pics[i] = m_vps_[vps_id].vps_max_num_reorder_pics[0];
            m_vps_[vps_id].vps_max_latency_increase_plus1[i] = m_vps_[vps_id].vps_max_latency_increase_plus1[0];
        }
    }
    m_vps_[vps_id].vps_max_layer_id = Parser::ReadBits(nalu, offset, 6);
    m_vps_[vps_id].vps_num_layer_sets_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    for (int i = 1; i <= m_vps_[vps_id].vps_num_layer_sets_minus1; i++) {
        for (int j = 0; j <= m_vps_[vps_id].vps_max_layer_id; j++) {
            m_vps_[vps_id].layer_id_included_flag[i][j] = Parser::GetBit(nalu, offset);
        }
    }
    m_vps_[vps_id].vps_timing_info_present_flag = Parser::GetBit(nalu, offset);
    if(m_vps_[vps_id].vps_timing_info_present_flag) {
        m_vps_[vps_id].vps_num_units_in_tick = Parser::ReadBits(nalu, offset, 32);
        m_vps_[vps_id].vps_time_scale = Parser::ReadBits(nalu, offset, 32);
        m_vps_[vps_id].vps_poc_proportional_to_timing_flag = Parser::GetBit(nalu, offset);
        if(m_vps_[vps_id].vps_poc_proportional_to_timing_flag) {
            m_vps_[vps_id].vps_num_ticks_poc_diff_one_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        m_vps_[vps_id].vps_num_hrd_parameters = Parser::ExpGolomb::ReadUe(nalu, offset);
        for (int i = 0; i<m_vps_[vps_id].vps_num_hrd_parameters; i++) {
            m_vps_[vps_id].hrd_layer_set_idx[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            if (i > 0) {
                m_vps_[vps_id].cprms_present_flag[i] = Parser::GetBit(nalu, offset);
            }
            //parse HRD parameters
            ParseHrdParameters(&m_vps_[vps_id].hrd_parameters[i], m_vps_[vps_id].cprms_present_flag[i], m_vps_[vps_id].vps_max_sub_layers_minus1, nalu, size, offset);
        }
    }
    m_vps_[vps_id].vps_extension_flag = Parser::GetBit(nalu, offset);

#if DBGINFO
    PrintVps(&m_vps_[vps_id]);
#endif // DBGINFO
}

void HEVCVideoParser::ParseSps(uint8_t *nalu, size_t size) {
    SpsData *sps_ptr = nullptr;
    size_t offset = 0;

    uint32_t vps_id = Parser::ReadBits(nalu, offset, 4);
    uint32_t max_sub_layer_minus1 = Parser::ReadBits(nalu, offset, 3);
    uint32_t sps_temporal_id_nesting_flag = Parser::GetBit(nalu, offset);
    H265ProfileTierLevel ptl;
    memset (&ptl, 0, sizeof(ptl));
    ParsePtl(&ptl, true, max_sub_layer_minus1, nalu, size, offset);

    uint32_t sps_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr = &m_sps_[sps_id];

    memset(sps_ptr, 0, sizeof(SpsData));
    sps_ptr->sps_video_parameter_set_id = vps_id;
    sps_ptr->sps_max_sub_layers_minus1 = max_sub_layer_minus1;
    sps_ptr->sps_temporal_id_nesting_flag = sps_temporal_id_nesting_flag;
    memcpy (&sps_ptr->profile_tier_level, &ptl, sizeof(ptl));
    sps_ptr->sps_seq_parameter_set_id = sps_id;
    sps_ptr->chroma_format_idc = Parser::ExpGolomb::ReadUe(nalu, offset);
    if (sps_ptr->chroma_format_idc == 3) {
        sps_ptr->separate_colour_plane_flag = Parser::GetBit(nalu, offset);
    }
    sps_ptr->pic_width_in_luma_samples = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->pic_height_in_luma_samples = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->conformance_window_flag = Parser::GetBit(nalu, offset);
    if (sps_ptr->conformance_window_flag) {
        sps_ptr->conf_win_left_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_ptr->conf_win_right_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_ptr->conf_win_top_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_ptr->conf_win_bottom_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    sps_ptr->bit_depth_luma_minus8 = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->bit_depth_chroma_minus8 = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->log2_max_pic_order_cnt_lsb_minus4 = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->sps_sub_layer_ordering_info_present_flag = Parser::GetBit(nalu, offset);
    for (int i = 0; i <= sps_ptr->sps_max_sub_layers_minus1; i++) {
        if (sps_ptr->sps_sub_layer_ordering_info_present_flag || (i == 0)) {
            sps_ptr->sps_max_dec_pic_buffering_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            sps_ptr->sps_max_num_reorder_pics[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            sps_ptr->sps_max_latency_increase_plus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        else {
            sps_ptr->sps_max_dec_pic_buffering_minus1[i] = sps_ptr->sps_max_dec_pic_buffering_minus1[0];
            sps_ptr->sps_max_num_reorder_pics[i] = sps_ptr->sps_max_num_reorder_pics[0];
            sps_ptr->sps_max_latency_increase_plus1[i] = sps_ptr->sps_max_latency_increase_plus1[0];
        }
    }
    sps_ptr->log2_min_luma_coding_block_size_minus3 = Parser::ExpGolomb::ReadUe(nalu, offset);

    int log2_min_cu_size = sps_ptr->log2_min_luma_coding_block_size_minus3 + 3;

    sps_ptr->log2_diff_max_min_luma_coding_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);

    int max_cu_depth_delta = sps_ptr->log2_diff_max_min_luma_coding_block_size;
    sps_ptr->max_cu_width = ( 1<<(log2_min_cu_size + max_cu_depth_delta));
    sps_ptr->max_cu_height = ( 1<<(log2_min_cu_size + max_cu_depth_delta));

    sps_ptr->log2_min_transform_block_size_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);

    uint32_t quadtree_tu_log2_min_size = sps_ptr->log2_min_transform_block_size_minus2 + 2;
    int add_cu_depth = max (0, log2_min_cu_size - (int)quadtree_tu_log2_min_size);
    sps_ptr->max_cu_depth = (max_cu_depth_delta + add_cu_depth);

    sps_ptr->log2_diff_max_min_transform_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->max_transform_hierarchy_depth_inter = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_ptr->max_transform_hierarchy_depth_intra = Parser::ExpGolomb::ReadUe(nalu, offset);

    // Infer dimensional variables
    int min_cb_log2_size_y = sps_ptr->log2_min_luma_coding_block_size_minus3 + 3;  // MinCbLog2SizeY
    int ctb_log2_size_y = min_cb_log2_size_y + sps_ptr->log2_diff_max_min_luma_coding_block_size;  // CtbLog2SizeY
    int ctb_size_y = 1 << ctb_log2_size_y;  // CtbSizeY
    pic_width_in_ctbs_y_ = (sps_ptr->pic_width_in_luma_samples + ctb_size_y - 1) / ctb_size_y;  // PicWidthInCtbsY
    pic_height_in_ctbs_y_ = (sps_ptr->pic_height_in_luma_samples + ctb_size_y - 1) / ctb_size_y;  // PicHeightInCtbsY
    pic_size_in_ctbs_y_ = pic_width_in_ctbs_y_ * pic_height_in_ctbs_y_;  // PicSizeInCtbsY

    sps_ptr->scaling_list_enabled_flag = Parser::GetBit(nalu, offset);
    if (sps_ptr->scaling_list_enabled_flag) {
        // Set up default values first
        SetDefaultScalingList(&sps_ptr->scaling_list_data);

        sps_ptr->sps_scaling_list_data_present_flag = Parser::GetBit(nalu, offset);
        if (sps_ptr->sps_scaling_list_data_present_flag) {
            ParseScalingList(&sps_ptr->scaling_list_data, nalu, size, offset, sps_ptr);
        }
    }
    sps_ptr->amp_enabled_flag = Parser::GetBit(nalu, offset);
    sps_ptr->sample_adaptive_offset_enabled_flag = Parser::GetBit(nalu, offset);
    sps_ptr->pcm_enabled_flag = Parser::GetBit(nalu, offset);
    if (sps_ptr->pcm_enabled_flag) {
        sps_ptr->pcm_sample_bit_depth_luma_minus1 = Parser::ReadBits(nalu, offset, 4);
        sps_ptr->pcm_sample_bit_depth_chroma_minus1 = Parser::ReadBits(nalu, offset, 4);
        sps_ptr->log2_min_pcm_luma_coding_block_size_minus3 = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_ptr->log2_diff_max_min_pcm_luma_coding_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_ptr->pcm_loop_filter_disabled_flag = Parser::GetBit(nalu, offset);
    }
    sps_ptr->num_short_term_ref_pic_sets = Parser::ExpGolomb::ReadUe(nalu, offset);
    for (int i=0; i<sps_ptr->num_short_term_ref_pic_sets; i++) {
        //short_term_ref_pic_set( i )
        ParseShortTermRefPicSet(&sps_ptr->st_rps[i], i, sps_ptr->num_short_term_ref_pic_sets, sps_ptr->st_rps, nalu, size, offset);
    }
    sps_ptr->long_term_ref_pics_present_flag = Parser::GetBit(nalu, offset);
    if (sps_ptr->long_term_ref_pics_present_flag) {
        sps_ptr->num_long_term_ref_pics_sps = Parser::ExpGolomb::ReadUe(nalu, offset);  //max is 32
        sps_ptr->lt_rps.num_of_pics = sps_ptr->num_long_term_ref_pics_sps;
        for (int i=0; i<sps_ptr->num_long_term_ref_pics_sps; i++) {
            //The number of bits used to represent lt_ref_pic_poc_lsb_sps[ i ] is equal to log2_max_pic_order_cnt_lsb_minus4 + 4.
            sps_ptr->lt_ref_pic_poc_lsb_sps[i] = Parser::ReadBits(nalu, offset, (sps_ptr->log2_max_pic_order_cnt_lsb_minus4 + 4));
            sps_ptr->used_by_curr_pic_lt_sps_flag[i] = Parser::GetBit(nalu, offset);
            sps_ptr->lt_rps.pocs[i]=sps_ptr->lt_ref_pic_poc_lsb_sps[i];
            sps_ptr->lt_rps.used_by_curr_pic[i] = sps_ptr->used_by_curr_pic_lt_sps_flag[i];            
        }
    }
    sps_ptr->sps_temporal_mvp_enabled_flag = Parser::GetBit(nalu, offset);
    sps_ptr->strong_intra_smoothing_enabled_flag = Parser::GetBit(nalu, offset);
    sps_ptr->vui_parameters_present_flag = Parser::GetBit(nalu, offset);
    if (sps_ptr->vui_parameters_present_flag) {
        //vui_parameters()
        ParseVui(&sps_ptr->vui_parameters, sps_ptr->sps_max_sub_layers_minus1, nalu, size, offset);
    }
    sps_ptr->sps_extension_flag = Parser::GetBit(nalu, offset);

#if DBGINFO
    PrintSps(sps_ptr);
#endif // DBGINFO
}

void HEVCVideoParser::ParsePps(uint8_t *nalu, size_t size) {
    int i;
    size_t offset = 0;
    uint32_t pps_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    PpsData *pps_ptr = &m_pps_[pps_id];
    memset(pps_ptr, 0, sizeof(PpsData));

    pps_ptr->pps_pic_parameter_set_id = pps_id;
    pps_ptr->pps_seq_parameter_set_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    pps_ptr->dependent_slice_segments_enabled_flag = Parser::GetBit(nalu, offset);
    pps_ptr->output_flag_present_flag = Parser::GetBit(nalu, offset);
    pps_ptr->num_extra_slice_header_bits = Parser::ReadBits(nalu, offset, 3);
    pps_ptr->sign_data_hiding_enabled_flag = Parser::GetBit(nalu, offset);
    pps_ptr->cabac_init_present_flag = Parser::GetBit(nalu, offset);
    pps_ptr->num_ref_idx_l0_default_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    pps_ptr->num_ref_idx_l1_default_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    pps_ptr->init_qp_minus26 = Parser::ExpGolomb::ReadSe(nalu, offset);
    pps_ptr->constrained_intra_pred_flag = Parser::GetBit(nalu, offset);
    pps_ptr->transform_skip_enabled_flag = Parser::GetBit(nalu, offset);
    pps_ptr->cu_qp_delta_enabled_flag = Parser::GetBit(nalu, offset);
    if (pps_ptr->cu_qp_delta_enabled_flag) {
        pps_ptr->diff_cu_qp_delta_depth = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    pps_ptr->pps_cb_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
    pps_ptr->pps_cr_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
    pps_ptr->pps_slice_chroma_qp_offsets_present_flag = Parser::GetBit(nalu, offset);
    pps_ptr->weighted_pred_flag = Parser::GetBit(nalu, offset);
    pps_ptr->weighted_bipred_flag = Parser::GetBit(nalu, offset);
    pps_ptr->transquant_bypass_enabled_flag = Parser::GetBit(nalu, offset);
    pps_ptr->tiles_enabled_flag = Parser::GetBit(nalu, offset);
    pps_ptr->entropy_coding_sync_enabled_flag = Parser::GetBit(nalu, offset);
    if (pps_ptr->tiles_enabled_flag) {
        pps_ptr->num_tile_columns_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        pps_ptr->num_tile_rows_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        pps_ptr->uniform_spacing_flag = Parser::GetBit(nalu, offset);
        if (!pps_ptr->uniform_spacing_flag) {
            int temp_size = pic_width_in_ctbs_y_; // PicWidthInCtbsY
            for (i = 0; i < pps_ptr->num_tile_columns_minus1; i++) {
                pps_ptr->column_width_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
                temp_size -= pps_ptr->column_width_minus1[i] + 1;
            }
            pps_ptr->column_width_minus1[i] = temp_size - 1; // last column at num_tile_columns_minus1

            temp_size = pic_height_in_ctbs_y_; // PicHeightInCtbsY
            for (i = 0; i < pps_ptr->num_tile_rows_minus1; i++) {
                pps_ptr->row_height_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
                temp_size -= pps_ptr->row_height_minus1[i] + 1;
            }
            pps_ptr->row_height_minus1[i] = temp_size - 1;  // last row at num_tile_rows_minus1
        }
        else {
            for (i = 0; i <= pps_ptr->num_tile_columns_minus1; i++) {
                pps_ptr->column_width_minus1[i] = ((i + 1) * pic_width_in_ctbs_y_) / (pps_ptr->num_tile_columns_minus1 + 1) - (i * pic_width_in_ctbs_y_) / (pps_ptr->num_tile_columns_minus1 + 1) - 1;
            }
            for (i = 0; i <= pps_ptr->num_tile_rows_minus1; i++) {
                pps_ptr->row_height_minus1[i] = ((i + 1) * pic_height_in_ctbs_y_) / (pps_ptr->num_tile_rows_minus1 + 1) - (i * pic_height_in_ctbs_y_) / (pps_ptr->num_tile_rows_minus1 + 1) - 1;
            }
        }
        pps_ptr->loop_filter_across_tiles_enabled_flag = Parser::GetBit(nalu, offset);
    }
    else {
        pps_ptr->loop_filter_across_tiles_enabled_flag = 1;
        pps_ptr->uniform_spacing_flag = 1;
        pps_ptr->num_tile_columns_minus1 = 0;
        pps_ptr->num_tile_rows_minus1 = 0;
    }
    pps_ptr->pps_loop_filter_across_slices_enabled_flag = Parser::GetBit(nalu, offset);
    pps_ptr->deblocking_filter_control_present_flag = Parser::GetBit(nalu, offset);
    if (pps_ptr->deblocking_filter_control_present_flag) {
        pps_ptr->deblocking_filter_override_enabled_flag = Parser::GetBit(nalu, offset);
        pps_ptr->pps_deblocking_filter_disabled_flag = Parser::GetBit(nalu, offset);
        if (!pps_ptr->pps_deblocking_filter_disabled_flag) {
            pps_ptr->pps_beta_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
            pps_ptr->pps_tc_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
        }
    }
    pps_ptr->pps_scaling_list_data_present_flag = Parser::GetBit(nalu, offset);
    if (pps_ptr->pps_scaling_list_data_present_flag) {
        // Set up default values first
        SetDefaultScalingList(&pps_ptr->scaling_list_data);

        ParseScalingList(&pps_ptr->scaling_list_data, nalu, size, offset, &m_sps_[pps_ptr->pps_seq_parameter_set_id]);
    }
    else {
        pps_ptr->scaling_list_data = m_sps_[pps_ptr->pps_seq_parameter_set_id].scaling_list_data;
    }
    pps_ptr->lists_modification_present_flag = Parser::GetBit(nalu, offset);
    pps_ptr->log2_parallel_merge_level_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);
    pps_ptr->slice_segment_header_extension_present_flag = Parser::GetBit(nalu, offset);
    pps_ptr->pps_extension_present_flag = Parser::GetBit(nalu, offset);
    if (pps_ptr->pps_extension_present_flag) {
        pps_ptr->pps_range_extension_flag = Parser::GetBit(nalu, offset);
        pps_ptr->pps_multilayer_extension_flag = Parser::GetBit(nalu, offset);
        pps_ptr->pps_extension_6bits = Parser::ReadBits(nalu, offset, 6);
    }

    // pps_range_extension()
    if (pps_ptr->pps_range_extension_flag) {
        if (pps_ptr->transform_skip_enabled_flag) {
            pps_ptr->log2_max_transform_skip_block_size_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        pps_ptr->cross_component_prediction_enabled_flag = Parser::GetBit(nalu, offset);
        pps_ptr->chroma_qp_offset_list_enabled_flag = Parser::GetBit(nalu, offset);
        if (pps_ptr->chroma_qp_offset_list_enabled_flag) {
            pps_ptr->diff_cu_chroma_qp_offset_depth = Parser::ExpGolomb::ReadUe(nalu, offset);
            pps_ptr->chroma_qp_offset_list_len_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
            for (int i = 0; i <= pps_ptr->chroma_qp_offset_list_len_minus1; i++) {
                pps_ptr->cb_qp_offset_list[i] = Parser::ExpGolomb::ReadSe(nalu, offset);
                pps_ptr->cr_qp_offset_list[i] = Parser::ExpGolomb::ReadSe(nalu, offset);
            }
        }
        pps_ptr->log2_sao_offset_scale_luma = Parser::ExpGolomb::ReadUe(nalu, offset);
        pps_ptr->log2_sao_offset_scale_chroma = Parser::ExpGolomb::ReadUe(nalu, offset);
    }

#if DBGINFO
    PrintPps(pps_ptr);
#endif // DBGINFO
}

bool HEVCVideoParser::ParseSliceHeader(uint8_t *nalu, size_t size) {
    PpsData *pps_ptr = nullptr;
    SpsData *sps_ptr = nullptr;
    size_t offset = 0;
    SliceHeaderData temp_sh;
    memset(m_sh_, 0, sizeof(SliceHeaderData));
    memset(&temp_sh, 0, sizeof(temp_sh));

    temp_sh.first_slice_segment_in_pic_flag = m_sh_->first_slice_segment_in_pic_flag = Parser::GetBit(nalu, offset);
    if (IsIrapPic(&slice_nal_unit_header_)) {
        temp_sh.no_output_of_prior_pics_flag = m_sh_->no_output_of_prior_pics_flag = Parser::GetBit(nalu, offset);
    }

    // Set active VPS, SPS and PPS for the current slice
    m_active_pps_id_ = Parser::ExpGolomb::ReadUe(nalu, offset);
    temp_sh.slice_pic_parameter_set_id = m_sh_->slice_pic_parameter_set_id = m_active_pps_id_;
    pps_ptr = &m_pps_[m_active_pps_id_];
    if (m_active_sps_id_ != pps_ptr->pps_seq_parameter_set_id) {
        m_active_sps_id_ = pps_ptr->pps_seq_parameter_set_id;
        new_sps_activated_ = true;  // Note: clear this flag after the actions are taken.
    }
    sps_ptr = &m_sps_[m_active_sps_id_];
    m_active_vps_id_ = sps_ptr->sps_video_parameter_set_id;

    // Check video dimension change
    if ( pic_width_ != sps_ptr->pic_width_in_luma_samples || pic_height_ != sps_ptr->pic_height_in_luma_samples) {
        pic_width_ = sps_ptr->pic_width_in_luma_samples;
        pic_height_ = sps_ptr->pic_height_in_luma_samples;
    }

    if (!m_sh_->first_slice_segment_in_pic_flag) {
        if (pps_ptr->dependent_slice_segments_enabled_flag) {
            temp_sh.dependent_slice_segment_flag = m_sh_->dependent_slice_segment_flag = Parser::GetBit(nalu, offset);
        }

        int bits_slice_segment_address = (int)ceilf(log2f((float)pic_size_in_ctbs_y_));
        
        temp_sh.slice_segment_address = m_sh_->slice_segment_address = Parser::ReadBits(nalu, offset, bits_slice_segment_address);   
    }

    if (!m_sh_->dependent_slice_segment_flag) {
        for (int i = 0; i < pps_ptr->num_extra_slice_header_bits; i++) {
            m_sh_->slice_reserved_flag[i] = Parser::GetBit(nalu, offset);
        }
        m_sh_->slice_type = Parser::ExpGolomb::ReadUe(nalu, offset);
        if (pps_ptr->output_flag_present_flag) {
            m_sh_->pic_output_flag = Parser::GetBit(nalu, offset);
        }
        else {
            m_sh_->pic_output_flag = 1;  // default value
        }
        if (sps_ptr->separate_colour_plane_flag) {
            m_sh_->colour_plane_id = Parser::ReadBits(nalu, offset, 2);
        }

        if (slice_nal_unit_header_.nal_unit_type != NAL_UNIT_CODED_SLICE_IDR_W_RADL && slice_nal_unit_header_.nal_unit_type != NAL_UNIT_CODED_SLICE_IDR_N_LP) {
            //length of slice_pic_order_cnt_lsb is log2_max_pic_order_cnt_lsb_minus4 + 4 bits.
            m_sh_->slice_pic_order_cnt_lsb = Parser::ReadBits(nalu, offset, (sps_ptr->log2_max_pic_order_cnt_lsb_minus4 + 4));

            m_sh_->short_term_ref_pic_set_sps_flag = Parser::GetBit(nalu, offset);
            int32_t pos = offset;
            if (!m_sh_->short_term_ref_pic_set_sps_flag) {
                ParseShortTermRefPicSet(&m_sh_->st_rps, sps_ptr->num_short_term_ref_pic_sets, sps_ptr->num_short_term_ref_pic_sets, sps_ptr->st_rps, nalu, size, offset);
            }
            else {
                if (sps_ptr->num_short_term_ref_pic_sets > 1) {
                    int num_bits = 0;
                    while ((1 << num_bits) < sps_ptr->num_short_term_ref_pic_sets) {
                        num_bits++;
                    }
                    if (num_bits > 0) {
                        m_sh_->short_term_ref_pic_set_idx = Parser::ReadBits(nalu, offset, num_bits);
                    }
                }
                else {
                    m_sh_->short_term_ref_pic_set_idx = 0;
                }

                // Copy the SPS RPS to slice RPS
                m_sh_->st_rps = sps_ptr->st_rps[m_sh_->short_term_ref_pic_set_idx];
            }
            m_sh_->short_term_ref_pic_set_size = offset - pos;

            if (sps_ptr->long_term_ref_pics_present_flag) {
                if (sps_ptr->num_long_term_ref_pics_sps > 0) {
                    m_sh_->num_long_term_sps = Parser::ExpGolomb::ReadUe(nalu, offset);
                }
                m_sh_->num_long_term_pics = Parser::ExpGolomb::ReadUe(nalu, offset);

                int bits_for_ltrp_in_sps = 0;
                while (sps_ptr->num_long_term_ref_pics_sps > (1 << bits_for_ltrp_in_sps)) {
                    bits_for_ltrp_in_sps++;
                }
                m_sh_->lt_rps.num_of_pics = m_sh_->num_long_term_sps + m_sh_->num_long_term_pics;
                for (int i = 0; i < (m_sh_->num_long_term_sps + m_sh_->num_long_term_pics); i++) {
                    if (i < m_sh_->num_long_term_sps) {
                        if (sps_ptr->num_long_term_ref_pics_sps > 1) {
                            if( bits_for_ltrp_in_sps > 0) {
                                m_sh_->lt_idx_sps[i] = Parser::ReadBits(nalu, offset, bits_for_ltrp_in_sps);
                                m_sh_->lt_rps.pocs[i] = sps_ptr->lt_rps.pocs[m_sh_->lt_idx_sps[i]];  // PocLsbLt[]
                                m_sh_->lt_rps.used_by_curr_pic[i] = sps_ptr->lt_rps.used_by_curr_pic[m_sh_->lt_idx_sps[i]];  // UsedByCurrPicLt[]
                            }
                        }
                    }
                    else {
                        m_sh_->poc_lsb_lt[i] = Parser::ReadBits(nalu, offset, (sps_ptr->log2_max_pic_order_cnt_lsb_minus4 + 4));
                        m_sh_->used_by_curr_pic_lt_flag[i] = Parser::GetBit(nalu, offset);
                        m_sh_->lt_rps.pocs[i] = m_sh_->poc_lsb_lt[i];  // PocLsbLt[]
                        m_sh_->lt_rps.used_by_curr_pic[i] = m_sh_->used_by_curr_pic_lt_flag[i];  // UsedByCurrPicLt[]
                    }
                    m_sh_->delta_poc_msb_present_flag[i] = Parser::GetBit(nalu, offset);
                    if (m_sh_->delta_poc_msb_present_flag[i]) {
                        // Store DeltaPocMsbCycleLt in delta_poc_msb_cycle_lt for later use
                        int delta_poc_msb_cycle_lt = Parser::ExpGolomb::ReadUe(nalu, offset);
                        if ( i == 0 || i == m_sh_->num_long_term_sps) {
                            m_sh_->delta_poc_msb_cycle_lt[i] = delta_poc_msb_cycle_lt;
                        }
                        else {
                            m_sh_->delta_poc_msb_cycle_lt[i] = delta_poc_msb_cycle_lt + m_sh_->delta_poc_msb_cycle_lt[i - 1];
                        }
                    }
                }
            }
            if (sps_ptr->sps_temporal_mvp_enabled_flag) {
                m_sh_->slice_temporal_mvp_enabled_flag = Parser::GetBit(nalu, offset);
            }
        }

        int chroma_array_type = sps_ptr->separate_colour_plane_flag ? 0 : sps_ptr->chroma_format_idc;  // ChromaArrayType
        if (sps_ptr->sample_adaptive_offset_enabled_flag) {
            m_sh_->slice_sao_luma_flag = Parser::GetBit(nalu, offset);
            if (chroma_array_type)
            {
                m_sh_->slice_sao_chroma_flag = Parser::GetBit(nalu, offset);
            }
        }

        if (m_sh_->slice_type != HEVC_SLICE_TYPE_I) {
            m_sh_->num_ref_idx_active_override_flag = Parser::GetBit(nalu, offset);
            if (m_sh_->num_ref_idx_active_override_flag) {
                m_sh_->num_ref_idx_l0_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
                if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
                    m_sh_->num_ref_idx_l1_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
                }
            }
            else {
                m_sh_->num_ref_idx_l0_active_minus1 = pps_ptr->num_ref_idx_l0_default_active_minus1;
                if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
                    m_sh_->num_ref_idx_l1_active_minus1 = pps_ptr->num_ref_idx_l1_default_active_minus1;
                }
            }

            // 7.3.6.2 Reference picture list modification
            // Calculate NumPicTotalCurr
            num_pic_total_curr_ = 0;
            H265ShortTermRPS *st_rps_ptr = &m_sh_->st_rps;
            // Check the combined list
            for (int i = 0; i < st_rps_ptr->num_of_delta_poc; i++) {
                if (st_rps_ptr->used_by_curr_pic[i]) {
                    num_pic_total_curr_++;
                }
            }

            H265LongTermRPS *lt_rps_ptr = &m_sh_->lt_rps;
            // Check the combined list
            for (int i = 0; i < lt_rps_ptr->num_of_pics; i++) {
                if (lt_rps_ptr->used_by_curr_pic[i]) {
                    num_pic_total_curr_++;
                }
            }

            if (pps_ptr->lists_modification_present_flag && num_pic_total_curr_ > 1)
            {
                int list_entry_bits = 0;
                while ((1 << list_entry_bits) < num_pic_total_curr_) {
                    list_entry_bits++;
                }

                m_sh_->ref_pic_list_modification_flag_l0 = Parser::GetBit(nalu, offset);
                if (m_sh_->ref_pic_list_modification_flag_l0) {
                    for (int i = 0; i < m_sh_->num_ref_idx_l0_active_minus1; i++) {
                        m_sh_->list_entry_l0[i] = Parser::ReadBits(nalu, offset, list_entry_bits);
                    }
                }

                if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
                    m_sh_->ref_pic_list_modification_flag_l1 = Parser::GetBit(nalu, offset);
                    if (m_sh_->ref_pic_list_modification_flag_l1) {
                        for (int i = 0; i < m_sh_->num_ref_idx_l1_active_minus1; i++) {
                            m_sh_->list_entry_l1[i] = Parser::ReadBits(nalu, offset, list_entry_bits);
                        }
                    }
                }
            }

            if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
                m_sh_->mvd_l1_zero_flag = Parser::GetBit(nalu, offset);
            }
            if (pps_ptr->cabac_init_present_flag) {
                m_sh_->cabac_init_flag = Parser::GetBit(nalu, offset);
            }

            if (m_sh_->slice_temporal_mvp_enabled_flag) {
                if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
                    m_sh_->collocated_from_l0_flag = Parser::GetBit(nalu, offset);
                }
                if ((m_sh_->collocated_from_l0_flag && m_sh_->num_ref_idx_l0_active_minus1 > 0) || (!m_sh_->collocated_from_l0_flag && m_sh_->num_ref_idx_l1_active_minus1 > 0)) {
                    m_sh_->collocated_ref_idx = Parser::ExpGolomb::ReadUe(nalu, offset);
                }
            }

            if ((pps_ptr->weighted_pred_flag && m_sh_->slice_type == HEVC_SLICE_TYPE_P) || (pps_ptr->weighted_bipred_flag && m_sh_->slice_type == HEVC_SLICE_TYPE_B)) {
                ParsePredWeightTable(m_sh_, chroma_array_type, nalu, offset);
            }
            m_sh_->five_minus_max_num_merge_cand = Parser::ExpGolomb::ReadUe(nalu, offset);
        }

        m_sh_->slice_qp_delta = Parser::ExpGolomb::ReadSe(nalu, offset);
        if (pps_ptr->pps_slice_chroma_qp_offsets_present_flag) {
            m_sh_->slice_cb_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
            m_sh_->slice_cr_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
        }
        if (pps_ptr->chroma_qp_offset_list_enabled_flag) {
            m_sh_->cu_chroma_qp_offset_enabled_flag = Parser::GetBit(nalu, offset);
        }
        if (pps_ptr->deblocking_filter_override_enabled_flag) {
            m_sh_->deblocking_filter_override_flag = Parser::GetBit(nalu, offset);
        }
        if (m_sh_->deblocking_filter_override_flag) {
            m_sh_->slice_deblocking_filter_disabled_flag = Parser::GetBit(nalu, offset);
            if ( !m_sh_->slice_deblocking_filter_disabled_flag ) {
                m_sh_->slice_beta_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
                m_sh_->slice_tc_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
            }
        }

        if (pps_ptr->pps_loop_filter_across_slices_enabled_flag && (m_sh_->slice_sao_luma_flag || m_sh_->slice_sao_chroma_flag ||
!m_sh_->slice_deblocking_filter_disabled_flag)) {
            m_sh_->slice_loop_filter_across_slices_enabled_flag = Parser::GetBit(nalu, offset);
        }

        memcpy(m_sh_copy_, m_sh_, sizeof(SliceHeaderData));
    }
    else {
        //dependant slice
        memcpy(m_sh_, m_sh_copy_, sizeof(SliceHeaderData));
        m_sh_->first_slice_segment_in_pic_flag = temp_sh.first_slice_segment_in_pic_flag;
        m_sh_->no_output_of_prior_pics_flag = temp_sh.no_output_of_prior_pics_flag;
        m_sh_->slice_pic_parameter_set_id = temp_sh.slice_pic_parameter_set_id;
        m_sh_->dependent_slice_segment_flag = temp_sh.dependent_slice_segment_flag;
        m_sh_->slice_segment_address = temp_sh.slice_segment_address;
    }
    if (pps_ptr->tiles_enabled_flag || pps_ptr->entropy_coding_sync_enabled_flag) {
        int max_num_entry_point_offsets;  // 7.4.7.1
        if (!pps_ptr->tiles_enabled_flag && pps_ptr->entropy_coding_sync_enabled_flag) {
            max_num_entry_point_offsets = pic_height_in_ctbs_y_ - 1;
        }
        else if (pps_ptr->tiles_enabled_flag && !pps_ptr->entropy_coding_sync_enabled_flag) {
            max_num_entry_point_offsets = (pps_ptr->num_tile_columns_minus1 + 1) * (pps_ptr->num_tile_rows_minus1 + 1) - 1;
        }
        else {
            max_num_entry_point_offsets = (pps_ptr->num_tile_columns_minus1 + 1) * pic_height_in_ctbs_y_ - 1;
        }
        m_sh_->num_entry_point_offsets = Parser::ExpGolomb::ReadUe(nalu, offset);
        if (m_sh_->num_entry_point_offsets > max_num_entry_point_offsets) {
            m_sh_->num_entry_point_offsets = max_num_entry_point_offsets;
        }

 #if 0 // do not parse syntax parameters that are not used by HW decode
       if (m_sh_->num_entry_point_offsets) {
            m_sh_->offset_len_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
            for (int i = 0; i < m_sh_->num_entry_point_offsets; i++) {
                m_sh_->entry_point_offset_minus1[i] = Parser::ReadBits(nalu, offset, m_sh_->offset_len_minus1 + 1);
            }
        }
#endif
    }

#if 0 // do not parse syntax parameters that are not used by HW decode
    if (pps_ptr->slice_segment_header_extension_present_flag) {
        m_sh_->slice_segment_header_extension_length = Parser::ExpGolomb::ReadUe(nalu, offset);
        for (int i = 0; i < m_sh_->slice_segment_header_extension_length; i++) {
            m_sh_->slice_segment_header_extension_data_byte[i] = Parser::ReadBits(nalu, offset, 8);
        }
    }
#endif

#if DBGINFO
    PrintSliceSegHeader(m_sh_);
#endif // DBGINFO

    return false;
}

void HEVCVideoParser::ParseSeiMessage(uint8_t *nalu, size_t size) {
    int offset = 0; // byte offset
    int payload_type;
    int payload_size;

    do {
        payload_type = 0;
        while (nalu[offset] == 0xFF) {
            payload_type += 255;  // ff_byte
            offset++;
        }
        payload_type += nalu[offset];  // last_payload_type_byte
        offset++;

        payload_size = 0;
        while (nalu[offset] == 0xFF) {
            payload_size += 255;  // ff_byte
            offset++;
        }
        payload_size += nalu[offset];  // last_payload_size_byte
        offset++;

        // We start with INIT_SEI_MESSAGE_COUNT. Should be enough for normal use cases. If not, resize.
        if((sei_message_count_ + 1) > sei_message_list_.size()) {
            sei_message_list_.resize((sei_message_count_ + 1));
        }
        sei_message_list_[sei_message_count_].sei_message_type = payload_type;
        sei_message_list_[sei_message_count_].sei_message_size = payload_size;

        if (sei_payload_buf_) {
            if ((payload_size + sei_payload_size_) > sei_payload_buf_size_) {
                uint8_t *tmp_ptr = (uint8_t*)malloc(payload_size + sei_payload_size_);
                memcpy(tmp_ptr, sei_payload_buf_, sei_payload_size_); // save the existing payload
                free(sei_payload_buf_);
                sei_payload_buf_ = tmp_ptr;
            }
        }
        else {
            // First payload, sei_payload_size_ is 0.
            sei_payload_buf_size_ = payload_size > INIT_SEI_PAYLOAD_BUF_SIZE ? payload_size : INIT_SEI_PAYLOAD_BUF_SIZE;
            sei_payload_buf_ = (uint8_t*)malloc(sei_payload_buf_size_);
        }
        // Append the current payload to sei_payload_buf_
        memcpy(sei_payload_buf_ + sei_payload_size_, nalu + offset, payload_size);

        sei_payload_size_ += payload_size;
        sei_message_count_++;

        offset += payload_size;
    } while (offset < size && nalu[offset] != 0x80);
}

bool HEVCVideoParser::IsIdrPic(NalUnitHeader *nal_header_ptr) {
    return (nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP);
}

bool HEVCVideoParser::IsBlaPic(NalUnitHeader *nal_header_ptr) {
    return (nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_BLA_W_LP || nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_BLA_W_RADL || nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_BLA_N_LP);
}

bool HEVCVideoParser::IsCraPic(NalUnitHeader *nal_header_ptr) {
    return (nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA_NUT);
}

bool HEVCVideoParser::IsRaslPic(NalUnitHeader *nal_header_ptr) {
    return (nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_RASL_N || nal_header_ptr->nal_unit_type == NAL_UNIT_CODED_SLICE_RASL_R);
}

bool HEVCVideoParser::IsIrapPic(NalUnitHeader *nal_header_ptr) {
    return (nal_header_ptr->nal_unit_type >= NAL_UNIT_CODED_SLICE_BLA_W_LP && nal_header_ptr->nal_unit_type <= NAL_UNIT_RESERVED_IRAP_VCL23);
}

void HEVCVideoParser::CalculateCurrPOC() {
    if (IsIdrPic(&slice_nal_unit_header_)) {
        curr_pic_info_.pic_order_cnt = 0;
        curr_pic_info_.prev_poc_lsb = 0;
        curr_pic_info_.prev_poc_msb = 0;
        curr_pic_info_.slice_pic_order_cnt_lsb = 0;
    }
    else {
        int max_poc_lsb = 1 << (m_sps_[m_active_sps_id_].log2_max_pic_order_cnt_lsb_minus4 + 4);  // MaxPicOrderCntLsb
        int poc_msb;  // PicOrderCntMsb
        // If the current picture is an IRAP picture with NoRaslOutputFlag equal to 1, PicOrderCntMsb is set equal to 0.
        if (IsIrapPic(&slice_nal_unit_header_) && no_rasl_output_flag_ == 1) {
            poc_msb = 0;
        }
        else {
            if ((m_sh_->slice_pic_order_cnt_lsb < curr_pic_info_.prev_poc_lsb) && ((curr_pic_info_.prev_poc_lsb - m_sh_->slice_pic_order_cnt_lsb) >= (max_poc_lsb / 2))) {
                poc_msb = curr_pic_info_.prev_poc_msb + max_poc_lsb;
            }
            else if ((m_sh_->slice_pic_order_cnt_lsb > curr_pic_info_.prev_poc_lsb) && ((m_sh_->slice_pic_order_cnt_lsb - curr_pic_info_.prev_poc_lsb) > (max_poc_lsb / 2))) {
                poc_msb = curr_pic_info_.prev_poc_msb - max_poc_lsb;
            }
            else {
                poc_msb = curr_pic_info_.prev_poc_msb;
            }
        }

        curr_pic_info_.pic_order_cnt = poc_msb + m_sh_->slice_pic_order_cnt_lsb;
        curr_pic_info_.prev_poc_lsb = m_sh_->slice_pic_order_cnt_lsb;
        curr_pic_info_.prev_poc_msb = poc_msb;
        curr_pic_info_.slice_pic_order_cnt_lsb = m_sh_->slice_pic_order_cnt_lsb;
    }
}

void HEVCVideoParser::DeocdeRps() {
    int i, j, k;
    int curr_delta_proc_msb_present_flag[HEVC_MAX_NUM_REF_PICS] = {0}; // CurrDeltaPocMsbPresentFlag
    int foll_delta_poc_msb_present_flag[HEVC_MAX_NUM_REF_PICS] = {0}; // FollDeltaPocMsbPresentFlag
    int max_poc_lsb = 1 << (m_sps_[m_active_sps_id_].log2_max_pic_order_cnt_lsb_minus4 + 4);  // MaxPicOrderCntLsb

    // When the current picture is an IRAP picture with NoRaslOutputFlag equal to 1, all reference pictures with
    // nuh_layer_id equal to currPicLayerId currently in the DPB (if any) are marked as "unused for reference".
    if (IsIrapPic(&slice_nal_unit_header_) && no_rasl_output_flag_ == 1) {
        for (i = 0; i < HVC_MAX_DPB_FRAMES; i++) {
            dpb_buffer_.frame_buffer_list[i].is_reference = kUnusedForReference;
        }
    }

    if (IsIdrPic(&slice_nal_unit_header_)) {
        num_poc_st_curr_before_ = 0;
        num_poc_st_curr_after_ = 0;
        num_poc_st_foll_ = 0;
        num_poc_lt_curr_ = 0;
        num_poc_lt_foll_ = 0;
        memset(poc_st_curr_before_, 0, sizeof(int32_t) * HEVC_MAX_NUM_REF_PICS);
        memset(poc_st_curr_after_, 0, sizeof(int32_t) * HEVC_MAX_NUM_REF_PICS);
        memset(poc_st_foll_, 0, sizeof(int32_t) * HEVC_MAX_NUM_REF_PICS);
        memset(poc_lt_curr_, 0, sizeof(int32_t) * HEVC_MAX_NUM_REF_PICS);
        memset(poc_lt_foll_, 0, sizeof(int32_t) * HEVC_MAX_NUM_REF_PICS);
    }
    else {
        H265ShortTermRPS *rps_ptr = &m_sh_->st_rps;
        for (i = 0, j = 0, k = 0; i < rps_ptr->num_negative_pics; i++) {
            if (rps_ptr->used_by_curr_pic[i]) { // UsedByCurrPicS0
                poc_st_curr_before_[j++] = curr_pic_info_.pic_order_cnt + rps_ptr->delta_poc[i]; // DeltaPocS0
            }
            else {
                poc_st_foll_[k++] = curr_pic_info_.pic_order_cnt + rps_ptr->delta_poc[i];
            }
        }
        num_poc_st_curr_before_ = j;

        // UsedByCurrPicS1 follows UsedByCurrPicS0 in used_by_curr_pic. DeltaPocS1 follows DeltaPocS0 in delta_poc.
        for (j = 0; i < rps_ptr->num_of_delta_poc; i++ ) {
            if (rps_ptr->used_by_curr_pic[i]) { // UsedByCurrPicS1
                poc_st_curr_after_[j++] = curr_pic_info_.pic_order_cnt + rps_ptr->delta_poc[i]; // DeltaPocS1
            }
            else {
                poc_st_foll_[k++] = curr_pic_info_.pic_order_cnt + rps_ptr->delta_poc[i]; // DeltaPocS1
            }
        }
        num_poc_st_curr_after_ = j;
        num_poc_st_foll_ = k;

        H265LongTermRPS *lt_rps_ptr = &m_sh_->lt_rps;
        for (i = 0, j = 0, k = 0; i < lt_rps_ptr->num_of_pics; i++) {
            uint32_t poc_lt = lt_rps_ptr->pocs[i];  // oocLt
            if (m_sh_->delta_poc_msb_present_flag[i]) {
                poc_lt += curr_pic_info_.pic_order_cnt - m_sh_->delta_poc_msb_cycle_lt[i] * max_poc_lsb - curr_pic_info_.pic_order_cnt & (max_poc_lsb - 1);
            }

            if (lt_rps_ptr->used_by_curr_pic[i]) {
                poc_lt_curr_[j] = poc_lt;
                curr_delta_proc_msb_present_flag[j++] = m_sh_->delta_poc_msb_present_flag[i];
            }
            else {
                poc_lt_foll_[k] = poc_lt;
                foll_delta_poc_msb_present_flag[k++] = m_sh_->delta_poc_msb_present_flag[i];
            }
        }
        num_poc_lt_curr_ = j;
        num_poc_lt_foll_ = k;

        /*
        * RPS derivation and picture marking
        */
        // Init as "no reference picture"
        for (i = 0; i < HEVC_MAX_NUM_REF_PICS; i++) {
            ref_pic_set_st_curr_before_[i] = 0xFF;
            ref_pic_set_st_curr_after_[i] = 0xFF;
            ref_pic_set_st_foll_[i] = 0xFF;
            ref_pic_set_lt_curr_[i] = 0xFF;
            ref_pic_set_lt_foll_[i] = 0xFF;
        }

        // Mark all in DPB as unused. We will mark them back while we go through the ref lists. The rest will be actually unused.
        for (i = 0; i < HVC_MAX_DPB_FRAMES; i++) {
            dpb_buffer_.frame_buffer_list[i].is_reference = kUnusedForReference;
        }

        /// Short term reference pictures
        for (i = 0; i < num_poc_st_curr_before_; i++) {
            for (j = 0; j < HVC_MAX_DPB_FRAMES; j++) {
                if (poc_st_curr_before_[i] == dpb_buffer_.frame_buffer_list[j].pic_order_cnt) {
                    ref_pic_set_st_curr_before_[i] = j;  // RefPicSetStCurrBefore. Use DPB buffer index for now
                    dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForShortTerm;
                    break;
                }
            }
        }

        for (i = 0; i < num_poc_st_curr_after_; i++) {
            for (j = 0; j < HVC_MAX_DPB_FRAMES; j++) {
                if (poc_st_curr_after_[i] == dpb_buffer_.frame_buffer_list[j].pic_order_cnt) {
                    ref_pic_set_st_curr_after_[i] = j;  // RefPicSetStCurrAfter
                    dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForShortTerm;
                    break;
                }
            }
        }

        for ( i = 0; i < num_poc_st_foll_; i++ ) {
            for (j = 0; j < HVC_MAX_DPB_FRAMES; j++) {
                if (poc_st_foll_[i] == dpb_buffer_.frame_buffer_list[j].pic_order_cnt) {
                    ref_pic_set_st_foll_[i] = j;  // RefPicSetStFoll
                    dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForShortTerm;
                    break;
                }
            }
        }

        /// Long term reference pictures
        for (i = 0; i < num_poc_lt_curr_; i++) {
            for (j = 0; j < HVC_MAX_DPB_FRAMES; j++) {
                if(!curr_delta_proc_msb_present_flag[i]) {
                    if (poc_lt_curr_[i] == (dpb_buffer_.frame_buffer_list[j].pic_order_cnt & (max_poc_lsb - 1))) {
                        ref_pic_set_lt_curr_[i] = j;  // RefPicSetLtCurr
                        dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForLongTerm;
                        break;
                    }
                }
                else {
                    if (poc_lt_curr_[i] == dpb_buffer_.frame_buffer_list[j].pic_order_cnt) {
                        ref_pic_set_lt_curr_[i] = j;  // RefPicSetLtCurr
                        dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForLongTerm;
                        break;
                    }
                }
            }
        }

        for (i = 0; i < num_poc_lt_foll_; i++) {
            for (j = 0; j < HVC_MAX_DPB_FRAMES; j++) {
                if(!foll_delta_poc_msb_present_flag[i]) {
                    if (poc_lt_foll_[i] == (dpb_buffer_.frame_buffer_list[j].pic_order_cnt & (max_poc_lsb - 1))) {
                        ref_pic_set_lt_foll_[i] = j;  // RefPicSetLtFoll
                        dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForLongTerm;
                        break;
                    }
                }
                else {
                    if (poc_lt_foll_[i] == dpb_buffer_.frame_buffer_list[j].pic_order_cnt) {
                        ref_pic_set_lt_foll_[i] = j;  // RefPicSetLtFoll
                        dpb_buffer_.frame_buffer_list[j].is_reference = kUsedForLongTerm;
                        break;
                    }
                }
            }
        }
    }
}

void HEVCVideoParser::ConstructRefPicLists() {

    uint32_t num_rps_curr_temp_list; // NumRpsCurrTempList0 or NumRpsCurrTempList1;
    int i, j;
    int rIdx;
    uint32_t ref_pic_list_temp[HEVC_MAX_NUM_REF_PICS];  // RefPicListTemp0 or RefPicListTemp1

    /// List 0
    rIdx = 0;
    num_rps_curr_temp_list = std::max(m_sh_->num_ref_idx_l0_active_minus1 + 1, num_pic_total_curr_);

    // Error handling to prevent infinite loop
    if ((num_poc_st_curr_before_ + num_poc_st_curr_after_ + num_poc_lt_curr_) < num_rps_curr_temp_list) {
        return;
    }
    while (rIdx < num_rps_curr_temp_list) {
        for (i = 0; i < num_poc_st_curr_before_ && rIdx < num_rps_curr_temp_list; rIdx++, i++) {
            ref_pic_list_temp[rIdx] = ref_pic_set_st_curr_before_[i];
        }

        for (i = 0; i < num_poc_st_curr_after_ && rIdx < num_rps_curr_temp_list; rIdx++, i++) {
            ref_pic_list_temp[rIdx] = ref_pic_set_st_curr_after_[i];
        }

        for (i = 0; i < num_poc_lt_curr_ && rIdx < num_rps_curr_temp_list; rIdx++, i++) {
            ref_pic_list_temp[rIdx] = ref_pic_set_lt_curr_[i];
        }
    }

    for( rIdx = 0; rIdx <= m_sh_->num_ref_idx_l0_active_minus1; rIdx++) {
        ref_pic_list_0_[rIdx] = m_sh_->ref_pic_list_modification_flag_l0 ? ref_pic_list_temp[m_sh_->list_entry_l0[rIdx]] : ref_pic_list_temp[rIdx];
    }

    /// List 1
    if (m_sh_->slice_type == HEVC_SLICE_TYPE_B) {
        rIdx = 0;
        num_rps_curr_temp_list = std::max(m_sh_->num_ref_idx_l1_active_minus1 + 1, num_pic_total_curr_);

        // Error handling to prevent infinite loop
        if ((num_poc_st_curr_before_ + num_poc_st_curr_after_ + num_poc_lt_curr_) < num_rps_curr_temp_list) {
            return;
        }

        while (rIdx < num_rps_curr_temp_list) {
            for (i = 0; i < num_poc_st_curr_after_ && rIdx < num_rps_curr_temp_list; rIdx++, i++) {
                ref_pic_list_temp[rIdx] = ref_pic_set_st_curr_after_[i];
            }

            for (i = 0; i < num_poc_st_curr_before_ && rIdx < num_rps_curr_temp_list; rIdx++, i++ ) {
                ref_pic_list_temp[rIdx] = ref_pic_set_st_curr_before_[i];
            }

            for (i = 0; i < num_poc_lt_curr_ && rIdx < num_rps_curr_temp_list; rIdx++, i++ ) {
                ref_pic_list_temp[rIdx] = ref_pic_set_lt_curr_[i];
            }
        }

        for( rIdx = 0; rIdx <= m_sh_->num_ref_idx_l1_active_minus1; rIdx++) {
            ref_pic_list_1_[rIdx] = m_sh_->ref_pic_list_modification_flag_l1 ? ref_pic_list_temp[m_sh_->list_entry_l1[rIdx]] : ref_pic_list_temp[rIdx];
        }
    }
}

int HEVCVideoParser::FindFreeBufAndMark() {
    int i;

    // Clear usage flags. For now, buffers that are unused for refrence are marked as empty.
    // Todo: implement HEVC DPU output policy
    dpb_buffer_.dpb_fullness = 0;
    for (i = 0; i < HVC_MAX_DPB_FRAMES; i++) {
        if (dpb_buffer_.frame_buffer_list[i].is_reference == kUnusedForReference) {
            dpb_buffer_.frame_buffer_list[i].use_status = 0;
        }
        else {
            dpb_buffer_.frame_buffer_list[i].use_status = 3;  // frame used
            dpb_buffer_.dpb_fullness++;
        }
    }

    if (dpb_buffer_.dpb_fullness == HVC_MAX_DPB_FRAMES) {
        // Buffer is full
        return PARSER_NOT_FOUND;
    }
    else {
        for (i = 0; i < HVC_MAX_DPB_FRAMES; i++) {
            if (dpb_buffer_.frame_buffer_list[i].use_status == 0) {
                break;
            }
        }
    }

    curr_pic_info_.pic_idx = i;
    curr_pic_info_.is_reference = kUsedForShortTerm;
    dpb_buffer_.frame_buffer_list[i].pic_order_cnt = curr_pic_info_.pic_order_cnt;
    dpb_buffer_.frame_buffer_list[i].prev_poc_lsb = curr_pic_info_.prev_poc_lsb;
    dpb_buffer_.frame_buffer_list[i].prev_poc_msb = curr_pic_info_.prev_poc_msb;
    dpb_buffer_.frame_buffer_list[i].slice_pic_order_cnt_lsb = curr_pic_info_.slice_pic_order_cnt_lsb;
    dpb_buffer_.frame_buffer_list[i].is_reference = kUsedForShortTerm;
    dpb_buffer_.frame_buffer_list[i].use_status = 3;
    dpb_buffer_.dpb_fullness++;

    return PARSER_OK;
}

size_t HEVCVideoParser::EBSPtoRBSP(uint8_t *streamBuffer,size_t begin_bytepos, size_t end_bytepos) {
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
            }
            else if (tmp < 0x03) {
            }
        }
        if (tmp == 0x00) {
            count++;
        }
        else {
            count = 0;
        }
        streamBuffer_i++;
    }
    return end_bytepos - begin_bytepos + reduce_count;
}

#if DBGINFO
void HEVCVideoParser::PrintVps(HEVCVideoParser::VpsData *vps_ptr) {
    MSG("=== hevc_video_parameter_set_t ===");
    MSG("vps_video_parameter_set_id               = " <<  vps_ptr->vps_video_parameter_set_id);
    MSG("vps_base_layer_internal_flag             = " <<  vps_ptr->vps_base_layer_internal_flag);
    MSG("vps_base_layer_available_flag            = " <<  vps_ptr->vps_base_layer_available_flag);
    MSG("vps_max_layers_minus1                    = " <<  vps_ptr->vps_max_layers_minus1);
    MSG("vps_max_sub_layers_minus1                = " <<  vps_ptr->vps_max_sub_layers_minus1);
    MSG("vps_temporal_id_nesting_flag             = " <<  vps_ptr->vps_temporal_id_nesting_flag);
    MSG("vps_reserved_0xffff_16bits               = " <<  vps_ptr->vps_reserved_0xffff_16bits);

    MSG("Profile tier level:");
    MSG("general_profile_space                    = " <<  vps_ptr->profile_tier_level.general_profile_space);
    MSG("general_tier_flag                        = " <<  vps_ptr->profile_tier_level.general_tier_flag);
    MSG("general_profile_idc                      = " <<  vps_ptr->profile_tier_level.general_profile_idc);
    MSG_NO_NEWLINE("general_profile_compatibility_flag[32]: ");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << vps_ptr->profile_tier_level.general_profile_compatibility_flag[i]);
    }
    MSG("");
    MSG("general_progressive_source_flag          = " <<  vps_ptr->profile_tier_level.general_progressive_source_flag);
    MSG("general_interlaced_source_flag           = " <<  vps_ptr->profile_tier_level.general_interlaced_source_flag);
    MSG("general_non_packed_constraint_flag       = " <<  vps_ptr->profile_tier_level.general_non_packed_constraint_flag);
    MSG("general_frame_only_constraint_flag       = " <<  vps_ptr->profile_tier_level.general_frame_only_constraint_flag);
    MSG("general_reserved_zero_44bits             = " <<  vps_ptr->profile_tier_level.general_reserved_zero_44bits);
    MSG("general_level_idc                        = " <<  vps_ptr->profile_tier_level.general_level_idc);

    MSG("vps_sub_layer_ordering_info_present_flag = " <<  vps_ptr->vps_sub_layer_ordering_info_present_flag);
    MSG_NO_NEWLINE("vps_max_dec_pic_buffering_minus1[]: ");
    for(int i = 0; i < 7; i++) {
        MSG_NO_NEWLINE(" " << vps_ptr->vps_max_dec_pic_buffering_minus1[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("vps_max_num_reorder_pics[]: ");
    for(int i = 0; i < 7; i++) {
        MSG_NO_NEWLINE(" " << vps_ptr->vps_max_num_reorder_pics[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("vps_max_latency_increase_plus1[]: ");
    for(int i = 0; i < 7; i++) {
        MSG_NO_NEWLINE(" " << vps_ptr->vps_max_latency_increase_plus1[i]);
    }
    MSG("");
    MSG("vps_max_layer_id                         = " <<  vps_ptr->vps_max_layer_id);
    MSG("vps_num_layer_sets_minus1                = " <<  vps_ptr->vps_num_layer_sets_minus1);
    MSG("vps_timing_info_present_flag             = " <<  vps_ptr->vps_timing_info_present_flag);
    MSG("vps_num_hrd_parameters                   = " <<  vps_ptr->vps_num_hrd_parameters);
    MSG("vps_extension_flag                       = " <<  vps_ptr->vps_extension_flag);
    MSG("vps_extension_data_flag                  = " <<  vps_ptr->vps_extension_data_flag);
    MSG("");
}

void HEVCVideoParser::PrintSps(HEVCVideoParser::SpsData *sps_ptr) {
    MSG("=== hevc_sequence_parameter_set_t ===");
    MSG("sps_video_parameter_set_id                = " <<  sps_ptr->sps_video_parameter_set_id);
    MSG("sps_max_sub_layers_minus1                 = " <<  sps_ptr->sps_max_sub_layers_minus1);
    MSG("sps_temporal_id_nesting_flag              = " <<  sps_ptr->sps_temporal_id_nesting_flag);

    MSG("Profile tier level:");
    MSG("general_profile_space                     = " <<  sps_ptr->profile_tier_level.general_profile_space);
    MSG("general_tier_flag                         = " <<  sps_ptr->profile_tier_level.general_tier_flag);
    MSG("general_profile_idc                       = " <<  sps_ptr->profile_tier_level.general_profile_idc);
    MSG("general_profile_compatibility_flag[32]:");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << sps_ptr->profile_tier_level.general_profile_compatibility_flag[i]);
    }
    MSG("");
    MSG("general_progressive_source_flag           = " <<  sps_ptr->profile_tier_level.general_progressive_source_flag);
    MSG("general_interlaced_source_flag            = " <<  sps_ptr->profile_tier_level.general_interlaced_source_flag);
    MSG("general_non_packed_constraint_flag        = " <<  sps_ptr->profile_tier_level.general_non_packed_constraint_flag);
    MSG("general_frame_only_constraint_flag        = " <<  sps_ptr->profile_tier_level.general_frame_only_constraint_flag);
    MSG("general_reserved_zero_44bits              = " <<  sps_ptr->profile_tier_level.general_reserved_zero_44bits);
    MSG("general_level_idc                         = " <<  sps_ptr->profile_tier_level.general_level_idc);

    MSG("sps_seq_parameter_set_id                  = " <<  sps_ptr->sps_seq_parameter_set_id);
    MSG("chroma_format_idc                         = " <<  sps_ptr->chroma_format_idc);
    MSG("separate_colour_plane_flag                = " <<  sps_ptr->separate_colour_plane_flag);
    MSG("pic_width_in_luma_samples                 = " <<  sps_ptr->pic_width_in_luma_samples);
    MSG("pic_height_in_luma_samples                = " <<  sps_ptr->pic_height_in_luma_samples);
    MSG("conformance_window_flag                   = " <<  sps_ptr->conformance_window_flag);
    MSG("conf_win_left_offset                      = " <<  sps_ptr->conf_win_left_offset);
    MSG("conf_win_right_offset                     = " <<  sps_ptr->conf_win_right_offset);
    MSG("conf_win_top_offset                       = " <<  sps_ptr->conf_win_top_offset);
    MSG("conf_win_bottom_offset                    = " <<  sps_ptr->conf_win_bottom_offset);
    MSG("bit_depth_luma_minus8                     = " <<  sps_ptr->bit_depth_luma_minus8);
    MSG("bit_depth_chroma_minus8                   = " <<  sps_ptr->bit_depth_chroma_minus8);
    MSG("log2_max_pic_order_cnt_lsb_minus4         = " <<  sps_ptr->log2_max_pic_order_cnt_lsb_minus4);
    MSG("sps_sub_layer_ordering_info_present_flag  = " <<  sps_ptr->sps_sub_layer_ordering_info_present_flag);
    MSG_NO_NEWLINE("sps_max_dec_pic_buffering_minus1[]:");
    for(int i = 0; i < 7; i++) {
        MSG_NO_NEWLINE(" " << sps_ptr->sps_max_dec_pic_buffering_minus1[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("sps_max_num_reorder_pics[]:");
    for(int i = 0; i < 7; i++) {
        MSG_NO_NEWLINE(" " << sps_ptr->sps_max_num_reorder_pics[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("sps_max_latency_increase_plus1[]:");
    for(int i = 0; i < 7; i++) {
        MSG_NO_NEWLINE(" " << sps_ptr->sps_max_latency_increase_plus1[i]);
    }
    MSG("");
    MSG("log2_min_luma_coding_block_size_minus3    = " <<  sps_ptr->log2_min_luma_coding_block_size_minus3);
    MSG("log2_diff_max_min_luma_coding_block_size  = " <<  sps_ptr->log2_diff_max_min_luma_coding_block_size);
    MSG("log2_min_transform_block_size_minus2      = " <<  sps_ptr->log2_min_transform_block_size_minus2);
    MSG("log2_diff_max_min_transform_block_size    = " <<  sps_ptr->log2_diff_max_min_transform_block_size);
    MSG("max_transform_hierarchy_depth_inter       = " <<  sps_ptr->max_transform_hierarchy_depth_inter);
    MSG("max_transform_hierarchy_depth_intra       = " <<  sps_ptr->max_transform_hierarchy_depth_intra);
    MSG("scaling_list_enabled_flag                 = " <<  sps_ptr->scaling_list_enabled_flag);
    MSG("sps_scaling_list_data_present_flag        = " <<  sps_ptr->sps_scaling_list_data_present_flag);
    MSG("Scaling list:");
    for (int i = 0; i < H265_SCALING_LIST_SIZE_NUM; i++) {
        for (int j = 0; j < H265_SCALING_LIST_NUM; j++) {
            MSG_NO_NEWLINE("scaling_list[" << i <<"][" << j << "][]:")
            for (int k = 0; k < H265_SCALING_LIST_MAX_I; k++) {
                MSG_NO_NEWLINE(" " << sps_ptr->scaling_list_data.scaling_list[i][j][k]);
            }
            MSG("");
        }
    }

    MSG("amp_enabled_flag                          = " <<  sps_ptr->amp_enabled_flag);
    MSG("sample_adaptive_offset_enabled_flag       = " <<  sps_ptr->sample_adaptive_offset_enabled_flag);
    MSG("pcm_enabled_flag                          = " <<  sps_ptr->pcm_enabled_flag);
    MSG("pcm_sample_bit_depth_luma_minus1          = " <<  sps_ptr->pcm_sample_bit_depth_luma_minus1);
    MSG("pcm_sample_bit_depth_chroma_minus1        = " <<  sps_ptr->pcm_sample_bit_depth_chroma_minus1);
    MSG("log2_min_pcm_luma_coding_block_size_minus3 = " <<  sps_ptr->log2_min_pcm_luma_coding_block_size_minus3);
    MSG("log2_diff_max_min_pcm_luma_coding_block_size = " <<  sps_ptr->log2_diff_max_min_pcm_luma_coding_block_size);
    MSG("pcm_loop_filter_disabled_flag             = " <<  sps_ptr->pcm_loop_filter_disabled_flag);
    MSG("num_short_term_ref_pic_sets               = " <<  sps_ptr->num_short_term_ref_pic_sets);

    if (sps_ptr->num_short_term_ref_pic_sets) {
        for(int i = 0; i < sps_ptr->num_short_term_ref_pic_sets; i++) {
            PrintStRps(&sps_ptr->st_rps[i]);
        }
    }

    MSG("long_term_ref_pics_present_flag           = " <<  sps_ptr->long_term_ref_pics_present_flag);
    MSG("num_long_term_ref_pics_sps                = " <<  sps_ptr->num_long_term_ref_pics_sps);
    
    if (sps_ptr->num_long_term_ref_pics_sps) {
        MSG("lt_ref_pic_poc_lsb_sps[%u]:  " <<  sps_ptr->num_long_term_ref_pics_sps);
        for(int i = 0; i < sps_ptr->num_long_term_ref_pics_sps; i++) {
            MSG_NO_NEWLINE(" " << sps_ptr->lt_ref_pic_poc_lsb_sps[i]);
        }
        MSG("");
        MSG("used_by_curr_pic_lt_sps_flag[%u]:  " <<  sps_ptr->num_long_term_ref_pics_sps);
        for(int i = 0; i < sps_ptr->num_long_term_ref_pics_sps; i++) {
            MSG_NO_NEWLINE(" " << sps_ptr->used_by_curr_pic_lt_sps_flag[i]);
        }
        MSG("");
    }

    PrintLtRefInfo(&sps_ptr->lt_rps);

    MSG("sps_temporal_mvp_enabled_flag             = " <<  sps_ptr->sps_temporal_mvp_enabled_flag);
    MSG("strong_intra_smoothing_enabled_flag       = " <<  sps_ptr->strong_intra_smoothing_enabled_flag);
    MSG("vui_parameters_present_flag               = " <<  sps_ptr->vui_parameters_present_flag);

    MSG("sps_extension_present_flag                = " <<  sps_ptr->sps_extension_flag);
    MSG("");
}

void HEVCVideoParser::PrintPps(HEVCVideoParser::PpsData *pps_ptr) {
    MSG("=== hevc_picture_parameter_set_t ===");
    MSG("pps_pic_parameter_set_id                    = " <<  pps_ptr->pps_pic_parameter_set_id);
    MSG("pps_seq_parameter_set_id                    = " <<  pps_ptr->pps_seq_parameter_set_id);
    MSG("dependent_slice_segments_enabled_flag       = " <<  pps_ptr->dependent_slice_segments_enabled_flag);
    MSG("output_flag_present_flag                    = " <<  pps_ptr->output_flag_present_flag);
    MSG("num_extra_slice_header_bits                 = " <<  pps_ptr->num_extra_slice_header_bits);
    MSG("sign_data_hiding_enabled_flag               = " <<  pps_ptr->sign_data_hiding_enabled_flag);
    MSG("cabac_init_present_flag                     = " <<  pps_ptr->cabac_init_present_flag);
    MSG("num_ref_idx_l0_default_active_minus1        = " <<  pps_ptr->num_ref_idx_l0_default_active_minus1);
    MSG("num_ref_idx_l1_default_active_minus1        = " <<  pps_ptr->num_ref_idx_l1_default_active_minus1);
    MSG("init_qp_minus26                             = " <<  pps_ptr->init_qp_minus26);
    MSG("constrained_intra_pred_flag                 = " <<  pps_ptr->constrained_intra_pred_flag);
    MSG("transform_skip_enabled_flag                 = " <<  pps_ptr->transform_skip_enabled_flag);
    MSG("cu_qp_delta_enabled_flag                    = " <<  pps_ptr->cu_qp_delta_enabled_flag);
    MSG("diff_cu_qp_delta_depth                      = " <<  pps_ptr->diff_cu_qp_delta_depth);
    MSG("pps_cb_qp_offset                            = " <<  pps_ptr->pps_cb_qp_offset);
    MSG("pps_cr_qp_offset                            = " <<  pps_ptr->pps_cr_qp_offset);
    MSG("pps_slice_chroma_qp_offsets_present_flag    = " <<  pps_ptr->pps_slice_chroma_qp_offsets_present_flag);
    MSG("weighted_pred_flag                          = " <<  pps_ptr->weighted_pred_flag);
    MSG("weighted_bipred_flag                        = " <<  pps_ptr->weighted_bipred_flag);
    MSG("transquant_bypass_enabled_flag              = " <<  pps_ptr->transquant_bypass_enabled_flag);
    MSG("tiles_enabled_flag                          = " <<  pps_ptr->tiles_enabled_flag);
    MSG("entropy_coding_sync_enabled_flag            = " <<  pps_ptr->entropy_coding_sync_enabled_flag);
    MSG("num_tile_columns_minus1                     = " <<  pps_ptr->num_tile_columns_minus1);
    MSG("num_tile_rows_minus1                        = " <<  pps_ptr->num_tile_rows_minus1);
    MSG("uniform_spacing_flag                        = " <<  pps_ptr->uniform_spacing_flag);
    if (!pps_ptr->uniform_spacing_flag) {
        MSG_NO_NEWLINE("column_width_minus1[" << pps_ptr->num_tile_columns_minus1 << "]");
        for (int i = 0; i < pps_ptr->num_tile_columns_minus1; i++) {
            MSG_NO_NEWLINE(" " << pps_ptr->column_width_minus1[i]);
        }
        MSG("");
        MSG_NO_NEWLINE("row_height_minus1[" << pps_ptr->num_tile_rows_minus1 << "]");
        for (int i = 0; i < pps_ptr->num_tile_rows_minus1; i++) {
            MSG_NO_NEWLINE(" " << pps_ptr->row_height_minus1[i]);
        }
        MSG("");
    }
    MSG("loop_filter_across_tiles_enabled_flag       = " <<  pps_ptr->loop_filter_across_tiles_enabled_flag);
    MSG("pps_loop_filter_across_slices_enabled_flag  = " <<  pps_ptr->pps_loop_filter_across_slices_enabled_flag);
    MSG("deblocking_filter_control_present_flag      = " <<  pps_ptr->deblocking_filter_control_present_flag);
    MSG("deblocking_filter_override_enabled_flag     = " <<  pps_ptr->deblocking_filter_override_enabled_flag);
    MSG("pps_deblocking_filter_disabled_flag         = " <<  pps_ptr->pps_deblocking_filter_disabled_flag);
    MSG("pps_beta_offset_div2                        = " <<  pps_ptr->pps_beta_offset_div2);
    MSG("pps_tc_offset_div2                          = " <<  pps_ptr->pps_tc_offset_div2);
    MSG("pps_scaling_list_data_present_flag          = " <<  pps_ptr->pps_scaling_list_data_present_flag);
    MSG("Scaling list:");
    for (int i = 0; i < H265_SCALING_LIST_SIZE_NUM; i++) {
        for (int j = 0; j < H265_SCALING_LIST_NUM; j++) {
            MSG_NO_NEWLINE("scaling_list[" << i <<"][" << j << "][]:")
            for (int k = 0; k < H265_SCALING_LIST_MAX_I; k++) {
                MSG_NO_NEWLINE(" " << pps_ptr->scaling_list_data.scaling_list[i][j][k]);
            }
            MSG("");
        }
    }

    MSG("lists_modification_present_flag             = " <<  pps_ptr->lists_modification_present_flag);
    MSG("log2_parallel_merge_level_minus2            = " <<  pps_ptr->log2_parallel_merge_level_minus2);
    MSG("slice_segment_header_extension_present_flag = " <<  pps_ptr->slice_segment_header_extension_present_flag);
    MSG("pps_extension_present_flag                  = " <<  pps_ptr->pps_extension_present_flag);
    MSG("");
}

void HEVCVideoParser::PrintSliceSegHeader(HEVCVideoParser::SliceHeaderData *slice_header_ptr) {
    MSG("=== hevc_slice_segment_header_t ===");
    MSG("first_slice_segment_in_pic_flag             = " <<  slice_header_ptr->first_slice_segment_in_pic_flag);
    MSG("no_output_of_prior_pics_flag                = " <<  slice_header_ptr->no_output_of_prior_pics_flag);
    MSG("slice_pic_parameter_set_id                  = " <<  slice_header_ptr->slice_pic_parameter_set_id);
    MSG("dependent_slice_segment_flag                = " <<  slice_header_ptr->dependent_slice_segment_flag);
    MSG("slice_segment_address                       = " <<  slice_header_ptr->slice_segment_address);
    MSG("slice_type                                  = " <<  slice_header_ptr->slice_type);
    MSG("pic_output_flag                             = " <<  slice_header_ptr->pic_output_flag);
    MSG("colour_plane_id                             = " <<  slice_header_ptr->colour_plane_id);
    MSG("slice_pic_order_cnt_lsb                     = " <<  slice_header_ptr->slice_pic_order_cnt_lsb);
    MSG("short_term_ref_pic_set_sps_flag             = " <<  slice_header_ptr->short_term_ref_pic_set_sps_flag);
    MSG("short_term_ref_pic_set_idx                  = " <<  slice_header_ptr->short_term_ref_pic_set_idx);

    PrintStRps(&slice_header_ptr->st_rps);

    MSG("num_long_term_sps                           = " <<  slice_header_ptr->num_long_term_sps);
    MSG("num_long_term_pics                          = " <<  slice_header_ptr->num_long_term_pics);
    MSG_NO_NEWLINE("lt_idx_sps[]:");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->lt_idx_sps[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("poc_lsb_lt[]:");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->poc_lsb_lt[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("used_by_curr_pic_lt_flag[]:");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->used_by_curr_pic_lt_flag[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("delta_poc_msb_present_flag[]:");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->delta_poc_msb_present_flag[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("delta_poc_msb_cycle_lt[]:");
    for(int i = 0; i < 32; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->delta_poc_msb_cycle_lt[i]);
    }
    MSG("");

    PrintLtRefInfo(&slice_header_ptr->lt_rps);

    MSG("slice_temporal_mvp_enabled_flag             = " <<  slice_header_ptr->slice_temporal_mvp_enabled_flag);
    MSG("slice_sao_luma_flag                         = " <<  slice_header_ptr->slice_sao_luma_flag);
    MSG("slice_sao_chroma_flag                       = " <<  slice_header_ptr->slice_sao_chroma_flag);

    MSG("num_ref_idx_active_override_flag            = " <<  slice_header_ptr->num_ref_idx_active_override_flag);
    MSG("num_ref_idx_l0_active_minus1                = " <<  slice_header_ptr->num_ref_idx_l0_active_minus1);
    MSG("num_ref_idx_l1_active_minus1                = " <<  slice_header_ptr->num_ref_idx_l1_active_minus1);
    MSG("ref_pic_list_modification_flag_l0           = " <<  slice_header_ptr->ref_pic_list_modification_flag_l0);
    MSG("ref_pic_list_modification_flag_l1           = " <<  slice_header_ptr->ref_pic_list_modification_flag_l1);
    MSG_NO_NEWLINE("list_entry_l0[]:");
    for(int i = 0; i < 16; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->list_entry_l0[i]);
    }
    MSG("");
    MSG_NO_NEWLINE("list_entry_l1[]:");
    for(int i = 0; i < 16; i++) {
        MSG_NO_NEWLINE(" " << slice_header_ptr->list_entry_l1[i]);
    }
    MSG("");
    MSG("mvd_l1_zero_flag                            = " <<  slice_header_ptr->mvd_l1_zero_flag);
    MSG("cabac_init_flag                             = " <<  slice_header_ptr->cabac_init_flag);
    MSG("collocated_from_l0_flag                     = " <<  slice_header_ptr->collocated_from_l0_flag);
    MSG("collocated_ref_idx                          = " <<  slice_header_ptr->collocated_ref_idx);
    MSG("five_minus_max_num_merge_cand               = " <<  slice_header_ptr->five_minus_max_num_merge_cand);
    MSG("slice_qp_delta                              = " <<  slice_header_ptr->slice_qp_delta);
    MSG("slice_cb_qp_offset                          = " <<  slice_header_ptr->slice_cb_qp_offset);
    MSG("slice_cr_qp_offset                          = " <<  slice_header_ptr->slice_cr_qp_offset);
    MSG("cu_chroma_qp_offset_enabled_flag            = " <<  slice_header_ptr->cu_chroma_qp_offset_enabled_flag);
    MSG("deblocking_filter_override_flag             = " <<  slice_header_ptr->deblocking_filter_override_flag);
    MSG("slice_deblocking_filter_disabled_flag       = " <<  slice_header_ptr->slice_deblocking_filter_disabled_flag);
    MSG("slice_beta_offset_div2                      = " <<  slice_header_ptr->slice_beta_offset_div2);
    MSG("slice_tc_offset_div2                        = " <<  slice_header_ptr->slice_tc_offset_div2);
    MSG("slice_loop_filter_across_slices_enabled_flag = " <<  slice_header_ptr->slice_loop_filter_across_slices_enabled_flag);
    MSG("num_entry_point_offsets                     = " <<  slice_header_ptr->num_entry_point_offsets);
    MSG("offset_len_minus1                           = " <<  slice_header_ptr->offset_len_minus1);
    MSG("slice_segment_header_extension_length       = " <<  slice_header_ptr->slice_segment_header_extension_length);
    MSG("");
}

void HEVCVideoParser::PrintStRps(HEVCVideoParser::H265ShortTermRPS *rps_ptr) {
    MSG("==== Short-term reference picture set =====")
    MSG("inter_ref_pic_set_prediction_flag           = " <<  rps_ptr->inter_ref_pic_set_prediction_flag);
    MSG("delta_idx_minus1                            = " <<  rps_ptr->delta_idx_minus1);
    MSG("delta_rps_sign                              = " <<  rps_ptr->delta_rps_sign);
    MSG("abs_delta_rps_minus1                        = " <<  rps_ptr->abs_delta_rps_minus1);
    MSG_NO_NEWLINE("rps->used_by_curr_pic_flag[]:");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->used_by_curr_pic_flag[j]);
    }
    MSG("");
    MSG_NO_NEWLINE("use_delta_flag[]:");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->use_delta_flag[j]);
    }
    MSG("");
    MSG("num_negative_pics                           = " <<  rps_ptr->num_negative_pics);
    MSG("num_positive_pics                           = " <<  rps_ptr->num_positive_pics);
    MSG("num_of_pics                                 = " <<  rps_ptr->num_of_pics);
    MSG("num_of_delta_poc                            = " <<  rps_ptr->num_of_delta_poc);

    MSG_NO_NEWLINE("delta_poc_s0_minus1[]:");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->delta_poc_s0_minus1[j]);
    }
    MSG("");
    MSG_NO_NEWLINE("used_by_curr_pic_s0_flag[]:");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->used_by_curr_pic_s0_flag[j]);
    }
    MSG("");
    MSG_NO_NEWLINE("delta_poc_s1_minus1[]:");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->delta_poc_s1_minus1[j]);
    }
    MSG("");
    MSG_NO_NEWLINE("used_by_curr_pic_s1_flag[]:");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->used_by_curr_pic_s1_flag[j]);
    }
    MSG("");

    MSG_NO_NEWLINE("delta_poc[16] (DeltaPocS0 + DeltaPocS1):");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->delta_poc[j]);
    }
    MSG("");
    MSG_NO_NEWLINE("used_by_curr_pic[16] (UsedByCurrPicS0 + UsedByCurrPicS1):");
    for(int j = 0; j < 16; j++) {
        MSG_NO_NEWLINE(" " << rps_ptr->used_by_curr_pic[j]);
    }
    MSG("");
}

void HEVCVideoParser::PrintLtRefInfo(HEVCVideoParser::H265LongTermRPS *lt_info_ptr) {
    MSG("==== Long-term reference picture info =====");
    MSG("num_of_pics                 = " <<  lt_info_ptr->num_of_pics);
    MSG_NO_NEWLINE("pocs[]:");
    for(int j = 0; j < 32; j++) {
        MSG_NO_NEWLINE(" " << lt_info_ptr->pocs[j]);
    }
    MSG("");
    MSG_NO_NEWLINE("used_by_curr_pic[]:");
    for(int j = 0; j < 32; j++) {
        MSG_NO_NEWLINE(" " << lt_info_ptr->used_by_curr_pic[j]);
    }
    MSG("");
}

void HEVCVideoParser::PrintSeiMessage(HEVCVideoParser::SeiMessageData *sei_message_ptr) {
    MSG("=== hevc_sei_message_info ===");
    MSG("payload_type               = " <<  sei_message_ptr->payload_type);
    MSG("payload_size               = " <<  sei_message_ptr->payload_size);
    MSG_NO_NEWLINE("reserved[3]: ");
    for(int i = 0; i < 3; i++) {
        MSG_NO_NEWLINE(" " << sei_message_ptr->reserved[i]);
    }
    MSG("");
}
#endif // DBGINFO