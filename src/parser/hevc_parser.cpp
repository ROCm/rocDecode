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

//size_id = 0
extern int scaling_list_default_0[1][6][16];
//size_id = 1, 2
extern int scaling_list_default_1_2[2][6][64];
//size_id = 3
extern int scaling_list_default_3[1][2][64];

BitStreamParser* CreateHEVCVideoParser() {
    return new HEVCVideoParser();
}

/**
 * @brief Constructs a new HEVCVideoParser object
 * 
 */
HEVCVideoParser::HEVCVideoParser() :
    m_use_start_codes_(false),
    m_current_frame_timestamp_(0),
    m_packet_count_(0),
    m_eof_(false),
    m_fps_(0),
    m_max_frames_number_(0) { 
        m_pmemory_ = new uint8_t [DATA_STREAM_SIZE];
        m_allocated_size_ = DATA_STREAM_SIZE;
        m_memory_size_ = sizeof(m_pmemory_);
        m_pos_ = 0;
}

/**
 * @brief Function to initialize any hevc parser related members
 * 
 * @return rocDecStatus 
 */
rocDecStatus HEVCVideoParser::Initialize(RocdecParserParams *pParams) {
        //todo::
    RocVideoParser::Initialize(pParams);
    return ROCDEC_NOT_IMPLEMENTED;
}

/**
 * @brief Function to Parse video data: Typically called from application when a demuxed picture is ready to be parsed
 * 
 * @param pData: IN: pointer to demuxed data packet
 * @return rocDecStatus 
 */
rocDecStatus HEVCVideoParser::ParseVideoData(RocdecSourceDataPacket *pData) {
    return ROCDEC_NOT_IMPLEMENTED;
}

void HEVCVideoParser::FindFirstFrameSPSandPPS() {
    Seek(PARSER_SEEK_BEGIN, 0, NULL);
    FindSPSandPPS();
}

HEVCVideoParser::~HEVCVideoParser() {
    std::cout << "parsed frames: " << m_packet_count_ << std::endl;
    Close();
}

ParserResult HEVCVideoParser::ReInit() {
    m_current_frame_timestamp_ = 0;
    Seek(PARSER_SEEK_BEGIN, 0, NULL);
    m_packet_count_ = 0;
    m_eof_ = false;
    return PARSER_OK;
}

static const int s_win_unit_x[]={1,2,2,1};
static const int s_win_unit_y[]={1,2,1,1};

static int GetWinUnitX (int chromaFormatIdc) { return s_win_unit_x[chromaFormatIdc]; }
// static int GetWinUnitX (int chromaFormatIdc) { return s_win_unit_y[chromaFormatIdc]; }

void HEVCVideoParser::SetFrameRate(double fps) {
    m_fps_ = fps;
}

double HEVCVideoParser::GetFrameRate() const {
    if(m_fps_ != 0) {
        return m_fps_;
    }
    if(m_sps_map_.size() > 0) {
        const SpsData &sps = m_sps_map_.cbegin()->second;
        if(sps.vui_parameters_present_flag && sps.vui_parameters.vui_timing_info_present_flag && sps.vui_parameters.vui_num_units_in_tick) {
            // according to the latest h264 standard nuit_field_based_flag is always = 1 and therefore this must be divided by two 
            // some old clips may get wrong FPS. This is just a sample. Use container information
            return (double)sps.vui_parameters.vui_time_scale / sps.vui_parameters.vui_num_units_in_tick / 2;
        }
    }
    return 25.0;
}

HEVCVideoParser::NalUnitHeader HEVCVideoParser::ReadNextNaluUnit(size_t *offset, size_t *nalu, size_t *size) {
    *size = 0;
    size_t start_offset = *offset;

    bool new_nal_found = false;
    size_t zeros_count = 0;

    while (!new_nal_found) {
        // read next portion if needed
        size_t ready = m_read_data_.GetSize() - *offset;
        printf("ReadNextNaluUnit: remaining data size for read: %zu\n", ready);
        if (ready == 0) {
            if (m_eof_ == false) {
                m_read_data_.SetSize(m_read_data_.GetSize() + m_read_size_);
                ready = 0;
                Read(m_read_data_.GetData() + *offset, m_read_size_, &ready);
            }
            if (ready != m_read_size_ && ready != 0) {
                m_read_data_.SetSize(m_read_data_.GetSize() - (m_read_size_ - ready));
            }
            if (ready == 0 ) {
                if (m_eof_ == false)
                m_read_data_.SetSize(m_read_data_.GetSize() - m_read_size_);

                //m_eof_ = true;
                new_nal_found = start_offset != *offset; 
                *offset = m_read_data_.GetSize();
                break; // EOF
            }
        }

        uint8_t* data = m_read_data_.GetData();
        if (data == nullptr) { // check data before adding the offset
            NalUnitHeader header_nalu;
            header_nalu.nal_unit_type = NAL_UNIT_INVALID;
            return header_nalu; // no data read
        }
        data += *offset; // don't forget the offset!

        for (size_t i = 0; i < ready; i++) {
            uint8_t ch = *data++;
            if (0 == ch) {
                zeros_count++;
            }
            else {
                if (1 == ch && zeros_count >=2) { // We found a start code in Annex B stream
                    if (*offset + (i - zeros_count) > start_offset) {
                        ready = i - zeros_count;
                        new_nal_found = true; // new NAL
                        break; 
                    }
                    else {
                        *nalu = *offset + zeros_count + 1;
                    }
                }
                zeros_count = 0;
            }
        }
        // if zeros found but not a new NAL - continue with zeros_count on the next iteration
        *offset += ready;
    }
    if (!new_nal_found) {
        NalUnitHeader header_nalu;
        header_nalu.nal_unit_type = NAL_UNIT_INVALID;
        return header_nalu; // EOF
    }
    *size = *offset - *nalu;
    // get NAL type
    return GetNaluUnitType(m_read_data_.GetData() + *nalu);
}

