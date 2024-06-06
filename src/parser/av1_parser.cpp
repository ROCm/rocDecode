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
    seen_frame_header_ = 0;
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
    if (p_data->payload && p_data->payload_size) {
        if (ParsePictureData(p_data->payload, p_data->payload_size) != PARSER_OK) {
            ERR(STR("Parser failed!"));
            return ROCDEC_RUNTIME_ERROR;
        }

      pic_count_++;
    } else if (!(p_data->flags & ROCDEC_PKT_ENDOFSTREAM)) {
        // If no payload and EOS is not set, treated as invalid.
        return ROCDEC_INVALID_PARAMETER;
    }
    return ROCDEC_SUCCESS;
}

ParserResult Av1VideoParser::ParsePictureData(const uint8_t *p_stream, uint32_t pic_data_size) {
    ParserResult ret = PARSER_OK;
    pic_data_buffer_ptr_ = (uint8_t*)p_stream;
    pic_data_size_ = pic_data_size;
    curr_byte_offset_ = 0;

    while (ReadObuHeaderAndSize() != PARSER_EOF) {
        switch (obu_header_.obu_type) {
            case kObuTemporalDelimiter: {
                seen_frame_header_ = 0;
                break;
            }
            case kObuSequenceHeader: {
                ParseSequenceHeaderObu(pic_data_buffer_ptr_ + obu_byte_offset_, obu_size_);
                break;
            }
            case kObuFrameHeader: {
                if (seen_frame_header_ != 0) {
                    ERR("If obu_type is equal to OBU_FRAME_HEADER, it is a requirement of bitstream conformance that SeenFrameHeader is equal to 0.");
                    return PARSER_INVALID_ARG;
                }
                int bytes_parsed;
                if ((ret = ParseFrameHeaderObu(pic_data_buffer_ptr_ + obu_byte_offset_, obu_size_, &bytes_parsed)) != PARSER_OK) {
                    return ret;
                }
                break;
            }
            case kObuRedundantFrameHeader: {
                if (seen_frame_header_ != 1) {
                    ERR("If obu_type is equal to OBU_REDUNDANT_FRAME_HEADER, it is a requirement of bitstream conformance that SeenFrameHeader is equal to 1.");
                    return PARSER_INVALID_ARG;
                }
                int bytes_parsed;
                if ((ret = ParseFrameHeaderObu(pic_data_buffer_ptr_ + obu_byte_offset_, obu_size_, &bytes_parsed)) != PARSER_OK) {
                    return ret;
                }
                break;
            }
            case kObuFrame: {
                int bytes_parsed;
                if ((ret = ParseFrameHeaderObu(pic_data_buffer_ptr_ + obu_byte_offset_, obu_size_, &bytes_parsed)) != PARSER_OK) {
                    return ret;
                }
                obu_byte_offset_ += bytes_parsed;
                if (obu_size_ > bytes_parsed) {
                    obu_size_ -= bytes_parsed;
                    ParseTileGroupObu(pic_data_buffer_ptr_ + obu_byte_offset_, obu_size_);
                } else {
                    ERR("Frame OBU size error.");
                    return PARSER_OUT_OF_RANGE;
                }
                break;
            }
            case kObuTileGroup: {
                ParseTileGroupObu(pic_data_buffer_ptr_ + obu_byte_offset_, obu_size_);
                break;
            }

            default:
                break;
        }
    };
    return PARSER_OK;
}

ParserResult Av1VideoParser::ParseObuHeader(const uint8_t *p_stream) {
    size_t offset = 0;
    obu_header_.size = 1;
    if (Parser::GetBit(p_stream, offset) != 0) {
        ERR("Syntax error: obu_forbidden_bit must be set to 0.");
        return PARSER_INVALID_ARG;
    }
    obu_header_.obu_type = Parser::ReadBits(p_stream, offset, 4);
    obu_header_.obu_extension_flag = Parser::GetBit(p_stream, offset);
    obu_header_.obu_has_size_field = Parser::GetBit(p_stream, offset);
    if (!obu_header_.obu_has_size_field) {
        ERR("Syntax error: Section 5.2: obu_has_size_field must be equal to 1.");
        return PARSER_INVALID_ARG;
    }
    if (Parser::GetBit(p_stream, offset) != 0) {
        ERR("Syntax error: obu_reserved_1bit must be set to 0.");
        return PARSER_INVALID_ARG;
    }
    if (obu_header_.obu_extension_flag) {
        obu_header_.size += 1;
        obu_header_.temporal_id = Parser::ReadBits(p_stream, offset, 3);
        obu_header_.spatial_id = Parser::ReadBits(p_stream, offset, 2);
        if (Parser::ReadBits(p_stream, offset, 3) != 0) {
            ERR("Syntax error: extension_header_reserved_3bits must be set to 0.\n");
        return PARSER_INVALID_ARG;
        }
    }
    return PARSER_OK;
}

ParserResult Av1VideoParser::ReadObuHeaderAndSize() {
    ParserResult ret = PARSER_OK;
    if (curr_byte_offset_ >= pic_data_size_) {
        return PARSER_EOF;
    }
    uint8_t *p_stream = pic_data_buffer_ptr_ + curr_byte_offset_;
    if ((ret = ParseObuHeader(p_stream)) != PARSER_OK) {
        return ret;
    }
    curr_byte_offset_ += obu_header_.size;
    p_stream += obu_header_.size;

    uint32_t bytes_read;
    obu_size_ = ReadLeb128(p_stream, &bytes_read);
    obu_byte_offset_ = curr_byte_offset_ + bytes_read;
    curr_byte_offset_ = obu_byte_offset_ + obu_size_;
    return PARSER_OK;
}

