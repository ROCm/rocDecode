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
#include "vp9_parser.h"

Vp9VideoParser::Vp9VideoParser() {
    memset(&curr_pic_, 0, sizeof(Vp9Picture));
    memset(&dpb_buffer_, 0, sizeof(DecodedPictureBuffer));
    InitDpb();
}

Vp9VideoParser::~Vp9VideoParser() {
}

rocDecStatus Vp9VideoParser::Initialize(RocdecParserParams *p_params) {
    rocDecStatus ret;
    if ((ret = RocVideoParser::Initialize(p_params)) != ROCDEC_SUCCESS) {
        return ret;
    }
    // Set display delay to at least DECODE_BUF_POOL_EXTENSION (2) to prevent synchronous submission
    if (parser_params_.max_display_delay < DECODE_BUF_POOL_EXTENSION) {
        parser_params_.max_display_delay = DECODE_BUF_POOL_EXTENSION;
    }
    CheckAndAdjustDecBufPoolSize(VP9_NUM_REF_FRAMES);
    return ROCDEC_SUCCESS;
}

rocDecStatus Vp9VideoParser::UnInitialize() {
    return ROCDEC_SUCCESS;
}

rocDecStatus Vp9VideoParser::ParseVideoData(RocdecSourceDataPacket *p_data) { 
    if (p_data->payload && p_data->payload_size) {
        curr_pts_ = p_data->pts;
        if (ParsePictureData(p_data->payload, p_data->payload_size) != PARSER_OK) {
            ERR(STR("Parser failed!"));
            return ROCDEC_RUNTIME_ERROR;
        }
    } else if (!(p_data->flags & ROCDEC_PKT_ENDOFSTREAM)) {
        // If no payload and EOS is not set, treated as invalid.
        return ROCDEC_INVALID_PARAMETER;
    }
    if (p_data->flags & ROCDEC_PKT_ENDOFSTREAM) {
        if (FlushDpb() != PARSER_OK) {
            return ROCDEC_RUNTIME_ERROR;
        }
    }
    return ROCDEC_SUCCESS;
}

ParserResult Vp9VideoParser::ParsePictureData(const uint8_t *p_stream, uint32_t pic_data_size) {
    ParserResult ret = PARSER_OK;
    pic_data_buffer_ptr_ = (uint8_t*)p_stream;
    pic_data_size_ = pic_data_size;
    curr_byte_offset_ = 0;

    int bytes_parsed = 0;
    ParseUncompressedHeader(pic_data_buffer_ptr_ + curr_byte_offset_, pic_data_size, &bytes_parsed);

    return PARSER_OK;
}