ParserResult HEVCVideoParser::QueryOutput(ParserBuffer** pp_data) {
    if ((m_eof_ && m_read_data_.GetSize() == 0) || m_max_frames_number_ && m_packet_count_ >= m_max_frames_number_) {
        return PARSER_EOF;
    }
    bool new_picture_detected = false;
    size_t packet_size = 0;
    size_t read_size = 0;
    std::vector<size_t> nalu_starts;
    std::vector<size_t> nalu_sizes;
    size_t data_offset = 0;
    bool b_slice_found = false;
    uint32_t prev_slice_nal_unit_type = 0;

    do {
        size_t nalu_size = 0;
        size_t nalu_offset = 0;
        size_t nalu_annex_boffset = data_offset;
        NalUnitHeader   nalu_header = ReadNextNaluUnit(&data_offset, &nalu_offset, &nalu_size);
        if (b_slice_found == true) {
            if (prev_slice_nal_unit_type != nalu_header.nal_unit_type) {
                new_picture_detected = true;
            }
        }

        if (NAL_UNIT_ACCESS_UNIT_DELIMITER == nalu_header.nal_unit_type) {
            if (packet_size > 0) {
                new_picture_detected = true;
            }
        }
        else if (NAL_UNIT_PREFIX_SEI == nalu_header.nal_unit_type) {
            if (b_slice_found) {
                new_picture_detected = true;
            }
        }
        else if (
        NAL_UNIT_CODED_SLICE_TRAIL_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TRAIL_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TLA_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TSA_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_STSA_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_STSA_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_W_LP == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_W_RADL == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_N_LP == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_IDR_W_RADL == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_IDR_N_LP == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_CRA == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RADL_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RADL_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RASL_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RASL_R == nalu_header.nal_unit_type
        ) {
            if (b_slice_found == true) {
                if (prev_slice_nal_unit_type != nalu_header.nal_unit_type) {
                    new_picture_detected = true;
                }
                else {
                    AccessUnitSigns nalu_access_units_signs;
                    nalu_access_units_signs.Parse(m_read_data_.GetData() + nalu_offset, nalu_size, m_sps_map_, m_pps_map_);
                    new_picture_detected = nalu_access_units_signs.IsNewPicture() && b_slice_found;
                }
                b_slice_found = true;
                prev_slice_nal_unit_type = nalu_header.nal_unit_type;
            }
            else {
                AccessUnitSigns nalu_access_units_signs;
                nalu_access_units_signs.Parse(m_read_data_.GetData() + nalu_offset, nalu_size, m_sps_map_, m_pps_map_);
                new_picture_detected = nalu_access_units_signs.IsNewPicture() && b_slice_found;
                b_slice_found = true;
                prev_slice_nal_unit_type = nalu_header.nal_unit_type;
            }
        }

		if (nalu_size > 0 && !new_picture_detected ) {
            packet_size += nalu_size;
            if (!m_use_start_codes_) {
                packet_size += nal_unit_length_size_;
                nalu_starts.push_back(nalu_offset);
                nalu_sizes.push_back(nalu_size);
            }
            else {
                size_t startCodeSize = nalu_offset - nalu_annex_boffset;
                packet_size += startCodeSize;
            }
        }
        if (!new_picture_detected) {
            read_size = data_offset;
        }
        if (nalu_header.nal_unit_type == NAL_UNIT_INVALID) {
	  		break;
        }
    } while (!new_picture_detected);

    ParserResult ar = ParserBuffer::AllocBuffer(PARSER_MEMORY_HOST, packet_size, pp_data);
    if (ar != PARSER_OK) {
        return ar;
    }
    
    uint8_t *data = (uint8_t*)(*pp_data)->GetNative();
    if (m_use_start_codes_) {
        memcpy(data, m_read_data_.GetData(), packet_size);
    }
    else {
        for (size_t i=0; i < nalu_starts.size(); i++) {
            // copy size
            uint32_t nalu_size= (uint32_t)nalu_sizes[i];
            *data++ = (nalu_size >> 24);
            *data++ = static_cast<uint8_t>(((nalu_size & 0x00FF0000) >> 16));
            *data++ = ((nalu_size & 0x0000FF00) >> 8);
            *data++ = ((nalu_size & 0x000000FF));
            memcpy(data, m_read_data_.GetData() + nalu_starts[i], nalu_size);
            data += nalu_size;
        }
    }
    (*pp_data)->SetPts(m_current_frame_timestamp_);
    int64_t frame_duration = int64_t(PARSER_SECOND / GetFrameRate()); // In 100 NanoSeconds
    (*pp_data)->SetDuration(frame_duration);
    m_current_frame_timestamp_ += frame_duration;

    // shift remaining data in m_ReadData
    size_t remaining_data = m_read_data_.GetSize() - read_size;
    memmove(m_read_data_.GetData(), m_read_data_.GetData()+read_size, remaining_data);
    m_read_data_.SetSize(remaining_data);

    m_packet_count_++;

    return PARSER_OK;
}

