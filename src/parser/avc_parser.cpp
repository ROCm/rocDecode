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

#include <algorithm>
#include "avc_parser.h"

AvcVideoParser::AvcVideoParser() {
    prev_pic_order_cnt_msb_ = 0;
    prev_pic_order_cnt_lsb_ = 0;
    prev_top_field_order_cnt_ = 0;
    prev_frame_num_offset_t = 0;
    prev_frame_num_ = 0;
    prev_has_mmco_5_ = 0;
    curr_has_mmco_5_ = 0;
    prev_ref_pic_bottom_field_ = 0;
    curr_ref_pic_bottom_field_ = 0;
}

AvcVideoParser::~AvcVideoParser() {
}

rocDecStatus AvcVideoParser::Initialize(RocdecParserParams *p_params) {
    return RocVideoParser::Initialize(p_params);
}

rocDecStatus AvcVideoParser::UnInitialize() {
    return ROCDEC_SUCCESS;
}

rocDecStatus AvcVideoParser::ParseVideoData(RocdecSourceDataPacket *p_data) {
    if (p_data->payload && p_data->payload_size) {
        if (ParsePictureData(p_data->payload, p_data->payload_size) != PARSER_OK) {
            ERR(STR("Parser failed!"));
            return ROCDEC_RUNTIME_ERROR;
        }

        // Init Roc decoder for the first time or reconfigure the existing decoder
        if (new_sps_activated_) {
            if (NotifyNewSps(&sps_list_[active_sps_id_]) != PARSER_OK) {
                return ROCDEC_RUNTIME_ERROR;
            }
            new_sps_activated_ = false;
        }

        // Decode the picture
        if (SendPicForDecode() != PARSER_OK) {
            ERR(STR("Failed to decode!"));
            return ROCDEC_RUNTIME_ERROR;
        }

        pic_count_++;
    } else if (!(p_data->flags & ROCDEC_PKT_ENDOFSTREAM)) {
        // If no payload and EOS is not set, treated as invalid.
        return ROCDEC_INVALID_PARAMETER;
    }

    return ROCDEC_SUCCESS;
}