ParserResult Vp9VideoParser::NotifyNewSequence() {
    video_format_params_.codec = rocDecVideoCodec_AV1;
    /*video_format_params_.frame_rate.numerator = frame_rate_.numerator;
    video_format_params_.frame_rate.denominator = frame_rate_.denominator;
    video_format_params_.bit_depth_luma_minus8 = p_seq_header->color_config.bit_depth - 8;
    video_format_params_.bit_depth_chroma_minus8 = p_seq_header->color_config.bit_depth - 8;
    video_format_params_.progressive_sequence = 1;
    video_format_params_.min_num_decode_surfaces = dec_buf_pool_size_;
    video_format_params_.coded_width = pic_width_;
    video_format_params_.coded_height = pic_height_;

    // 6.4.2. Color config semantics
    if (p_seq_header->color_config.mono_chrome == 1 && p_seq_header->color_config.subsampling_x == 1 && p_seq_header->color_config.subsampling_y == 1) {
            video_format_params_.chroma_format = rocDecVideoChromaFormat_Monochrome;
    } else if (p_seq_header->color_config.mono_chrome == 0 && p_seq_header->color_config.subsampling_x == 1 && p_seq_header->color_config.subsampling_y == 1) {
        video_format_params_.chroma_format = rocDecVideoChromaFormat_420;
    } else if (p_seq_header->color_config.mono_chrome == 0 && p_seq_header->color_config.subsampling_x == 1 && p_seq_header->color_config.subsampling_y == 0) {
        video_format_params_.chroma_format = rocDecVideoChromaFormat_422;
    } else if (p_seq_header->color_config.mono_chrome == 0 && p_seq_header->color_config.subsampling_x == 0 && p_seq_header->color_config.subsampling_y == 0) {
        video_format_params_.chroma_format = rocDecVideoChromaFormat_444;
    } else {
        ERR("Incorrect chroma format.");
        return PARSER_INVALID_FORMAT;
    }

    video_format_params_.display_area.left = 0;
    video_format_params_.display_area.top = 0;
    video_format_params_.display_area.right = p_frame_header->render_size.render_width;
    video_format_params_.display_area.bottom = p_frame_header->render_size.render_height;
    video_format_params_.bitrate = 0;

    // Dispaly aspect ratio
    int disp_width = (video_format_params_.display_area.right - video_format_params_.display_area.left);
    int disp_height = (video_format_params_.display_area.bottom - video_format_params_.display_area.top);
    int gcd = std::__gcd(disp_width, disp_height); // greatest common divisor
    video_format_params_.display_aspect_ratio.x = disp_width / gcd;
    video_format_params_.display_aspect_ratio.y = disp_height / gcd;

    video_format_params_.video_signal_description = {0};
    video_format_params_.seqhdr_data_length = 0;

    // callback function with RocdecVideoFormat params filled out
    if (pfn_sequece_cb_(parser_params_.user_data, &video_format_params_) == 0) {
        ERR("Sequence callback function failed.");
        return PARSER_FAIL;
    } else {
        return PARSER_OK;
    }*/
    
    return PARSER_OK;
}

ParserResult Vp9VideoParser::SendPicForDecode() {
    int i, j;
    dec_pic_params_ = {0};

    /*dec_pic_params_.pic_width = pic_width_;
    dec_pic_params_.pic_height = pic_height_;
    dec_pic_params_.curr_pic_idx = curr_pic_.dec_buf_idx;
    dec_pic_params_.field_pic_flag = 0;
    dec_pic_params_.bottom_field_flag = 0;
    dec_pic_params_.second_field = 0;

    dec_pic_params_.bitstream_data_len = pic_stream_data_size_;
    dec_pic_params_.bitstream_data = pic_stream_data_ptr_;
    dec_pic_params_.num_slices = tile_group_data_.num_tiles;

    dec_pic_params_.ref_pic_flag = 1;
    dec_pic_params_.intra_pic_flag = p_frame_header->frame_is_intra;

    // Set up the picture parameter buffer
    RocdecVp9PicParams *p_pic_param = &dec_pic_params_.pic_params.vp9;*/

    if (pfn_decode_picture_cb_(parser_params_.user_data, &dec_pic_params_) == 0) {
        ERR("Decode error occurred.");
        return PARSER_FAIL;
    } else {
        return PARSER_OK;
    }
}



void Vp9VideoParser::InitDpb() {
    int i;
    memset(&dpb_buffer_, 0, sizeof(DecodedPictureBuffer));
    for (i = 0; i < VP9_NUM_REF_FRAMES; i++) {
        dpb_buffer_.frame_store[i].pic_idx = i;
        dpb_buffer_.frame_store[i].use_status = kNotUsed;
    }
}

ParserResult Vp9VideoParser::FlushDpb() {
    if (pfn_display_picture_cb_ && num_output_pics_ > 0) {
        if (OutputDecodedPictures(true) != PARSER_OK) {
            return PARSER_FAIL;
        }
    }
    return PARSER_OK;
}