void HEVCVideoParser::FindSPSandPPS() {
    ExtraDataBuilder extra_data_builder;

    size_t data_offset = 0;
    do {
        
        size_t nalu_size = 0;
        size_t nalu_offset = 0;
        NalUnitHeader nalu_header = ReadNextNaluUnit(&data_offset, &nalu_offset, &nalu_size);

        if (nalu_header.nal_unit_type == NAL_UNIT_INVALID ) {
            break; // EOF
        }

        if (nalu_header.nal_unit_type == NAL_UNIT_SPS) {
            m_EBSP_to_RBSP_data_.SetSize(nalu_size);
            memcpy(m_EBSP_to_RBSP_data_.GetData(), m_read_data_.GetData() + nalu_offset, nalu_size);
            size_t newNaluSize = EBSPtoRBSP(m_EBSP_to_RBSP_data_.GetData(),0, nalu_size);

            SpsData sps;
            sps.Parse(m_EBSP_to_RBSP_data_.GetData(), newNaluSize);
            m_sps_map_[sps.sps_video_parameter_set_id] = sps;
            extra_data_builder.AddSPS(m_read_data_.GetData()+nalu_offset, nalu_size);
        }
        else if (nalu_header.nal_unit_type == NAL_UNIT_PPS) {
            m_EBSP_to_RBSP_data_.SetSize(nalu_size);
            memcpy(m_EBSP_to_RBSP_data_.GetData(), m_read_data_.GetData() + nalu_offset, nalu_size);
            size_t newNaluSize = EBSPtoRBSP(m_EBSP_to_RBSP_data_.GetData(),0, nalu_size);

            PpsData pps;
            pps.Parse(m_EBSP_to_RBSP_data_.GetData(), newNaluSize);
            m_pps_map_[pps.pps_pic_parameter_set_id] = pps;
            extra_data_builder.AddPPS(m_read_data_.GetData()+nalu_offset, nalu_size);
        }
        else if (
        NAL_UNIT_CODED_SLICE_TRAIL_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TRAIL_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TLA_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_TSA_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_STSA_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_STSA_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_W_LP == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_W_RADL == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_BLA_N_LP == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_IDR_W_RADL == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_IDR_N_LP == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_CRA == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RADL_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RADL_R == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RASL_N == nalu_header.nal_unit_type
        || NAL_UNIT_CODED_SLICE_RASL_R == nalu_header.nal_unit_type
        ) {
            break; // frame data
        }
    } while (true);

    Seek(PARSER_SEEK_BEGIN, 0, NULL);
    m_read_data_.SetSize(0);
    // It will fail if SPS or PPS are absent
    extra_data_builder.GetExtradata(m_extra_data_);
}