void Av1VideoParser::ParseSequenceHeaderObu(uint8_t *p_stream, size_t size) {
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

ParserResult Av1VideoParser::ParseFrameHeaderObu(uint8_t *p_stream, size_t size, int *p_bytes_parsed) {
    if (seen_frame_header_ == 1) {
        // frame_header_copy(). Use the existing frame_header_obu
    } else {
        seen_frame_header_ = 1;
        ParserResult ret;
        if ((ret = ParseUncompressedHeader(p_stream, size, p_bytes_parsed)) != PARSER_OK) {
            return ret;
        }
        if (frame_header_.show_existing_frame) {
            // decode_frame_wrapup()
            seen_frame_header_ = 0;
        } else {
            tile_num_ = 0;
            seen_frame_header_ = 1;
        }
    }
    return PARSER_OK;
}

ParserResult Av1VideoParser::ParseUncompressedHeader(uint8_t *p_stream, size_t size, int *p_bytes_parsed) {
    size_t offset = 0;  // current bit offset
    Av1SequenceHeader *p_seq_header = &seq_header_;
    Av1FrameHeader *p_frame_header = &frame_header_;
    uint32_t frame_id_len = 0;
    uint32_t all_frames = (1 << NUM_REF_FRAMES) - 1;
    int i;

    memset(p_frame_header, 0, sizeof(Av1FrameHeader));

    if (p_seq_header->frame_id_numbers_present_flag) {
        frame_id_len = p_seq_header->additional_frame_id_length_minus_1 + p_seq_header-> delta_frame_id_length_minus_2 + 3;
    }

    if (p_seq_header->reduced_still_picture_header) {
        p_frame_header->show_existing_frame = 0;
        p_frame_header->frame_type = kKeyFrame;
        p_frame_header->frame_is_intra = 1;
        p_frame_header->show_frame = 1;
        p_frame_header->showable_frame = 0;
    } else {
        p_frame_header->show_existing_frame = Parser::GetBit(p_stream, offset);
        if (p_frame_header->show_existing_frame == 1) {
            p_frame_header->frame_to_show_map_idx = Parser::ReadBits(p_stream, offset, 3);
            if (p_seq_header->decoder_model_info_present_flag && !p_seq_header->timing_info.equal_picture_interval) {
                // temporal_point_info()
                p_frame_header->temporal_point_info.frame_presentation_time = Parser::ReadBits(p_stream, offset, p_seq_header->decoder_model_info.frame_presentation_time_length_minus_1 + 1);
            }
            p_frame_header->refresh_frame_flags = 0;
            if (p_seq_header->frame_id_numbers_present_flag) {
                p_frame_header->display_frame_id = Parser::ReadBits(p_stream, offset, frame_id_len);
            }
            p_frame_header->frame_type = ref_frame_type_[p_frame_header->frame_to_show_map_idx];
            if (p_frame_header->frame_type == kKeyFrame) {
                p_frame_header->refresh_frame_flags = all_frames;
            }
            if (p_seq_header->film_grain_params_present) {
                // Todo
                ERR("Film grain param loading not implemented.\n");
                // load_grain_params(p_frame_header->frame_to_show_map_idx);
                return PARSER_NOT_IMPLEMENTED;
            }

            return PARSER_OK;
        }

        p_frame_header->frame_type = Parser::ReadBits(p_stream, offset, 2);
        p_frame_header->frame_is_intra = (p_frame_header->frame_type == kIntraOnlyFrame) || (p_frame_header->frame_type == kKeyFrame);
        p_frame_header->show_frame = Parser::GetBit(p_stream, offset);
        if (p_frame_header->show_frame && p_seq_header->decoder_model_info_present_flag && !p_seq_header->timing_info.equal_picture_interval) {
            // temporal_point_info()
            p_frame_header->temporal_point_info.frame_presentation_time = Parser::ReadBits(p_stream, offset, p_seq_header->decoder_model_info.frame_presentation_time_length_minus_1 + 1);
        }
        if (p_frame_header->show_frame) {
            p_frame_header->showable_frame = p_frame_header->frame_type != kKeyFrame;
        } else {
            p_frame_header->showable_frame = Parser::GetBit(p_stream, offset);
        }
        if (p_frame_header->frame_type == kSwitchFrame || (p_frame_header->frame_type == kKeyFrame && p_frame_header->show_frame)) {
            p_frame_header->error_resilient_mode = 1;
        } else {
            p_frame_header->error_resilient_mode = Parser::GetBit(p_stream, offset);
        }
    }

    if (p_frame_header->frame_type == kKeyFrame && p_frame_header->show_frame) {
        for (i = 0; i < NUM_REF_FRAMES; i++) {
            ref_valid_[i] = 0;
            ref_order_hint_[i] = 0;
        }
        for (i = 0; i < REFS_PER_FRAME; i++) {
            p_frame_header->order_hints[kLastFrame + i] = 0;
        }
    }

    p_frame_header->disable_cdf_update = Parser::GetBit(p_stream, offset);
    if (p_seq_header->seq_force_screen_content_tools == SELECT_SCREEN_CONTENT_TOOLS) {
        p_frame_header->allow_screen_content_tools = Parser::GetBit(p_stream, offset);
    } else {
        p_frame_header->allow_screen_content_tools = p_seq_header->seq_force_screen_content_tools;
    }

    if (p_frame_header->allow_screen_content_tools) {
        if (p_seq_header->seq_force_integer_mv == SELECT_INTEGER_MV) {
            p_frame_header->force_integer_mv = Parser::GetBit(p_stream, offset);
        } else {
            p_frame_header->force_integer_mv = p_seq_header->seq_force_integer_mv;
        }
    } else {
        p_frame_header->force_integer_mv = 0;
    }
    if (p_frame_header->frame_is_intra) {
        p_frame_header->force_integer_mv = 1;
    }

    if (p_seq_header->frame_id_numbers_present_flag) {
        p_frame_header->prev_frame_id = p_frame_header->current_frame_id;
        p_frame_header->current_frame_id = Parser::ReadBits(p_stream, offset, frame_id_len);
        MarkRefFrames(p_seq_header, p_frame_header, frame_id_len);
    } else {
        p_frame_header->current_frame_id = 0;
    }

    if (p_frame_header->frame_type == kSwitchFrame) {
        p_frame_header->frame_size_override_flag = 1;
    } else if (p_seq_header->reduced_still_picture_header) {
        p_frame_header->frame_size_override_flag = 0;
    } else {
        p_frame_header->frame_size_override_flag = Parser::GetBit(p_stream, offset);
    }

    p_frame_header->order_hint = Parser::ReadBits(p_stream, offset, p_seq_header->order_hint_bits);
    if (p_frame_header->frame_is_intra || p_frame_header->error_resilient_mode) {
        p_frame_header->primary_ref_frame = PRIMARY_REF_NONE;
    } else {
        p_frame_header->primary_ref_frame = Parser::ReadBits(p_stream, offset, 3);
    }

    if (p_seq_header->decoder_model_info_present_flag) {
        p_frame_header->buffer_removal_time_present_flag = Parser::GetBit(p_stream, offset);
        if (p_frame_header->buffer_removal_time_present_flag) {
            for (int op_num = 0; op_num <= p_seq_header->operating_points_cnt_minus_1; op_num++) {
                if (p_seq_header->decoder_model_present_for_this_op[op_num]) {
                    uint32_t op_pt_idc = p_seq_header->operating_point_idc[op_num];
                    uint32_t in_temporal_layer = (op_pt_idc >> temporal_id_) & 1;
                    uint32_t in_spatial_layer = (op_pt_idc >> (spatial_id_ + 8)) & 1;
                    if (op_pt_idc == 0 || (in_temporal_layer && in_spatial_layer)) {
                        p_frame_header->buffer_removal_time[op_num] = Parser::ReadBits(p_stream, offset, p_seq_header->decoder_model_info.buffer_removal_time_length_minus_1 + 1);
                    }
                }
            }
        }
    }

    p_frame_header->allow_high_precision_mv = 0;
    p_frame_header->use_ref_frame_mvs = 0;
    p_frame_header->allow_intrabc = 0;

    if (p_frame_header->frame_type == kSwitchFrame || (p_frame_header->frame_type == kKeyFrame && p_frame_header->show_frame)) {
        p_frame_header->refresh_frame_flags = all_frames;
    } else {
        p_frame_header->refresh_frame_flags = Parser::ReadBits(p_stream, offset, 8);
    }
    // Clear reference list for kKeyFrame
    if (p_frame_header->frame_type == kKeyFrame) {
        for (i = 0; i < REFS_PER_FRAME; i++) {
            ref_pictures_[i].index = INVALID_INDEX;
        }
    }
    if (!p_frame_header->frame_is_intra || p_frame_header->refresh_frame_flags != all_frames) {
        if (p_frame_header->error_resilient_mode && p_seq_header->enable_order_hint) {
            for (i = 0; i < NUM_REF_FRAMES; i++) {
                p_frame_header->ref_order_hint[i] = Parser::ReadBits(p_stream, offset, p_seq_header->order_hint_bits);
                if (p_frame_header->ref_order_hint[i] != ref_order_hint_[i]) {
                    ref_valid_[i] = 0;
                }
            }
        }
    }

    if (p_frame_header->frame_is_intra) {
        FrameSize(p_stream, offset, p_seq_header, p_frame_header);
        RenderSize(p_stream, offset, p_frame_header);
        if (p_frame_header->allow_screen_content_tools && p_frame_header->frame_size.upscaled_width == p_frame_header->frame_size.frame_width) {
            p_frame_header->allow_intrabc = Parser::GetBit(p_stream, offset);
        }
    } else {
        if (!p_seq_header->enable_order_hint) {
            p_frame_header->frame_refs_short_signaling = 0;
        } else {
            p_frame_header->frame_refs_short_signaling = Parser::GetBit(p_stream, offset);
            if (p_frame_header->frame_refs_short_signaling) {
                p_frame_header->last_frame_idx = Parser::ReadBits(p_stream, offset, 3);
                p_frame_header->gold_frame_idx = Parser::ReadBits(p_stream, offset, 3);
                // 7.8. Set frame refs process
                SetFrameRefs(p_seq_header, p_frame_header);
            }
        }

        for (int i = 0; i < REFS_PER_FRAME; i++) {
            if (!p_frame_header->frame_refs_short_signaling) {
                p_frame_header->ref_frame_idx[i] = Parser::ReadBits(p_stream, offset, 3);
            }
            if (p_seq_header->frame_id_numbers_present_flag) {
                p_frame_header->delta_frame_id_minus_1 = Parser::ReadBits(p_stream, offset, p_seq_header->delta_frame_id_length_minus_2 + 2);
                uint32_t delta_frame_id = p_frame_header->delta_frame_id_minus_1 + 1;
                p_frame_header->expected_frame_id[i] = ((p_frame_header->current_frame_id + (1 << frame_id_len) - delta_frame_id ) % (1 << frame_id_len));
                if (p_frame_header->expected_frame_id[i] != ref_frame_id_[p_frame_header->ref_frame_idx[i]] || ref_valid_[p_frame_header->ref_frame_idx[i]] == 0) {
                    ERR("Syntax Error: Reference buffer frame ID mismatch.\n");
                    return PARSER_INVALID_ARG;
                }
            }
        }

        if (p_frame_header->frame_size_override_flag && !p_frame_header->error_resilient_mode) {
            FrameSizeWithRefs(p_stream, offset, p_seq_header, p_frame_header);
        } else {
            FrameSize(p_stream, offset, p_seq_header, p_frame_header);
            RenderSize(p_stream, offset, p_frame_header);
        }

        if (p_frame_header->force_integer_mv) {
            p_frame_header->allow_high_precision_mv = 0;
        } else {
            p_frame_header->allow_high_precision_mv = Parser::GetBit(p_stream, offset);
        }

        // read_interpolation_filter()
        p_frame_header->is_filter_switchable = Parser::GetBit(p_stream, offset);
        if (p_frame_header->is_filter_switchable == 1) {
            p_frame_header->interpolation_filter = kSwitchable;
        } else {
            p_frame_header->interpolation_filter = Parser::ReadBits(p_stream, offset, 2);
        }
        p_frame_header->is_motion_mode_switchable = Parser::GetBit(p_stream, offset);
        if (p_frame_header->error_resilient_mode || !p_seq_header->enable_ref_frame_mvs) {
            p_frame_header->use_ref_frame_mvs = 0;
        } else {
            p_frame_header->use_ref_frame_mvs = Parser::GetBit(p_stream, offset);
        }

        for (i = 0; i < REFS_PER_FRAME; i++) {
            uint32_t ref_frame = kLastFrame + i;
            uint32_t hint = ref_order_hint_[p_frame_header->ref_frame_idx[i]];
            p_frame_header->order_hints[ref_frame] = hint;
            if (!p_seq_header->enable_order_hint) {
                p_frame_header->ref_frame_sign_bias[ref_frame] = 0;
            } else {
                p_frame_header->ref_frame_sign_bias[ref_frame] = GetRelativeDist(p_seq_header, hint, p_frame_header->order_hint) > 0;
            }
        }
    }

    // Generate reference map for the next picture
    int ref_index = 0;
    for (int mask = p_frame_header->refresh_frame_flags; mask; mask >>= 1) {
        if (mask & 1) {
            ref_pic_map_next_[ref_index] = new_fb_index_;
        } else {
            ref_pic_map_next_[ref_index] = ref_pic_map_[ref_index];
        }
        ++ref_index;
    }
    for (; ref_index < NUM_REF_FRAMES; ++ref_index) {
        ref_pic_map_next_[ref_index] = ref_pic_map_[ref_index];
    }

    if (p_seq_header->reduced_still_picture_header || p_frame_header->disable_cdf_update) {
        p_frame_header->disable_frame_end_update_cdf = 1;
    } else {
        p_frame_header->disable_frame_end_update_cdf = Parser::GetBit(p_stream, offset);
    }

    if (p_frame_header->primary_ref_frame == PRIMARY_REF_NONE) {
        // Todo: check need for implementation
        //init_non_coeff_cdfs();
        SetupPastIndependence(p_frame_header);
    } else {
        // Todo: check need for implementation
        //load_cdfs();
        //load_previous();
    }

    if (p_frame_header->use_ref_frame_mvs == 1) {
        // Todo: check need for implementation
        //motion_field_estimation());
    }

    TileInfo(p_stream, offset, p_seq_header, p_frame_header);
    QuantizationParams(p_stream, offset, p_seq_header, p_frame_header);
    SegmentationParams(p_stream, offset, p_frame_header);
    DeltaQParams(p_stream, offset, p_frame_header);
    DeltaLFParams(p_stream, offset, p_frame_header);

    if (p_frame_header->primary_ref_frame == PRIMARY_REF_NONE) {
        // Todo: check need for implementation
        // init_coeff_cdfs();
    } else {
        // Todo: check need for implementation
        //load_previous_segment_ids();
    }

    p_frame_header->coded_lossless = 1;
    for (int segment_id = 0; segment_id < MAX_SEGMENTS; segment_id++) {
        int qindex = GetQIndex(p_frame_header, 1, segment_id);
        p_frame_header->lossless_array[segment_id] = qindex == 0 && p_frame_header->quantization_params.delta_q_y_dc == 0 && p_frame_header->quantization_params.delta_q_u_ac == 0 && p_frame_header->quantization_params.delta_q_u_dc == 0 && p_frame_header->quantization_params.delta_q_v_ac == 0 && p_frame_header->quantization_params.delta_q_v_dc == 0;
        if (!p_frame_header->lossless_array[segment_id]) {
            p_frame_header->coded_lossless = 0;
        }
        if (p_frame_header->quantization_params.using_qmatrix) {
            if (p_frame_header->lossless_array[segment_id]) {
                p_frame_header->seg_qm_level[0][segment_id] = 15;
                p_frame_header->seg_qm_level[1][segment_id] = 15;
                p_frame_header->seg_qm_level[2][segment_id] = 15;
            } else {
                p_frame_header->seg_qm_level[0][segment_id] = p_frame_header->quantization_params.qm_y;
                p_frame_header->seg_qm_level[1][segment_id] = p_frame_header->quantization_params.qm_u;
                p_frame_header->seg_qm_level[2][segment_id] = p_frame_header->quantization_params.qm_v;
            }
        }
    }

    p_frame_header->all_lossless = p_frame_header->coded_lossless && (p_frame_header->frame_size.frame_width == p_frame_header->frame_size.upscaled_width);

    LoopFilterParams(p_stream, offset, p_seq_header, p_frame_header);
    CdefParams(p_stream, offset, p_seq_header, p_frame_header);
    LrParams(p_stream, offset, p_seq_header, p_frame_header);
    ReadTxMode(p_stream, offset, p_frame_header);

    // frame_reference_mode()
    if (p_frame_header->frame_is_intra) {
        p_frame_header->frame_reference_mode.reference_select = 0;
    } else {
        p_frame_header->frame_reference_mode.reference_select = Parser::GetBit(p_stream, offset);
    }

    SkipModeParams(p_stream, offset, p_seq_header, p_frame_header);

    if (p_frame_header->frame_is_intra || p_frame_header->error_resilient_mode || !p_seq_header->enable_warped_motion) {
        p_frame_header->allow_warped_motion = 0;
    } else {
        p_frame_header->allow_warped_motion = Parser::GetBit(p_stream, offset);
    }

    p_frame_header->reduced_tx_set = Parser::GetBit(p_stream, offset);

    GlobalMotionParams(p_stream, offset, p_frame_header);
    FilmGrainParams(p_stream, offset, p_seq_header, p_frame_header);

    // Update reference frames
    for (i = 0; i < NUM_REF_FRAMES; i++) {
        if ((p_frame_header->refresh_frame_flags >> i) & 1) {
            ref_order_hint_[i] = p_frame_header->order_hint;
        }
    }

    *p_bytes_parsed = (offset + 7) >> 3;
    return PARSER_OK;
}

void Av1VideoParser::ParseTileGroupObu(uint8_t *p_stream, size_t size) {
    size_t offset = 0;  // current bit offset
    Av1SequenceHeader *p_seq_header = &seq_header_;
    Av1FrameHeader *p_frame_header = &frame_header_;
    Av1TileGroupDataInfo *p_tile_group = &tile_group_data_;
    uint32_t num_tiles;
    uint32_t tile_start_and_end_present_flag = 0;
    uint32_t tg_start, tg_end;
    uint32_t header_types = 0;
    uint32_t tile_cols = p_frame_header->tile_info.tile_cols;
    uint32_t tile_rows = p_frame_header->tile_info.tile_rows;
    uint8_t *p_tg_buf = p_stream;
    uint32_t tg_size = size;

    memset(p_tile_group, 0, sizeof(Av1TileGroupDataInfo));
    p_tile_group->buffer_ptr = p_tg_buf;
    p_tile_group->buffer_size = tg_size;

    // First parse the header
    num_tiles = tile_cols * tile_rows;
    if (num_tiles > 1) {
        tile_start_and_end_present_flag = Parser::GetBit(p_stream, offset);
    }
    if (num_tiles == 1 || !tile_start_and_end_present_flag) {
        tg_start = 0;
        tg_end = num_tiles - 1;
    } else {
        uint32_t tile_bits = p_frame_header->tile_info.tile_cols_log2 + p_frame_header->tile_info.tile_rows_log2;
        tg_start = Parser::ReadBits(p_stream, offset, tile_bits);
        tg_end = Parser::ReadBits(p_stream, offset, tile_bits);
    }

    header_types = ((offset + 7) >> 3);
    p_tg_buf += header_types;
    tg_size -= header_types;
    for (int tile_num = tg_start; tile_num <= tg_end; tile_num++) {
        int tile_row = tile_num / tile_cols;
        int tile_col = tile_num % tile_cols;
        int last_tile = tile_num == tg_end;
        if (last_tile) {
            p_tile_group->tile_data_info[tile_row][tile_col].size = tg_size;
            p_tile_group->tile_data_info[tile_row][tile_col].offset = p_tg_buf - p_tile_group->buffer_ptr;
        } else {
            uint32_t tile_size_bytes = p_frame_header->tile_info.tile_size_bytes_minus_1 + 1;
            uint32_t tile_size = ReadLeBytes(p_tg_buf, tile_size_bytes) + 1;
            p_tile_group->tile_data_info[tile_row][tile_col].size = tile_size;
            p_tile_group->tile_data_info[tile_row][tile_col].offset = p_tg_buf + tile_size_bytes - p_tile_group->buffer_ptr;
            tg_size -= tile_size + tile_size_bytes;
            p_tg_buf += tile_size + tile_size_bytes;
        }
    }

    if (tg_end == num_tiles - 1) {
        if (!frame_header_.disable_frame_end_update_cdf) {
            //frame_end_update_cdf();
        }
        //decode_frame_wrapup();
        seen_frame_header_ = 0;
    }
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

void Av1VideoParser::MarkRefFrames(Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header, uint32_t id_len) {
    int diff_len = p_seq_header->delta_frame_id_length_minus_2 + 2;
    for (int i = 0; i < NUM_REF_FRAMES; i++) {
        if (p_frame_header->frame_type == kKeyFrame && p_frame_header->show_frame) {
            ref_valid_[i] = 0;
        } else if (p_frame_header->current_frame_id > (1 << diff_len)) {
            if (ref_frame_id_[i] > p_frame_header->current_frame_id || ref_frame_id_[i] < (p_frame_header->current_frame_id - (1 << diff_len))) {
                ref_valid_[i] = 0;
            }
        } else {
            if (ref_frame_id_[i] > p_frame_header->current_frame_id && ref_frame_id_[i] < ((1 << id_len) + p_frame_header->current_frame_id - (1 << diff_len))) {
                ref_valid_[ i ] = 0;
            }
        }
    }
}

void Av1VideoParser::FrameSize(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    if (p_frame_header->frame_size_override_flag) {
        p_frame_header->frame_size.frame_width_minus_1 = Parser::ReadBits(p_stream, offset, p_seq_header->frame_width_bits_minus_1 + 1);
        p_frame_header->frame_size.frame_width = p_frame_header->frame_size.frame_width_minus_1 + 1;
        p_frame_header->frame_size.frame_height_minus_1 = Parser::ReadBits(p_stream, offset, p_seq_header->frame_height_bits_minus_1 + 1);
        p_frame_header->frame_size.frame_height = p_frame_header->frame_size.frame_height_minus_1 + 1;
    } else {
        p_frame_header->frame_size.frame_width = p_seq_header->max_frame_width_minus_1 + 1;
        p_frame_header->frame_size.frame_height = p_seq_header->max_frame_height_minus_1 + 1;
    }
    SuperResParams(p_stream, offset, p_seq_header, p_frame_header);
    ComputeImageSize(p_frame_header);
}

void Av1VideoParser::SuperResParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    uint32_t super_res_denom;
    if (p_seq_header->enable_superres) {
        p_frame_header->frame_size.superres_params.use_superres = Parser::GetBit(p_stream, offset);
    } else {
        p_frame_header->frame_size.superres_params.use_superres = 0;
    }
    if (p_frame_header->frame_size.superres_params.use_superres) {
        p_frame_header->frame_size.superres_params.coded_denom = Parser::ReadBits(p_stream, offset, SUPERRES_DENOM_BITS);
        super_res_denom = p_frame_header->frame_size.superres_params.coded_denom + SUPERRES_DENOM_MIN;
    } else {
        super_res_denom = SUPERRES_NUM;
    }
    p_frame_header->frame_size.upscaled_width = p_frame_header->frame_size.frame_width;
    p_frame_header->frame_size.frame_width = (p_frame_header->frame_size.upscaled_width * SUPERRES_NUM + (super_res_denom / 2)) / super_res_denom;
}

void Av1VideoParser::ComputeImageSize(Av1FrameHeader *p_frame_header) {
    p_frame_header->frame_size.mi_cols = 2 * ((p_frame_header->frame_size.frame_width + 7) >> 3);
    p_frame_header->frame_size.mi_rows = 2 * ((p_frame_header->frame_size.frame_height + 7) >> 3);
}

void Av1VideoParser::RenderSize(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    p_frame_header->render_size.render_and_frame_size_different = Parser::GetBit(p_stream, offset);
    if (p_frame_header->render_size.render_and_frame_size_different) {
        p_frame_header->render_size.render_width_minus_1 = Parser::ReadBits(p_stream, offset, 16);
        p_frame_header->render_size.render_height_minus_1 = Parser::ReadBits(p_stream, offset, 16);
        p_frame_header->render_size.render_width = p_frame_header->render_size.render_width_minus_1 + 1;
        p_frame_header->render_size.render_height = p_frame_header->render_size.render_height_minus_1 + 1;
    } else {
        p_frame_header->render_size.render_width = p_frame_header->frame_size.upscaled_width;
        p_frame_header->render_size.render_height = p_frame_header->frame_size.frame_height;
    }
}

void Av1VideoParser::SetFrameRefs(Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    int i;
    int used_frame[NUM_REF_FRAMES];
    int curr_frame_hint;
    int shifted_order_hints[NUM_REF_FRAMES];
    int ref;

    for (i = 0; i < REFS_PER_FRAME; i++) {
        p_frame_header->ref_frame_idx[i] = -1;
    }
    p_frame_header->ref_frame_idx[kLastFrame - kLastFrame] = p_frame_header->last_frame_idx;
    p_frame_header->ref_frame_idx[kGoldenFrame - kLastFrame] = p_frame_header->gold_frame_idx;
    for (i = 0; i < NUM_REF_FRAMES; i++) {
        used_frame[ i ] = 0;
    }
    used_frame[p_frame_header->last_frame_idx] = 1;
    used_frame[p_frame_header->gold_frame_idx ] = 1;

    curr_frame_hint = 1 << (p_seq_header->order_hint_bits - 1);
    for (i = 0; i < NUM_REF_FRAMES; i++) {
        shifted_order_hints[i] = curr_frame_hint + GetRelativeDist(p_seq_header, ref_order_hint_[i], p_frame_header->order_hint);
    }

    // The kAltRefFrame reference is set to be a backward reference to the frame with highest output order.
    ref = FindLatestBackward(shifted_order_hints, used_frame, curr_frame_hint);
    if (ref >= 0) {
        p_frame_header->ref_frame_idx[kAltRefFrame - kLastFrame] = ref;
        used_frame[ref] = 1;
    }
    // The kBwdRefFrame reference is set to be a backward reference to the closest frame.
    ref = FindEarliestBackward(shifted_order_hints, used_frame, curr_frame_hint);
    if ( ref >= 0 )
    {
        p_frame_header->ref_frame_idx[kBwdRefFrame - kLastFrame] = ref;
        used_frame[ref] = 1;
    }
    // The kAltRef2Frame reference is set to the next closest backward reference.
    ref = FindEarliestBackward(shifted_order_hints, used_frame, curr_frame_hint);
    if ( ref >= 0 )
    {
        p_frame_header->ref_frame_idx[kAltRef2Frame - kLastFrame] = ref;
        used_frame[ref] = 1;
    }

    // The remaining references are set to be forward references in anti-chronological order.
    int ref_frame_list[REFS_PER_FRAME - 2] = {kLast2Frame, kLast3Frame, kBwdRefFrame, kAltRef2Frame, kAltRefFrame};
    for (i = 0; i < REFS_PER_FRAME - 2; i++) {
        int refFrame = ref_frame_list[i];
        if (p_frame_header->ref_frame_idx[refFrame - kLastFrame] < 0) {
            ref = FindLatestForward(shifted_order_hints, used_frame, curr_frame_hint);
            if (ref >= 0) {
                p_frame_header->ref_frame_idx[refFrame - kLastFrame] = ref;
                used_frame[ref] = 1;
            }
        }
    }

    // Finally, any remaining references are set to the reference frame with smallest output order.
    ref = -1;
    for (i = 0; i < NUM_REF_FRAMES; i++) {
        int earliest_order_hint = 9999;
        int hint = shifted_order_hints[i];
        if (ref < 0 || hint < earliest_order_hint) {
            ref = i;
            earliest_order_hint = hint;
        }
    }
    for (i = 0; i < REFS_PER_FRAME; i++) {
        if (p_frame_header->ref_frame_idx[i] < 0) {
            p_frame_header->ref_frame_idx[i] = ref;
        }
    }
}

int Av1VideoParser::GetRelativeDist(Av1SequenceHeader *p_seq_header, int a, int b) {
    if (!p_seq_header->enable_order_hint) {
        return 0;
    }
    int diff = a - b;
    int m = 1 << (p_seq_header->order_hint_bits - 1);
    diff = (diff & (m - 1)) - (diff & m);
    return diff;
}

int Av1VideoParser::FindLatestBackward(int *shifted_order_hints, int *used_frame, int curr_frame_hint) {
    int ref = -1;
    int hint;
    int latest_order_hint = -9999;

    for (int i = 0; i < NUM_REF_FRAMES; i++) {
        hint = shifted_order_hints[i];
        if (!used_frame[i] && hint >= curr_frame_hint && (ref < 0 || hint >= latest_order_hint)) {
            ref = i;
            latest_order_hint = hint;
        }
    }
    return ref;
}

int Av1VideoParser::FindEarliestBackward(int *shifted_order_hints, int *used_frame, int curr_frame_hint) {
    int ref = -1;
    int hint;
    int earliest_order_hint = 9999;

    for (int i = 0; i < NUM_REF_FRAMES; i++) {
        hint = shifted_order_hints[i];
        if (!used_frame[i] && hint >= curr_frame_hint && (ref < 0 || hint < earliest_order_hint)) {
            ref = i;
            earliest_order_hint = hint;
        }
    }
    return ref;
}

int Av1VideoParser::FindLatestForward(int *shifted_order_hints, int *used_frame, int curr_frame_hint) {
    int ref = -1;
    int hint;
    int latest_order_hint = -9999;

    for (int i = 0; i < NUM_REF_FRAMES; i++) {
        hint = shifted_order_hints[i];
        if (!used_frame[i] && hint < curr_frame_hint && (ref < 0 || hint >= latest_order_hint)) {
            ref = i;
            latest_order_hint = hint;
        }
    }
    return ref;
}

void Av1VideoParser::FrameSizeWithRefs(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    for (int i = 0; i < REFS_PER_FRAME; i++) {
        p_frame_header->found_ref = Parser::GetBit(p_stream, offset);
        if (p_frame_header->found_ref) {
            // Todo
            ERR("Warning: Need to implement! found_ref == 1 case.\n");
#if 0
            UpscaledWidth = RefUpscaledWidth[ ref_frame_idx[ i ] ]
            FrameWidth = UpscaledWidth
            FrameHeight = RefFrameHeight[ ref_frame_idx[ i ] ]
            RenderWidth = RefRenderWidth[ ref_frame_idx[ i ] ]
            RenderHeight = RefRenderHeight[ ref_frame_idx[ i ] ]
#endif
            break;
        }
    }

    if (p_frame_header->found_ref == 0) {
        FrameSize(p_stream, offset, p_seq_header, p_frame_header);
        RenderSize(p_stream, offset, p_frame_header);
    } else {
        SuperResParams(p_stream, offset, p_seq_header, p_frame_header);
        ComputeImageSize(p_frame_header);
    }
}

void Av1VideoParser::SetupPastIndependence(Av1FrameHeader *p_frame_header) {
    for (int i = 0; i < MAX_SEGMENTS; i++) {
        for (int j = 0; j < SEG_LVL_MAX; j++) {
            p_frame_header->segmentation_params.feature_data[i][j] = 0;
            p_frame_header->segmentation_params.feature_enabled_flags[i][j] = 0;
        }
    }
    // Block level: PrevSegmentIds[ row ][ col ] is set equal to 0 for row = 0..MiRows-1 and col = 0..MiCols-1.
    for (int ref = kLastFrame; ref <= kAltRefFrame; ref++) {
        p_frame_header->global_motion_params.gm_type[ref] = kIdentity;
        for (int i = 0; i < 6; i++) {
            prev_gm_params_[ref][i] = (i % 3 == 2) ? 1 << WARPEDMODEL_PREC_BITS : 0;
        }
    }

    p_frame_header->loop_filter_params.loop_filter_delta_enabled = 1;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kIntraFrame] = 1;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kLastFrame] = 0;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kLast2Frame] = 0;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kLast3Frame] = 0;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kBwdRefFrame] = 0;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kGoldenFrame] = -1;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kAltRefFrame] = -1;
    p_frame_header->loop_filter_params.loop_filter_ref_deltas[kAltRef2Frame] = -1;
    p_frame_header->loop_filter_params.loop_filter_mode_deltas[0] = 0;
    p_frame_header->loop_filter_params.loop_filter_mode_deltas[1] = 0;
}