ParserResult AvcVideoParser::ParsePictureData(const uint8_t *p_stream, uint32_t pic_data_size) {
    ParserResult ret = PARSER_OK;

    pic_data_buffer_ptr_ = (uint8_t*)p_stream;
    pic_data_size_ = pic_data_size;
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
            return ret;
        }

        // Parse the NAL unit
        if (nal_unit_size_) {
            // start code + NAL unit header = 4 bytes
            int ebsp_size = nal_unit_size_ - 4 > RBSP_BUF_SIZE ? RBSP_BUF_SIZE : nal_unit_size_ - 4; // only copy enough bytes for header parsing

            nal_unit_header_ = ParseNalUnitHeader(pic_data_buffer_ptr_[curr_start_code_offset_ + 3]);
            switch (nal_unit_header_.nal_unit_type) {
                case kAvcNalTypeSeq_Parameter_Set: {
                    memcpy(rbsp_buf_, (pic_data_buffer_ptr_ + curr_start_code_offset_ + 4), ebsp_size);
                    rbsp_size_ = EbspToRbsp(rbsp_buf_, 0, ebsp_size);
                    ParseSps(rbsp_buf_, rbsp_size_);
                    break;
                }

                case kAvcNalTypePic_Parameter_Set: {
                    memcpy(rbsp_buf_, (pic_data_buffer_ptr_ + curr_start_code_offset_ + 4), ebsp_size);
                    rbsp_size_ = EbspToRbsp(rbsp_buf_, 0, ebsp_size);
                    ParsePps(rbsp_buf_, rbsp_size_);
                    break;
                }
                
                case kAvcNalTypeSlice_IDR:
                case kAvcNalTypeSlice_Non_IDR:
                case kAvcNalTypeSlice_Data_Partition_A:
                case kAvcNalTypeSlice_Data_Partition_B:
                case kAvcNalTypeSlice_Data_Partition_C: {
                    memcpy(rbsp_buf_, (pic_data_buffer_ptr_ + curr_start_code_offset_ + 4), ebsp_size);
                    rbsp_size_ = EbspToRbsp(rbsp_buf_, 0, ebsp_size);
                    // For each picture, only parse the first slice header
                    if (slice_num_ == 0) {
                        // Save slice NAL unit header
                        slice_nal_unit_header_ = nal_unit_header_;

                        // Use the data directly from demuxer without copying
                        pic_stream_data_ptr_ = pic_data_buffer_ptr_ + curr_start_code_offset_;
                        // Picture stream data size is calculated as the diff between the frame end and the first slice offset.
                        // This is to consider the possibility of non-slice NAL units between slices.
                        pic_stream_data_size_ = pic_data_size - curr_start_code_offset_;

                        ParseSliceHeader(rbsp_buf_, rbsp_size_, &slice_header_0_);

                        // Start decode process
                        // Set current picture properties
                        CalculateCurrPoc(); // 8.2.1
                        prev_has_mmco_5_ = curr_has_mmco_5_;
                        prev_ref_pic_bottom_field_ = curr_ref_pic_bottom_field_;
                        if (slice_header_0_.field_pic_flag) {
                            if (slice_header_0_.bottom_field_flag) {
                                curr_pic_.pic_structure = kBottomField;
                            } else {
                                curr_pic_.pic_structure = kTopField;
                            }
                        } else {
                            curr_pic_.pic_structure = kFrame;
                        }
                        curr_pic_.frame_num = slice_header_0_.frame_num;
                    }
                    slice_num_++;
                    break;
                }

                case kAvcNalTypeSEI_Info: {
                    if (pfn_get_sei_message_cb_) {
                        // Todo
                    }
                    break;
                }

                case kAvcNalTypeEnd_Of_Seq: {
                    break;
                }

                case kAvcNalTypeEnd_Of_Stream: {
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

    return PARSER_OK;
}

ParserResult AvcVideoParser::NotifyNewSps(AvcSeqParameterSet *p_sps) {
    video_format_params_.codec = rocDecVideoCodec_AVC;
    video_format_params_.frame_rate.numerator = frame_rate_.numerator;
    video_format_params_.frame_rate.denominator = frame_rate_.denominator;
    video_format_params_.bit_depth_luma_minus8 = p_sps->bit_depth_luma_minus8;
    video_format_params_.bit_depth_chroma_minus8 = p_sps->bit_depth_chroma_minus8;
    video_format_params_.progressive_sequence = p_sps->frame_mbs_only_flag ? 1 : 0;
    video_format_params_.min_num_decode_surfaces = dpb_buffer_.dpb_size;
    video_format_params_.coded_width = pic_width_;
    video_format_params_.coded_height = pic_height_;
    video_format_params_.chroma_format = static_cast<rocDecVideoChromaFormat>(p_sps->chroma_format_idc);

    // Table 6-1
    int sub_width_c, sub_height_c;
    switch (video_format_params_.chroma_format) {
        case rocDecVideoChromaFormat_Monochrome: {
            sub_width_c = 0; // not defined
            sub_height_c = 0; // not defined
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
            if (p_sps->separate_colour_plane_flag) {
                sub_width_c = 0; // not defined
                sub_height_c = 0; // not defined
            } else {
                sub_width_c = 1;
                sub_height_c = 1;
            }
            break;
        }
        default:
            ERR(STR("Error: Sequence Callback function - Chroma Format is not supported"));
            return PARSER_FAIL;
    }
    int chroma_array_type = p_sps->separate_colour_plane_flag ? 0 : p_sps->chroma_format_idc;
    int crop_unit_x, crop_unit_y;
    if (chroma_array_type == 0) {
        crop_unit_x = 1; // (7-19)
        crop_unit_y = 2 - p_sps->frame_mbs_only_flag; // (7-20)
    } else {
        crop_unit_x = sub_width_c; // (7-21)
        crop_unit_y = sub_height_c * (2 - p_sps->frame_mbs_only_flag); // (7-22)
    }
    if (p_sps->frame_cropping_flag) {
        video_format_params_.display_area.left = crop_unit_x * p_sps->frame_crop_left_offset;
        video_format_params_.display_area.top = crop_unit_y * p_sps->frame_crop_top_offset;
        video_format_params_.display_area.right = pic_width_ - (crop_unit_x * p_sps->frame_crop_right_offset);
        video_format_params_.display_area.bottom = pic_height_ - (crop_unit_y * p_sps->frame_crop_bottom_offset);
    }  else { // default values
        video_format_params_.display_area.left = 0;
        video_format_params_.display_area.top = 0;
        video_format_params_.display_area.right = pic_width_;
        video_format_params_.display_area.bottom = pic_height_;
    }

    video_format_params_.bitrate = 0;

    // Dispaly aspect ratio
    // Table E-1.
    static const Rational avc_sar[] = {
        {0, 0}, // unspecified
        {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
        {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, {4, 3}, {3, 2}, {2, 1},
    };
    Rational sar;
    sar.numerator = 1; // set to square pixel if not present or unspecified
    sar.denominator = 1; // set to square pixel if not present or unspecified
    if (p_sps->vui_parameters_present_flag) {
        if (p_sps->vui_seq_parameters.aspect_ratio_info_present_flag) {
            if (p_sps->vui_seq_parameters.aspect_ratio_idc == 255 /*Extended_SAR*/) {
                sar.numerator = p_sps->vui_seq_parameters.sar_width;
                sar.denominator = p_sps->vui_seq_parameters.sar_height;
            } else if (p_sps->vui_seq_parameters.aspect_ratio_idc > 0 && p_sps->vui_seq_parameters.aspect_ratio_idc < 17) {
                sar = avc_sar[p_sps->vui_seq_parameters.aspect_ratio_idc];
            }
        }
    }
    int disp_width = (video_format_params_.display_area.right - video_format_params_.display_area.left) * sar.numerator;
    int disp_height = (video_format_params_.display_area.bottom - video_format_params_.display_area.top) * sar.denominator;
    int gcd = std::__gcd(disp_width, disp_height); // greatest common divisor
    video_format_params_.display_aspect_ratio.x = disp_width / gcd;
    video_format_params_.display_aspect_ratio.y = disp_height / gcd;

    if (p_sps->vui_parameters_present_flag) {
        video_format_params_.video_signal_description.video_format = p_sps->vui_seq_parameters.video_format;
        video_format_params_.video_signal_description.video_full_range_flag = p_sps->vui_seq_parameters.video_full_range_flag;
        video_format_params_.video_signal_description.color_primaries = p_sps->vui_seq_parameters.colour_primaries;
        video_format_params_.video_signal_description.transfer_characteristics = p_sps->vui_seq_parameters.transfer_characteristics;
        video_format_params_.video_signal_description.matrix_coefficients = p_sps->vui_seq_parameters.matrix_coefficients;
        video_format_params_.video_signal_description.reserved_zero_bits = 0;
    }
    video_format_params_.seqhdr_data_length = 0;

    // callback function with RocdecVideoFormat params filled out
    if (pfn_sequece_cb_(parser_params_.user_data, &video_format_params_) == 0) {
        ERR("Sequence callback function failed.");
        return PARSER_FAIL;
    } else {
        return PARSER_OK;
    }
}

ParserResult AvcVideoParser::SendPicForDecode() {
    int i, j;
    AvcSeqParameterSet *p_sps = &sps_list_[active_sps_id_];
    AvcPicParameterSet *p_pps = &pps_list_[active_pps_id_];
    AvcSliceHeader *p_slice_header = &slice_header_0_;
    dec_pic_params_ = {0};

    dec_pic_params_.pic_width = pic_width_;
    dec_pic_params_.pic_height = pic_height_;
    dec_pic_params_.curr_pic_idx = curr_pic_.pic_idx;
    dec_pic_params_.field_pic_flag = p_slice_header->field_pic_flag;
    dec_pic_params_.bottom_field_flag = p_slice_header->bottom_field_flag;
    if (p_slice_header->field_pic_flag) {
        dec_pic_params_.second_field = (pic_count_ & 1) ? 1 : 0;
    } else {
        dec_pic_params_.second_field = 1;
    }

    dec_pic_params_.bitstream_data_len = pic_stream_data_size_;
    dec_pic_params_.bitstream_data = pic_stream_data_ptr_;
    dec_pic_params_.num_slices = slice_num_;
    dec_pic_params_.slice_data_offsets = nullptr;

    dec_pic_params_.ref_pic_flag = slice_nal_unit_header_.nal_ref_idc;
    dec_pic_params_.intra_pic_flag = p_slice_header->slice_type == kAvcSliceTypeI || p_slice_header->slice_type == kAvcSliceTypeI_7 || p_slice_header->slice_type == kAvcSliceTypeSI || p_slice_header->slice_type == kAvcSliceTypeSI_9;

    // Set up the picture parameter buffer
    RocdecAvcPicParams *p_pic_param = &dec_pic_params_.pic_params.avc;

    // Current picture
    p_pic_param->curr_pic.pic_idx = curr_pic_.pic_idx;
    if (curr_pic_.is_reference == kUsedForLongTerm) {
        p_pic_param->curr_pic.frame_idx = curr_pic_.long_term_pic_num;
    } else {
        p_pic_param->curr_pic.frame_idx = curr_pic_.frame_num;
    }
    p_pic_param->curr_pic.flags = 0;
    if (curr_pic_.pic_structure != kFrame) {
        p_pic_param->curr_pic.flags |= curr_pic_.pic_structure == kBottomField ? RocdecAvcPicture_FLAGS_BOTTOM_FIELD : RocdecAvcPicture_FLAGS_TOP_FIELD;
    }
    if (curr_pic_.is_reference != kUnusedForReference) {
        p_pic_param->curr_pic.flags |= curr_pic_.is_reference == kUsedForShortTerm ? RocdecAvcPicture_FLAGS_SHORT_TERM_REFERENCE : RocdecAvcPicture_FLAGS_LONG_TERM_REFERENCE;
    }
    p_pic_param->curr_pic.top_field_order_cnt = curr_pic_.top_field_order_cnt;
    p_pic_param->curr_pic.bottom_field_order_cnt = curr_pic_.bottom_field_order_cnt;

    // Reference pictures
    int buf_index = 0;
    for (i = 0; i < AVC_MAX_DPB_FRAMES; i++) {
        AvcPicture *p_ref_pic = &dpb_buffer_.frame_buffer_list[i];
        if (p_ref_pic->is_reference != kUnusedForReference) {
            p_pic_param->ref_frames[buf_index].pic_idx = p_ref_pic->pic_idx;
            if ( p_ref_pic->is_reference == kUsedForLongTerm) {
                p_pic_param->ref_frames[buf_index].frame_idx = p_ref_pic->long_term_pic_num;
            } else {
                p_pic_param->ref_frames[buf_index].frame_idx = p_ref_pic->frame_num;
            }
            p_pic_param->ref_frames[buf_index].flags = 0;
            if (p_ref_pic->pic_structure != kFrame) {
                p_pic_param->ref_frames[buf_index].flags |= p_ref_pic->pic_structure == kBottomField ? RocdecAvcPicture_FLAGS_BOTTOM_FIELD : RocdecAvcPicture_FLAGS_TOP_FIELD;
            }
            p_pic_param->ref_frames[buf_index].flags |= p_ref_pic->is_reference == kUsedForShortTerm ? RocdecAvcPicture_FLAGS_SHORT_TERM_REFERENCE : RocdecAvcPicture_FLAGS_LONG_TERM_REFERENCE;
            buf_index++;
        }
    }

    for (i = buf_index; i < AVC_MAX_DPB_FRAMES; i++) {
        p_pic_param->ref_frames[i].pic_idx = 0xFF;
    }

    p_pic_param->picture_width_in_mbs_minus1 = p_sps->pic_width_in_mbs_minus1;
    p_pic_param->picture_height_in_mbs_minus1 = (2 - p_sps->frame_mbs_only_flag) * (p_sps->pic_height_in_map_units_minus1 + 1) - 1;
    p_pic_param->bit_depth_luma_minus8 = p_sps->bit_depth_luma_minus8;
    p_pic_param->bit_depth_chroma_minus8 = p_sps->bit_depth_chroma_minus8;
    p_pic_param->num_ref_frames = p_sps->max_num_ref_frames;

    p_pic_param->seq_fields.bits.chroma_format_idc = p_sps->chroma_format_idc;
    p_pic_param->seq_fields.bits.residual_colour_transform_flag = p_sps->separate_colour_plane_flag;
    p_pic_param->seq_fields.bits.gaps_in_frame_num_value_allowed_flag = p_sps->gaps_in_frame_num_value_allowed_flag;
    p_pic_param->seq_fields.bits.frame_mbs_only_flag = p_sps->frame_mbs_only_flag;
    p_pic_param->seq_fields.bits.mb_adaptive_frame_field_flag = p_sps->mb_adaptive_frame_field_flag;
    p_pic_param->seq_fields.bits.direct_8x8_inference_flag = p_sps->direct_8x8_inference_flag;
    p_pic_param->seq_fields.bits.MinLumaBiPredSize8x8 = p_sps->level_idc >= 31;  // A.3.3.2
    p_pic_param->seq_fields.bits.log2_max_frame_num_minus4 = p_sps->log2_max_frame_num_minus4;
    p_pic_param->seq_fields.bits.pic_order_cnt_type = p_sps->pic_order_cnt_type;
    p_pic_param->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = p_sps->log2_max_pic_order_cnt_lsb_minus4;
    p_pic_param->seq_fields.bits.delta_pic_order_always_zero_flag = p_sps->delta_pic_order_always_zero_flag;

    p_pic_param->pic_init_qp_minus26 = p_pps->pic_init_qp_minus26;
    p_pic_param->pic_init_qs_minus26 = p_pps->pic_init_qs_minus26;
    p_pic_param->chroma_qp_index_offset = p_pps->chroma_qp_index_offset;
    p_pic_param->second_chroma_qp_index_offset = p_pps->second_chroma_qp_index_offset;

    p_pic_param->pic_fields.bits.entropy_coding_mode_flag = p_pps->entropy_coding_mode_flag;
    p_pic_param->pic_fields.bits.weighted_pred_flag = p_pps->weighted_pred_flag;
    p_pic_param->pic_fields.bits.weighted_bipred_idc = p_pps->weighted_bipred_idc;
    p_pic_param->pic_fields.bits.transform_8x8_mode_flag = p_pps->transform_8x8_mode_flag;
    p_pic_param->pic_fields.bits.field_pic_flag = p_slice_header->field_pic_flag;
    p_pic_param->pic_fields.bits.constrained_intra_pred_flag = p_pps->constrained_intra_pred_flag;
    p_pic_param->pic_fields.bits.pic_order_present_flag = p_pps->bottom_field_pic_order_in_frame_present_flag;
    p_pic_param->pic_fields.bits.deblocking_filter_control_present_flag = p_pps->deblocking_filter_control_present_flag;
    p_pic_param->pic_fields.bits.redundant_pic_cnt_present_flag = p_pps->redundant_pic_cnt_present_flag;
    p_pic_param->pic_fields.bits.reference_pic_flag = slice_nal_unit_header_.nal_ref_idc != 0;

    p_pic_param->frame_num = p_slice_header->frame_num;

    // Set up slice parameters
    RocdecAvcSliceParams *p_slice_param = &dec_pic_params_.slice_params.avc;

    p_slice_param->slice_data_size = pic_stream_data_size_;
    p_slice_param->slice_data_offset = 0;
    p_slice_param->slice_data_flag = 0; // VA_SLICE_DATA_FLAG_ALL;
    p_slice_param->slice_data_bit_offset = 0;
    p_slice_param->first_mb_in_slice = p_slice_header->first_mb_in_slice;
    p_slice_param->slice_type = p_slice_header->slice_type;
    p_slice_param->direct_spatial_mv_pred_flag = p_slice_header->direct_spatial_mv_pred_flag;
    p_slice_param->num_ref_idx_l0_active_minus1 = p_slice_header->num_ref_idx_l0_active_minus1;
    p_slice_param->num_ref_idx_l1_active_minus1 = p_slice_header->num_ref_idx_l1_active_minus1;
    p_slice_param->cabac_init_idc = p_slice_header->cabac_init_idc;
    p_slice_param->slice_qp_delta = p_slice_header->slice_qp_delta;
    p_slice_param->disable_deblocking_filter_idc = p_slice_header->disable_deblocking_filter_idc;
    p_slice_param->slice_alpha_c0_offset_div2 = p_slice_header->slice_alpha_c0_offset_div2;
    p_slice_param->slice_beta_offset_div2 = p_slice_header->slice_beta_offset_div2;
    p_slice_param->luma_log2_weight_denom = p_slice_header->pred_weight_table.luma_log2_weight_denom;
    p_slice_param->chroma_log2_weight_denom = p_slice_header->pred_weight_table.chroma_log2_weight_denom;

    // Ref lists
    for (i = 0; i < dpb_buffer_.num_short_term + dpb_buffer_.num_long_term; i++) {
        AvcPicture *p_ref_pic = &ref_list_0_[i];
        if (p_ref_pic->is_reference != kUnusedForReference) {
            p_slice_param->ref_pic_list_0[i].pic_idx = p_ref_pic->pic_idx;
            if ( p_ref_pic->is_reference == kUsedForLongTerm) {
                p_slice_param->ref_pic_list_0[i].frame_idx = p_ref_pic->long_term_pic_num;
            } else {
                p_slice_param->ref_pic_list_0[i].frame_idx = p_ref_pic->frame_num;
            }
            p_slice_param->ref_pic_list_0[i].flags = 0;
            if (p_ref_pic->pic_structure != kFrame) {
                p_slice_param->ref_pic_list_0[i].flags |= p_ref_pic->pic_structure == kBottomField ? RocdecAvcPicture_FLAGS_BOTTOM_FIELD : RocdecAvcPicture_FLAGS_TOP_FIELD;
            }
            p_slice_param->ref_pic_list_0[i].flags |= p_ref_pic->is_reference == kUsedForShortTerm ? RocdecAvcPicture_FLAGS_SHORT_TERM_REFERENCE : RocdecAvcPicture_FLAGS_LONG_TERM_REFERENCE;
        }
    }

    if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6 ) {
        AvcPicture *p_ref_pic = &ref_list_1_[i];
        if (p_ref_pic->is_reference != kUnusedForReference) {
            p_slice_param->ref_pic_list_1[i].pic_idx = p_ref_pic->pic_idx;
            if ( p_ref_pic->is_reference == kUsedForLongTerm) {
                p_slice_param->ref_pic_list_1[i].frame_idx = p_ref_pic->long_term_pic_num;
            } else {
                p_slice_param->ref_pic_list_1[i].frame_idx = p_ref_pic->frame_num;
            }
            p_slice_param->ref_pic_list_1[i].flags = 0;
            if (p_ref_pic->pic_structure != kFrame) {
                p_slice_param->ref_pic_list_1[i].flags |= p_ref_pic->pic_structure == kBottomField ? RocdecAvcPicture_FLAGS_BOTTOM_FIELD : RocdecAvcPicture_FLAGS_TOP_FIELD;
            }
            p_slice_param->ref_pic_list_1[i].flags |= p_ref_pic->is_reference == kUsedForShortTerm ? RocdecAvcPicture_FLAGS_SHORT_TERM_REFERENCE : RocdecAvcPicture_FLAGS_LONG_TERM_REFERENCE;
        }
    }

    // Prediction weight table
    // Note luma_weight_l0_flag should be an array. Set it using the first one in the table.
    p_slice_param->luma_weight_l0_flag = p_slice_header->pred_weight_table.weight_factor[0].luma_weight_l0_flag;
    for (i = 0; i <= p_slice_header->num_ref_idx_l0_active_minus1; i++) {
        p_slice_param->luma_weight_l0[i] = p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l0;
        p_slice_param->luma_offset_l0[i] = p_slice_header->pred_weight_table.weight_factor[i].luma_offset_l0;
    }

    // Note chroma_weight_l0_flag should be an array. Set it using the first one in the table.
    p_slice_param->chroma_weight_l0_flag = p_slice_header->pred_weight_table.weight_factor[0].chroma_weight_l0_flag;
    for (i = 0; i <= p_slice_header->num_ref_idx_l0_active_minus1; i++) {
        for (j = 0; j < 2; j++) {
            p_slice_param->chroma_weight_l0[i][j] = p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l0[j];
            p_slice_param->chroma_offset_l0[i][j] = p_slice_header->pred_weight_table.weight_factor[i].chroma_offset_l0[j];
        }
    }
    if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6 ) {
        // Note luma_weight_l1_flag should be an array. Set it using the first one in the table.
        p_slice_param->luma_weight_l1_flag = p_slice_header->pred_weight_table.weight_factor[0].luma_weight_l1_flag;
        for (i = 0; i <= p_slice_header->num_ref_idx_l1_active_minus1; i++) {
            p_slice_param->luma_weight_l1[i] = p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l1;
            p_slice_param->luma_offset_l1[i] = p_slice_header->pred_weight_table.weight_factor[i].luma_offset_l1;
        }
        // Note chroma_weight_l0_flag should be an array. Set it using the first one in the table.
        p_slice_param->chroma_weight_l1_flag = p_slice_header->pred_weight_table.weight_factor[0].chroma_weight_l1_flag;
        for (i = 0; i <= p_slice_header->num_ref_idx_l1_active_minus1; i++) {
            for (j = 0; j < 2; j++) {
                p_slice_param->chroma_weight_l1[i][j] = p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l1[j];
                p_slice_param->chroma_offset_l1[i][j] = p_slice_header->pred_weight_table.weight_factor[i].chroma_offset_l1[j];
            }
        }
    }

    // Set up scaling lists
    RocdecAvcIQMatrix *p_iq_matrix = &dec_pic_params_.iq_matrix.avc;
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 16; j++) {
            p_iq_matrix->scaling_list_4x4[i][j] = p_pps->scaling_list_4x4[i][j];
        }
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 64; j++) {
            p_iq_matrix->scaling_list_8x8[i][j] = p_pps->scaling_list_8x8[i][j];
        }
    }

    if (pfn_decode_picture_cb_(parser_params_.user_data, &dec_pic_params_) == 0) {
        ERR("Decode error occurred.");
        return PARSER_FAIL;
    } else {
        return PARSER_OK;
    }
}