bool HEVCVideoParser::SpsData::Parse(uint8_t *nalu, size_t size) {
    size_t offset = 16; // 2 bytes NALU header + 
    uint32_t active_vps = Parser::ReadBits(nalu, offset,4);
    uint32_t max_sub_layer_minus1 = Parser::ReadBits(nalu, offset,3);
    sps_temporal_id_nesting_flag = Parser::GetBit(nalu, offset);
    H265ProfileTierLevel ptl;
    memset (&ptl,0,sizeof(ptl));
    ParsePTL(&ptl, true, max_sub_layer_minus1, nalu, size, offset);
    uint32_t sps_id = Parser::ExpGolomb::ReadUe(nalu, offset);

    sps_video_parameter_set_id = active_vps;
    sps_max_sub_layers_minus1 = max_sub_layer_minus1;
    memcpy (&profile_tier_level,&ptl,sizeof(ptl));
    sps_seq_parameter_set_id = sps_id;

    chroma_format_idc = Parser::ExpGolomb::ReadUe(nalu, offset);
    if (chroma_format_idc == 3) {
        separate_colour_plane_flag = Parser::GetBit(nalu, offset);
    }
    pic_width_in_luma_samples = Parser::ExpGolomb::ReadUe(nalu, offset);
    pic_height_in_luma_samples = Parser::ExpGolomb::ReadUe(nalu, offset);
    conformance_window_flag = Parser::GetBit(nalu, offset);
    if (conformance_window_flag) {
        conf_win_left_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        conf_win_right_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        conf_win_top_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        conf_win_bottom_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    bit_depth_luma_minus8 = Parser::ExpGolomb::ReadUe(nalu, offset);
    bit_depth_chroma_minus8 = Parser::ExpGolomb::ReadUe(nalu, offset);
    log2_max_pic_order_cnt_lsb_minus4 = Parser::ExpGolomb::ReadUe(nalu, offset);
    sps_sub_layer_ordering_info_present_flag = Parser::GetBit(nalu, offset);
    for (uint32_t i = (sps_sub_layer_ordering_info_present_flag?0:sps_max_sub_layers_minus1); i <= sps_max_sub_layers_minus1; i++) {
        sps_max_dec_pic_buffering_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_max_num_reorder_pics[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        sps_max_latency_increase_plus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    log2_min_luma_coding_block_size_minus3 = Parser::ExpGolomb::ReadUe(nalu, offset);

    int log2_min_cu_size = log2_min_luma_coding_block_size_minus3 +3;

    log2_diff_max_min_luma_coding_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);

    int max_cu_depth_delta = log2_diff_max_min_luma_coding_block_size;
    max_cu_width = ( 1<<(log2_min_cu_size + max_cu_depth_delta) );
    max_cu_height = ( 1<<(log2_min_cu_size + max_cu_depth_delta) );

    log2_min_transform_block_size_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);

    uint32_t quadtree_tu_log2_min_size = log2_min_transform_block_size_minus2 + 2;
    int add_cu_depth = std::max (0, log2_min_cu_size - (int)quadtree_tu_log2_min_size );
    max_cu_depth = (max_cu_depth_delta + add_cu_depth);

    log2_diff_max_min_transform_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);
    max_transform_hierarchy_depth_inter = Parser::ExpGolomb::ReadUe(nalu, offset);
    max_transform_hierarchy_depth_intra = Parser::ExpGolomb::ReadUe(nalu, offset);
    scaling_list_enabled_flag = Parser::GetBit(nalu, offset);
    if (scaling_list_enabled_flag) {
        sps_scaling_list_data_present_flag = Parser::GetBit(nalu, offset);
        if (sps_scaling_list_data_present_flag) {
            ParseScalingList(&scaling_list_data, nalu, size, offset);
        }
    }
    amp_enabled_flag = Parser::GetBit(nalu, offset);
    sample_adaptive_offset_enabled_flag = Parser::GetBit(nalu, offset);
    pcm_enabled_flag = Parser::GetBit(nalu, offset);
    if (pcm_enabled_flag) {
        pcm_sample_bit_depth_luma_minus1 = Parser::ReadBits(nalu, offset,4);
        pcm_sample_bit_depth_chroma_minus1 = Parser::ReadBits(nalu, offset,4);
        log2_min_pcm_luma_coding_block_size_minus3 = Parser::ExpGolomb::ReadUe(nalu, offset);
        log2_diff_max_min_pcm_luma_coding_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);
        pcm_loop_filter_disabled_flag = Parser::GetBit(nalu, offset);
    }
    num_short_term_ref_pic_sets = Parser::ExpGolomb::ReadUe(nalu, offset);
    for (uint32_t i=0; i<num_short_term_ref_pic_sets; i++) {
        //short_term_ref_pic_set( i )
        ParseShortTermRefPicSet(&stRPS[i], i, num_short_term_ref_pic_sets, stRPS, nalu, size, offset);
    }
    long_term_ref_pics_present_flag = Parser::GetBit(nalu, offset);
    if (long_term_ref_pics_present_flag) {
        num_long_term_ref_pics_sps = Parser::ExpGolomb::ReadUe(nalu, offset);
        ltRPS.num_of_pics = num_long_term_ref_pics_sps;
        for (uint32_t i=0; i<num_long_term_ref_pics_sps; i++) {
            //The number of bits used to represent lt_ref_pic_poc_lsb_sps[ i ] is equal to log2_max_pic_order_cnt_lsb_minus4 + 4.
            lt_ref_pic_poc_lsb_sps[i] = Parser::ReadBits(nalu, offset,(log2_max_pic_order_cnt_lsb_minus4 + 4));
            used_by_curr_pic_lt_sps_flag[i] = Parser::GetBit(nalu, offset);
            ltRPS.POCs[i]=lt_ref_pic_poc_lsb_sps[i];
            ltRPS.used_by_curr_pic[i] = used_by_curr_pic_lt_sps_flag[i];            
        }
    }
    sps_temporal_mvp_enabled_flag = Parser::GetBit(nalu, offset);
    strong_intra_smoothing_enabled_flag = Parser::GetBit(nalu, offset);
    vui_parameters_present_flag = Parser::GetBit(nalu, offset);
    if (vui_parameters_present_flag) {
        //vui_parameters()
        ParseVUI(&vui_parameters, sps_max_sub_layers_minus1, nalu, size, offset);
    }
    sps_extension_flag = Parser::GetBit(nalu, offset);
    if( sps_extension_flag ) {
        //while( more_rbsp_data( ) )
            //sps_extension_data_flag u(1)
    }
    return true;
}

bool HEVCVideoParser::PpsData::Parse(uint8_t *nalu, size_t size) {
    size_t offset = 16; // 2 bytes NALU header

    uint32_t pps_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    
    pps_pic_parameter_set_id = pps_id;
    uint32_t active_sps = Parser::ExpGolomb::ReadUe(nalu, offset);

    pps_seq_parameter_set_id = active_sps;
    dependent_slice_segments_enabled_flag = Parser::GetBit(nalu, offset);
    output_flag_present_flag = Parser::GetBit(nalu, offset);
    num_extra_slice_header_bits = Parser::ReadBits(nalu, offset,3);
    sign_data_hiding_enabled_flag = Parser::GetBit(nalu, offset);
    cabac_init_present_flag = Parser::GetBit(nalu, offset);
    num_ref_idx_l0_default_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    num_ref_idx_l1_default_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    init_qp_minus26 = Parser::ExpGolomb::ReadSe(nalu, offset);
    constrained_intra_pred_flag = Parser::GetBit(nalu, offset);
    transform_skip_enabled_flag = Parser::GetBit(nalu, offset);
    cu_qp_delta_enabled_flag = Parser::GetBit(nalu, offset);
    if (cu_qp_delta_enabled_flag) {
        diff_cu_qp_delta_depth = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    pps_cb_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
    pps_cr_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
    pps_slice_chroma_qp_offsets_present_flag = Parser::GetBit(nalu, offset);
    weighted_pred_flag = Parser::GetBit(nalu, offset);
    weighted_bipred_flag = Parser::GetBit(nalu, offset);
    transquant_bypass_enabled_flag = Parser::GetBit(nalu, offset);
    tiles_enabled_flag = Parser::GetBit(nalu, offset);
    entropy_coding_sync_enabled_flag = Parser::GetBit(nalu, offset);
    if (tiles_enabled_flag) {
        num_tile_columns_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        num_tile_rows_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        uniform_spacing_flag = Parser::GetBit(nalu, offset);
        if (!uniform_spacing_flag) {
            for (uint32_t i=0; i<num_tile_columns_minus1; i++) {
                column_width_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            }
            for (uint32_t i=0; i<num_tile_rows_minus1; i++) {
                row_height_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            }
        }
        loop_filter_across_tiles_enabled_flag = Parser::GetBit(nalu, offset);
    }
    else {
         loop_filter_across_tiles_enabled_flag = 1;
    }
    pps_loop_filter_across_slices_enabled_flag = Parser::GetBit(nalu, offset);
    deblocking_filter_control_present_flag = Parser::GetBit(nalu, offset);
    if (deblocking_filter_control_present_flag) {
        deblocking_filter_override_enabled_flag = Parser::GetBit(nalu, offset);
        pps_deblocking_filter_disabled_flag = Parser::GetBit(nalu, offset);
        if (!pps_deblocking_filter_disabled_flag) {
            pps_beta_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
            pps_tc_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
        }
    }
    pps_scaling_list_data_present_flag = Parser::GetBit(nalu, offset);
    if (pps_scaling_list_data_present_flag) {
        SpsData::ParseScalingList(&scaling_list_data, nalu, size, offset);
    }
    lists_modification_present_flag = Parser::GetBit(nalu, offset);
    log2_parallel_merge_level_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);
    slice_segment_header_extension_present_flag = Parser::GetBit(nalu, offset);
    pps_extension_flag = Parser::GetBit(nalu, offset);
    if (pps_extension_flag) {
        //while( more_rbsp_data( ) )
            //pps_extension_data_flag u(1)
        //rbsp_trailing_bits( )
    }
    return true;
}