void Av1VideoParser::TileInfo(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    int32_t sb_cols;
    int32_t sb_rows;
    int32_t sb_shift;
    int32_t sb_size;
    int32_t max_tile_width_sb;
    int32_t max_tile_height_sb;
    int32_t max_tile_area_sb;
    int32_t min_log2_tile_cols;
    int32_t min_log2_tile_rows;
    int32_t max_log2_tile_cols;
    int32_t max_log2_tile_rows;
    int32_t min_log2_tiles;
    int32_t tile_width_sb;
    int32_t tile_height_sb;
    int32_t max_width;
    int32_t max_height;
    int32_t size_sb;
    int start_sb;
    int i;

    sb_cols = p_seq_header->use_128x128_superblock ? ((p_frame_header->frame_size.mi_cols + 31) >> 5) : ((p_frame_header->frame_size.mi_cols + 15) >> 4);
    sb_rows = p_seq_header->use_128x128_superblock ? ((p_frame_header->frame_size.mi_rows + 31) >> 5) : ((p_frame_header->frame_size.mi_rows + 15 ) >> 4);
    sb_shift = p_seq_header->use_128x128_superblock ? 5 : 4;
    sb_size = sb_shift + 2;
    max_tile_width_sb = MAX_TILE_WIDTH >> sb_size;
    max_tile_area_sb = MAX_TILE_AREA >> (2 * sb_size);
    min_log2_tile_cols = TileLog2(max_tile_width_sb, sb_cols);
    max_log2_tile_cols = TileLog2(1, std::min(sb_cols, MAX_TILE_COLS));
    max_log2_tile_rows = TileLog2(1, std::min(sb_rows, MAX_TILE_ROWS));
    min_log2_tiles = std::max(min_log2_tile_cols, static_cast<int>(TileLog2(max_tile_area_sb, sb_rows * sb_cols)));

    p_frame_header->tile_info.uniform_tile_spacing_flag = Parser::GetBit(p_stream, offset);
    if (p_frame_header->tile_info.uniform_tile_spacing_flag) {
        p_frame_header->tile_info.tile_cols_log2 = min_log2_tile_cols;
        while (p_frame_header->tile_info.tile_cols_log2 < max_log2_tile_cols) {
            p_frame_header->tile_info.increment_tile_cols_log2 = Parser::GetBit(p_stream, offset);
            if (p_frame_header->tile_info.increment_tile_cols_log2 == 1) {
                p_frame_header->tile_info.tile_cols_log2++;
            } else {
                break;
            }
        }

        tile_width_sb = (sb_cols + (1 << p_frame_header->tile_info.tile_cols_log2) - 1) >> p_frame_header->tile_info.tile_cols_log2;
        i = 0;
        for (start_sb = 0; start_sb < sb_cols; start_sb += tile_width_sb) {
            p_frame_header->tile_info.mi_col_starts[i] = start_sb << sb_shift;
            i += 1;
        }
        p_frame_header->tile_info.mi_col_starts[i] = p_frame_header->frame_size.mi_cols;
        p_frame_header->tile_info.tile_cols = i;

        min_log2_tile_rows = std::max(min_log2_tiles - p_frame_header->tile_info.tile_cols_log2, 0);
        p_frame_header->tile_info.tile_rows_log2 = min_log2_tile_rows;
        while (p_frame_header->tile_info.tile_rows_log2 < max_log2_tile_rows) {
            p_frame_header->tile_info.increment_tile_rows_log2 = Parser::GetBit(p_stream, offset);
            if (p_frame_header->tile_info.increment_tile_rows_log2 == 1) {
                p_frame_header->tile_info.tile_rows_log2++;
            } else {
                break;
            }
        }

        tile_height_sb = (sb_rows + (1 << p_frame_header->tile_info.tile_rows_log2) - 1) >> p_frame_header->tile_info.tile_rows_log2;
        i = 0;
        for (start_sb = 0; start_sb < sb_rows; start_sb += tile_height_sb ) {
            p_frame_header->tile_info.mi_row_starts[i] = start_sb << sb_shift;
            i += 1;
        }
        p_frame_header->tile_info.mi_row_starts[i] = p_frame_header->frame_size.mi_rows;
        p_frame_header->tile_info.tile_rows = i;
    } else {
        int widest_tile_sb = 0;
        start_sb = 0;
        for (i = 0; start_sb < sb_cols; i++) {
            p_frame_header->tile_info.mi_col_starts[i] = start_sb << sb_shift;
            max_width = std::min(sb_cols - start_sb, max_tile_width_sb);
            p_frame_header->tile_info.width_in_sbs_minus_1 = ReadUnsignedNonSymmetic(p_stream, offset, max_width);
            size_sb = p_frame_header->tile_info.width_in_sbs_minus_1 + 1;
            widest_tile_sb = std::max(size_sb, widest_tile_sb);
            start_sb += size_sb;
        }
        p_frame_header->tile_info.mi_col_starts[i] = p_frame_header->frame_size.mi_cols;
        p_frame_header->tile_info.tile_cols = i;
        p_frame_header->tile_info.tile_cols_log2 = TileLog2(1, p_frame_header->tile_info.tile_cols);

        if (min_log2_tiles > 0) {
            max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
        } else {
            max_tile_area_sb = sb_rows * sb_cols;
        }
        max_tile_height_sb = std::max(max_tile_area_sb / widest_tile_sb, static_cast<int>(1));

        start_sb = 0;
        for (i = 0; start_sb < sb_rows; i++) {
            p_frame_header->tile_info.mi_row_starts[i] = start_sb << sb_shift;
            max_height = std::min(sb_rows - start_sb, max_tile_height_sb);
            p_frame_header->tile_info.height_in_sbs_minus_1 = ReadUnsignedNonSymmetic(p_stream, offset, max_height);
            size_sb = p_frame_header->tile_info.height_in_sbs_minus_1 + 1;
            start_sb += size_sb;
        }
        p_frame_header->tile_info.mi_row_starts[ i ] = p_frame_header->frame_size.mi_rows;
        p_frame_header->tile_info.tile_rows = i;
        p_frame_header->tile_info.tile_rows_log2 = TileLog2(1, p_frame_header->tile_info.tile_rows);
    }

    if (p_frame_header->tile_info.tile_cols_log2 > 0 || p_frame_header->tile_info.tile_rows_log2 > 0) {
        p_frame_header->tile_info.context_update_tile_id = Parser::ReadBits(p_stream, offset, p_frame_header->tile_info.tile_rows_log2 + p_frame_header->tile_info.tile_cols_log2);
        p_frame_header->tile_info.tile_size_bytes_minus_1 = Parser::ReadBits(p_stream, offset, 2);
    } else {
        p_frame_header->tile_info.context_update_tile_id = 0;
    }
}