AvcNalUnitHeader AvcVideoParser::ParseNalUnitHeader(uint8_t header_byte) {
    size_t bit_offset = 0;
    AvcNalUnitHeader nal_header;

    nal_header.forbidden_zero_bit = Parser::GetBit(&header_byte, bit_offset);
    nal_header.nal_ref_idc = Parser::ReadBits(&header_byte, bit_offset, 2);
    nal_header.nal_unit_type = Parser::ReadBits(&header_byte, bit_offset, 5);
    return nal_header;
}

const int Flat_4x4_16[16] = {
    16,16,16,16,
    16,16,16,16,
    16,16,16,16,
    16,16,16,16
};

const int Flat_8x8_16[64] = {
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16
};

const int Default_4x4_Intra[16] = {
    6, 13, 13, 20,
    20, 20, 28, 28,
    28, 28, 32, 32,
    32, 37, 37, 42
};

const int Default_4x4_Inter[16] = {
    10, 14, 14, 20,
    20, 20, 24, 24,
    24, 24, 27, 27,
    27, 30, 30, 34
};

const int Default_8x8_Intra[64] = {
    6, 10, 10, 13, 11, 13, 16, 16,
    16, 16, 18, 18, 18, 18, 18, 23,
    23, 23, 23, 23, 23, 25, 25, 25,
    25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29,
    29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36,
    36, 36, 38, 38, 38, 40, 40, 42
};

const int Default_8x8_Inter[64] = {
    9, 13, 13, 15, 13, 15, 17, 17,
    17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22,
    22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25,
    25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30,
    30, 30, 32, 32, 32, 33, 33, 35
};

void AvcVideoParser::ParseSps(uint8_t *p_stream, size_t size) {
    size_t offset = 0;  // current bit offset
    AvcSeqParameterSet *p_sps = nullptr;

    // Parse and temporarily store till set id
    uint32_t profile_idc = Parser::ReadBits(p_stream, offset, 8);
    uint32_t constraint_set0_flag = Parser::GetBit(p_stream, offset);
    uint32_t constraint_set1_flag = Parser::GetBit(p_stream, offset);
    uint32_t constraint_set2_flag = Parser::GetBit(p_stream, offset);
    uint32_t constraint_set3_flag = Parser::GetBit(p_stream, offset);
    uint32_t constraint_set4_flag = Parser::GetBit(p_stream, offset);
    uint32_t constraint_set5_flag = Parser::GetBit(p_stream, offset);
    uint32_t reserved_zero_2bits = Parser::ReadBits(p_stream, offset, 2);
    uint32_t level_idc = Parser::ReadBits(p_stream, offset, 8);
    uint32_t seq_parameter_set_id = Parser::ExpGolomb::ReadUe(p_stream, offset);

    p_sps = &sps_list_[seq_parameter_set_id];
    memset(p_sps, 0, sizeof(AvcSeqParameterSet));

    p_sps->profile_idc = profile_idc;
    p_sps->constraint_set0_flag = constraint_set0_flag;
    p_sps->constraint_set1_flag = constraint_set1_flag;
    p_sps->constraint_set2_flag = constraint_set2_flag;
    p_sps->constraint_set3_flag = constraint_set3_flag;
    p_sps->constraint_set4_flag = constraint_set4_flag;
    p_sps->constraint_set5_flag = constraint_set5_flag;
    p_sps->reserved_zero_2bits = reserved_zero_2bits;
    p_sps->level_idc = level_idc;
    p_sps->seq_parameter_set_id = seq_parameter_set_id;

    if (p_sps->profile_idc == 100 || 
        p_sps->profile_idc == 110 ||
        p_sps->profile_idc == 122 || 
        p_sps->profile_idc == 244 ||
        p_sps->profile_idc == 44  ||
        p_sps->profile_idc == 83  ||
        p_sps->profile_idc == 86  ||
        p_sps->profile_idc == 118 ||
        p_sps->profile_idc == 128 ||
        p_sps->profile_idc == 138 ||
        p_sps->profile_idc == 139 ||
        p_sps->profile_idc == 134 ||
        p_sps->profile_idc == 135) {
        p_sps->chroma_format_idc = Parser::ExpGolomb::ReadUe(p_stream, offset);
        if (p_sps->chroma_format_idc == 3) {
            p_sps->separate_colour_plane_flag = Parser::GetBit(p_stream, offset);
        }
        
        p_sps->bit_depth_luma_minus8 = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_sps->bit_depth_chroma_minus8 = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_sps->qpprime_y_zero_transform_bypass_flag = Parser::GetBit(p_stream, offset);
        p_sps->seq_scaling_matrix_present_flag = Parser::GetBit(p_stream, offset);
        if (p_sps->seq_scaling_matrix_present_flag == 1) {
            for (int i = 0; i < ((p_sps->chroma_format_idc != 3) ? 8 : 12); i++) {
                p_sps->seq_scaling_list_present_flag[i] = Parser::GetBit(p_stream, offset);
                if (p_sps->seq_scaling_list_present_flag[i] == 1) {
                    if ( i < 6 ) {
                        GetScalingList(p_stream, offset, p_sps->scaling_list_4x4[i], 16, &p_sps->use_default_scaling_matrix_4x4_flag[i]);
                    } else {
                        GetScalingList(p_stream, offset, p_sps->scaling_list_8x8[i - 6], 64, &p_sps->use_default_scaling_matrix_8x8_flag[i - 6]);
                    }
                }
            }
        }
    } else {
        p_sps->chroma_format_idc = 1;
    }

    // Setup default scaling list if needed
    if (p_sps->seq_scaling_matrix_present_flag == 0) {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 16; j++) {
                p_sps->scaling_list_4x4[i][j] = Flat_4x4_16[j];
            }
        }
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 64; j++) {
                p_sps->scaling_list_8x8[i][j] = Flat_8x8_16[j];
            }
        }
    } else {
        // 4 x 4
        for (int i = 0; i < 6; i++) {
            if (!p_sps->seq_scaling_list_present_flag[i]) { // fall back rule set A
                if (i == 0) {
                    for (int j = 0; j < 16; j++) {
                        p_sps->scaling_list_4x4[i][j] = Default_4x4_Intra[j];
                    }
                } else if (i == 3) {
                    for (int j = 0; j < 16; j++) {
                        p_sps->scaling_list_4x4[i][j] = Default_4x4_Inter[j];
                    }
                } else {
                    for (int j = 0; j < 16; j++) {
                        p_sps->scaling_list_4x4[i][j] = p_sps->scaling_list_4x4[i - 1][j];
                    }
                }
            } else {
                if (p_sps->use_default_scaling_matrix_4x4_flag[i]) {
                    if (i < 3) {
                        for (int j = 0; j < 16; j++) {
                            p_sps->scaling_list_4x4[i][j] = Default_4x4_Intra[j];
                        }
                    } else {
                        for (int j = 0; j < 16; j++) {
                            p_sps->scaling_list_4x4[i][j] = Default_4x4_Inter[j];
                        }
                    }
                }
            }
        }
        
        // 8 x 8
        for (int i = 0; i < 6; i++) {
            if(!p_sps->seq_scaling_list_present_flag[i + 6]) { // fall back rule set A
                if (i == 0) {
                    for (int j = 0; j < 64; j++) {
                        p_sps->scaling_list_8x8[i][j] = Default_8x8_Intra[j];
                    }
                } else if (i == 1) {
                    for (int j = 0; j < 64; j++) {
                        p_sps->scaling_list_8x8[i][j] = Default_8x8_Inter[j];
                    }
                } else {
                    for (int j = 0; j < 64; j++) {
                        p_sps->scaling_list_8x8[i][j] = p_sps->scaling_list_8x8[i - 2][j];
                    }
                }
            } else {
                if (p_sps->use_default_scaling_matrix_8x8_flag[i]) {
                    if (i % 2 == 0) {
                        for (int j = 0; j < 64; j++) {
                            p_sps->scaling_list_8x8[i][j] = Default_8x8_Intra[j];
                        }
                    } else {
                        for (int j = 0; j < 64; j++) {
                            p_sps->scaling_list_8x8[i][j] = Default_8x8_Inter[j];
                        }
                    }
                }
            }
        }
    }

    p_sps->log2_max_frame_num_minus4 = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_sps->pic_order_cnt_type = Parser::ExpGolomb::ReadUe(p_stream, offset);
    if (p_sps->pic_order_cnt_type == 0 ) {
        p_sps->log2_max_pic_order_cnt_lsb_minus4 = Parser::ExpGolomb::ReadUe(p_stream, offset);
        //max_pic_order_cnt_lsb = pow(2, p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
    } else if (p_sps->pic_order_cnt_type == 1) {
        p_sps->delta_pic_order_always_zero_flag = Parser::GetBit(p_stream, offset);
        p_sps->offset_for_non_ref_pic = Parser::ExpGolomb::ReadSe(p_stream, offset);
        p_sps->offset_for_top_to_bottom_field = Parser::ExpGolomb::ReadSe(p_stream, offset);
        p_sps->num_ref_frames_in_pic_order_cnt_cycle = Parser::ExpGolomb::ReadUe(p_stream, offset);
        for (int i = 0; i < p_sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
            p_sps->offset_for_ref_frame[i] = Parser::ExpGolomb::ReadSe(p_stream, offset);
        }
    }

    p_sps->max_num_ref_frames = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_sps->gaps_in_frame_num_value_allowed_flag = Parser::GetBit(p_stream, offset);
    p_sps->pic_width_in_mbs_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_sps->pic_height_in_map_units_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_sps->frame_mbs_only_flag = Parser::GetBit(p_stream, offset);
    if (!p_sps->frame_mbs_only_flag) {
        p_sps->mb_adaptive_frame_field_flag = Parser::GetBit(p_stream, offset);
    }

    p_sps->direct_8x8_inference_flag = Parser::GetBit(p_stream, offset);
    p_sps->frame_cropping_flag = Parser::GetBit(p_stream, offset);
    if (p_sps->frame_cropping_flag) {
        p_sps->frame_crop_left_offset = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_sps->frame_crop_right_offset = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_sps->frame_crop_top_offset = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_sps->frame_crop_bottom_offset = Parser::ExpGolomb::ReadUe(p_stream, offset);
    }

    p_sps->vui_parameters_present_flag = Parser::GetBit(p_stream, offset);
    if (p_sps->vui_parameters_present_flag == 1) {
        GetVuiParameters(p_stream, offset, &p_sps->vui_seq_parameters);
    }

    p_sps->is_received = 1;  // confirm SPS with seq_parameter_set_id received (but not activated)

#if DBGINFO
    PrintSps(p_sps);
#endif // DBGINFO
}