ParserResult Vp9VideoParser::FindFreeInDecBufPool() {
    int dec_buf_index;
    // Find a free buffer in decode/display buffer pool to store the decoded image
    for (dec_buf_index = 0; dec_buf_index < dec_buf_pool_size_; dec_buf_index++) {
        if (decode_buffer_pool_[dec_buf_index].use_status == kNotUsed) {
            break;
        }
    }
    if (dec_buf_index == dec_buf_pool_size_) {
        ERR("Could not find a free buffer in decode buffer pool for decoded image.");
        return PARSER_NOT_FOUND;
    }
    curr_pic_.dec_buf_idx = dec_buf_index;
    decode_buffer_pool_[dec_buf_index].use_status |= kFrameUsedForDecode;
    // Todo decode_buffer_pool_[dec_buf_index].pic_order_cnt = curr_pic_.order_hint;
    decode_buffer_pool_[dec_buf_index].pts = curr_pts_;
    return PARSER_OK;
}

ParserResult Vp9VideoParser::FindFreeInDpbAndMark() {
    int i;
    /*for (i = 0; i < VP9_NUM_REF_FRAMES; i++ ) {
        if (dpb_buffer_.dec_ref_count[i] == 0) {
            break;
        }
    }
    if (i == VP9_NUM_REF_FRAMES) {
        ERR("DPB buffer overflow!");
        return PARSER_NOT_FOUND;
    }
    curr_pic_.pic_idx = i;
    curr_pic_.use_status = kFrameUsedForDecode;
    dpb_buffer_.frame_store[curr_pic_.pic_idx] = curr_pic_;
    dpb_buffer_.dec_ref_count[curr_pic_.pic_idx]++;
    // Mark as used in decode/display buffer pool
    if (pfn_display_picture_cb_ && curr_pic_.show_frame) {
        int disp_idx = 0xFF;
        if (seq_header_.film_grain_params_present && frame_header_.film_grain_params.apply_grain) {
            disp_idx = curr_pic_.fg_buf_idx;
        } else {
            disp_idx = curr_pic_.dec_buf_idx;
        }
        decode_buffer_pool_[disp_idx].use_status |= kFrameUsedForDisplay;
        decode_buffer_pool_[disp_idx].pts = curr_pts_;
        // Insert into output/display picture list
        if (num_output_pics_ >= dec_buf_pool_size_) {
            ERR("Display list size larger than decode buffer pool size!");
            return PARSER_OUT_OF_RANGE;
        } else {
            output_pic_list_[num_output_pics_] = disp_idx;
            num_output_pics_++;
        }
    }*/

    return PARSER_OK;
}