uint32_t Av1VideoParser::TileLog2(uint32_t blk_size, uint32_t target) {
    uint32_t k;
    for (k = 0; (blk_size << k) < target; k++ ) {}
    return k;
}

void Av1VideoParser::QuantizationParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    p_frame_header->quantization_params.base_q_idx = Parser::ReadBits(p_stream, offset, 8);
    p_frame_header->quantization_params.delta_q_y_dc = ReadDeltaQ(p_stream, offset, p_frame_header);

    if (p_seq_header->color_config.num_planes > 1) {
        if (p_seq_header->color_config.separate_uv_delta_q) {
            p_frame_header->quantization_params.diff_uv_delta = Parser::GetBit(p_stream, offset);
        } else {
            p_frame_header->quantization_params.diff_uv_delta = 0;
        }
        p_frame_header->quantization_params.delta_q_u_dc = ReadDeltaQ(p_stream, offset, p_frame_header);
        p_frame_header->quantization_params.delta_q_u_ac = ReadDeltaQ(p_stream, offset, p_frame_header);

        if (p_frame_header->quantization_params.diff_uv_delta) {
            p_frame_header->quantization_params.delta_q_v_dc = ReadDeltaQ(p_stream, offset, p_frame_header);
            p_frame_header->quantization_params.delta_q_v_ac = ReadDeltaQ(p_stream, offset, p_frame_header);
        } else {
            p_frame_header->quantization_params.delta_q_v_dc = p_frame_header->quantization_params.delta_q_u_dc;
            p_frame_header->quantization_params.delta_q_v_ac = p_frame_header->quantization_params.delta_q_u_ac;
        }
    } else {
        p_frame_header->quantization_params.delta_q_u_dc = 0;
        p_frame_header->quantization_params.delta_q_u_ac = 0;
        p_frame_header->quantization_params.delta_q_v_dc = 0;
        p_frame_header->quantization_params.delta_q_v_ac = 0;
    }

    p_frame_header->quantization_params.using_qmatrix = Parser::GetBit(p_stream, offset);
    if (p_frame_header->quantization_params.using_qmatrix) {
        p_frame_header->quantization_params.qm_y = Parser::ReadBits(p_stream, offset, 4);
        p_frame_header->quantization_params.qm_u = Parser::ReadBits(p_stream, offset, 4);
        if (!p_seq_header->color_config.separate_uv_delta_q) {
            p_frame_header->quantization_params.qm_v = p_frame_header->quantization_params.qm_u;
        } else {
            p_frame_header->quantization_params.qm_v = Parser::ReadBits(p_stream, offset, 4);
        }
    }
}