void AvcVideoParser::ParsePps(uint8_t *p_stream, size_t stream_size_in_byte) {
    AvcSeqParameterSet *p_sps = nullptr;
    AvcPicParameterSet *p_pps = nullptr;
    size_t offset = 0; // current bit offset

    // Parse and temporarily store
    uint32_t pic_parameter_set_id = Parser::ExpGolomb::ReadUe(p_stream, offset);
    uint32_t seq_parameter_set_id = Parser::ExpGolomb::ReadUe(p_stream, offset);

    p_sps = &sps_list_[seq_parameter_set_id];
    p_pps = &pps_list_[pic_parameter_set_id];
    memset(p_pps, 0, sizeof(AvcPicParameterSet));	

    p_pps->pic_parameter_set_id = pic_parameter_set_id;
    p_pps->seq_parameter_set_id = seq_parameter_set_id;

    p_pps->entropy_coding_mode_flag = Parser::GetBit(p_stream, offset);
    p_pps->bottom_field_pic_order_in_frame_present_flag = Parser::GetBit(p_stream, offset);

    p_pps->num_slice_groups_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
    if (p_pps->num_slice_groups_minus1 > 0) {
        p_pps->slice_group_map_type = Parser::ExpGolomb::ReadUe(p_stream, offset);
        if (p_pps->slice_group_map_type == 0) {
            for (int i_group = 0; i_group <= p_pps->num_slice_groups_minus1; i_group++) {
                p_pps->run_length_minus1[i_group] = Parser::ExpGolomb::ReadUe(p_stream, offset);
            }
        } else if (p_pps->slice_group_map_type == 2) {
            for (int i_group = 0; i_group < p_pps->num_slice_groups_minus1; i_group++ ) {
                p_pps->top_left[i_group] = Parser::ExpGolomb::ReadUe(p_stream, offset);
                p_pps->bottom_right[i_group] = Parser::ExpGolomb::ReadUe(p_stream, offset);
            }
        } else if (p_pps->slice_group_map_type == 3 || p_pps->slice_group_map_type == 4 || p_pps->slice_group_map_type == 5) {
            p_pps->slice_group_change_direction_flag = Parser::GetBit(p_stream, offset);
            p_pps->slice_group_change_rate_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
        } else if (p_pps->slice_group_map_type == 6) {
            p_pps->pic_size_in_map_units_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
            int slice_group_id_size = ceil(log2(p_pps->num_slice_groups_minus1 + 1));
            for (int i = 0; i <= p_pps->pic_size_in_map_units_minus1; i++) {
                //p_pps->slice_group_id[i] = Parser::ReadBits(p_stream, offset, slice_group_id_size);
                int temp = Parser::ReadBits(p_stream, offset, slice_group_id_size);
                ERR("AVC PPS parsing: slice_group_id memory not allocaed!");
            }
        }
    }

    p_pps->num_ref_idx_l0_default_active_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_pps->num_ref_idx_l1_default_active_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_pps->weighted_pred_flag = Parser::GetBit(p_stream, offset);
    p_pps->weighted_bipred_idc = Parser::ReadBits(p_stream, offset, 2);
    p_pps->pic_init_qp_minus26 = Parser::ExpGolomb::ReadSe(p_stream, offset);
    p_pps->pic_init_qs_minus26 = Parser::ExpGolomb::ReadSe(p_stream, offset);
    p_pps->chroma_qp_index_offset = Parser::ExpGolomb::ReadSe(p_stream, offset);
    p_pps->deblocking_filter_control_present_flag = Parser::GetBit(p_stream, offset);
    p_pps->constrained_intra_pred_flag = Parser::GetBit(p_stream, offset);
    p_pps->redundant_pic_cnt_present_flag = Parser::GetBit(p_stream, offset);

    if (MoreRbspData(p_stream, stream_size_in_byte, offset)) {
        p_pps->transform_8x8_mode_flag = Parser::GetBit(p_stream, offset);
        p_pps->pic_scaling_matrix_present_flag = Parser::GetBit(p_stream, offset);
        if (p_pps->pic_scaling_matrix_present_flag == 1) {
            int count = p_sps->chroma_format_idc != 3 ? 2 : 6;
            for (int i = 0; i < 6 + count * p_pps->transform_8x8_mode_flag; i++) {
                p_pps->pic_scaling_list_present_flag [i] = Parser::GetBit(p_stream, offset);
                if (p_pps->pic_scaling_list_present_flag[i] == 1) {
                    if ( i < 6 ) {
                        GetScalingList(p_stream, offset, p_pps->scaling_list_4x4[i], 16, &p_pps->use_default_scaling_matrix_4x4_flag[i]);
                    } else {
                        GetScalingList(p_stream, offset, p_pps->scaling_list_8x8[i - 6], 64, &p_pps->use_default_scaling_matrix_8x8_flag[i - 6]);
                    }
                }
            }
        }
        p_pps->second_chroma_qp_index_offset = Parser::ExpGolomb::ReadSe(p_stream, offset);
    } else {
        /// When second_chroma_qp_index_offset is not present, it shall be inferred to be equal to chroma_qp_index_offset.
        p_pps->second_chroma_qp_index_offset = p_pps->chroma_qp_index_offset;
    }

    // Setup default scaling list if needed
    if (p_pps->pic_scaling_matrix_present_flag == 0) {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 16; j++) {
                p_pps->scaling_list_4x4[i][j] = p_sps->scaling_list_4x4[i][j];
            }
        }
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 64; j++) {
                p_pps->scaling_list_8x8[i][j] = p_sps->scaling_list_8x8[i][j];
            }
        }
    } else {
        // 4 x 4
        for (int i = 0; i < 6; i++) {
            if (p_pps->pic_scaling_list_present_flag[i] == 0) {
                if (i == 0) {
                    if ( p_sps->seq_scaling_matrix_present_flag == 0) { // fall back rule set A
                        for (int j = 0; j < 16; j++) {
                            p_pps->scaling_list_4x4[i][j] = Default_4x4_Intra[j];
                        }
                    } else { // fall back rule set B
                        for (int j = 0; j < 16; j++) {
                            p_pps->scaling_list_4x4[i][j] = p_sps->scaling_list_4x4[i][j];
                        }
                    }
                } else if (i == 3) {
                    if (p_sps->seq_scaling_matrix_present_flag == 0) { // fall back rule set A
                        for (int j = 0; j < 16; j++) {
                            p_pps->scaling_list_4x4[i][j] = Default_4x4_Inter[j];
                        }
                    } else { // fall back rule set B
                        for (int j = 0; j < 16; j++) {
                            p_pps->scaling_list_4x4[i][j] = p_sps->scaling_list_4x4[i][j];
                        }
                    }
                } else {
                    for (int j = 0; j < 16; j++) {
                        p_pps->scaling_list_4x4[i][j] = p_pps->scaling_list_4x4[i - 1][j];
                    }
                }
            } else {
                if (p_pps->use_default_scaling_matrix_4x4_flag[i]) {
                    if( i < 3 ) {
                        for (int j = 0; j < 16; j++) {
                            p_pps->scaling_list_4x4[i][j] = Default_4x4_Intra[j];
                        }
                    } else {
                        for (int j = 0; j < 16; j++) {
                            p_pps->scaling_list_4x4[i][j] = Default_4x4_Inter[j];
                        }
                    }
                }
            }
        }

        // 8 x 8
        for (int i = 0; i < 6; i++) {
            if (p_pps->pic_scaling_list_present_flag[i + 6] == 0) {
                if (i == 0) {
                    if ( p_sps->seq_scaling_matrix_present_flag == 0) { // fall back rule set A
                        for (int j = 0; j < 64; j++) {
                            p_pps->scaling_list_8x8[i][j] = Default_8x8_Intra[j];
                        }
                    } else { // fall back rule set B
                        for (int j = 0; j < 64; j++) {
                            p_pps->scaling_list_8x8[i][j] = p_sps->scaling_list_8x8[i][j];
                        }
                    }
                } else if (i == 1) {
                    if ( p_sps->seq_scaling_matrix_present_flag == 0) { // fall back rule set A
                        for (int j = 0; j < 64; j++) {
                            p_pps->scaling_list_8x8[i][j] = Default_8x8_Inter[j];
                        }
                    } else { // fall back rule set B
                        for (int j = 0; j < 64; j++) {
                            p_pps->scaling_list_8x8[i][j] = p_sps->scaling_list_8x8[i][j];
                        }
                    }
                } else {
                    for (int j = 0; j < 64; j++) {
                        p_pps->scaling_list_8x8[i][j] = p_pps->scaling_list_8x8[i - 2][j];
                    }
                }
            } else {
                if (p_pps->use_default_scaling_matrix_8x8_flag[i]) {
                    if ( i % 2 == 0) {
                        for (int j = 0; j < 64; j++) {
                            p_pps->scaling_list_8x8[i][j] = Default_8x8_Intra[j];
                        }
                    } else {
                        for (int j = 0; j < 64; j++) {
                            p_pps->scaling_list_8x8[i][j] = Default_8x8_Inter[j];
                        }
                    }
                }
            }
        }
    }

    p_pps->is_received = 1;  // confirm PPS with pic_parameter_set_id received (but not activated)

#if DBGINFO
    PrintPps(p_pps);
#endif // DBGINFO
}