ParserResult Vp9VideoParser::ParseUncompressedHeader(uint8_t *p_stream, size_t size, int *p_bytes_parsed) {
    ParserResult ret = PARSER_OK;
    size_t offset = 0;  // current bit offset
    Vp9UncompressedHeader *p_uncomp_header = &uncompressed_header_;

    p_uncomp_header->frame_marker = Parser::ReadBits(p_stream, offset, 2);
    p_uncomp_header->profile_low_bit = Parser::GetBit(p_stream, offset);
    p_uncomp_header->profile_high_bit = Parser::GetBit(p_stream, offset);
    p_uncomp_header->profile = (p_uncomp_header->profile_high_bit << 1) + p_uncomp_header->profile_low_bit;
    if (p_uncomp_header->profile == 3) {
        p_uncomp_header->reserved_zero = Parser::GetBit(p_stream, offset);
        if (p_uncomp_header->reserved_zero) {
            ERR("Syntax error: reserved_zero in Uncompressed header is not 0 when Profile is 3");
            return PARSER_INVALID_ARG;
        }
    }
    p_uncomp_header->show_existing_frame = Parser::GetBit(p_stream, offset);
    if (p_uncomp_header->show_existing_frame) {
        p_uncomp_header->frame_to_show_map_idx = Parser::ReadBits(p_stream, offset, 3);
        header_size_in_bytes_ = 0;
        p_uncomp_header->refresh_frame_flags = 0;
        loop_filter_level_ = 0;
        return PARSER_OK;
    }
    last_frame_type_ = p_uncomp_header->frame_type;
    p_uncomp_header->frame_type = Parser::GetBit(p_stream, offset);
    p_uncomp_header->show_frame = Parser::GetBit(p_stream, offset);
    p_uncomp_header->error_resilient_mode = Parser::GetBit(p_stream, offset);
    if (p_uncomp_header->frame_type == kVp9KeyFrame) {
        if ((ret = FrameSyncCode(p_stream, offset, p_uncomp_header)) != PARSER_OK) {
            return ret;
        }
        if ((ret = ColorConfig(p_stream, offset, p_uncomp_header)) != PARSER_OK) {
            return ret;
        }
        FrameSize(p_stream, offset, p_uncomp_header);
        RenderSize(p_stream, offset, p_uncomp_header);
        p_uncomp_header->refresh_frame_flags = 0xFF;
        frame_is_intra_ = 1;
    } else {
        if (p_uncomp_header->show_frame == 0) {
            p_uncomp_header->intra_only = Parser::GetBit(p_stream, offset);
        } else {
            p_uncomp_header->intra_only = 0;
        }
        frame_is_intra_ = p_uncomp_header->intra_only;
        if (p_uncomp_header->error_resilient_mode == 0) {
            p_uncomp_header->reset_frame_context = Parser::ReadBits(p_stream, offset, 2);
        } else {
            p_uncomp_header->reset_frame_context = 0;
        }
        if (p_uncomp_header->intra_only == 1) {
            if ((ret = FrameSyncCode(p_stream, offset, p_uncomp_header)) != PARSER_OK) {
                return ret;
            }
            if (p_uncomp_header->profile > 0) {
                if ((ret = ColorConfig(p_stream, offset, p_uncomp_header)) != PARSER_OK) {
                    return ret;
                }                
            } else {
                p_uncomp_header->color_config.color_space = CS_BT_601;
                p_uncomp_header->color_config.subsampling_x = 1;
                p_uncomp_header->color_config.subsampling_y = 1;
                p_uncomp_header->color_config.bit_depth = 8;
            }
            p_uncomp_header->refresh_frame_flags = Parser::ReadBits(p_stream, offset, 8);
            FrameSize(p_stream, offset, p_uncomp_header);
            RenderSize(p_stream, offset, p_uncomp_header);
        } else {
            p_uncomp_header->refresh_frame_flags = Parser::ReadBits(p_stream, offset, 8);
            for (int i = 0; i < VP9_REFS_PER_FRAME; i++) {
                p_uncomp_header->ref_frame_idx[i] = Parser::ReadBits(p_stream, offset, 3);
                p_uncomp_header->ref_frame_sign_bias[kVp9LastFrame + i] = Parser::GetBit(p_stream, offset);
            }
            FrameSizeWithRefs(p_stream, offset, p_uncomp_header);
            p_uncomp_header->allow_high_precision_mv = Parser::GetBit(p_stream, offset);
            // read_interpolation_filter()
            uint8_t literal_to_type[4] = {kVp9EightTapSmooth, kVp9EightTap, kVp9EightTapSharp, kVp9Bilinear};
            p_uncomp_header->is_filter_switchable = Parser::GetBit(p_stream, offset);
            if (p_uncomp_header->is_filter_switchable) {
                p_uncomp_header->interpolation_filter = kVp9Switchable;
            } else {
                p_uncomp_header->raw_interpolation_filter = Parser::ReadBits(p_stream, offset, 2);
                p_uncomp_header->interpolation_filter = literal_to_type[p_uncomp_header->raw_interpolation_filter];
            }
        }
    }
    if (p_uncomp_header->error_resilient_mode == 0) {
        p_uncomp_header->refresh_frame_context = Parser::GetBit(p_stream, offset);
        p_uncomp_header->frame_parallel_decoding_mode = Parser::GetBit(p_stream, offset);
    } else {
        p_uncomp_header->refresh_frame_context = 0;
        p_uncomp_header->frame_parallel_decoding_mode = 1;
    }
    p_uncomp_header->frame_context_idx = Parser::ReadBits(p_stream, offset, 2);
    if (frame_is_intra_ || p_uncomp_header->error_resilient_mode) {
        // Todo: setup_past_independence()
        if (p_uncomp_header->frame_type == kVp9KeyFrame || p_uncomp_header->error_resilient_mode == 1 || p_uncomp_header->reset_frame_context == 3) {
            for (int i = 0; i < 4; i++) {
                // Todo: save_probs( i )
            }
        } else if (p_uncomp_header->reset_frame_context == 2) {
            // Todo: save_probs(p_uncomp_header->frame_context_idx)
        }
        p_uncomp_header->frame_context_idx = 0;
    }
    LoopFilterParams(p_stream, offset, p_uncomp_header);
    QuantizationParams(p_stream, offset, p_uncomp_header);
    SegmentationParams(p_stream, offset, p_uncomp_header);
    TileInfo(p_stream, offset, p_uncomp_header);

    p_uncomp_header->header_size_in_bytes = Parser::ReadBits(p_stream, offset, 16);

    *p_bytes_parsed = (offset + 7) >> 3;
    return PARSER_OK;
}