void HEVCVideoParser::SpsData::ParsePTL(H265ProfileTierLevel *ptl, bool profile_present_flag, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    if (profile_present_flag) {
        ptl->general_profile_space = Parser::ReadBits(nalu, offset,2);
        ptl->general_tier_flag = Parser::GetBit(nalu, offset);
        ptl->general_profile_idc = Parser::ReadBits(nalu, offset,5);
        for (int i = 0; i < 32; i++) {
            ptl->general_profile_compatibility_flag[i] = Parser::GetBit(nalu, offset);
        }
        ptl->general_progressive_source_flag = Parser::GetBit(nalu, offset);
        ptl->general_interlaced_source_flag = Parser::GetBit(nalu, offset);
        ptl->general_non_packed_constraint_flag = Parser::GetBit(nalu, offset);
        ptl->general_frame_only_constraint_flag = Parser::GetBit(nalu, offset);
        //ReadBits is limited to 32 
        //ptl->general_reserved_zero_44bits = Parser::ReadBits(nalu, offset,44);
        offset += 44;
    }

    ptl->general_level_idc = Parser::ReadBits(nalu, offset,8);
    for(uint32_t i = 0; i < max_num_sub_layers_minus1; i++) {
        ptl->sub_layer_profile_present_flag[i] = Parser::GetBit(nalu, offset);
        ptl->sub_layer_level_present_flag[i] = Parser::GetBit(nalu, offset);
    }
    if (max_num_sub_layers_minus1 > 0) {
        for(uint32_t i=max_num_sub_layers_minus1; i<8; i++) {               
            ptl->reserved_zero_2bits[i] = Parser::ReadBits(nalu, offset,2);
        }
    }
    for (uint32_t i = 0; i < max_num_sub_layers_minus1; i++) {
        if (ptl->sub_layer_profile_present_flag[i]) {
            ptl->sub_layer_profile_space[i] = Parser::ReadBits(nalu, offset,2);
            ptl->sub_layer_tier_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_profile_idc[i] = Parser::ReadBits(nalu, offset,5);
            for (int j = 0; j < 32; j++) {
                ptl->sub_layer_profile_compatibility_flag[i][j] = Parser::GetBit(nalu, offset);
            }
            ptl->sub_layer_progressive_source_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_interlaced_source_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_non_packed_constraint_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_frame_only_constraint_flag[i] = Parser::GetBit(nalu, offset);
            ptl->sub_layer_reserved_zero_44bits[i] = Parser::ReadBits(nalu, offset,44);
        }
        if (ptl->sub_layer_level_present_flag[i]) {
            ptl->sub_layer_level_idc[i] = Parser::ReadBits(nalu, offset,8);
        }
    }
}