ParserResult AvcVideoParser::ParseSliceHeader(uint8_t *p_stream, size_t stream_size_in_byte, AvcSliceHeader *p_slice_header) {
    int i;
    size_t offset = 0;  // current bit offset
    AvcSeqParameterSet *p_sps = nullptr;
    AvcPicParameterSet *p_pps = nullptr;

    curr_has_mmco_5_ = 0;
    memset(p_slice_header, 0, sizeof(AvcSliceHeader));

    p_slice_header->first_mb_in_slice = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_slice_header->slice_type = Parser::ExpGolomb::ReadUe(p_stream, offset);
    p_slice_header->pic_parameter_set_id = Parser::ExpGolomb::ReadUe(p_stream, offset);

    // Set active SPS and PPS for the current slice
    active_pps_id_ = p_slice_header->pic_parameter_set_id;
    p_pps = &pps_list_[active_pps_id_];
    if (active_sps_id_ != p_pps->seq_parameter_set_id) {
        active_sps_id_ = p_pps->seq_parameter_set_id;
        p_sps = &sps_list_[active_sps_id_];
        // Re-set DPB size.
        dpb_buffer_.dpb_size = p_sps->max_num_ref_frames + 1;
        dpb_buffer_.dpb_size = dpb_buffer_.dpb_size > AVC_MAX_DPB_FRAMES ? AVC_MAX_DPB_FRAMES : dpb_buffer_.dpb_size;
        new_sps_activated_ = true;  // Note: clear this flag after the actions are taken.
    }
    p_sps = &sps_list_[active_sps_id_];

    // Check video dimension change
    uint32_t curr_pic_width = (p_sps->pic_width_in_mbs_minus1 + 1) * AVC_MACRO_BLOCK_SIZE;
    uint32_t curr_pic_height = (2 - p_sps->frame_mbs_only_flag) * (p_sps->pic_height_in_map_units_minus1 + 1) * AVC_MACRO_BLOCK_SIZE;
    if ( pic_width_ != curr_pic_width || pic_height_ != curr_pic_height) {
        pic_width_ = curr_pic_width;
        pic_height_ = curr_pic_height;
        // Take care of the case where a new SPS replaces the old SPS with the same id but with different dimensions
        // Re-set DPB size.
        dpb_buffer_.dpb_size = p_sps->max_num_ref_frames + 1;
        dpb_buffer_.dpb_size = dpb_buffer_.dpb_size > AVC_MAX_DPB_FRAMES ? AVC_MAX_DPB_FRAMES : dpb_buffer_.dpb_size;
        new_sps_activated_ = true;  // Note: clear this flag after the actions are taken.
    }

    if (p_sps->separate_colour_plane_flag == 1) {
        p_slice_header->colour_plane_id = Parser::ReadBits(p_stream, offset, 2);
    }
    p_slice_header->frame_num = Parser::ReadBits(p_stream, offset, p_sps->log2_max_frame_num_minus4 + 4);

    if (p_sps->frame_mbs_only_flag != 1) {
        p_slice_header->field_pic_flag = Parser::GetBit(p_stream, offset);
        if (p_slice_header->field_pic_flag == 1)
        {
            p_slice_header->bottom_field_flag = Parser::GetBit(p_stream, offset);
        }
    } else {
        p_slice_header->field_pic_flag = 0;
        p_slice_header->bottom_field_flag = 0;
    }

    if ( nal_unit_header_.nal_ref_idc ) {
        curr_ref_pic_bottom_field_ = p_slice_header->bottom_field_flag;
    }
    
    if (nal_unit_header_.nal_unit_type == kAvcNalTypeSlice_IDR) {
        p_slice_header->idr_pic_id = Parser::ExpGolomb::ReadUe(p_stream, offset);
    }

    if (p_sps->pic_order_cnt_type == 0) {
        p_slice_header->pic_order_cnt_lsb = Parser::ReadBits(p_stream, offset, p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        if (p_pps->bottom_field_pic_order_in_frame_present_flag == 1 && p_slice_header->field_pic_flag != 1 ) {
            p_slice_header->delta_pic_order_cnt_bottom = Parser::ExpGolomb::ReadSe(p_stream, offset);
        }
    }

    if (p_sps->pic_order_cnt_type == 1 && p_sps->delta_pic_order_always_zero_flag != 1) {
        p_slice_header->delta_pic_order_cnt[0] = Parser::ExpGolomb::ReadSe(p_stream, offset);
        if (p_pps->bottom_field_pic_order_in_frame_present_flag == 1 && p_slice_header->field_pic_flag != 1) {
            p_slice_header->delta_pic_order_cnt[1] = Parser::ExpGolomb::ReadSe(p_stream, offset);
        }
    }

    if (p_pps->redundant_pic_cnt_present_flag == 1) {
        p_slice_header->redundant_pic_cnt = Parser::ExpGolomb::ReadUe(p_stream, offset);
    }

    if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6 ) { // B-Slice
        p_slice_header->direct_spatial_mv_pred_flag = Parser::GetBit(p_stream, offset);
    }

    if (p_slice_header->slice_type == kAvcSliceTypeP || p_slice_header->slice_type == kAvcSliceTypeP_5 ||
        p_slice_header->slice_type == kAvcSliceTypeSP || p_slice_header->slice_type == kAvcSliceTypeSP_8 ||
        p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6) {
        p_slice_header->num_ref_idx_active_override_flag = Parser::GetBit(p_stream, offset);
        if (p_slice_header->num_ref_idx_active_override_flag == 1) {
            p_slice_header->num_ref_idx_l0_active_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
            if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6) {
                p_slice_header->num_ref_idx_l1_active_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
            }
        } else {
            p_slice_header->num_ref_idx_l0_active_minus1 = p_pps->num_ref_idx_l0_default_active_minus1;
            p_slice_header->num_ref_idx_l1_active_minus1 = p_pps->num_ref_idx_l1_default_active_minus1;
        }
    }

    // Bail out for NAL unit type 20/21
    if ( nal_unit_header_.nal_unit_type == 21 || nal_unit_header_.nal_unit_type == 21) {
        return PARSER_NOT_SUPPORTED;
    }

    // Ref picture list modification
    int modification_of_pic_nums_idc;
    if (p_slice_header->slice_type != kAvcSliceTypeI && p_slice_header->slice_type != kAvcSliceTypeSI &&
        p_slice_header->slice_type != kAvcSliceTypeI_7 && p_slice_header->slice_type != kAvcSliceTypeSI_9) {
        p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l0 = Parser::GetBit(p_stream, offset);
        if (p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l0 == 1) {
            i = 0;
            do {
                modification_of_pic_nums_idc = Parser::ExpGolomb::ReadUe(p_stream, offset);
                p_slice_header->ref_pic_list.modification_l0[i].modification_of_pic_nums_idc = modification_of_pic_nums_idc;
                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
                    p_slice_header->ref_pic_list.modification_l0[i].abs_diff_pic_num_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
                } else if (modification_of_pic_nums_idc == 2) {
                    p_slice_header->ref_pic_list.modification_l0[i].long_term_pic_num = Parser::ExpGolomb::ReadUe(p_stream, offset);
                }
                i++;
            } while (modification_of_pic_nums_idc != 3);
        }
    }

    if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6) {
        p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l1 = Parser::GetBit(p_stream, offset);
        if (p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l1 == 1) {
            i = 0;
            do {
                modification_of_pic_nums_idc = Parser::ExpGolomb::ReadUe(p_stream, offset);
                p_slice_header->ref_pic_list.modification_l1[i].modification_of_pic_nums_idc = modification_of_pic_nums_idc;
                if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
                    p_slice_header->ref_pic_list.modification_l1[i].abs_diff_pic_num_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
                } else if(modification_of_pic_nums_idc == 2) {
                    p_slice_header->ref_pic_list.modification_l1[i].long_term_pic_num = Parser::ExpGolomb::ReadUe(p_stream, offset);
                }
                i++;
            } while (modification_of_pic_nums_idc != 3);
        }
    }

    // Prediction weight table
    if ((p_pps->weighted_pred_flag == 1 &&
            ((p_slice_header->slice_type == kAvcSliceTypeP || p_slice_header->slice_type == kAvcSliceTypeP_5) ||
            (p_slice_header->slice_type == kAvcSliceTypeSP || p_slice_header->slice_type == kAvcSliceTypeSP_8))) ||
        (p_pps->weighted_bipred_idc == 1 &&
            (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6))) {
        p_slice_header->pred_weight_table.luma_log2_weight_denom = Parser::ExpGolomb::ReadUe(p_stream, offset);
        
        int ChromaArrayType = p_sps->separate_colour_plane_flag == 0 ? p_sps->chroma_format_idc : 0;
        if (ChromaArrayType != 0) {
            p_slice_header->pred_weight_table.chroma_log2_weight_denom = Parser::ExpGolomb::ReadUe(p_stream, offset);
        }
        
        for (i = 0; i <= p_slice_header->num_ref_idx_l0_active_minus1; i++) {
            p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l0_flag = Parser::GetBit(p_stream, offset);
            if (p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l0_flag == 1) {
                p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l0 = Parser::ExpGolomb::ReadSe(p_stream, offset);
                p_slice_header->pred_weight_table.weight_factor[i].luma_offset_l0 = Parser::ExpGolomb::ReadSe(p_stream, offset);
            } else {
                p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l0 = 1 << p_slice_header->pred_weight_table.luma_log2_weight_denom;
                p_slice_header->pred_weight_table.weight_factor[i].luma_offset_l0 = 0;
            }
            
            if (ChromaArrayType != 0) {
                p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l0_flag = Parser::GetBit(p_stream, offset);
                if (p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l0_flag == 1) {
                    for (int j = 0; j < 2; j++) {
                        p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l0[j] = Parser::ExpGolomb::ReadSe(p_stream, offset);
                        p_slice_header->pred_weight_table.weight_factor[i].chroma_offset_l0[j] = Parser::ExpGolomb::ReadSe(p_stream, offset);
                    }
                } else {
                    for (int j = 0; j < 2; j++) {
                        p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l0[j] = 1 << p_slice_header->pred_weight_table.chroma_log2_weight_denom;
                        p_slice_header->pred_weight_table.weight_factor[i].chroma_offset_l0[j] = 0;
                    }
                }
            }
        }
        
        if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6) {
            for (int i = 0; i <= p_slice_header->num_ref_idx_l1_active_minus1; i++) {
                p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l1_flag = Parser::GetBit(p_stream, offset);
                if (p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l1_flag == 1) {
                    p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l1 = Parser::ExpGolomb::ReadSe(p_stream, offset);
                    p_slice_header->pred_weight_table.weight_factor[i].luma_offset_l1 = Parser::ExpGolomb::ReadSe(p_stream, offset);
                } else {
                    p_slice_header->pred_weight_table.weight_factor[i].luma_weight_l1 = 1 << p_slice_header->pred_weight_table.luma_log2_weight_denom;
                    p_slice_header->pred_weight_table.weight_factor[i].luma_offset_l1 = 0;
                }

                if (ChromaArrayType != 0 ) {
                    p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l1_flag = Parser::GetBit(p_stream, offset);
                    if (p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l1_flag == 1) {
                        for (int j = 0; j < 2; j++) {
                            p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l1[j] = Parser::ExpGolomb::ReadSe(p_stream, offset);
                            p_slice_header->pred_weight_table.weight_factor[i].chroma_offset_l1[j] = Parser::ExpGolomb::ReadSe(p_stream, offset);
                        }
                    } else {
                        for (int j = 0; j < 2; j++) {
                            p_slice_header->pred_weight_table.weight_factor[i].chroma_weight_l1[j] = 1 << p_slice_header->pred_weight_table.chroma_log2_weight_denom;
                            p_slice_header->pred_weight_table.weight_factor[i].chroma_offset_l1[j] = 0;
                        }
                    }
                }
            }
        }
    }

    // Decoded reference picture marking.
    int memory_management_control_operation;
    if (nal_unit_header_.nal_ref_idc != 0) {
        if (nal_unit_header_.nal_unit_type == kAvcNalTypeSlice_IDR) {
            p_slice_header->dec_ref_pic_marking.no_output_of_prior_pics_flag = Parser::GetBit(p_stream, offset);
            p_slice_header->dec_ref_pic_marking.long_term_reference_flag = Parser::GetBit(p_stream, offset);
        } else {
            p_slice_header->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag = Parser::GetBit(p_stream, offset);
            if (p_slice_header->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag == 1) {
                i = 0;
                do {
                    memory_management_control_operation = Parser::ExpGolomb::ReadUe(p_stream, offset);
                    p_slice_header->dec_ref_pic_marking.mmco[i].memory_management_control_operation = memory_management_control_operation;
                    
                    if (memory_management_control_operation == 1 || memory_management_control_operation == 3) {
                        p_slice_header->dec_ref_pic_marking.mmco[i].difference_of_pic_nums_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
                    }
                    if (memory_management_control_operation == 2) {
                        p_slice_header->dec_ref_pic_marking.mmco[i].long_term_pic_num = Parser::ExpGolomb::ReadUe(p_stream, offset);
                    }
                    if (memory_management_control_operation == 3 || memory_management_control_operation == 6) {
                        p_slice_header->dec_ref_pic_marking.mmco[i].long_term_frame_idx = Parser::ExpGolomb::ReadUe(p_stream, offset);
                    }
                    if (memory_management_control_operation == 4) {
                        p_slice_header->dec_ref_pic_marking.mmco[i].max_long_term_frame_idx_plus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
                    }
                    if ( memory_management_control_operation == 5) {
                        curr_has_mmco_5_ = 1;
                    }
                    i++;
                } while (memory_management_control_operation != 0);
                p_slice_header->dec_ref_pic_marking.mmco_count = i - 1;
            }
        }
    }

    if (p_pps->entropy_coding_mode_flag == 1 &&
        p_slice_header->slice_type != kAvcSliceTypeI && p_slice_header->slice_type != kAvcSliceTypeSI &&
        p_slice_header->slice_type != kAvcSliceTypeI_7 && p_slice_header->slice_type != kAvcSliceTypeSI_9) {
        p_slice_header->cabac_init_idc = Parser::ExpGolomb::ReadUe(p_stream, offset);
    }
    p_slice_header->slice_qp_delta = Parser::ExpGolomb::ReadSe(p_stream, offset);
    if (p_slice_header->slice_type == kAvcSliceTypeSP || p_slice_header->slice_type == kAvcSliceTypeSI ||
        p_slice_header->slice_type == kAvcSliceTypeSP_8 || p_slice_header->slice_type == kAvcSliceTypeSI_9) {
        if (p_slice_header->slice_type == kAvcSliceTypeSP || p_slice_header->slice_type == kAvcSliceTypeSP_8) {
            p_slice_header->sp_for_switch_flag = Parser::GetBit(p_stream, offset);
        }
        p_slice_header->slice_qs_delta = Parser::ExpGolomb::ReadSe(p_stream, offset);
    }

    if (p_pps->deblocking_filter_control_present_flag == 1) {
        p_slice_header->disable_deblocking_filter_idc = Parser::ExpGolomb::ReadUe(p_stream, offset);
        if (p_slice_header->disable_deblocking_filter_idc != 1) {
            p_slice_header->slice_alpha_c0_offset_div2 = Parser::ExpGolomb::ReadSe(p_stream, offset);
            p_slice_header->slice_beta_offset_div2 = Parser::ExpGolomb::ReadSe(p_stream, offset);
        }
    }
    if (p_pps->num_slice_groups_minus1 > 0 && p_pps->slice_group_map_type >= 3 && p_pps->slice_group_map_type <= 5) {
        int size = ceil(log2((double)(p_sps->pic_height_in_map_units_minus1+1) / (double)(p_pps->slice_group_change_rate_minus1+1) + 1));
        p_slice_header->slice_group_change_cycle = Parser::ReadBits(p_stream, offset, size);
    }