ParserResult Vp9VideoParser::FrameSyncCode(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    p_uncomp_header->frame_sync_code.frame_sync_byte_0 = Parser::ReadBits(p_stream, offset, 8);
    if (p_uncomp_header->frame_sync_code.frame_sync_byte_0 != 0x49) {
        ERR("Syntax error: frame_sync_byte_0 is " + TOSTR(p_uncomp_header->frame_sync_code.frame_sync_byte_0) + " but shall be equal to 0x49.");
        return PARSER_INVALID_ARG;
    }
    p_uncomp_header->frame_sync_code.frame_sync_byte_1 = Parser::ReadBits(p_stream, offset, 8);
    if (p_uncomp_header->frame_sync_code.frame_sync_byte_1 != 0x83) {
        ERR("Syntax error: frame_sync_byte_1 is " + TOSTR(p_uncomp_header->frame_sync_code.frame_sync_byte_1) + " but shall be equal to 0x83.");
        return PARSER_INVALID_ARG;
    }
    p_uncomp_header->frame_sync_code.frame_sync_byte_2 = Parser::ReadBits(p_stream, offset, 8);
    if (p_uncomp_header->frame_sync_code.frame_sync_byte_2 != 0x42) {
        ERR("Syntax error: frame_sync_byte_2 is " + TOSTR(p_uncomp_header->frame_sync_code.frame_sync_byte_2) + " but shall be equal to 0x42.");
        return PARSER_INVALID_ARG;
    }
    return PARSER_OK;
}