uint32_t Av1VideoParser::ReadDeltaQ(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    p_frame_header->quantization_params.delta_coded = Parser::GetBit(p_stream, offset);
    if (p_frame_header->quantization_params.delta_coded) {
        p_frame_header->quantization_params.delta_q = ReadSigned(p_stream, offset, 1 + 6);
    } else {
        p_frame_header->quantization_params.delta_q = 0;
    }
    return p_frame_header->quantization_params.delta_q;
}

void Av1VideoParser::SegmentationParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    int i, j;
    int clipped_value;
    uint32_t bits_to_read;
    uint32_t segmentation_feature_bits[SEG_LVL_MAX] = {8,6,6,6,6,3,0,0};
    uint32_t segmentation_feature_signed[SEG_LVL_MAX] = { 1, 1, 1, 1, 1, 0, 0, 0 };
    uint32_t segmentation_feature_max[SEG_LVL_MAX] = {255, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, 7, 0, 0 };

    p_frame_header->segmentation_params.segmentation_enabled = Parser::GetBit(p_stream, offset);
    if (p_frame_header->segmentation_params.segmentation_enabled == 1) {
        if (p_frame_header->primary_ref_frame == PRIMARY_REF_NONE) {
            p_frame_header->segmentation_params.segmentation_update_map = 1;
            p_frame_header->segmentation_params.segmentation_temporal_update = 0;
            p_frame_header->segmentation_params.segmentation_update_data = 1;
        } else {
            p_frame_header->segmentation_params.segmentation_update_map = Parser::GetBit(p_stream, offset);
            if (p_frame_header->segmentation_params.segmentation_update_map == 1) {
                p_frame_header->segmentation_params.segmentation_temporal_update = Parser::GetBit(p_stream, offset);
            }
            p_frame_header->segmentation_params.segmentation_update_data = Parser::GetBit(p_stream, offset);
        }

        if (p_frame_header->segmentation_params.segmentation_update_data == 1) {
            for (i = 0; i < MAX_SEGMENTS; i++) {
                for (j = 0; j < SEG_LVL_MAX; j++) {
                    p_frame_header->segmentation_params.feature_value = 0;
                    p_frame_header->segmentation_params.feature_enabled = Parser::GetBit(p_stream, offset);
                    p_frame_header->segmentation_params.feature_enabled_flags[i][j] = p_frame_header->segmentation_params.feature_enabled;
                    clipped_value = 0;
                    if (p_frame_header->segmentation_params.feature_enabled == 1) {
                        bits_to_read = segmentation_feature_bits[j];
                        int limit = segmentation_feature_max[j];
                        if (segmentation_feature_signed[j] == 1) {
                            p_frame_header->segmentation_params.feature_value = ReadSigned(p_stream, offset, 1 + bits_to_read);
                            clipped_value = std::clamp(static_cast<int>(p_frame_header->segmentation_params.feature_value), -limit, limit);
                        } else {
                            p_frame_header->segmentation_params.feature_value = Parser::ReadBits(p_stream, offset, bits_to_read);
                            clipped_value = std::clamp(static_cast<int>(p_frame_header->segmentation_params.feature_value), 0, limit);
                        }
                    }
                    p_frame_header->segmentation_params.feature_data[i][j] = clipped_value;
                }
            }
        }
    } else {
        for (i = 0; i < MAX_SEGMENTS; i++) {
            for (j = 0; j < SEG_LVL_MAX; j++) {
                p_frame_header->segmentation_params.feature_enabled_flags[i][j] = 0;
                p_frame_header->segmentation_params.feature_data[i][j] = 0;
            }
        }
    }

    p_frame_header->segmentation_params.seg_id_pre_skip = 0;
    p_frame_header->segmentation_params.last_active_seg_id = 0;

    for (i = 0; i < MAX_SEGMENTS; i++) {
        for (j = 0; j < SEG_LVL_MAX; j++) {
            if (p_frame_header->segmentation_params.feature_enabled_flags[i][j]) {
                p_frame_header->segmentation_params.last_active_seg_id = i;
                if (j >= SEG_LVL_REF_FRAME) {
                    p_frame_header->segmentation_params.seg_id_pre_skip = 1;
                }
            }
        }
    }
}