#if DBGINFO
    PrintSliceHeader(p_slice_header);
#endif // DBGINFO

    return PARSER_OK;
}

void AvcVideoParser::GetScalingList(uint8_t *p_stream, size_t &offset, uint32_t *scaling_list, uint32_t list_size, uint32_t *use_default_scaling_matrix_flag) {
    int32_t last_scale, next_scale, delta_scale;

    last_scale = 8;
    next_scale = 8;
    for (int j = 0; j < list_size; j++) {
        if (next_scale != 0) {
            delta_scale = Parser::ExpGolomb::ReadSe(p_stream, offset);
            next_scale = (last_scale + delta_scale + 256) % 256;
            *use_default_scaling_matrix_flag = (j == 0 && next_scale == 0);
        }
        
        scaling_list[j] = (next_scale == 0) ? last_scale : next_scale;
        last_scale = scaling_list[j];
    }
}

void AvcVideoParser::GetVuiParameters(uint8_t *p_stream, size_t &offset, AvcVuiSeqParameters *p_vui_params) {
    p_vui_params->aspect_ratio_info_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->aspect_ratio_info_present_flag == 1) {
        p_vui_params->aspect_ratio_idc = Parser::ReadBits(p_stream, offset, 8);
        if (p_vui_params->aspect_ratio_idc == 255 /*Extended_SAR*/) {
            p_vui_params->sar_width = Parser::ReadBits(p_stream, offset, 16);
            p_vui_params->sar_height = Parser::ReadBits(p_stream, offset, 16);
        }
    }

    p_vui_params->overscan_info_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->overscan_info_present_flag == 1) {
        p_vui_params->overscan_appropriate_flag = Parser::GetBit(p_stream, offset);
    }

    p_vui_params->video_signal_type_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->video_signal_type_present_flag == 1) {
        p_vui_params->video_format = Parser::ReadBits(p_stream, offset, 3);
        p_vui_params->video_full_range_flag = Parser::GetBit(p_stream, offset);
        p_vui_params->colour_description_present_flag = Parser::GetBit(p_stream, offset);
        if (p_vui_params->colour_description_present_flag == 1) {
            p_vui_params->colour_primaries = Parser::ReadBits(p_stream, offset, 8);
            p_vui_params->transfer_characteristics = Parser::ReadBits(p_stream, offset, 8);
            p_vui_params->matrix_coefficients = Parser::ReadBits(p_stream, offset, 8);
        }
    }

    p_vui_params->chroma_loc_info_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->chroma_loc_info_present_flag == 1) {
        p_vui_params->chroma_sample_loc_type_top_field = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->chroma_sample_loc_type_bottom_field = Parser::ExpGolomb::ReadUe(p_stream, offset);
    }

    p_vui_params->timing_info_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->timing_info_present_flag == 1) {
        p_vui_params->num_units_in_tick = Parser::ReadBits(p_stream, offset, 32);
        p_vui_params->time_scale = Parser::ReadBits(p_stream, offset, 32);
        p_vui_params->fixed_frame_rate_flag = Parser::GetBit(p_stream, offset);
    }
    
    p_vui_params->nal_hrd_parameters_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->nal_hrd_parameters_present_flag == 1 ) {
        p_vui_params->nal_hrd_parameters.cpb_cnt_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->nal_hrd_parameters.bit_rate_scale = Parser::ReadBits(p_stream, offset, 4);
        p_vui_params->nal_hrd_parameters.cpb_size_scale = Parser::ReadBits(p_stream, offset, 4);
        for (int SchedSelIdx = 0; SchedSelIdx <= p_vui_params->nal_hrd_parameters.cpb_cnt_minus1; SchedSelIdx ++) {
            p_vui_params->nal_hrd_parameters.bit_rate_value_minus1[SchedSelIdx] = Parser::ExpGolomb::ReadUe(p_stream, offset);
            p_vui_params->nal_hrd_parameters.cpb_size_value_minus1[SchedSelIdx] = Parser::ExpGolomb::ReadUe(p_stream, offset);
            p_vui_params->nal_hrd_parameters.cbr_flag[SchedSelIdx] = Parser::ReadBits(p_stream, offset, 1);
        }
        p_vui_params->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1 = Parser::ReadBits(p_stream, offset, 5);
        p_vui_params->nal_hrd_parameters.cpb_removal_delay_length_minus1 = Parser::ReadBits(p_stream, offset, 5);
        p_vui_params->nal_hrd_parameters.dpb_output_delay_length_minus1 = Parser::ReadBits(p_stream, offset, 5);
        p_vui_params->nal_hrd_parameters.time_offset_length = Parser::ReadBits(p_stream, offset, 5);
    }
    
    p_vui_params->vcl_hrd_parameters_present_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->vcl_hrd_parameters_present_flag == 1) {
        p_vui_params->vcl_hrd_parameters.cpb_cnt_minus1 = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->vcl_hrd_parameters.bit_rate_scale = Parser::ReadBits(p_stream, offset, 4);
        p_vui_params->vcl_hrd_parameters.cpb_size_scale = Parser::ReadBits(p_stream, offset, 4);
        for (int SchedSelIdx = 0; SchedSelIdx <= p_vui_params->vcl_hrd_parameters.cpb_cnt_minus1; SchedSelIdx ++) {
            p_vui_params->vcl_hrd_parameters.bit_rate_value_minus1[SchedSelIdx] = Parser::ExpGolomb::ReadUe(p_stream, offset);
            p_vui_params->vcl_hrd_parameters.cpb_size_value_minus1[SchedSelIdx] = Parser::ExpGolomb::ReadUe(p_stream, offset);
            p_vui_params->vcl_hrd_parameters.cbr_flag[SchedSelIdx] = Parser::GetBit(p_stream, offset);
        }
        p_vui_params->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1 = Parser::ReadBits(p_stream, offset, 5);
        p_vui_params->vcl_hrd_parameters.cpb_removal_delay_length_minus1 = Parser::ReadBits(p_stream, offset, 5);
        p_vui_params->vcl_hrd_parameters.dpb_output_delay_length_minus1 = Parser::ReadBits(p_stream, offset, 5);
        p_vui_params->vcl_hrd_parameters.time_offset_length = Parser::ReadBits(p_stream, offset, 5);
    }
    if (p_vui_params->nal_hrd_parameters_present_flag == 1 || p_vui_params->vcl_hrd_parameters_present_flag == 1) {
        p_vui_params->low_delay_hrd_flag = Parser::GetBit(p_stream, offset);
    }
    
    p_vui_params->pic_struct_present_flag = Parser::GetBit(p_stream, offset);
    p_vui_params->bitstream_restriction_flag = Parser::GetBit(p_stream, offset);
    if (p_vui_params->bitstream_restriction_flag) {
        p_vui_params->motion_vectors_over_pic_boundaries_flag = Parser::GetBit(p_stream, offset);
        p_vui_params->max_bytes_per_pic_denom = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->max_bits_per_mb_denom = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->log2_max_mv_length_horizontal = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->log2_max_mv_length_vertical = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->num_reorder_frames = Parser::ExpGolomb::ReadUe(p_stream, offset);
        p_vui_params->max_dec_frame_buffering = Parser::ExpGolomb::ReadUe(p_stream, offset);
    }
}

bool AvcVideoParser::MoreRbspData(uint8_t *p_stream, size_t stream_size_in_byte, size_t bit_offset) {
    bool more_rbsp_bits = false;
    uint8_t curr_byte = p_stream[bit_offset >> 3];
    uint8_t next_bytes[3];
    uint32_t next_byte_offset = (bit_offset + 7) >> 3;
    int bit_offset_in_byte = bit_offset % 8;
    
    /// If the following bytes are not start code, we have more RBSP data. If we don't have enough bytes
    /// in the stream, pad with 0.
    next_bytes[0] = next_byte_offset < stream_size_in_byte ? p_stream[next_byte_offset] : 0;
    next_bytes[1] = (next_byte_offset + 1) < stream_size_in_byte ? p_stream[next_byte_offset + 1] : 0;
    next_bytes[2] = (next_byte_offset + 2) < stream_size_in_byte ? p_stream[next_byte_offset + 2] : 0;

    if ( (next_bytes[0] == 0x00 && next_bytes[1] == 0x00 && next_bytes[2] == 0x00) ||  // padding zero bytes
         (next_bytes[0] == 0x00 && next_bytes[1] == 0x00 && next_bytes[2] == 0x01) ) { // start code
        /// Continue checking the existence of the trailing bits in the current byte.
    } else {
        return true;
    }
        
    /// Check if RBSP trailing bits immediately follow
    if ((bit_offset_in_byte) == 0) {
        if (curr_byte == 0x80) { // RBSP trailing bits
            more_rbsp_bits = false;
        } else {
            more_rbsp_bits = true;
        }
    } else {
        uint8_t curr_bit = curr_byte & (0x80 >> bit_offset_in_byte);
        if ( curr_bit == 0 ) {
            more_rbsp_bits = true; // rbsp_stop_one_bit has to be 1.
        } else {
            /// If this is the last bit, need to grab the next byte
            if ( bit_offset_in_byte == 7 ) {
                if ( next_bytes[0] != 0 ) {
                    more_rbsp_bits = true;
                } else {
                    more_rbsp_bits = false;
                }
            } else {
                bit_offset_in_byte++;
                for (int i = bit_offset_in_byte; i < 8; i++) {
                    curr_bit = curr_byte & (0x80 >> i);
                    if ( curr_bit ) {
                        more_rbsp_bits = true;
                        break;
                    }
                }
            }
        }
    }
    
    return more_rbsp_bits;
}