void HEVCVideoParser::SpsData::ParseSubLayerHrdParameters(H265SubLayerHrdParameters *sub_hrd, uint32_t CpbCnt, bool sub_pic_hrd_params_present_flag, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    for (uint32_t i = 0; i <= CpbCnt; i++) {
        sub_hrd->bit_rate_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        sub_hrd->cpb_size_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        if(sub_pic_hrd_params_present_flag) {
            sub_hrd->cpb_size_du_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            sub_hrd->bit_rate_du_value_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        sub_hrd->cbr_flag[i] = Parser::GetBit(nalu, offset);
    }
}

void HEVCVideoParser::SpsData::ParseHrdParameters(H265HrdParameters *hrd, bool common_inf_present_flag, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t size,size_t &offset) {
    if (common_inf_present_flag) {
        hrd->nal_hrd_parameters_present_flag = Parser::GetBit(nalu, offset);
        hrd->vcl_hrd_parameters_present_flag = Parser::GetBit(nalu, offset);
        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag) {
            hrd->sub_pic_hrd_params_present_flag = Parser::GetBit(nalu, offset);
            if (hrd->sub_pic_hrd_params_present_flag) {
                hrd->tick_divisor_minus2 = Parser::ReadBits(nalu, offset,8);
                hrd->du_cpb_removal_delay_increment_length_minus1 = Parser::ReadBits(nalu, offset,5);
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = Parser::GetBit(nalu, offset);
                hrd->dpb_output_delay_du_length_minus1 = Parser::ReadBits(nalu, offset,5);
            }
            hrd->bit_rate_scale = Parser::ReadBits(nalu, offset,4);
            hrd->cpb_size_scale = Parser::ReadBits(nalu, offset,4);
            if (hrd->sub_pic_hrd_params_present_flag) {
                hrd->cpb_size_du_scale = Parser::ReadBits(nalu, offset,4);
            }
            hrd->initial_cpb_removal_delay_length_minus1 = Parser::ReadBits(nalu, offset,5);
            hrd->au_cpb_removal_delay_length_minus1 = Parser::ReadBits(nalu, offset,5);
            hrd->dpb_output_delay_length_minus1 = Parser::ReadBits(nalu, offset,5);
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

void HEVCVideoParser::SpsData::ParseScalingList(H265ScalingListData * s_data, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    for (int size_id = 0; size_id < 4; size_id++) {
        for (int matrix_id = 0; matrix_id < ((size_id == 3) ? 2:6); matrix_id++) {
            s_data->scaling_list_pred_mode_flag[size_id][matrix_id] = Parser::GetBit(nalu, offset);
            if(!s_data->scaling_list_pred_mode_flag[size_id][matrix_id]) {
                s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id] = Parser::ExpGolomb::ReadUe(nalu, offset);

                int ref_matrix_id = matrix_id - s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id];
                int coef_num = std::min(64, (1<< (4 + (size_id<<1))));

                //fill in scaling_list_dc_coef_minus8
                if (!s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id]) {
                    if (size_id > 1) {
                        s_data->scaling_list_dc_coef_minus8[size_id-2][matrix_id] = 8;
                    }
                }
                else {
                    if (size_id > 1) {
                        s_data->scaling_list_dc_coef_minus8[size_id-2][matrix_id] = s_data->scaling_list_dc_coef_minus8[size_id-2][ref_matrix_id];
                    }
                }

                for (int i = 0; i < coef_num; i++) {
                    if (s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id] == 0) {
                        if (size_id == 0) {
                            s_data->scaling_list[size_id][matrix_id][i] = scaling_list_default_0[size_id][matrix_id][i];
                        }
                        else if(size_id == 1 || size_id == 2) {
                            s_data->scaling_list[size_id][matrix_id][i] = scaling_list_default_1_2[size_id][matrix_id][i];
                        }
                        else if(size_id == 3) {
                            s_data->scaling_list[size_id][matrix_id][i] = scaling_list_default_3[size_id][matrix_id][i];
                        }
                    }
                    else {
                        s_data->scaling_list[size_id][matrix_id][i] = s_data->scaling_list[size_id][ref_matrix_id][i];
                    }
                }
            }
            else {
                int next_coef = 8;
                int coef_num = std::min(64, (1 << (4 + (size_id << 1))));
                if (size_id > 1) {
                    s_data->scaling_list_dc_coef_minus8[size_id-2][matrix_id] = Parser::ExpGolomb::ReadSe(nalu, offset);
                    next_coef = s_data->scaling_list_dc_coef_minus8[size_id-2][matrix_id] + 8;
                }
                for (int i = 0; i < coef_num; i++) {
                    s_data->scaling_list_delta_coef = Parser::ExpGolomb::ReadSe(nalu, offset);
                    next_coef = (next_coef + s_data->scaling_list_delta_coef +256)%256;
                    s_data->scaling_list[size_id][matrix_id][i] = next_coef;
                }
            }
        }
    }
}