void Av1VideoParser::DeltaQParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    p_frame_header->delta_q_params.delta_q_res = 0;
    p_frame_header->delta_q_params.delta_q_present = 0;
    if (p_frame_header->quantization_params.base_q_idx > 0) {
        p_frame_header->delta_q_params.delta_q_present = Parser::GetBit(p_stream, offset);
    }
    if (p_frame_header->delta_q_params.delta_q_present) {
        p_frame_header->delta_q_params.delta_q_res = Parser::ReadBits(p_stream, offset, 2);
    }
}

void Av1VideoParser::DeltaLFParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    p_frame_header->delta_lf_params.delta_lf_present = 0;
    p_frame_header->delta_lf_params.delta_lf_res = 0;
    p_frame_header->delta_lf_params.delta_lf_multi = 0;
    if (p_frame_header->delta_q_params.delta_q_present) {
        if (!p_frame_header->allow_intrabc) {
            p_frame_header->delta_lf_params.delta_lf_present = Parser::GetBit(p_stream, offset);
        }
        if (p_frame_header->delta_lf_params.delta_lf_present) {
            p_frame_header->delta_lf_params.delta_lf_res = Parser::ReadBits(p_stream, offset, 2);
            p_frame_header->delta_lf_params.delta_lf_multi = Parser::GetBit(p_stream, offset);
        }
    }
}

int Av1VideoParser::GetQIndex(Av1FrameHeader *p_frame_header, int ignore_delta_q, int segment_id) {
    // seg_feature_active_idx(segment_id, SEG_LVL_ALT_Q)
    int seg_feature_active_idx = p_frame_header->segmentation_params.segmentation_enabled && p_frame_header->segmentation_params.feature_enabled_flags[segment_id][SEG_LVL_ALT_Q];
    if (seg_feature_active_idx == 1) {
        int data = p_frame_header->segmentation_params.feature_data[segment_id][SEG_LVL_ALT_Q];
        int q_index = p_frame_header->quantization_params.base_q_idx + data;
        // CurrentQIndex is base_q_idx at tile level: If ignoreDeltaQ is equal to 0 and delta_q_present is equal to 1, set qindex equal to CurrentQIndex + data.
        std::clamp(q_index, 0, 255);
        return q_index;
    } else if (ignore_delta_q == 0 && p_frame_header->delta_q_params.delta_q_present == 1) {
        return p_frame_header->quantization_params.base_q_idx; // CurrentQIndex is base_q_idx at tile level
    } else {
        return p_frame_header->quantization_params.base_q_idx;
    }
}

void Av1VideoParser::LoopFilterParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    int i;

    if (p_frame_header->coded_lossless || p_frame_header->allow_intrabc) {
        p_frame_header->loop_filter_params.loop_filter_level[0] = 0;
        p_frame_header->loop_filter_params.loop_filter_level[1] = 0;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kIntraFrame] = 1;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kLastFrame] = 0;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kLast2Frame] = 0;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kLast3Frame] = 0;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kBwdRefFrame] = 0;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kGoldenFrame] = -1;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kAltRefFrame] = -1;
        p_frame_header->loop_filter_params.loop_filter_ref_deltas[kAltRef2Frame] = -1;
        for (i = 0; i < 2; i++) {
            p_frame_header->loop_filter_params.loop_filter_mode_deltas[i] = 0;
        }
        return;
    }

    p_frame_header->loop_filter_params.loop_filter_level[0] = Parser::ReadBits(p_stream, offset, 6);
    p_frame_header->loop_filter_params.loop_filter_level[1] = Parser::ReadBits(p_stream, offset, 6);
    if (p_seq_header->color_config.num_planes > 1) {
        if (p_frame_header->loop_filter_params.loop_filter_level[0] || p_frame_header->loop_filter_params.loop_filter_level[1]) {
            p_frame_header->loop_filter_params.loop_filter_level[2] = Parser::ReadBits(p_stream, offset, 6);
            p_frame_header->loop_filter_params.loop_filter_level[3] = Parser::ReadBits(p_stream, offset, 6);
        }
    }

    p_frame_header->loop_filter_params.loop_filter_sharpness = Parser::ReadBits(p_stream, offset, 3);
    p_frame_header->loop_filter_params.loop_filter_delta_enabled = Parser::GetBit(p_stream, offset);
    if (p_frame_header->loop_filter_params.loop_filter_delta_enabled == 1) {
        p_frame_header->loop_filter_params.loop_filter_delta_update = Parser::GetBit(p_stream, offset);
        if (p_frame_header->loop_filter_params.loop_filter_delta_update == 1) {
            for (i = 0; i < TOTAL_REFS_PER_FRAME; i++) {
                p_frame_header->loop_filter_params.update_ref_delta = Parser::GetBit(p_stream, offset);
                if (p_frame_header->loop_filter_params.update_ref_delta == 1) {
                    p_frame_header->loop_filter_params.loop_filter_ref_deltas[i] = ReadSigned(p_stream, offset, 1 + 6);
                }
            }
            for (i = 0; i < 2; i++) {
                p_frame_header->loop_filter_params.update_mode_delta = Parser::GetBit(p_stream, offset);
                if ( p_frame_header->loop_filter_params.update_mode_delta == 1 )
                {
                    p_frame_header->loop_filter_params.loop_filter_mode_deltas[i] = ReadSigned(p_stream, offset, 1 + 6);
                }
            }
        }
    }
}