// 8.2.1
void AvcVideoParser::CalculateCurrPoc() {
    AvcSeqParameterSet *p_sps = &sps_list_[active_sps_id_];
    AvcSliceHeader *p_slice_header = &slice_header_0_;
    int frame_num_offset; // FrameNumOffset

    int max_pic_order_cnt_lsb = 1 << (p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4); // MaxPicOrderCntLsb
    int max_frame_num = 1 << (p_sps->log2_max_frame_num_minus4 + 4); // max_frame_num

    if (p_sps->pic_order_cnt_type == 0) {
        if (slice_nal_unit_header_.nal_unit_type == kAvcNalTypeSlice_IDR) {
            prev_pic_order_cnt_msb_ = 0;
            prev_pic_order_cnt_lsb_ = 0;
        } else {
            if (prev_has_mmco_5_) {
                if (prev_ref_pic_bottom_field_) {
                    prev_pic_order_cnt_msb_ = 0;
                    prev_pic_order_cnt_lsb_ = 0;
                } else {
                    prev_pic_order_cnt_msb_ = 0;
                    prev_pic_order_cnt_lsb_ = prev_top_field_order_cnt_;
                }
            } else {
                /// See the end of this section
            }
        }

        int pic_order_cnt_msb;
		if ( (p_slice_header->pic_order_cnt_lsb < prev_pic_order_cnt_lsb_) && ((prev_pic_order_cnt_lsb_ - p_slice_header->pic_order_cnt_lsb) >= (max_pic_order_cnt_lsb / 2))) {
			pic_order_cnt_msb = prev_pic_order_cnt_msb_ + max_pic_order_cnt_lsb;
        } else if ((p_slice_header->pic_order_cnt_lsb > prev_pic_order_cnt_lsb_) && ( (p_slice_header->pic_order_cnt_lsb - prev_pic_order_cnt_lsb_) > (max_pic_order_cnt_lsb / 2))) {
			pic_order_cnt_msb = prev_pic_order_cnt_msb_ - max_pic_order_cnt_lsb;
        } else {
			pic_order_cnt_msb = prev_pic_order_cnt_msb_;
        }

		if (!p_slice_header->field_pic_flag || !p_slice_header->bottom_field_flag) {
			curr_pic_.top_field_order_cnt = pic_order_cnt_msb + p_slice_header->pic_order_cnt_lsb;
        }
		if (!p_slice_header->field_pic_flag) {
			curr_pic_.bottom_field_order_cnt = curr_pic_.top_field_order_cnt + p_slice_header->delta_pic_order_cnt_bottom;
        } else if (p_slice_header->bottom_field_flag) {
			curr_pic_.bottom_field_order_cnt = pic_order_cnt_msb + p_slice_header->pic_order_cnt_lsb;
        }
        if (slice_nal_unit_header_.nal_ref_idc) {
			prev_pic_order_cnt_msb_ = pic_order_cnt_msb;
            prev_pic_order_cnt_lsb_ = p_slice_header->pic_order_cnt_lsb;
            prev_top_field_order_cnt_ = curr_pic_.top_field_order_cnt;
        }
    } else if (p_sps->pic_order_cnt_type == 1) {
        int abs_frame_num; // absFrameNum

		if (slice_nal_unit_header_.nal_unit_type == kAvcNalTypeSlice_IDR){
			frame_num_offset = 0;
        } else {
            if (prev_has_mmco_5_) {
                prev_frame_num_offset_t = 0;
                prev_frame_num_ = 0;
            } else {
                /// See the end of this section
            }
            if (prev_frame_num_ > p_slice_header->frame_num) {
                frame_num_offset = prev_frame_num_offset_t + max_frame_num;
            } else {
                frame_num_offset = prev_frame_num_offset_t;
            }
        }
        
        if (p_sps->num_ref_frames_in_pic_order_cnt_cycle) {
            abs_frame_num = frame_num_offset + p_slice_header->frame_num;
        } else {
            abs_frame_num = 0;
        }
        if ((!slice_nal_unit_header_.nal_ref_idc) && abs_frame_num > 0) {
            abs_frame_num--;
        }
        
        int expected_delta_per_pic_order_cnt_cycle = 0; // ExpectedDeltaPerPicOrderCntCycle
        if (p_sps->num_ref_frames_in_pic_order_cnt_cycle) {
            for (int i = 0; i < p_sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
                expected_delta_per_pic_order_cnt_cycle += p_sps->offset_for_ref_frame[i];
            }
        }
        
        int expected_pic_order_cnt; // expectedPicOrderCnt
        if( abs_frame_num > 0 ) {
            int pic_order_cnt_cycle_cnt = (abs_frame_num - 1) / p_sps->num_ref_frames_in_pic_order_cnt_cycle; // picOrderCntCycleCnt
            int frame_num_in_pic_order_cnt_cycle = (abs_frame_num - 1) % p_sps->num_ref_frames_in_pic_order_cnt_cycle; // frameNumInPicOrderCntCycle
            expected_pic_order_cnt = pic_order_cnt_cycle_cnt * expected_delta_per_pic_order_cnt_cycle;
            for (int i = 0; i <= frame_num_in_pic_order_cnt_cycle; i++) {
                expected_pic_order_cnt += p_sps->offset_for_ref_frame[i];
            }
        } else {
            expected_pic_order_cnt = 0;
        }
        if (!slice_nal_unit_header_.nal_ref_idc ) {
            expected_pic_order_cnt += p_sps->offset_for_non_ref_pic;
        }
        
        if (!p_slice_header->field_pic_flag) {
            curr_pic_.top_field_order_cnt = expected_pic_order_cnt + p_slice_header->delta_pic_order_cnt[0];
            curr_pic_.bottom_field_order_cnt = curr_pic_.top_field_order_cnt + p_sps->offset_for_top_to_bottom_field + p_slice_header->delta_pic_order_cnt[1];
        } else if (p_slice_header->bottom_field_flag) {
            curr_pic_.bottom_field_order_cnt = expected_pic_order_cnt + p_sps->offset_for_top_to_bottom_field + p_slice_header->delta_pic_order_cnt[0];
        } else {
            curr_pic_.top_field_order_cnt = expected_pic_order_cnt + p_slice_header->delta_pic_order_cnt[0];
        }
        
        prev_frame_num_ = p_slice_header->frame_num;
        prev_frame_num_offset_t = frame_num_offset;
    } else if (p_sps->pic_order_cnt_type == 2) {
        if (slice_nal_unit_header_.nal_unit_type == kAvcNalTypeSlice_IDR) {
            frame_num_offset = 0;
            curr_pic_.top_field_order_cnt = 0;
            curr_pic_.bottom_field_order_cnt = 0;
        } else {
            if (prev_has_mmco_5_) {
                prev_frame_num_offset_t = 0;
            } else {
                /// See the end of this section
            }
            
            if (prev_frame_num_ > p_slice_header->frame_num) {
                frame_num_offset = prev_frame_num_offset_t + max_frame_num;
            } else {
                frame_num_offset = prev_frame_num_offset_t;
            }
            
            int temp_pic_order_cnt; // tempPicOrderCnt
            if (slice_nal_unit_header_.nal_ref_idc == 0) {
                temp_pic_order_cnt = 2 * (frame_num_offset + p_slice_header->frame_num) - 1;
            } else {
                temp_pic_order_cnt = 2 * (frame_num_offset + p_slice_header->frame_num);
            }
            
            if (!p_slice_header->field_pic_flag) {
                curr_pic_.top_field_order_cnt = temp_pic_order_cnt;
                curr_pic_.bottom_field_order_cnt = temp_pic_order_cnt;
            } else if (p_slice_header->bottom_field_flag) {
                curr_pic_.bottom_field_order_cnt = temp_pic_order_cnt;
            } else {
                curr_pic_.top_field_order_cnt = temp_pic_order_cnt;
            }
        }
        
        prev_frame_num_ = p_slice_header->frame_num;
        prev_frame_num_offset_t = frame_num_offset;
	}

    if (p_slice_header->field_pic_flag) {
        curr_pic_.pic_order_cnt = p_slice_header->bottom_field_flag ? curr_pic_.bottom_field_order_cnt : curr_pic_.top_field_order_cnt;
    } else {
        curr_pic_.pic_order_cnt = curr_pic_.top_field_order_cnt <= curr_pic_.bottom_field_order_cnt ? curr_pic_.top_field_order_cnt : curr_pic_.bottom_field_order_cnt;
    }
}

// 8.2.4
static inline int ComparePicNumDesc(const void *p_pic_info_1, const void *p_pic_info_2) {
    int pic_num_1 = ((AvcVideoParser::AvcPicture*)p_pic_info_1)->pic_num;
    int pic_num_2 = ((AvcVideoParser::AvcPicture*)p_pic_info_2)->pic_num;

    if (pic_num_1 < pic_num_2) {
        return 1;
    }
    if (pic_num_1 > pic_num_2) {
        return -1;
    } else {
        return 0;
    }
}

static inline int CompareLongTermPicNumAsc(const void *p_pic_info_1, const void *p_pic_info_2) {
    int long_term_pic_num_1 = ((AvcVideoParser::AvcPicture*)p_pic_info_1)->long_term_pic_num;
    int long_term_pic_num_2 = ((AvcVideoParser::AvcPicture*)p_pic_info_2)->long_term_pic_num;

    if (long_term_pic_num_1 < long_term_pic_num_2) {
        return -1;
    }
    if (long_term_pic_num_1 > long_term_pic_num_2) {
        return 1;
    } else {
        return 0;
    }
}

void AvcVideoParser::SetupReflist() {
    AvcSeqParameterSet *p_sps = &sps_list_[active_sps_id_];
    int max_frame_num = 1 << (p_sps->log2_max_frame_num_minus4 + 4);
    AvcSliceHeader *p_slice_header = &slice_header_0_;
    int i;

    // 8.2.4.1. Calculate picture numbers
    for (i = 0; i < AVC_MAX_DPB_FRAMES; i++) {
        AvcPicture *p_ref_pic = &dpb_buffer_.frame_buffer_list[i];

        if (p_ref_pic->is_reference == kUsedForShortTerm) {
            p_ref_pic->frame_num = p_ref_pic->frame_num;
            // Eq. 8-27
            if (p_ref_pic->frame_num > curr_pic_.frame_num) {
                p_ref_pic->frame_num_wrap = p_ref_pic->frame_num - max_frame_num;
            } else {
                p_ref_pic->frame_num_wrap = p_ref_pic->frame_num;
            }

            if (curr_pic_.pic_structure == kFrame) {
                p_ref_pic->pic_num = p_ref_pic->frame_num_wrap;  // Eq. 8-28
            } else if (((curr_pic_.pic_structure == kTopField) && (p_ref_pic->pic_structure == kTopField)) || ((curr_pic_.pic_structure == kBottomField) && (p_ref_pic->pic_structure == kBottomField))) {
                p_ref_pic->pic_num = 2 * p_ref_pic->frame_num_wrap + 1;  // Eq. 8-30
            } else {
                p_ref_pic->pic_num = 2 * p_ref_pic->frame_num_wrap;  // Eq. 8-31
            }
        } else if (p_ref_pic->is_reference == kUsedForLongTerm) {
            // Note: Todo: assign long_term_frame_idx in MarkDecodedRefPic()
            if (curr_pic_.pic_structure == kFrame) {
                p_ref_pic->long_term_pic_num = p_ref_pic->long_term_frame_idx;  // Eq. 8-29
            } else if (((curr_pic_.pic_structure == kTopField) && (p_ref_pic->pic_structure == kTopField)) || ((curr_pic_.pic_structure == kBottomField) && (p_ref_pic->pic_structure == kBottomField))) {
                p_ref_pic->long_term_pic_num = 2 * p_ref_pic->long_term_frame_idx + 1;  // Eq. 8-32
            } else {
                p_ref_pic->long_term_pic_num = 2 * p_ref_pic->long_term_frame_idx;  // Eq. 8-33
            }
        }
    }

    // 8.2.4.2 Initialisation process for reference picture lists
    if (p_slice_header->slice_type == kAvcSliceTypeP || p_slice_header->slice_type == kAvcSliceTypeP_5) {
        if (curr_pic_.pic_structure == kFrame) {
            // Group short term ref pictures
            int ref_index = 0;
            for (i = 0; i < AVC_MAX_DPB_FRAMES; i++) {
                AvcPicture *p_ref_pic = &dpb_buffer_.frame_buffer_list[i];
                if (p_ref_pic->is_reference == kUsedForShortTerm) {
                    ref_list_0_[ref_index] = *p_ref_pic;
                    ref_index++;
                }
            }
            // Group long term ref pictures
            for (i = 0; i < AVC_MAX_DPB_FRAMES; i++) {
                AvcPicture *p_ref_pic = &dpb_buffer_.frame_buffer_list[i];
                if (p_ref_pic->is_reference == kUsedForLongTerm) {
                    ref_list_0_[ref_index] = *p_ref_pic;
                    ref_index++;
                }
            }
            // Sort short term refs with descending order of pic_num
            if (dpb_buffer_.num_short_term) {
                qsort((void*)ref_list_0_, dpb_buffer_.num_short_term, sizeof(AvcPicture), ComparePicNumDesc);
            }
            // Sort long term refs with ascending order of long_term_pic_num
            if (dpb_buffer_.num_long_term) {
                qsort((void*)&ref_list_0_[dpb_buffer_.num_short_term], dpb_buffer_.num_long_term, sizeof(AvcPicture), CompareLongTermPicNumAsc);
            }
        } else {
            std::cout << "Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Todo: Add P ref field list init." << std::endl;
        }
    } else {
        std::cout << "Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Todo: Add B ref list init." << std::endl;
    }

    // 8.2.4.3 Modification process for reference picture lists
    if (p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l0 == 1) {
        std::cout << "Error! Todo: Added ref list 0 modification support." << std::endl;
    }

    if (p_slice_header->slice_type == kAvcSliceTypeB || p_slice_header->slice_type == kAvcSliceTypeB_6) {
        if (p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l1 == 1) {
            std::cout << "Error! Todo: Added ref list 1 modification support." << std::endl;
        }
    }
}