void HEVCVideoParser::SpsData::ParseShortTermRefPicSet(H265ShortTermRPS *rps, int32_t st_rps_idx, uint32_t number_short_term_ref_pic_sets, H265ShortTermRPS rps_ref[], uint8_t *nalu, size_t /*size*/, size_t& offset) {
    uint32_t inter_rps_pred = 0;
    uint32_t delta_idx_minus1 = 0;
    int32_t i = 0;

    if (st_rps_idx != 0) {
        inter_rps_pred = Parser::GetBit(nalu, offset);
    }
    if (inter_rps_pred) {
        uint32_t delta_rps_sign, abs_delta_rps_minus1;
        bool used_by_curr_pic_flag[16] = {0};
        bool use_delta_flag[16] = {0};
        if (unsigned(st_rps_idx) == number_short_term_ref_pic_sets) {
            delta_idx_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        delta_rps_sign = Parser::GetBit(nalu, offset);
        abs_delta_rps_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        int32_t delta_rps = (int32_t) (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
        int32_t ref_idx = st_rps_idx - delta_idx_minus1 - 1;
        for (int j = 0; j <= (rps_ref[ref_idx].num_negative_pics + rps_ref[ref_idx].num_positive_pics); j++) {
            used_by_curr_pic_flag[j] = Parser::GetBit(nalu, offset);
            if (!used_by_curr_pic_flag[j]) {
                use_delta_flag[j] = Parser::GetBit(nalu, offset);
            }
            else {
                use_delta_flag[j] = 1;
            }
        }

        for (int j = rps_ref[ref_idx].num_positive_pics - 1; j >= 0; j--) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[rps_ref[ref_idx].num_negative_pics + j];  //positive delta_poc from ref_rps
            if (delta_poc < 0 && use_delta_flag[rps_ref[ref_idx].num_negative_pics + j]) {
                rps->delta_poc[i] = delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_negative_pics + j];
            }
        }
        if (delta_rps < 0 && use_delta_flag[rps_ref[ref_idx].num_of_pics]) {
            rps->delta_poc[i] = delta_rps;
            rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_of_pics];
        }
        for (int j = 0; j < rps_ref[ref_idx].num_negative_pics; j++) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[j];
            if (delta_poc < 0 && use_delta_flag[j]) {
                rps->delta_poc[i]=delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[j];
            }
        }
        rps->num_negative_pics = i;
        
        
        for (int j = rps_ref[ref_idx].num_negative_pics - 1; j >= 0; j--) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[j];  //positive delta_poc from ref_rps
            if (delta_poc > 0 && use_delta_flag[j]) {
                rps->delta_poc[i] = delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[j];
            }
        }
        if (delta_rps > 0 && use_delta_flag[rps_ref[ref_idx].num_of_pics]) {
            rps->delta_poc[i] = delta_rps;
            rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_of_pics];
        }
        for (int j = 0; j < rps_ref[ref_idx].num_positive_pics; j++) {
            int32_t delta_poc = delta_rps + rps_ref[ref_idx].delta_poc[rps_ref[ref_idx].num_negative_pics+j];
            if (delta_poc > 0 && use_delta_flag[rps_ref[ref_idx].num_negative_pics+j]) {
                rps->delta_poc[i]=delta_poc;
                rps->used_by_curr_pic[i++] = used_by_curr_pic_flag[rps_ref[ref_idx].num_negative_pics+j];
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
        uint32_t delta_poc_s0_minus1,delta_poc_s1_minus1;
        for (int j = 0; j < rps->num_negative_pics; j++) {
            delta_poc_s0_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
            poc = prev - delta_poc_s0_minus1 - 1;
            prev = poc;
            rps->delta_poc[j] = poc;
            rps->used_by_curr_pic[j] = Parser::GetBit(nalu, offset);
        }
        prev = 0;
        for (int j = rps->num_negative_pics; j < rps->num_negative_pics + rps->num_positive_pics; j++) {
            delta_poc_s1_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
            poc = prev + delta_poc_s1_minus1 + 1;
            prev = poc;
            rps->delta_poc[j] = poc;
            rps->used_by_curr_pic[j] = Parser::GetBit(nalu, offset);
        }
        rps->num_of_pics = rps->num_negative_pics + rps->num_positive_pics;
        rps->num_of_delta_poc = rps->num_negative_pics + rps->num_positive_pics;
    }
}