ParserResult Vp9VideoParser::ColorConfig(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    if (p_uncomp_header->profile >= 2) {
        p_uncomp_header->color_config.ten_or_twelve_bit = Parser::GetBit(p_stream, offset);
        p_uncomp_header->color_config.bit_depth = p_uncomp_header->color_config.ten_or_twelve_bit ? 12 : 10;
    } else {
        p_uncomp_header->color_config.bit_depth = 8;
    }
    p_uncomp_header->color_config.color_space = Parser::ReadBits(p_stream, offset, 3);
    if (p_uncomp_header->color_config.color_space != CS_RGB) {
        p_uncomp_header->color_config.color_range = Parser::GetBit(p_stream, offset);
        if (p_uncomp_header->profile == 1 || p_uncomp_header->profile == 3) {
            p_uncomp_header->color_config.subsampling_x = Parser::GetBit(p_stream, offset);
            p_uncomp_header->color_config.subsampling_y = Parser::GetBit(p_stream, offset);
            p_uncomp_header->color_config.reserved_zero = Parser::GetBit(p_stream, offset);
            if (p_uncomp_header->color_config.reserved_zero) {
                ERR("Syntax error: reserved_zero in color config is not 0 when Profile is 1 or 3");
                return PARSER_INVALID_ARG;
            }
        } else {
            p_uncomp_header->color_config.subsampling_x = 1;
            p_uncomp_header->color_config.subsampling_y = 1;
        }
    } else {
        p_uncomp_header->color_config.color_range = 1;
        if (p_uncomp_header->profile == 1 || p_uncomp_header->profile == 3) {
            p_uncomp_header->color_config.subsampling_x = 0;
            p_uncomp_header->color_config.subsampling_y = 0;
            p_uncomp_header->color_config.reserved_zero = Parser::GetBit(p_stream, offset);
            if (p_uncomp_header->color_config.reserved_zero) {
                ERR("Syntax error: reserved_zero in color config is not 0 when Profile is 1 or 3");
                return PARSER_INVALID_ARG;
            }
        }
    }
    return PARSER_OK;
}

void Vp9VideoParser::FrameSize(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    p_uncomp_header->frame_size.frame_width_minus_1 = Parser::ReadBits(p_stream, offset, 16);
    p_uncomp_header->frame_size.frame_height_minus_1 = Parser::ReadBits(p_stream, offset, 16);
    p_uncomp_header->frame_size.frame_width = p_uncomp_header->frame_size.frame_width_minus_1 + 1;
    p_uncomp_header->frame_size.frame_height = p_uncomp_header->frame_size.frame_height_minus_1 + 1;
    ComputeImageSize(p_uncomp_header);
}

void Vp9VideoParser::RenderSize(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    p_uncomp_header->render_size.render_and_frame_size_different = Parser::GetBit(p_stream, offset);
    if (p_uncomp_header->render_size.render_and_frame_size_different) {
        p_uncomp_header->render_size.render_width_minus_1 = Parser::ReadBits(p_stream, offset, 16);
        p_uncomp_header->render_size.render_height_minus_1 = Parser::ReadBits(p_stream, offset, 16);
        p_uncomp_header->render_size.render_width = p_uncomp_header->render_size.render_width_minus_1 + 1;
        p_uncomp_header->render_size.render_height = p_uncomp_header->render_size.render_height_minus_1 + 1;
    } else {
        p_uncomp_header->render_size.render_width_minus_1 = p_uncomp_header->frame_size.frame_width_minus_1;
        p_uncomp_header->render_size.render_height_minus_1 = p_uncomp_header->frame_size.frame_height_minus_1;
        p_uncomp_header->render_size.render_width = p_uncomp_header->frame_size.frame_width;
        p_uncomp_header->render_size.render_height = p_uncomp_header->frame_size.frame_height;
    }
}

void Vp9VideoParser::FrameSizeWithRefs(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    uint8_t found_ref;
    for (int i = 0; i < 3; i++) {
        found_ref = Parser::GetBit(p_stream, offset);
        if (found_ref) {
            p_uncomp_header->frame_size.frame_width = dpb_buffer_.ref_frame_width[p_uncomp_header->ref_frame_idx[i]];
            p_uncomp_header->frame_size.frame_height = dpb_buffer_.ref_frame_height[p_uncomp_header->ref_frame_idx[i]];
            break;
        }
    }
    if (found_ref == 0) {
        FrameSize(p_stream, offset, p_uncomp_header);
    } else {
        ComputeImageSize(p_uncomp_header);
    }
    RenderSize(p_stream, offset, p_uncomp_header);
}

