/*
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

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
#include "av1_parser.h"

Av1VideoParser::Av1VideoParser() {
    //todo: add constructor code
}

Av1VideoParser::~Av1VideoParser() {
}

rocDecStatus Av1VideoParser::Initialize(RocdecParserParams *p_params) {
    return RocVideoParser::Initialize(p_params);
}

rocDecStatus Av1VideoParser::UnInitialize() {
    return ROCDEC_SUCCESS;
}

rocDecStatus Av1VideoParser::ParseVideoData(RocdecSourceDataPacket *p_data) { 
    //to be implemented
    return ROCDEC_NOT_IMPLEMENTED;
}

void Av1VideoParser::ParseSequenceHeader(uint8_t *p_stream, size_t size) {
    Av1SequenceHeader *p_seq_header = &seq_header_;
    size_t offset = 0;  // current bit offset

    memset(p_seq_header, 0, sizeof(Av1SequenceHeader));

    p_seq_header->seq_profile = Parser::ReadBits(p_stream, offset, 3);
    p_seq_header->still_picture = Parser::GetBit(p_stream, offset);
    p_seq_header->reduced_still_picture_header = Parser::GetBit(p_stream, offset);

    if (p_seq_header->reduced_still_picture_header) {
        p_seq_header->timing_info_present_flag = 0;
        p_seq_header->decoder_model_info_present_flag = 0;
        p_seq_header->initial_display_delay_present_flag = 0;
        p_seq_header->operating_points_cnt_minus_1 = 0;
        p_seq_header->operating_point_idc[0] = 0;
        p_seq_header->seq_level_idx[0] = Parser::ReadBits(p_stream, offset, 5);
        p_seq_header->seq_tier[0] = 0;
        p_seq_header->decoder_model_present_for_this_op[0] = 0;
        p_seq_header->initial_display_delay_present_for_this_op[0] = 0;
    } else {
        p_seq_header->timing_info_present_flag = Parser::GetBit(p_stream, offset);
        if (p_seq_header->timing_info_present_flag) {
            // timing_info()
            p_seq_header->timing_info.num_units_in_display_tick = Parser::ReadBits(p_stream, offset, 32);
            p_seq_header->timing_info.time_scale = Parser::ReadBits(p_stream, offset, 32);
            p_seq_header->timing_info.equal_picture_interval = Parser::GetBit(p_stream, offset);
            if (p_seq_header->timing_info.equal_picture_interval) {
                p_seq_header->timing_info.num_ticks_per_picture_minus_1 = ReadUVLC(p_stream, offset);
            }

            p_seq_header->decoder_model_info_present_flag = Parser::GetBit(p_stream, offset);
            if (p_seq_header->decoder_model_info_present_flag) {
                p_seq_header->decoder_model_info.buffer_delay_length_minus_1 = Parser::ReadBits(p_stream, offset, 5);
                p_seq_header->decoder_model_info.num_units_in_decoding_tick = Parser::ReadBits(p_stream, offset, 32);
                p_seq_header->decoder_model_info.buffer_removal_time_length_minus_1 = Parser::ReadBits(p_stream, offset, 5);
                p_seq_header->decoder_model_info.frame_presentation_time_length_minus_1 = Parser::ReadBits(p_stream, offset, 5);
            }
        } else {
            p_seq_header->decoder_model_info_present_flag = 0;
        }

        p_seq_header->initial_display_delay_present_flag = Parser::GetBit(p_stream, offset);
        p_seq_header->operating_points_cnt_minus_1 = Parser::ReadBits(p_stream, offset, 5);
        for (int i = 0; i < p_seq_header->operating_points_cnt_minus_1 + 1; i++) {
            p_seq_header->operating_point_idc[i] = Parser::ReadBits(p_stream, offset, 12);
            p_seq_header->seq_level_idx[i] = Parser::ReadBits(p_stream, offset, 5);
            if (p_seq_header->seq_level_idx[i] > 7) {
                p_seq_header->seq_tier[i] = Parser::GetBit(p_stream, offset);
            } else {
                p_seq_header->seq_tier[i] = 0;
            }

            if (p_seq_header->decoder_model_info_present_flag) {
                p_seq_header->decoder_model_present_for_this_op[i] = Parser::GetBit(p_stream, offset);
                if (p_seq_header->decoder_model_present_for_this_op[i]) {
                    p_seq_header->operating_parameters_info[i].decoder_buffer_delay = Parser::ReadBits(p_stream, offset, p_seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1);
                    p_seq_header->operating_parameters_info[i].encoder_buffer_delay = Parser::ReadBits(p_stream, offset, p_seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1);
                    p_seq_header->operating_parameters_info[i].low_delay_mode_flag = Parser::GetBit(p_stream, offset);
                }
            } else {
                p_seq_header->decoder_model_present_for_this_op[i] = 0;
            }

            if (p_seq_header->initial_display_delay_present_flag) {
                p_seq_header->initial_display_delay_present_for_this_op[i] = Parser::GetBit(p_stream, offset);
                if (p_seq_header->initial_display_delay_present_for_this_op[i]) {
                    p_seq_header->initial_display_delay_minus_1[i] = Parser::ReadBits(p_stream, offset, 4);
                }
            }
        }
    }

    // Todo: Choose operating point.

    p_seq_header->frame_width_bits_minus_1 = Parser::ReadBits(p_stream, offset, 4);
    p_seq_header->frame_height_bits_minus_1 = Parser::ReadBits(p_stream, offset, 4);
    p_seq_header->max_frame_width_minus_1 = Parser::ReadBits(p_stream, offset, p_seq_header->frame_width_bits_minus_1 + 1);
    p_seq_header->max_frame_height_minus_1 = Parser::ReadBits(p_stream, offset, p_seq_header->frame_height_bits_minus_1 + 1);
    if (p_seq_header->reduced_still_picture_header) {
        p_seq_header->frame_id_numbers_present_flag = 0;
    } else {
        p_seq_header->frame_id_numbers_present_flag = Parser::GetBit(p_stream, offset);
    }
    if (p_seq_header->frame_id_numbers_present_flag) {
        p_seq_header->delta_frame_id_length_minus_2 = Parser::ReadBits(p_stream, offset, 4);
        p_seq_header->additional_frame_id_length_minus_1 = Parser::ReadBits(p_stream, offset, 3);
    }
    p_seq_header->use_128x128_superblock = Parser::GetBit(p_stream, offset);
    p_seq_header->enable_filter_intra = Parser::GetBit(p_stream, offset);
    p_seq_header->enable_intra_edge_filter = Parser::GetBit(p_stream, offset);

    if (p_seq_header->reduced_still_picture_header) {
        p_seq_header->enable_interintra_compound = 0;
        p_seq_header->enable_masked_compound = 0;
        p_seq_header->enable_warped_motion = 0;
        p_seq_header->enable_dual_filter = 0;
        p_seq_header->enable_order_hint = 0;
        p_seq_header->enable_jnt_comp = 0;
        p_seq_header->enable_ref_frame_mvs = 0;
        p_seq_header->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        p_seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
        p_seq_header->order_hint_bits = 0;
    } else {
        p_seq_header->enable_interintra_compound = Parser::GetBit(p_stream, offset);
        p_seq_header->enable_masked_compound = Parser::GetBit(p_stream, offset);
        p_seq_header->enable_warped_motion = Parser::GetBit(p_stream, offset);
        p_seq_header->enable_dual_filter = Parser::GetBit(p_stream, offset);
        p_seq_header->enable_order_hint = Parser::GetBit(p_stream, offset);
        if (p_seq_header->enable_order_hint) {
            p_seq_header->enable_jnt_comp = Parser::GetBit(p_stream, offset);
            p_seq_header->enable_ref_frame_mvs = Parser::GetBit(p_stream, offset);
        } else {
            p_seq_header->enable_jnt_comp = 0;
            p_seq_header->enable_ref_frame_mvs = 0;
        }

        p_seq_header->seq_choose_screen_content_tools = Parser::GetBit(p_stream, offset);
        if (p_seq_header->seq_choose_screen_content_tools) {
            p_seq_header->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        } else {
            p_seq_header->seq_force_screen_content_tools = Parser::GetBit(p_stream, offset);
        }
        if (p_seq_header->seq_force_screen_content_tools > 0) {
            p_seq_header->seq_choose_integer_mv = Parser::GetBit(p_stream, offset);
            if (p_seq_header->seq_choose_integer_mv) {
                p_seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
            } else {
                p_seq_header->seq_force_integer_mv = Parser::GetBit(p_stream, offset);
            }
        } else {
            p_seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
        }

        if (p_seq_header->enable_order_hint) {
            p_seq_header->order_hint_bits_minus_1 = Parser::ReadBits(p_stream, offset, 3);
            p_seq_header->order_hint_bits = p_seq_header->order_hint_bits_minus_1 + 1;
        } else {
            p_seq_header->order_hint_bits = 0;
        }
    }

    p_seq_header->enable_superres = Parser::GetBit(p_stream, offset);
    p_seq_header->enable_cdef = Parser::GetBit(p_stream, offset);
    p_seq_header->enable_restoration = Parser::GetBit(p_stream, offset);

    ParseColorConfig(p_stream, offset, p_seq_header);

    p_seq_header->film_grain_params_present = Parser::GetBit(p_stream, offset);
}

void Av1VideoParser::ParseColorConfig(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header) {
    p_seq_header->color_config.bit_depth = 8;
    
    p_seq_header->color_config.high_bitdepth = Parser::GetBit(p_stream, offset);
    if (p_seq_header->seq_profile == 2 && p_seq_header->color_config.high_bitdepth) {
        p_seq_header->color_config.twelve_bit = Parser::GetBit(p_stream, offset);
        p_seq_header->color_config.bit_depth = p_seq_header->color_config.twelve_bit ? 12 : 10;
    } else if (p_seq_header->seq_profile <= 2) {
        p_seq_header->color_config.bit_depth = p_seq_header->color_config.high_bitdepth ? 10 : 8;
    }

    if (p_seq_header->seq_profile == 1) {
        p_seq_header->color_config.mono_chrome = 0;
    } else {
        p_seq_header->color_config.mono_chrome = Parser::GetBit(p_stream, offset);
    }
    p_seq_header->color_config.num_planes = p_seq_header->color_config.mono_chrome ? 1 : 3;

    p_seq_header->color_config.color_description_present_flag = Parser::GetBit(p_stream, offset);
    if (p_seq_header->color_config.color_description_present_flag) {
        p_seq_header->color_config.color_primaries = Parser::ReadBits(p_stream, offset, 8);
        p_seq_header->color_config.transfer_characteristics = Parser::ReadBits(p_stream, offset, 8);
        p_seq_header->color_config.matrix_coefficients = Parser::ReadBits(p_stream, offset, 8);
    } else {
        p_seq_header->color_config.color_primaries = CP_UNSPECIFIED;
        p_seq_header->color_config.transfer_characteristics = TC_UNSPECIFIED;
        p_seq_header->color_config.matrix_coefficients = MC_UNSPECIFIED;
    }

    if (p_seq_header->color_config.mono_chrome) {
        p_seq_header->color_config.color_range = Parser::GetBit(p_stream, offset);
        p_seq_header->color_config.subsampling_x = 1;
        p_seq_header->color_config.subsampling_y = 1;
        p_seq_header->color_config.chroma_sample_position = CSP_UNKNOWN;
        p_seq_header->color_config.separate_uv_delta_q = 0;
        return;
    } else if (p_seq_header->color_config.color_primaries == CP_BT_709 && p_seq_header->color_config.transfer_characteristics == TC_SRGB && p_seq_header->color_config.matrix_coefficients == MC_IDENTITY) {
        p_seq_header->color_config.color_range = 1;
        p_seq_header->color_config.subsampling_x = 0;
        p_seq_header->color_config.subsampling_y = 0;
    } else {
        p_seq_header->color_config.color_range = Parser::GetBit(p_stream, offset);
        if (p_seq_header->seq_profile == 0) {
            p_seq_header->color_config.subsampling_x = 1;
            p_seq_header->color_config.subsampling_y = 1;
        } else if (p_seq_header->seq_profile == 1) {
            p_seq_header->color_config.subsampling_x = 0;
            p_seq_header->color_config.subsampling_y = 0;
        } else {
            if (p_seq_header->color_config.bit_depth == 12) {
                p_seq_header->color_config.subsampling_x = Parser::GetBit(p_stream, offset);
                if (p_seq_header->color_config.subsampling_x) {
                    p_seq_header->color_config.subsampling_y = Parser::GetBit(p_stream, offset);
                } else {
                    p_seq_header->color_config.subsampling_y = 0;
                }
            } else {
                p_seq_header->color_config.subsampling_x = 1;
                p_seq_header->color_config.subsampling_y = 0;
            }
        }

        if (p_seq_header->color_config.subsampling_x && p_seq_header->color_config.subsampling_y) {
            p_seq_header->color_config.chroma_sample_position = Parser::ReadBits(p_stream, offset, 2);
        }
    }

    p_seq_header->color_config.separate_uv_delta_q = Parser::GetBit(p_stream, offset);
}