void HEVCVideoParser::SpsData::ParseVUI(H265VuiParameters *vui, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t size, size_t &offset) {
    vui->aspect_ratio_info_present_flag = Parser::GetBit(nalu, offset);
    if (vui->aspect_ratio_info_present_flag) {
        vui->aspect_ratio_idc = Parser::ReadBits(nalu, offset,8);
        if (vui->aspect_ratio_idc == 255) {
            vui->sar_width = Parser::ReadBits(nalu, offset,16);
            vui->sar_height = Parser::ReadBits(nalu, offset,16);
        }
    }
    vui->overscan_info_present_flag = Parser::GetBit(nalu, offset);
    if (vui->overscan_info_present_flag) {
        vui->overscan_appropriate_flag = Parser::GetBit(nalu, offset);
    }
    vui->video_signal_type_present_flag = Parser::GetBit(nalu, offset);
    if (vui->video_signal_type_present_flag) {
        vui->video_format = Parser::ReadBits(nalu, offset,3);
        vui->video_full_range_flag = Parser::GetBit(nalu, offset);
        vui->colour_description_present_flag = Parser::GetBit(nalu, offset);
        if (vui->colour_description_present_flag) {
            vui->colour_primaries = Parser::ReadBits(nalu, offset,8);
            vui->transfer_characteristics = Parser::ReadBits(nalu, offset,8);
            vui->matrix_coeffs = Parser::ReadBits(nalu, offset,8);
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
        vui->vui_num_units_in_tick = Parser::ReadBits(nalu, offset,32);
        vui->vui_time_scale = Parser::ReadBits(nalu, offset,32);
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

bool HEVCVideoParser::AccessUnitSigns::Parse(uint8_t *nalu, size_t /*size*/, std::map<uint32_t, SpsData>&/*sps_map*/, std::map<uint32_t, PpsData>& /*pps_map*/) {
    size_t offset = 16; // 2 bytes NALU header
    b_new_picture = Parser::GetBit(nalu, offset);
    return true;
}

bool HEVCVideoParser::AccessUnitSigns::IsNewPicture() {
    return b_new_picture;
}

void HEVCVideoParser::ExtraDataBuilder::AddSPS(uint8_t *sps, size_t size) {
    m_sps_count_++;
    size_t pos = m_sps_.GetSize();
    uint16_t spsSize = size & max_sps_size_;
    m_sps_.SetSize(pos + spsSize +2);
    uint8_t *data = m_sps_.GetData() + pos;
    *data++ = Parser::GetLowByte(spsSize);
    *data++ = Parser::GetHiByte(spsSize);
    memcpy(data , sps, (size_t)spsSize);
}

void HEVCVideoParser::ExtraDataBuilder::AddPPS(uint8_t *pps, size_t size) {
    m_pps_count_++;
    size_t pos = m_pps_.GetSize();
    uint16_t ppsSize = size & max_pps_size_;
    m_pps_.SetSize(pos + ppsSize + 2);
    uint8_t *data = m_pps_.GetData() + pos;
    *data++ = Parser::GetLowByte(ppsSize);
    *data++ = Parser::GetHiByte(ppsSize);
    memcpy(data , pps, (size_t)ppsSize);
}

bool HEVCVideoParser::ExtraDataBuilder::GetExtradata(ByteArray &extradata) {
    if(m_sps_.GetSize() == 0  || m_pps_ .GetSize() == 0) {
        return false;
    }
    if (m_sps_count_ > 0x1F) {
        return false;
    }
    if (m_sps_.GetSize() < min_sps_size_) {
        return false;
    }
    extradata.SetSize(
        21 +                // reserved
        1 +                 // length size
        1 +                 // array size
        3 +                 // SPS type + SPS count (2)
        m_sps_.GetSize() +
        3 +                 // PPS type + PPS count (2)
        m_pps_.GetSize()
        );

    uint8_t *data = extradata.GetData();
    
    memset(data, 0, extradata.GetSize());

    *data = 0x01; // configurationVersion
    data+=21;
    *data++ = (0xFC | (nal_unit_length_size_ - 1));   // reserved(11111100) + lengthSizeMinusOne

    *data++ = static_cast<uint8_t>(2); // reserved(11100000) + numOfSequenceParameterSets

    *data++ = NAL_UNIT_SPS;
    *data++ = Parser::GetLowByte(static_cast<int16_t>(m_sps_count_));
    *data++ = Parser::GetHiByte(static_cast<int16_t>(m_sps_count_));

    memcpy(data, m_sps_.GetData(), m_sps_.GetSize());
    data += m_sps_.GetSize();

    *data++ = NAL_UNIT_PPS;
    *data++ = Parser::GetLowByte(static_cast<int16_t>(m_pps_count_));
    *data++ = Parser::GetHiByte(static_cast<int16_t>(m_pps_count_));
    memcpy(data, m_pps_.GetData(), m_pps_.GetSize());
    data += m_pps_.GetSize();
    return true;
}

bool HEVCVideoParser::CheckDataStreamEof(int n_video_bytes) {
    if (n_video_bytes <= 0) {
        m_eof_ = true;
        return true;
    }
    return false;
}

#define ZEROBYTES_SHORTSTARTCODE 2 //indicates the number of zero bytes in the short start-code prefix

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

//data stream functions
ParserResult HEVCVideoParser::Close() {
    m_pmemory_ = NULL,
    m_memory_size_ = 0,
    m_allocated_size_ = 0,
    m_pos_ = 0;
    return PARSER_OK;
}

ParserResult HEVCVideoParser::Realloc(size_t size) {
    if (size > m_memory_size_) {
        uint8_t* p_new_memory = new uint8_t [size];
        if (p_new_memory == NULL) {
            return PARSER_OUT_OF_MEMORY;
        }
        m_allocated_size_ = size;
        if (m_pmemory_ != NULL) {
            delete m_pmemory_;
        }
        m_pmemory_ = p_new_memory;
    }
    m_memory_size_ = size;
    return PARSER_OK;
}

ParserResult HEVCVideoParser::Read(void* p_data, size_t size, size_t* p_read) {
    if (p_data == NULL) {
        return PARSER_INVALID_POINTER;
    }
    if (m_pmemory_ == NULL) {
        return PARSER_NOT_INITIALIZED;
    }
    size_t to_read = std::min(size, m_memory_size_ - m_pos_);
    memcpy(p_data, m_pmemory_ + m_pos_, to_read);
    m_pos_ += to_read;
    if(p_read != NULL) {
        *p_read = to_read;
    }
    return PARSER_OK;
}

ParserResult HEVCVideoParser::Write(const void* p_data, size_t size, size_t* p_written) {
    if (p_data == NULL) {
        return PARSER_INVALID_POINTER;
    }
    m_pos_ = 0;
    if (Realloc(size)) {
        return PARSER_STREAM_NOT_ALLOCATED;
    }

    size_t to_write = std::min(size, m_memory_size_);
    memcpy(m_pmemory_, p_data, to_write);

    if(p_written != NULL) {
        *p_written = to_write;
    }
    return PARSER_OK;
}

ParserResult HEVCVideoParser::Seek(ParserSeekOrigin e_origin, int64_t i_position, int64_t* p_new_position) {
    switch(e_origin) {
    case PARSER_SEEK_BEGIN:
        m_pos_ = (size_t)i_position;
        break;

    case PARSER_SEEK_CURRENT:
        m_pos_ += (size_t)i_position;
        break;

    case PARSER_SEEK_END:
        m_pos_ = m_memory_size_ - (size_t)i_position;
        break;
    }

    if(m_pos_ > m_memory_size_) {
        m_pos_ = m_memory_size_;
    }
    if(p_new_position != NULL) {
        *p_new_position = m_pos_;
    }
    return PARSER_OK;
}

ParserResult HEVCVideoParser::GetSize(int64_t* p_size) {
    if (p_size != NULL) {
        return PARSER_INVALID_POINTER;
    }
    *p_size = m_memory_size_;
    return PARSER_OK;
}

//size_id = 0
int scaling_list_default_0 [1][6][16] =  {{{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16},
                                           {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}}};
//size_id = 1, 2
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
//size_id = 3
int scaling_list_default_3 [1][2][64] = {{{16,16,16,16,16,16,16,16,16,16,17,16,17,16,17,18,17,18,18,17,18,21,19,20,21,20,19,21,24,22,22,24,24,22,22,24,25,25,27,30,27,25,25,29,31,35,35,31,29,36,41,44,41,36,47,54,54,47,65,70,65,88,88,115},
                                          {16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,18,20,20,20,20,20,20,20,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,28,28,28,28,28,28,33,33,33,33,33,41,41,41,41,54,54,54,71,71,91}}};