void Vp9VideoParser::ComputeImageSize(Vp9UncompressedHeader *p_uncomp_header) {
    p_uncomp_header->frame_size.mi_cols = (p_uncomp_header->frame_size.frame_width + 7) >> 3;
    p_uncomp_header->frame_size.mi_rows = (p_uncomp_header->frame_size.frame_height + 7) >> 3;
    p_uncomp_header->frame_size.sb64_cols = (p_uncomp_header->frame_size.mi_cols + 7) >> 3;
    p_uncomp_header->frame_size.sb64_rows = (p_uncomp_header->frame_size.mi_rows + 7) >> 3;
    // Todo steps in 7.2.6
}

void Vp9VideoParser::LoopFilterParams(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    p_uncomp_header->loop_filter_params.loop_filter_level = Parser::ReadBits(p_stream, offset, 6);
    p_uncomp_header->loop_filter_params.loop_filter_sharpness = Parser::ReadBits(p_stream, offset, 3);
    p_uncomp_header->loop_filter_params.loop_filter_delta_enabled = Parser::GetBit(p_stream, offset);
    if (p_uncomp_header->loop_filter_params.loop_filter_delta_enabled) {
        p_uncomp_header->loop_filter_params.loop_filter_delta_update = Parser::GetBit(p_stream, offset);
        if (p_uncomp_header->loop_filter_params.loop_filter_delta_update) {
            for (int i = 0; i < 4; i++) {
                p_uncomp_header->loop_filter_params.update_ref_delta[i] = Parser::GetBit(p_stream, offset);
                if (p_uncomp_header->loop_filter_params.update_ref_delta[i]) {
                    p_uncomp_header->loop_filter_params.loop_filter_ref_deltas[i] = ReadSigned(p_stream, offset, 6);
                }
            }
            for (int i = 0; i < 2; i++) {
                p_uncomp_header->loop_filter_params.update_mode_delta[i] = Parser::GetBit(p_stream, offset);
                if (p_uncomp_header->loop_filter_params.update_mode_delta[i]) {
                    p_uncomp_header->loop_filter_params.loop_filter_mode_deltas[i] = ReadSigned(p_stream, offset, 6);
                }
            }
        }
    }
}

void Vp9VideoParser::QuantizationParams(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    p_uncomp_header->quantization_params.base_q_idx = Parser::ReadBits(p_stream, offset, 8);
    p_uncomp_header->quantization_params.delta_q_y_dc = ReadDeltaQ(p_stream, offset);
    p_uncomp_header->quantization_params.delta_q_uv_dc = ReadDeltaQ(p_stream, offset);
    p_uncomp_header->quantization_params.delta_q_uv_ac = ReadDeltaQ(p_stream, offset);
    p_uncomp_header->quantization_params.lossless = p_uncomp_header->quantization_params.base_q_idx == 0 && p_uncomp_header->quantization_params.delta_q_y_dc == 0 && p_uncomp_header->quantization_params.delta_q_uv_dc == 0 && p_uncomp_header->quantization_params.delta_q_uv_ac == 0;
}

int8_t Vp9VideoParser::ReadDeltaQ(const uint8_t *p_stream, size_t &offset) {
    uint8_t delta_coded;
    int8_t delta_q;
    delta_coded = Parser::GetBit(p_stream, offset);
    if (delta_coded) {
        delta_q = ReadSigned(p_stream, offset, 4);
    } else {
        delta_q = 0;
    }
    return delta_q;
}