void Av1VideoParser::CdefParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    if (p_frame_header->coded_lossless || p_frame_header->allow_intrabc ||!p_seq_header->enable_cdef) {
        p_frame_header->cdef_params.cdef_bits = 0;
        p_frame_header->cdef_params.cdef_y_pri_strength[0] = 0;
        p_frame_header->cdef_params.cdef_y_sec_strength[0] = 0;
        p_frame_header->cdef_params.cdef_uv_pri_strength[0] = 0;
        p_frame_header->cdef_params.cdef_uv_sec_strength[0] = 0;
        p_frame_header->cdef_params.cdef_damping = 3;
        return;
    }

    p_frame_header->cdef_params.cdef_damping_minus_3 = Parser::ReadBits(p_stream, offset, 2);
    p_frame_header->cdef_params.cdef_damping = p_frame_header->cdef_params.cdef_damping_minus_3 + 3;
    p_frame_header->cdef_params.cdef_bits = Parser::ReadBits(p_stream, offset, 2);
    for (int i = 0; i < (1 << p_frame_header->cdef_params.cdef_bits); i++) {
        p_frame_header->cdef_params.cdef_y_pri_strength[i] = Parser::ReadBits(p_stream, offset, 4);
        p_frame_header->cdef_params.cdef_y_sec_strength[i] = Parser::ReadBits(p_stream, offset, 2);
        if (p_frame_header->cdef_params.cdef_y_sec_strength[i] == 3) {
            p_frame_header->cdef_params.cdef_y_sec_strength[i] += 1;
        }

        if (p_seq_header->color_config.num_planes > 1) {
            p_frame_header->cdef_params.cdef_uv_pri_strength[i] = Parser::ReadBits(p_stream, offset, 4);
            p_frame_header->cdef_params.cdef_uv_sec_strength[i] = Parser::ReadBits(p_stream, offset, 2);
            if (p_frame_header->cdef_params.cdef_uv_sec_strength[i] == 3) {
                p_frame_header->cdef_params.cdef_uv_sec_strength[i] += 1;
            }
        }
    }
}

void Av1VideoParser::LrParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    uint32_t remap_lr_type[4] = {kRestoreNone, kRestoreSwitchable, kRestoreWiener, kRestoreSgrproj};

    if (p_frame_header->all_lossless || p_frame_header->allow_intrabc || !p_seq_header->enable_restoration) {
        p_frame_header->lr_params.frame_restoration_type[0] = kRestoreNone;
        p_frame_header->lr_params.frame_restoration_type[1] = kRestoreNone;
        p_frame_header->lr_params.frame_restoration_type[2] = kRestoreNone;
        p_frame_header->lr_params.uses_lr = 0;
        return;
    }

    p_frame_header->lr_params.uses_lr = 0;
    uint32_t uses_chroma_lr = 0;
    for (int i = 0; i < p_seq_header->color_config.num_planes; i++) {
        p_frame_header->lr_params.lr_type[i] = Parser::ReadBits(p_stream, offset, 2);
        p_frame_header->lr_params.frame_restoration_type[i] = remap_lr_type[p_frame_header->lr_params.lr_type[i]];
        if (p_frame_header->lr_params.frame_restoration_type[i] != kRestoreNone) {
            p_frame_header->lr_params.uses_lr = 1;
            if (i > 0) {
                uses_chroma_lr = 1;
            }
        }
    }

    if (p_frame_header->lr_params.uses_lr) {
        if (p_seq_header->use_128x128_superblock) {
            p_frame_header->lr_params.lr_unit_shift = Parser::GetBit(p_stream, offset);
            p_frame_header->lr_params.lr_unit_shift++;
        } else {
            p_frame_header->lr_params.lr_unit_shift = Parser::GetBit(p_stream, offset);
            if (p_frame_header->lr_params.lr_unit_shift) {
                p_frame_header->lr_params.lr_unit_extra_shift = Parser::GetBit(p_stream, offset);
                p_frame_header->lr_params.lr_unit_shift += p_frame_header->lr_params.lr_unit_extra_shift;
            }
        }

        p_frame_header->lr_params.loop_restoration_size[0] = RESTORATION_TILESIZE_MAX >> (2 - p_frame_header->lr_params.lr_unit_shift);
        if (p_seq_header->color_config.subsampling_x && p_seq_header->color_config.subsampling_y && uses_chroma_lr) {
            p_frame_header->lr_params.lr_uv_shift = Parser::GetBit(p_stream, offset);
        } else {
            p_frame_header->lr_params.lr_uv_shift = 0;
        }
        p_frame_header->lr_params.loop_restoration_size[1] = p_frame_header->lr_params.loop_restoration_size[0] >> p_frame_header->lr_params.lr_uv_shift;
        p_frame_header->lr_params.loop_restoration_size[2] = p_frame_header->lr_params.loop_restoration_size[0] >> p_frame_header->lr_params.lr_uv_shift;
    }
}

void Av1VideoParser::ReadTxMode(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    if (p_frame_header->coded_lossless == 1) {
        p_frame_header->tx_mode.tx_mode = kOnly4x4;
    } else {
        p_frame_header->tx_mode.tx_mode_select = Parser::GetBit(p_stream, offset);
        if (p_frame_header->tx_mode.tx_mode_select) {
            p_frame_header->tx_mode.tx_mode = kTxModeSelect;
        } else {
            p_frame_header->tx_mode.tx_mode = kTxModeLargest;
        }
    }
}

void Av1VideoParser::SkipModeParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    uint32_t skip_mode_allowed;
    int forward_idx, backward_idx;
    int forward_hint, backward_hint;
    int second_forward_idx, second_forward_hint;
    uint32_t ref_hint;
    int i;

    if (p_frame_header->frame_is_intra || !p_frame_header->frame_reference_mode.reference_select || !p_seq_header->enable_order_hint) {
        skip_mode_allowed = 0;
    } else {
        forward_idx = -1;
        backward_idx = -1;
        forward_hint = ref_order_hint_[0];  // init value. No effect.
        backward_hint = ref_order_hint_[1];  // init value. No effect.

        for (i = 0; i < REFS_PER_FRAME; i++) {
            ref_hint = ref_order_hint_[p_frame_header->ref_frame_idx[i]];
            if (GetRelativeDist(p_seq_header, ref_hint, p_frame_header->order_hint ) < 0) {
                if (forward_idx < 0 || GetRelativeDist(p_seq_header, ref_hint, forward_hint) > 0) {
                    forward_idx = i;
                    forward_hint = ref_hint;
                }
            } else if (GetRelativeDist(p_seq_header, ref_hint, p_frame_header->order_hint) > 0) {
                if (backward_idx < 0 || GetRelativeDist(p_seq_header, ref_hint, backward_hint) < 0 ) {
                    backward_idx = i;
                    backward_hint = ref_hint;
                }
            }
        }

        if (forward_idx < 0) {
            skip_mode_allowed = 0;
        } else if (backward_idx >= 0) {
            skip_mode_allowed = 1;
            p_frame_header->skip_mode_params.skip_mode_frame[0] = kLastFrame + std::min(forward_idx, backward_idx);
            p_frame_header->skip_mode_params.skip_mode_frame[1] = kLastFrame + std::max(forward_idx, backward_idx);
        } else {
            second_forward_idx = -1;
            second_forward_hint = ref_order_hint_[0];  // init value. No effect.
            for (i = 0; i < REFS_PER_FRAME; i++) {
                ref_hint = ref_order_hint_[p_frame_header->ref_frame_idx[i]];
                if (GetRelativeDist(p_seq_header, ref_hint, forward_hint ) < 0) {
                    if (second_forward_idx < 0 || GetRelativeDist(p_seq_header, ref_hint, second_forward_hint ) > 0) {
                        second_forward_idx = i;
                        second_forward_hint = ref_hint;
                    }
                }
            }
            if (second_forward_idx < 0) {
                skip_mode_allowed = 0;
            } else {
                skip_mode_allowed = 1;
                p_frame_header->skip_mode_params.skip_mode_frame[0] = kLastFrame + std::min(forward_idx, second_forward_idx);
                p_frame_header->skip_mode_params.skip_mode_frame[1] = kLastFrame + std::max(forward_idx, second_forward_idx);
            }
        }
    }

    if (skip_mode_allowed ) {
        p_frame_header->skip_mode_params.skip_mode_present = Parser::GetBit(p_stream, offset);
    } else {
        p_frame_header->skip_mode_params.skip_mode_present = 0;
    }
}

void Av1VideoParser::GlobalMotionParams(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header) {
    int ref;
    int type;

    for (ref = kLastFrame; ref <= kAltRefFrame; ref++) {
        p_frame_header->global_motion_params.gm_type[ref] = kIdentity;
        for (int i = 0; i < 6; i++) {
            p_frame_header->global_motion_params.gm_params[ref][i] = ( ( i % 3 == 2 ) ? 1 << WARPEDMODEL_PREC_BITS : 0 );
        }
    }
    if (p_frame_header->frame_is_intra) {
        return;
    }

    for (ref = kLastFrame; ref <= kAltRefFrame; ref++) {
        p_frame_header->global_motion_params.is_global = Parser::GetBit(p_stream, offset);
        if (p_frame_header->global_motion_params.is_global) {
            p_frame_header->global_motion_params.is_rot_zoom = Parser::GetBit(p_stream, offset);
            if (p_frame_header->global_motion_params.is_rot_zoom) {
                type = kRotZoom;
            } else {
                p_frame_header->global_motion_params.is_translation = Parser::GetBit(p_stream, offset);
                type = p_frame_header->global_motion_params.is_translation ? kTranslation : kAffine;
            }
        } else {
            type = kIdentity;
        }
        p_frame_header->global_motion_params.gm_type[ref] = type;

        if (type >= kRotZoom) {
            ReadGlobalParam(p_stream, offset, p_frame_header, type, ref, 2);
            ReadGlobalParam(p_stream, offset, p_frame_header, type, ref, 3);
            if (type == kAffine) {
                ReadGlobalParam(p_stream, offset, p_frame_header, type, ref, 4);
                ReadGlobalParam(p_stream, offset, p_frame_header, type, ref, 5);
            } else {
                p_frame_header->global_motion_params.gm_params[ref][4] = -p_frame_header->global_motion_params.gm_params[ref][3];
                p_frame_header->global_motion_params.gm_params[ref][5] = p_frame_header->global_motion_params.gm_params[ref][2];
            }
        }
        if ( type >= kTranslation ) {
            ReadGlobalParam(p_stream, offset, p_frame_header, type, ref, 0);
            ReadGlobalParam(p_stream, offset, p_frame_header, type, ref, 1);
        }
    }
}