#if DBGINFO
void AvcVideoParser::PrintSps(AvcSeqParameterSet *p_sps) {
    uint32_t i, j;
    
    MSG("=======================");
    MSG("Sequence parameter set: ");
    MSG("=======================");
    MSG("profile_idc = " << p_sps->profile_idc);
    MSG("level_idc = " << p_sps->level_idc);
    MSG("chroma_format_idc = " << p_sps->chroma_format_idc);
    MSG("separate_colour_plane_flag = " << p_sps->separate_colour_plane_flag);
    MSG("bit_depth_luma_minus8 = " << p_sps->bit_depth_luma_minus8);
    MSG("bit_depth_chroma_minus8 = " << p_sps->bit_depth_chroma_minus8);
    MSG("qpprime_y_zero_transform_bypass_flag = " << p_sps->qpprime_y_zero_transform_bypass_flag);
    MSG("seq_scaling_matrix_present_flag = " << p_sps->seq_scaling_matrix_present_flag);
    
    MSG_NO_NEWLINE("seq_scaling_list_present_flag[12]: ");
    for (i = 0; i < 12; i++) {
        MSG_NO_NEWLINE(" " << p_sps->seq_scaling_list_present_flag[i]);
    }
    MSG("");
    
    MSG("scaling_list_4x4[6][16]:");
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 16; j++) {
            MSG_NO_NEWLINE(" " << p_sps->scaling_list_4x4[i][j]);
        }
        MSG("");
    }
    MSG("");
    
    MSG("scaling_list_8x8[6][64]:");
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 64; j++) {
            MSG_NO_NEWLINE(" " << p_sps->scaling_list_8x8[i][j]);
        }
        MSG("");
    }
    MSG("");
    
    MSG_NO_NEWLINE("use_default_scaling_matrix_4x4_flag[6]: ");
    for (i = 0; i < 6; i++) {
        MSG_NO_NEWLINE(" " << p_sps->use_default_scaling_matrix_4x4_flag[i]);
    }
    MSG("");
    
    MSG_NO_NEWLINE("use_default_scaling_matrix_8x8_flag[6]: ");
    for (i = 0; i < 6; i++) {
        MSG_NO_NEWLINE(" " << p_sps->use_default_scaling_matrix_8x8_flag[i]);
    }
    MSG("");
    
    MSG("log2_max_frame_num_minus4 = " << p_sps->log2_max_frame_num_minus4);
    MSG("pic_order_cnt_type = " << p_sps->pic_order_cnt_type);
    MSG("log2_max_pic_order_cnt_lsb_minus4 = " << p_sps->log2_max_pic_order_cnt_lsb_minus4);
    MSG("delta_pic_order_always_zero_flag = " << p_sps->delta_pic_order_always_zero_flag);
    MSG("offset_for_non_ref_pic = " << p_sps->offset_for_non_ref_pic);
    MSG("offset_for_top_to_bottom_field = " << p_sps->offset_for_top_to_bottom_field);
    MSG("num_ref_frames_in_pic_order_cnt_cycle = " << p_sps->num_ref_frames_in_pic_order_cnt_cycle);
    MSG("offset_for_ref_frame[]: ....");
    MSG("max_num_ref_frames = " << p_sps->max_num_ref_frames);
    MSG("gaps_in_frame_num_value_allowed_flag = " << p_sps->gaps_in_frame_num_value_allowed_flag);
    MSG("pic_width_in_mbs_minus1 = " << p_sps->pic_width_in_mbs_minus1);
    MSG("pic_height_in_map_units_minus1 = " << p_sps->pic_height_in_map_units_minus1);
    MSG("frame_mbs_only_flag = " << p_sps->frame_mbs_only_flag);
    MSG("mb_adaptive_frame_field_flag = " << p_sps->mb_adaptive_frame_field_flag);
    MSG("direct_8x8_inference_flag = " << p_sps->direct_8x8_inference_flag);
    MSG("frame_cropping_flag = " << p_sps->frame_cropping_flag);
    MSG("frame_crop_left_offset = " << p_sps->frame_crop_left_offset);
    MSG("frame_crop_right_offset = " << p_sps->frame_crop_right_offset);
    MSG("frame_crop_top_offset = " << p_sps->frame_crop_top_offset);
    MSG("frame_crop_bottom_offset = " << p_sps->frame_crop_bottom_offset);
    MSG("vui_parameters_present_flag = " << p_sps->vui_parameters_present_flag);
    MSG("vui_seq_parameters: ....");
    MSG("");
}

void AvcVideoParser::PrintPps(AvcPicParameterSet *p_pps) {
    uint32_t i, j;
    
    MSG("=======================");
    MSG("Picture parameter set: ");
    MSG("=======================");
    MSG("pic_parameter_set_id = " << p_pps->pic_parameter_set_id);
    MSG("seq_parameter_set_id = " << p_pps->seq_parameter_set_id);
    MSG("entropy_coding_mode_flag = " << p_pps->entropy_coding_mode_flag);
    MSG("bottom_field_pic_order_in_frame_present_flag = " << p_pps->bottom_field_pic_order_in_frame_present_flag);
    MSG("num_slice_groups_minus1 = " << p_pps->num_slice_groups_minus1);
    MSG("slice_group_map_type = " << p_pps->slice_group_map_type);
    MSG("run_length_minus1[]: ....");
    MSG("top_left[]: ....");
    MSG("bottom_right[]: ....");
    MSG("slice_group_change_direction_flag = " << p_pps->slice_group_change_direction_flag);
    MSG("slice_group_change_rate_minus1 = " << p_pps->slice_group_change_rate_minus1);
    MSG("pic_size_in_map_units_minus1 = " << p_pps->pic_size_in_map_units_minus1);
    MSG("slice_group_id[]: ....");
    MSG("num_ref_idx_l0_default_active_minus1 = " << p_pps->num_ref_idx_l0_default_active_minus1);
    MSG("num_ref_idx_l1_default_active_minus1 = " << p_pps->num_ref_idx_l1_default_active_minus1);
    MSG("weighted_pred_flag = " << p_pps->weighted_pred_flag);
    MSG("weighted_bipred_idc = " << p_pps->weighted_bipred_idc);
    MSG("pic_init_qp_minus26 = " << p_pps->pic_init_qp_minus26);
    MSG("pic_init_qs_minus26 = " << p_pps->pic_init_qs_minus26);
    MSG("chroma_qp_index_offset = " << p_pps->chroma_qp_index_offset);
    MSG("deblocking_filter_control_present_flag = " << p_pps->deblocking_filter_control_present_flag);
    MSG("constrained_intra_pred_flag = " << p_pps->constrained_intra_pred_flag);
    MSG("redundant_pic_cnt_present_flag = " << p_pps->redundant_pic_cnt_present_flag);
    MSG("transform_8x8_mode_flag = " << p_pps->transform_8x8_mode_flag);
    MSG("pic_scaling_matrix_present_flag = " << p_pps->pic_scaling_matrix_present_flag);
    
    MSG_NO_NEWLINE("pic_scaling_list_present_flag[12]: ");
    for (i = 0; i < 12; i++) {
        MSG_NO_NEWLINE(" " << p_pps->pic_scaling_list_present_flag[i]);
    }
    MSG("");
    MSG("scaling_list_4x4[6][16]:");
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 16; j++) {
            MSG_NO_NEWLINE(" " << p_pps->scaling_list_4x4[i][j]);
        }
        MSG("");
    }
    MSG("");
    
    MSG("scaling_list_8x8[6][64]:");
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 64; j++) {
            MSG_NO_NEWLINE(" " << p_pps->scaling_list_8x8[i][j]);
        }
        MSG("");
    }
    MSG("");
    
    MSG_NO_NEWLINE("use_default_scaling_matrix_4x4_flag[6]: ");
    for (i = 0; i < 6; i++) {
        MSG_NO_NEWLINE(" " << p_pps->use_default_scaling_matrix_4x4_flag[i]);
    }
    MSG("");
    
    MSG_NO_NEWLINE("use_default_scaling_matrix_8x8_flag[6]: ");
    for (i = 0; i < 6; i++) {
        MSG_NO_NEWLINE(" " << p_pps->use_default_scaling_matrix_8x8_flag[i]);
    }
    MSG("");
    
    MSG("second_chroma_qp_index_offset = " << p_pps->second_chroma_qp_index_offset);
    MSG("");
}

void AvcVideoParser::PrintSliceHeader(AvcSliceHeader *p_slice_header) {
    uint32_t i, j;
        
    MSG("======================");
    MSG("Slice header");
    MSG("======================");
    MSG("first_mb_in_slice = " << p_slice_header->first_mb_in_slice);
    MSG("slice_type = " << p_slice_header->slice_type);
    MSG("pic_parameter_set_id = " << p_slice_header->pic_parameter_set_id);
    MSG("frame_num = " << p_slice_header->frame_num);
    MSG("field_pic_flag = " << p_slice_header->field_pic_flag);
    MSG("bottom_field_flag = " << p_slice_header->bottom_field_flag);
    MSG("idr_pic_id = " << p_slice_header->idr_pic_id);
    MSG("pic_order_cnt_lsb = " << p_slice_header->pic_order_cnt_lsb);
    MSG("delta_pic_order_cnt_bottom = " << p_slice_header->delta_pic_order_cnt_bottom);
    MSG("delta_pic_order_cnt[2] =  " << p_slice_header->delta_pic_order_cnt[0] << ", " << p_slice_header->delta_pic_order_cnt[1]);
    MSG("redundant_pic_cnt = " << p_slice_header->redundant_pic_cnt);
    MSG("direct_spatial_mv_pred_flag = " << p_slice_header->direct_spatial_mv_pred_flag);
    MSG("num_ref_idx_active_override_flag = " << p_slice_header->num_ref_idx_active_override_flag);
    MSG("num_ref_idx_l0_active_minus1 = " << p_slice_header->num_ref_idx_l0_active_minus1);
    MSG("num_ref_idx_l1_active_minus1 = " << p_slice_header->num_ref_idx_l1_active_minus1);
    
    MSG("Reference picture list modification:");
    MSG("ref_pic_list_modification_flag_l0 = " << p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l0);
    if ( p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l0 ){
        MSG("Modification operations for list 0: ");
        for (j = 0; j < AVC_MAX_REF_PICTURE_NUM; j++) {
            MSG_NO_NEWLINE("(" << p_slice_header->ref_pic_list.modification_l0[j].modification_of_pic_nums_idc << ", " << p_slice_header->ref_pic_list.modification_l0[j].abs_diff_pic_num_minus1 << ", " << p_slice_header->ref_pic_list.modification_l0[j].long_term_pic_num << ") ");
        }
        MSG("");
    }
    MSG("ref_pic_list_modification_flag_l1 = " << p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l1);
    if ( p_slice_header->ref_pic_list.ref_pic_list_modification_flag_l1 ) {
        MSG("Modification operations for list 1: ");
        for (j = 0; j < AVC_MAX_REF_PICTURE_NUM; j++) {
            MSG_NO_NEWLINE("(" << p_slice_header->ref_pic_list.modification_l1[j].modification_of_pic_nums_idc << ", " << p_slice_header->ref_pic_list.modification_l1[j].abs_diff_pic_num_minus1 << ", " << p_slice_header->ref_pic_list.modification_l1[j].long_term_pic_num << ") ");
        }
        MSG("");
    }
    
    MSG("pred_weight_table: ....");
    
    MSG("Decoded reference picture marking:");
    AvcDecRefPicMarking *refMarking = &(p_slice_header->dec_ref_pic_marking);
    MSG("no_output_of_prior_pics_flag = " << refMarking->no_output_of_prior_pics_flag);
    MSG("long_term_reference_flag = " << refMarking->long_term_reference_flag);
    MSG("adaptive_ref_pic_marking_mode_flag = " << refMarking->adaptive_ref_pic_marking_mode_flag);
    if ( refMarking->adaptive_ref_pic_marking_mode_flag ) {
        MSG("mmco_count = " << refMarking->mmco_count);
        for (j = 0; j < AVC_MAX_REF_PICTURE_NUM; j++) {
            MSG_NO_NEWLINE("(" << refMarking->mmco[j].memory_management_control_operation << ", " << refMarking->mmco[j].difference_of_pic_nums_minus1 << ", " << refMarking->mmco[j].long_term_pic_num << ", " << refMarking->mmco[j].long_term_frame_idx << ", " << refMarking->mmco[j].max_long_term_frame_idx_plus1 << ") ");
        }
    }
    
    MSG("cabac_init_idc = " << p_slice_header->cabac_init_idc);
    MSG("slice_qp_delta = " << p_slice_header->slice_qp_delta);
    MSG("sp_for_switch_flag = " << p_slice_header->sp_for_switch_flag);
    MSG("slice_qs_delta = " << p_slice_header->slice_qs_delta);
    MSG("disable_deblocking_filter_idc = " << p_slice_header->disable_deblocking_filter_idc);
    MSG("slice_alpha_c0_offset_div2 = " << p_slice_header->slice_alpha_c0_offset_div2);
    MSG("slice_beta_offset_div2 = " << p_slice_header->slice_beta_offset_div2);
    MSG("slice_group_change_cycle = " << p_slice_header->slice_group_change_cycle);
}
#endif // DBGINFO