void Vp9VideoParser::SegmentationParams(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    const uint8_t segmentation_feature_bits[VP9_SEG_LVL_MAX] = {8, 6, 2, 0};
    const uint8_t segmentation_feature_signed[VP9_SEG_LVL_MAX] = {1, 1, 0, 0};
    p_uncomp_header->segmentation_params.segmentation_enabled = Parser::GetBit(p_stream, offset);
    if (p_uncomp_header->segmentation_params.segmentation_enabled) {
        p_uncomp_header->segmentation_params.segmentation_update_map = Parser::GetBit(p_stream, offset);
        if (p_uncomp_header->segmentation_params.segmentation_update_map) {
            for (int i = 0; i < 7; i++) {
                p_uncomp_header->segmentation_params.segmentation_tree_probs[i] = ReadProb(p_stream, offset);
            }
            p_uncomp_header->segmentation_params.segmentation_temporal_update = Parser::GetBit(p_stream, offset);
            for (int i = 0; i < 3; i++) {
                p_uncomp_header->segmentation_params.segmentation_pred_prob[i] = p_uncomp_header->segmentation_params.segmentation_temporal_update ? ReadProb(p_stream, offset) : 255;
            }
        }
        p_uncomp_header->segmentation_params.segmentation_update_data = Parser::GetBit(p_stream, offset);
        if (p_uncomp_header->segmentation_params.segmentation_update_data) {
            p_uncomp_header->segmentation_params.segmentation_abs_or_delta_update = Parser::GetBit(p_stream, offset);
            for (int i = 0; i < VP9_MAX_SEGMENTS; i++) {
                for (int j = 0; j < VP9_SEG_LVL_MAX; j++) {
                    int feature_value = 0;
                    p_uncomp_header->segmentation_params.feature_enabled[i][j] = Parser::GetBit(p_stream, offset);
                    if (p_uncomp_header->segmentation_params.feature_enabled[i][j]) {
                        int bits_to_read = segmentation_feature_bits[j];
                        if (bits_to_read) {
                            feature_value = Parser::ReadBits(p_stream, offset, bits_to_read);
                        }
                        if (segmentation_feature_signed[j] == 1) {
                            uint8_t feature_sign = Parser::GetBit(p_stream, offset);
                            if (feature_sign) {
                                feature_value *= -1;
                            }
                        }
                    }
                    p_uncomp_header->segmentation_params.feature_data[i][j] = feature_value;
                }
            }
        }
    }
}

uint8_t Vp9VideoParser::ReadProb(const uint8_t *p_stream, size_t &offset) {
    uint8_t prob_coded;
    uint8_t prob;
    prob_coded = Parser::GetBit(p_stream, offset);
    if (prob_coded) {
        prob = Parser::ReadBits(p_stream, offset, 8);
    } else {
        prob = 255;
    }
    return prob;
}

void Vp9VideoParser::TileInfo(const uint8_t *p_stream, size_t &offset, Vp9UncompressedHeader *p_uncomp_header) {
    // calc_min_log2_tile_cols()
    int min_log2 = 0;
    while ((MAX_TILE_WIDTH_B64 << min_log2) < p_uncomp_header->frame_size.sb64_cols) {
        min_log2++;
    }
    p_uncomp_header->tile_info.min_log2_tile_cols = min_log2;
    // calc_max_log2_tile_cols()
    int max_log2 = 1;
    while ((p_uncomp_header->frame_size.sb64_cols >> max_log2) >= MIN_TILE_WIDTH_B64) {
        max_log2++;
    }
    p_uncomp_header->tile_info.max_log2_tile_cols = max_log2 - 1;
    p_uncomp_header->tile_info.tile_cols_log2 = p_uncomp_header->tile_info.min_log2_tile_cols;
    while (p_uncomp_header->tile_info.tile_cols_log2 < p_uncomp_header->tile_info.max_log2_tile_cols) {
        if (Parser::GetBit(p_stream, offset)) { // increment_tile_cols_log2
            p_uncomp_header->tile_info.tile_cols_log2++;
        } else {
            break;
        }
    }
    p_uncomp_header->tile_info.tile_rows_log2 = Parser::GetBit(p_stream, offset);
    if (p_uncomp_header->tile_info.tile_rows_log2) {
        uint8_t increment_tile_rows_log2 = Parser::GetBit(p_stream, offset);
        p_uncomp_header->tile_info.tile_rows_log2 += increment_tile_rows_log2;
    }
}