void Av1VideoParser::ReadGlobalParam(const uint8_t *p_stream, size_t &offset, Av1FrameHeader *p_frame_header, int type, int ref, int idx) {
    int abs_bits = GM_ABS_ALPHA_BITS;
    int prec_bits = GM_ALPHA_PREC_BITS;

    if (idx < 2) {
        if (type == kTranslation) {
            abs_bits = GM_ABS_TRANS_ONLY_BITS - !p_frame_header->allow_high_precision_mv;
            prec_bits = GM_TRANS_ONLY_PREC_BITS - !p_frame_header->allow_high_precision_mv;
        } else {
            abs_bits = GM_ABS_TRANS_BITS;
            prec_bits = GM_TRANS_PREC_BITS;
        }
    }
    int prec_diff = WARPEDMODEL_PREC_BITS - prec_bits;
    int round = (idx % 3) == 2 ? (1 << WARPEDMODEL_PREC_BITS) : 0;
    int sub = (idx % 3) == 2 ? (1 << prec_bits) : 0;
    int mx = (1 << abs_bits);
    int r = (prev_gm_params_[ref][idx] >> prec_diff) - sub;
    p_frame_header->global_motion_params.gm_params[ref][idx] = (DecodeSignedSubexpWithRef(p_stream, offset, -mx, mx + 1, r) << prec_diff) + round;
}

int Av1VideoParser::DecodeSignedSubexpWithRef(const uint8_t *p_stream, size_t &offset, int low, int high, int r) {
    int x = DecodeUnsignedSubexpWithRef(p_stream, offset, high - low, r - low);
    return x + low;
}

int Av1VideoParser::DecodeUnsignedSubexpWithRef(const uint8_t *p_stream, size_t &offset, int mx, int r) {
    int v = DecodeSubexp(p_stream, offset, mx);
    if ((r << 1) <= mx) {
        return InverseRecenter(r, v);
    } else {
        return mx - 1 - InverseRecenter(mx - 1 - r, v);
    }
}

int Av1VideoParser::DecodeSubexp(const uint8_t *p_stream, size_t &offset, int num_syms) {
    int i = 0;
    int mk = 0;
    int k = 3;
    while (1) {
        int b2 = i ? k + i - 1 : k;
        int a = 1 << b2;
        if (num_syms <= mk + 3 * a) {
            int subexp_final_bits = ReadUnsignedNonSymmetic(p_stream, offset, num_syms - mk);
            return subexp_final_bits + mk;
        } else {
            int subexp_more_bits = Parser::GetBit(p_stream, offset);
            if (subexp_more_bits) {
                i++;
                mk += a;
            } else {
                int subexp_bits = Parser::ReadBits(p_stream, offset, b2);
                return subexp_bits + mk;
            }
        }
    }
}

int Av1VideoParser::InverseRecenter(int r, int v) {
    if (v > 2 * r) {
        return v;
    } else if (v & 1) {
        return r - ((v + 1) >> 1);
    } else {
        return r + (v >> 1);
    }
}

void Av1VideoParser::FilmGrainParams(const uint8_t *p_stream, size_t &offset, Av1SequenceHeader *p_seq_header, Av1FrameHeader *p_frame_header) {
    int i;

    if (!p_seq_header->film_grain_params_present || (!p_frame_header->show_frame && !p_frame_header->showable_frame)) {
        // reset_grain_params()
        memset(&p_frame_header->film_grain_params, 0, sizeof(Av1FilmGrainParams));
        return;
    }
    p_frame_header->film_grain_params.apply_grain = Parser::GetBit(p_stream, offset);
    if ( !p_frame_header->film_grain_params.apply_grain )
    {
        // reset_grain_params()
        memset(&p_frame_header->film_grain_params, 0, sizeof(Av1FilmGrainParams));
        return;
    }

    p_frame_header->film_grain_params.grain_seed = Parser::ReadBits(p_stream, offset, 16);
    if (p_frame_header->frame_type == kInterFrame) {
        p_frame_header->film_grain_params.update_grain = Parser::GetBit(p_stream, offset);
    } else {
        p_frame_header->film_grain_params.update_grain = 1;
    }

    if (!p_frame_header->film_grain_params.update_grain) {
        p_frame_header->film_grain_params.film_grain_params_ref_idx = Parser::ReadBits(p_stream, offset, 3);
        int temp_grain_seed = p_frame_header->film_grain_params.grain_seed;
        //load_grain_params( film_grain_params_ref_idx );
        // Todo
        ERR("Warning: need to implement film grain param loading load_grain_params( film_grain_params_ref_idx ).\n");
        p_frame_header->film_grain_params.grain_seed = temp_grain_seed;
        return;
    }

    p_frame_header->film_grain_params.num_y_points = Parser::ReadBits(p_stream, offset, 4);
    for (i = 0; i < p_frame_header->film_grain_params.num_y_points; i++) {
        p_frame_header->film_grain_params.point_y_value[i] = Parser::ReadBits(p_stream, offset, 8);
        p_frame_header->film_grain_params.point_y_scaling[i] = Parser::ReadBits(p_stream, offset, 8);
    }

    if (p_seq_header->color_config.mono_chrome) {
        p_frame_header->film_grain_params.chroma_scaling_from_luma = 0;
    } else {
        p_frame_header->film_grain_params.chroma_scaling_from_luma = Parser::GetBit(p_stream, offset);
    }

    if (p_seq_header->color_config.mono_chrome || p_frame_header->film_grain_params.chroma_scaling_from_luma || (p_seq_header->color_config.subsampling_x == 1 && p_seq_header->color_config.subsampling_y == 1 && p_frame_header->film_grain_params.num_y_points == 0)) {
        p_frame_header->film_grain_params.num_cb_points = 0;
        p_frame_header->film_grain_params.num_cr_points = 0;
    } else {
        p_frame_header->film_grain_params.num_cb_points = Parser::ReadBits(p_stream, offset, 4);
        for (i = 0; i < p_frame_header->film_grain_params.num_cb_points; i++) {
            p_frame_header->film_grain_params.point_cb_value[i] = Parser::ReadBits(p_stream, offset, 8);
            p_frame_header->film_grain_params.point_cb_scaling[i] = Parser::ReadBits(p_stream, offset, 8);
        }
        p_frame_header->film_grain_params.num_cr_points = Parser::ReadBits(p_stream, offset, 4);
        for ( i = 0; i < p_frame_header->film_grain_params.num_cr_points; i++ )
        {
            p_frame_header->film_grain_params.point_cr_value[i] = Parser::ReadBits(p_stream, offset, 8);
            p_frame_header->film_grain_params.point_cr_scaling[i] = Parser::ReadBits(p_stream, offset, 8);
        }
    }

    p_frame_header->film_grain_params.grain_scaling_minus_8 = Parser::ReadBits(p_stream, offset, 2);
    p_frame_header->film_grain_params.ar_coeff_lag = Parser::ReadBits(p_stream, offset, 2);
    uint32_t num_pos_luma = 2 * p_frame_header->film_grain_params.ar_coeff_lag * (p_frame_header->film_grain_params.ar_coeff_lag + 1);
    uint32_t num_pos_chroma;
    if (p_frame_header->film_grain_params.num_y_points) {
        num_pos_chroma = num_pos_luma + 1;
        for (i = 0; i < num_pos_luma; i++) {
            p_frame_header->film_grain_params.ar_coeffs_y_plus_128[i] = Parser::ReadBits(p_stream, offset, 8);
        }
    } else {
        num_pos_chroma = num_pos_luma;
    }

    if (p_frame_header->film_grain_params.chroma_scaling_from_luma || p_frame_header->film_grain_params.num_cb_points) {
        for (i = 0; i < num_pos_chroma; i++) {
            p_frame_header->film_grain_params.ar_coeffs_cb_plus_128[i] = Parser::ReadBits(p_stream, offset, 8);
        }
    }

    if (p_frame_header->film_grain_params.chroma_scaling_from_luma || p_frame_header->film_grain_params.num_cr_points) {
        for (i = 0; i < num_pos_chroma; i++) {
            p_frame_header->film_grain_params.ar_coeffs_cr_plus_128[i] = Parser::ReadBits(p_stream, offset, 8);
        }
    }

    p_frame_header->film_grain_params.ar_coeff_shift_minus_6 = Parser::ReadBits(p_stream, offset, 2);
    p_frame_header->film_grain_params.grain_scale_shift = Parser::ReadBits(p_stream, offset, 2);

    if (p_frame_header->film_grain_params.num_cb_points) {
        p_frame_header->film_grain_params.cb_mult = Parser::ReadBits(p_stream, offset, 8);
        p_frame_header->film_grain_params.cb_luma_mult = Parser::ReadBits(p_stream, offset, 8);
        p_frame_header->film_grain_params.cb_offset = Parser::ReadBits(p_stream, offset, 9);
    }

    if (p_frame_header->film_grain_params.num_cr_points) {
        p_frame_header->film_grain_params.cr_mult = Parser::ReadBits(p_stream, offset, 8);
        p_frame_header->film_grain_params.cr_luma_mult = Parser::ReadBits(p_stream, offset, 8);
        p_frame_header->film_grain_params.cr_offset = Parser::ReadBits(p_stream, offset, 9);
    }

    p_frame_header->film_grain_params.overlap_flag = Parser::GetBit(p_stream, offset);
    p_frame_header->film_grain_params.clip_to_restricted_range = Parser::GetBit(p_stream, offset);
}
