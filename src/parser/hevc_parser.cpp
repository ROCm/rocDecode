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

HEVCVideoParser::HEVCVideoParser() {
    b_new_picture_ = false;
    m_vps_ = NULL;
    m_sps_ = NULL;
    m_pps_ = NULL;
    m_sh_ = NULL;
    m_sh_copy_ = NULL;
    m_slice_ = NULL;
}

rocDecStatus HEVCVideoParser::Initialize(RocdecParserParams *p_params) {
    ParserResult status = Init();
    if (status)
        return ROCDEC_RUNTIME_ERROR;
    RocVideoParser::Initialize(p_params);
    return ROCDEC_SUCCESS;
}

rocDecStatus HEVCVideoParser::ParseVideoData(RocdecSourceDataPacket *p_data) {
    bool status = ParseFrameData(p_data->payload, p_data->payload_size);
    if (!status) {
        ERR(STR("Parser failed!\n"));
        return ROCDEC_RUNTIME_ERROR;
    }
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
    if (m_slice_) {
        delete m_slice_;
    }
}

HEVCVideoParser::VpsData* HEVCVideoParser::AllocVps() {
    VpsData *p = nullptr;
    try {
        p = new VpsData [MAX_VPS_COUNT];
    }
    catch(const std::exception& e) {
        ERR(STR("Failed to alloc VPS Data, ") + STR(e.what()))
    }
    memset(p, 0, sizeof(VpsData) * MAX_VPS_COUNT);
    return p;
}

HEVCVideoParser::SpsData* HEVCVideoParser::AllocSps() {
    SpsData *p = nullptr;
    try {
        p = new SpsData [MAX_SPS_COUNT];
    }
    catch(const std::exception& e) {
        ERR(STR("Failed to alloc SPS Data, ") + STR(e.what()))
    }
    memset(p, 0, sizeof(SpsData) * MAX_SPS_COUNT);
    return p;
}

HEVCVideoParser::PpsData* HEVCVideoParser::AllocPps() {
    PpsData *p = nullptr;
    try {
        p = new PpsData [MAX_PPS_COUNT];
    }
    catch(const std::exception& e) {
        ERR(STR("Failed to alloc PPS Data, ") + STR(e.what()))
    }
    memset(p, 0, sizeof(PpsData) * MAX_PPS_COUNT);
    return p;
}

HEVCVideoParser::SliceData* HEVCVideoParser::AllocSlice() {
    SliceData *p = nullptr;
    try {
        p = new SliceData;
    }
    catch(const std::exception& e) {
        ERR(STR("Failed to alloc Slice Data, ") + STR(e.what()))
    }
    memset(p, 0, sizeof(SliceData));
    return p;
}

HEVCVideoParser::SliceHeaderData* HEVCVideoParser::AllocSliceHeader() {
    SliceHeaderData *p = nullptr;
    try {
        p = new SliceHeaderData;
    }
    catch(const std::exception& e) {
        ERR(STR("Failed to alloc Slice Header Data, ") + STR(e.what()))
    }
    memset(p, 0, sizeof(SliceHeaderData));
    return p;
}

ParserResult HEVCVideoParser::Init() {
    m_active_vps_id_ = -1; 
    m_active_sps_id_ = -1;
    m_active_pps_id_ = -1;
    b_new_picture_ = false;
    m_vps_ = AllocVps();
    m_sps_ = AllocSps();
    m_pps_ = AllocPps();
    m_slice_ = AllocSlice();
    m_sh_ = AllocSliceHeader();
    m_sh_copy_ = AllocSliceHeader();
    return PARSER_OK;
}

bool HEVCVideoParser::ParseFrameData(const uint8_t* p_stream, uint32_t frame_data_size) {
    int ret = PARSER_OK;
    NalUnitHeader nal_unit_header;

    frame_data_buffer_ptr_ = (uint8_t*)p_stream;
    frame_data_size_ = frame_data_size;
    curr_byte_offset_ = 0;
    start_code_num_ = 0;
    curr_start_code_offset_ = 0;
    next_start_code_offset_ = 0;

    slice_num_ = 0;

    do {
        ret = GetNalUnit();

        if (ret == PARSER_NOT_FOUND) {
            ERR(STR("Error: no start code found in the frame data.\n"));
            return false;
        }

        // Parse the NAL unit
        if (nal_unit_size_) {
            // start code + NAL unit header = 5 bytes
            int ebsp_size = nal_unit_size_ - 5 > RBSP_BUF_SIZE ? RBSP_BUF_SIZE : nal_unit_size_ - 5; // only copy enough bytes for header parsing

            nal_unit_header = ParseNalUnitHeader(&frame_data_buffer_ptr_[curr_start_code_offset_ + 3]);
            switch (nal_unit_header.nal_unit_type) {
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
                    ParseSliceHeader(nal_unit_header.nal_unit_type, m_rbsp_buf_, m_rbsp_size_);
                    slice_num_++;
                    break;
                }
                
                default:
                    // Do nothing for now.
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

void HEVCVideoParser::ParsePtl(H265ProfileTierLevel *ptl, bool profile_present_flag, uint32_t max_num_sub_layers_minus1, uint8_t *nalu, size_t /*size*/, size_t& offset) {
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
        //ReadBits is limited to 32
        offset += 44;
        // Todo: add constrant flags parsing.
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
            ptl->sub_layer_reserved_zero_44bits[i] = Parser::ReadBits(nalu, offset, 44);
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

void HEVCVideoParser::ParseScalingList(H265ScalingListData * s_data, uint8_t *nalu, size_t /*size*/, size_t& offset) {
    for (int size_id = 0; size_id < 4; size_id++) {
        for (int matrix_id = 0; matrix_id < ((size_id == 3) ? 2 : 6); matrix_id++) {
            s_data->scaling_list_pred_mode_flag[size_id][matrix_id] = Parser::GetBit(nalu, offset);
            if(!s_data->scaling_list_pred_mode_flag[size_id][matrix_id]) {
                s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id] = Parser::ExpGolomb::ReadUe(nalu, offset);

                int ref_matrix_id = matrix_id - s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id];
                int coef_num = std::min(64, (1 << (4 + (size_id << 1))));

                //fill in scaling_list_dc_coef_minus8
                if (!s_data->scaling_list_pred_matrix_id_delta[size_id][matrix_id]) {
                    if (size_id > 1) {
                        s_data->scaling_list_dc_coef_minus8[size_id - 2][matrix_id] = 8;
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
                    s_data->scaling_list_dc_coef_minus8[size_id - 2][matrix_id] = Parser::ExpGolomb::ReadSe(nalu, offset);
                    next_coef = s_data->scaling_list_dc_coef_minus8[size_id - 2][matrix_id] + 8;
                }
                for (int i = 0; i < coef_num; i++) {
                    s_data->scaling_list_delta_coef = Parser::ExpGolomb::ReadSe(nalu, offset);
                    next_coef = (next_coef + s_data->scaling_list_delta_coef +256) % 256;
                    s_data->scaling_list[size_id][matrix_id][i] = next_coef;
                }
            }
        }
    }
}

void HEVCVideoParser::ParseShortTermRefPicSet(H265ShortTermRPS *rps, int32_t st_rps_idx, uint32_t number_short_term_ref_pic_sets, H265ShortTermRPS rps_ref[], uint8_t *nalu, size_t /*size*/, size_t& offset) {
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
                rps->delta_poc[i] = delta_poc;
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
    m_vps_[vps_id].vps_reserved_three_2bits = Parser::ReadBits(nalu, offset, 2);
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
}

void HEVCVideoParser::ParseSps(uint8_t *nalu, size_t size) { 
    size_t offset = 0;
    uint32_t vpsId = Parser::ReadBits(nalu, offset, 4);
    uint32_t max_sub_layer_minus1 = Parser::ReadBits(nalu, offset, 3);
    uint32_t sps_temporal_id_nesting_flag = Parser::GetBit(nalu, offset);
    H265ProfileTierLevel ptl;
    memset (&ptl, 0, sizeof(ptl));
    ParsePtl(&ptl, true, max_sub_layer_minus1, nalu, size, offset);
    uint32_t sps_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    memset(&m_sps_[sps_id], 0, sizeof(m_sps_[sps_id]));
    m_sps_[sps_id].sps_video_parameter_set_id = vpsId;
    m_sps_[sps_id].sps_max_sub_layers_minus1 = max_sub_layer_minus1;
    m_sps_[sps_id].sps_temporal_id_nesting_flag = sps_temporal_id_nesting_flag;
    memcpy (&m_sps_[sps_id].profile_tier_level, &ptl, sizeof(ptl));
    m_sps_[sps_id].sps_seq_parameter_set_id = sps_id;
    m_sps_[sps_id].chroma_format_idc = Parser::ExpGolomb::ReadUe(nalu, offset);
    if (m_sps_[sps_id].chroma_format_idc == 3) {
        m_sps_[sps_id].separate_colour_plane_flag = Parser::GetBit(nalu, offset);
    }
    m_sps_[sps_id].pic_width_in_luma_samples = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].pic_height_in_luma_samples = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].conformance_window_flag = Parser::GetBit(nalu, offset);
    if (m_sps_[sps_id].conformance_window_flag)
    {
        m_sps_[sps_id].conf_win_left_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_sps_[sps_id].conf_win_right_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_sps_[sps_id].conf_win_top_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_sps_[sps_id].conf_win_bottom_offset = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    m_sps_[sps_id].bit_depth_luma_minus8 = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].bit_depth_chroma_minus8 = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].log2_max_pic_order_cnt_lsb_minus4 = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].sps_sub_layer_ordering_info_present_flag = Parser::GetBit(nalu, offset);
    for (int i = 0; i <= m_sps_[sps_id].sps_max_sub_layers_minus1; i++) {
        if (m_sps_[sps_id].sps_sub_layer_ordering_info_present_flag || (i == 0)) {
            m_sps_[sps_id].sps_max_dec_pic_buffering_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            m_sps_[sps_id].sps_max_num_reorder_pics[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            m_sps_[sps_id].sps_max_latency_increase_plus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
        }
        else {
            m_sps_[sps_id].sps_max_dec_pic_buffering_minus1[i] = m_sps_[sps_id].sps_max_dec_pic_buffering_minus1[0];
            m_sps_[sps_id].sps_max_num_reorder_pics[i] = m_sps_[sps_id].sps_max_num_reorder_pics[0];
            m_sps_[sps_id].sps_max_latency_increase_plus1[i] = m_sps_[sps_id].sps_max_latency_increase_plus1[0];
        }
    }
    m_sps_[sps_id].log2_min_luma_coding_block_size_minus3 = Parser::ExpGolomb::ReadUe(nalu, offset);

    int log2_min_cu_size = m_sps_[sps_id].log2_min_luma_coding_block_size_minus3 + 3;

    m_sps_[sps_id].log2_diff_max_min_luma_coding_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);

    int max_cu_depth_delta = m_sps_[sps_id].log2_diff_max_min_luma_coding_block_size;
    m_sps_[sps_id].max_cu_width = ( 1<<(log2_min_cu_size + max_cu_depth_delta));
    m_sps_[sps_id].max_cu_height = ( 1<<(log2_min_cu_size + max_cu_depth_delta));

    m_sps_[sps_id].log2_min_transform_block_size_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);

    uint32_t quadtree_tu_log2_min_size = m_sps_[sps_id].log2_min_transform_block_size_minus2 + 2;
    int add_cu_depth = max (0, log2_min_cu_size - (int)quadtree_tu_log2_min_size);
    m_sps_[sps_id].max_cu_depth = (max_cu_depth_delta + add_cu_depth);

    m_sps_[sps_id].log2_diff_max_min_transform_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].max_transform_hierarchy_depth_inter = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].max_transform_hierarchy_depth_intra = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_sps_[sps_id].scaling_list_enabled_flag = Parser::GetBit(nalu, offset);
    if (m_sps_[sps_id].scaling_list_enabled_flag) {
        m_sps_[sps_id].sps_scaling_list_data_present_flag = Parser::GetBit(nalu, offset);
        if (m_sps_[sps_id].sps_scaling_list_data_present_flag) {
            ParseScalingList(&m_sps_[sps_id].scaling_list_data, nalu, size, offset);
        }
    }
    m_sps_[sps_id].amp_enabled_flag = Parser::GetBit(nalu, offset);
    m_sps_[sps_id].sample_adaptive_offset_enabled_flag = Parser::GetBit(nalu, offset);
    m_sps_[sps_id].pcm_enabled_flag = Parser::GetBit(nalu, offset);
    if (m_sps_[sps_id].pcm_enabled_flag) {
        m_sps_[sps_id].pcm_sample_bit_depth_luma_minus1 = Parser::ReadBits(nalu, offset, 4);
        m_sps_[sps_id].pcm_sample_bit_depth_chroma_minus1 = Parser::ReadBits(nalu, offset, 4);
        m_sps_[sps_id].log2_min_pcm_luma_coding_block_size_minus3 = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_sps_[sps_id].log2_diff_max_min_pcm_luma_coding_block_size = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_sps_[sps_id].pcm_loop_filter_disabled_flag = Parser::GetBit(nalu, offset);
    }
    m_sps_[sps_id].num_short_term_ref_pic_sets = Parser::ExpGolomb::ReadUe(nalu, offset);
    for (int i=0; i<m_sps_[sps_id].num_short_term_ref_pic_sets; i++) {
        //short_term_ref_pic_set( i )
        ParseShortTermRefPicSet(&m_sps_[sps_id].st_rps[i], i, m_sps_[sps_id].num_short_term_ref_pic_sets, m_sps_[sps_id].st_rps, nalu, size, offset);
    }
    m_sps_[sps_id].long_term_ref_pics_present_flag = Parser::GetBit(nalu, offset);
    if (m_sps_[sps_id].long_term_ref_pics_present_flag) {
        m_sps_[sps_id].num_long_term_ref_pics_sps = Parser::ExpGolomb::ReadUe(nalu, offset);  //max is 32
        m_sps_[sps_id].lt_rps.num_of_pics = m_sps_[sps_id].num_long_term_ref_pics_sps;
        for (int i=0; i<m_sps_[sps_id].num_long_term_ref_pics_sps; i++) {
            //The number of bits used to represent lt_ref_pic_poc_lsb_sps[ i ] is equal to log2_max_pic_order_cnt_lsb_minus4 + 4.
            m_sps_[sps_id].lt_ref_pic_poc_lsb_sps[i] = Parser::ReadBits(nalu, offset, (m_sps_[sps_id].log2_max_pic_order_cnt_lsb_minus4 + 4));
            m_sps_[sps_id].used_by_curr_pic_lt_sps_flag[i] = Parser::GetBit(nalu, offset);
            m_sps_[sps_id].lt_rps.pocs[i]=m_sps_[sps_id].lt_ref_pic_poc_lsb_sps[i];
            m_sps_[sps_id].lt_rps.used_by_curr_pic[i] = m_sps_[sps_id].used_by_curr_pic_lt_sps_flag[i];            
        }
    }
    m_sps_[sps_id].sps_temporal_mvp_enabled_flag = Parser::GetBit(nalu, offset);
    m_sps_[sps_id].strong_intra_smoothing_enabled_flag = Parser::GetBit(nalu, offset);
    m_sps_[sps_id].vui_parameters_present_flag = Parser::GetBit(nalu, offset);
    if (m_sps_[sps_id].vui_parameters_present_flag) {
        //vui_parameters()
        ParseVui(&m_sps_[sps_id].vui_parameters, m_sps_[sps_id].sps_max_sub_layers_minus1, nalu, size, offset);
    }
    m_sps_[sps_id].sps_extension_flag = Parser::GetBit(nalu, offset);
}

void HEVCVideoParser::ParsePps(uint8_t *nalu, size_t size) {
    size_t offset = 0;
    uint32_t pps_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    memset(&m_pps_[pps_id], 0, sizeof(m_pps_[pps_id]));

    m_pps_[pps_id].pps_pic_parameter_set_id = pps_id;
    m_pps_[pps_id].pps_seq_parameter_set_id = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_pps_[pps_id].dependent_slice_segments_enabled_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].output_flag_present_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].num_extra_slice_header_bits = Parser::ReadBits(nalu, offset, 3);
    m_pps_[pps_id].sign_data_hiding_enabled_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].cabac_init_present_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].num_ref_idx_l0_default_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_pps_[pps_id].num_ref_idx_l1_default_active_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_pps_[pps_id].init_qp_minus26 = Parser::ExpGolomb::ReadSe(nalu, offset);
    m_pps_[pps_id].constrained_intra_pred_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].transform_skip_enabled_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].cu_qp_delta_enabled_flag = Parser::GetBit(nalu, offset);
    if (m_pps_[pps_id].cu_qp_delta_enabled_flag) {
        m_pps_[pps_id].diff_cu_qp_delta_depth = Parser::ExpGolomb::ReadUe(nalu, offset);
    }
    m_pps_[pps_id].pps_cb_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
    m_pps_[pps_id].pps_cr_qp_offset = Parser::ExpGolomb::ReadSe(nalu, offset);
    m_pps_[pps_id].pps_slice_chroma_qp_offsets_present_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].weighted_pred_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].weighted_bipred_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].transquant_bypass_enabled_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].tiles_enabled_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].entropy_coding_sync_enabled_flag = Parser::GetBit(nalu, offset);
    if (m_pps_[pps_id].tiles_enabled_flag) {
        m_pps_[pps_id].num_tile_columns_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_pps_[pps_id].num_tile_rows_minus1 = Parser::ExpGolomb::ReadUe(nalu, offset);
        m_pps_[pps_id].uniform_spacing_flag = Parser::GetBit(nalu, offset);
        if (!m_pps_[pps_id].uniform_spacing_flag) {
            for (int i = 0; i < m_pps_[pps_id].num_tile_columns_minus1; i++) {
                m_pps_[pps_id].column_width_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            }
            for (int i = 0; i < m_pps_[pps_id].num_tile_rows_minus1; i++) {
                m_pps_[pps_id].row_height_minus1[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
            }
        }
        m_pps_[pps_id].loop_filter_across_tiles_enabled_flag = Parser::GetBit(nalu, offset);
    }
    else {
        m_pps_[pps_id].loop_filter_across_tiles_enabled_flag = 1;
        m_pps_[pps_id].uniform_spacing_flag = 1;
    }
    m_pps_[pps_id].pps_loop_filter_across_slices_enabled_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].deblocking_filter_control_present_flag = Parser::GetBit(nalu, offset);
    if (m_pps_[pps_id].deblocking_filter_control_present_flag) {
        m_pps_[pps_id].deblocking_filter_override_enabled_flag = Parser::GetBit(nalu, offset);
        m_pps_[pps_id].pps_deblocking_filter_disabled_flag = Parser::GetBit(nalu, offset);
        if (!m_pps_[pps_id].pps_deblocking_filter_disabled_flag) {
            m_pps_[pps_id].pps_beta_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
            m_pps_[pps_id].pps_tc_offset_div2 = Parser::ExpGolomb::ReadSe(nalu, offset);
        }
    }
    m_pps_[pps_id].pps_scaling_list_data_present_flag = Parser::GetBit(nalu, offset);
    if (m_pps_[pps_id].pps_scaling_list_data_present_flag) {
        ParseScalingList(&m_pps_[pps_id].scaling_list_data, nalu, size, offset);
    }
    m_pps_[pps_id].lists_modification_present_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].log2_parallel_merge_level_minus2 = Parser::ExpGolomb::ReadUe(nalu, offset);
    m_pps_[pps_id].slice_segment_header_extension_present_flag = Parser::GetBit(nalu, offset);
    m_pps_[pps_id].pps_extension_flag = Parser::GetBit(nalu, offset);
}

bool HEVCVideoParser::ParseSliceHeader(uint32_t nal_unit_type, uint8_t *nalu, size_t size) {
    PpsData *pPps = NULL;
    SpsData *pSps = NULL;
    size_t offset = 0;
    SliceHeaderData temp_sh;
    memset(&temp_sh, 0, sizeof(temp_sh));

    temp_sh.first_slice_segment_in_pic_flag = m_sh_->first_slice_segment_in_pic_flag = Parser::GetBit(nalu, offset);
    if (nal_unit_type >= NAL_UNIT_CODED_SLICE_BLA_W_LP && nal_unit_type <= NAL_UNIT_RESERVED_IRAP_VCL23) {
        temp_sh.no_output_of_prior_pics_flag = m_sh_->no_output_of_prior_pics_flag = Parser::GetBit(nalu, offset);
    }

    // Set active VPS, SPS and PPS for the current slice
    m_active_pps_id_ = Parser::ExpGolomb::ReadUe(nalu, offset);
    temp_sh.slice_pic_parameter_set_id = m_sh_->slice_pic_parameter_set_id = m_active_pps_id_;
    pPps = &m_pps_[m_active_pps_id_];
    m_active_sps_id_ = pPps->pps_seq_parameter_set_id;
    pSps = &m_sps_[m_active_sps_id_];
    m_active_vps_id_ = pSps->sps_video_parameter_set_id;

    // Check video dimension change
    if ( pic_width_ != pSps->pic_width_in_luma_samples || pic_height_ != pSps->pic_height_in_luma_samples)
    {
        pic_width_ = pSps->pic_width_in_luma_samples;
        pic_height_ = pSps->pic_height_in_luma_samples;
        pic_dimension_changed_ = true;  // Note: clear this flag after the actions with size change are taken.
    }

    if (!m_sh_->first_slice_segment_in_pic_flag) {
        if (pPps->dependent_slice_segments_enabled_flag) {
            temp_sh.dependent_slice_segment_flag = m_sh_->dependent_slice_segment_flag = Parser::GetBit(nalu, offset);
        }

        int MinCbLog2SizeY = pSps->log2_min_luma_coding_block_size_minus3 + 3;
        int CtbLog2SizeY = MinCbLog2SizeY + pSps->log2_diff_max_min_luma_coding_block_size;
        int CtbSizeY = 1 << CtbLog2SizeY;
        int PicWidthInCtbsY = (pSps->pic_width_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
        int PicHeightInCtbsY = (pSps->pic_height_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
        int PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
        int bits_slice_segment_address = (int)ceilf(log2f((float)PicSizeInCtbsY));
        
        temp_sh.slice_segment_address = m_sh_->slice_segment_address = Parser::ReadBits(nalu, offset, bits_slice_segment_address);   
    }

    if (!m_sh_->dependent_slice_segment_flag) {
        for (int i = 0; i < pPps->num_extra_slice_header_bits; i++) {
            m_sh_->slice_reserved_flag[i] = Parser::GetBit(nalu, offset);
        }
        m_sh_->slice_type = Parser::ExpGolomb::ReadUe(nalu, offset);
        if (pPps->output_flag_present_flag) {
            m_sh_->pic_output_flag = Parser::GetBit(nalu, offset);
        }
        if (pSps->separate_colour_plane_flag) {
            m_sh_->colour_plane_id = Parser::ReadBits(nalu, offset, 2);
        }
        if (nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP) {
            m_slice_->curr_poc = 0;
            m_slice_->prev_poc = m_slice_->curr_poc;
            m_slice_->curr_poc_lsb = 0;
            m_slice_->curr_poc_msb = 0;
            m_slice_->prev_poc_lsb = m_slice_->curr_poc_lsb;
            m_slice_->prev_poc_msb = m_slice_->curr_poc_msb;
        }
        else {
            //length of slice_pic_order_cnt_lsb is log2_max_pic_order_cnt_lsb_minus4 + 4 bits.
            m_sh_->slice_pic_order_cnt_lsb = Parser::ReadBits(nalu, offset, (pSps->log2_max_pic_order_cnt_lsb_minus4 + 4));

            //get POC
            m_slice_->curr_poc_lsb = m_sh_->slice_pic_order_cnt_lsb;
            m_slice_->max_poc_lsb = 1 << (pSps->log2_max_pic_order_cnt_lsb_minus4 + 4);

            if (nal_unit_type >= NAL_UNIT_CODED_SLICE_BLA_W_LP && nal_unit_type < NAL_UNIT_CODED_SLICE_CRA_NUT) {
                m_slice_->curr_poc_msb = 0;
            }
            else {
                if ((m_slice_->curr_poc_lsb < m_slice_->prev_poc_lsb) && ((m_slice_->prev_poc_lsb - m_slice_->curr_poc_lsb) >= (m_slice_->max_poc_lsb / 2)))
                    m_slice_->curr_poc_msb = m_slice_->prev_poc_msb + m_slice_->max_poc_lsb;
                else if ((m_slice_->curr_poc_lsb > m_slice_->prev_poc_lsb) && ((m_slice_->curr_poc_lsb - m_slice_->prev_poc_lsb) > (m_slice_->max_poc_lsb / 2)))
                    m_slice_->curr_poc_msb = m_slice_->prev_poc_msb - m_slice_->max_poc_lsb;
                else
                    m_slice_->curr_poc_msb = m_slice_->prev_poc_msb;
            }

            m_slice_->curr_poc = m_slice_->curr_poc_lsb + m_slice_->curr_poc_msb;
            m_slice_->prev_poc = m_slice_->curr_poc;
            m_slice_->prev_poc_lsb = m_slice_->curr_poc_lsb;
            m_slice_->prev_poc_msb = m_slice_->curr_poc_msb;

            m_sh_->short_term_ref_pic_set_sps_flag = Parser::GetBit(nalu, offset);
            int32_t pos = offset;
            if (!m_sh_->short_term_ref_pic_set_sps_flag) {
                ParseShortTermRefPicSet(&m_sh_->st_rps, pSps->num_short_term_ref_pic_sets, pSps->num_short_term_ref_pic_sets, pSps->st_rps, nalu, size, offset);
            }
            else if (pSps->num_short_term_ref_pic_sets > 1) {
                int num_bits = 0;
                while ((1 << num_bits) < pSps->num_short_term_ref_pic_sets) {
                    num_bits++;
                }
                if (num_bits > 0) {
                    m_sh_->short_term_ref_pic_set_idx = Parser::ReadBits(nalu, offset, num_bits);
                }
            }
            m_sh_->short_term_ref_pic_set_size = offset - pos;

            if (pSps->long_term_ref_pics_present_flag) {
                if (pSps->num_long_term_ref_pics_sps > 0) {
                    m_sh_->num_long_term_sps = Parser::ExpGolomb::ReadUe(nalu, offset);
                }
                m_sh_->num_long_term_pics = Parser::ExpGolomb::ReadUe(nalu, offset);

                int bits_for_ltrp_in_sps = 0;
                while (pSps->num_long_term_ref_pics_sps > (1 << bits_for_ltrp_in_sps)) {
                    bits_for_ltrp_in_sps++;
                }
                m_sh_->lt_rps.num_of_pics = m_sh_->num_long_term_sps + m_sh_->num_long_term_pics;
                for (int i = 0; i < (m_sh_->num_long_term_sps + m_sh_->num_long_term_pics); i++) {
                    if (i < m_sh_->num_long_term_sps) {
                        if (pSps->num_long_term_ref_pics_sps > 1) {
                            if( bits_for_ltrp_in_sps > 0) {
                                m_sh_->lt_idx_sps[i] = Parser::ReadBits(nalu, offset, bits_for_ltrp_in_sps);
                                m_sh_->lt_rps.pocs[i] = pSps->lt_rps.pocs[m_sh_->lt_idx_sps[i]];
                                m_sh_->lt_rps.used_by_curr_pic[i] = pSps->lt_rps.used_by_curr_pic[m_sh_->lt_idx_sps[i]];
                            }
                        }
                    }
                    else {
                        m_sh_->poc_lsb_lt[i] = Parser::ReadBits(nalu, offset, (pSps->log2_max_pic_order_cnt_lsb_minus4 + 4));
                        m_sh_->used_by_curr_pic_lt_flag[i] = Parser::GetBit(nalu, offset);
                        m_sh_->lt_rps.pocs[i] = m_sh_->poc_lsb_lt[i];
                        m_sh_->lt_rps.used_by_curr_pic[i] = m_sh_->used_by_curr_pic_lt_flag[i];
                    }
                    m_sh_->delta_poc_msb_present_flag[i] = Parser::GetBit(nalu, offset);
                    if (m_sh_->delta_poc_msb_present_flag[i]) {
                        m_sh_->delta_poc_msb_cycle_lt[i] = Parser::ExpGolomb::ReadUe(nalu, offset);
                    }
                }
            }
            if (pSps->sps_temporal_mvp_enabled_flag) {
                m_sh_->slice_temporal_mvp_enabled_flag = Parser::GetBit(nalu, offset);
            }
        }
        memcpy(m_sh_copy_, m_sh_, sizeof(*m_sh_));
    }
    else {
        //dependant slice
        memcpy(m_sh_, m_sh_copy_, sizeof(*m_sh_copy_));
        m_sh_->first_slice_segment_in_pic_flag = temp_sh.first_slice_segment_in_pic_flag;
        m_sh_->no_output_of_prior_pics_flag = temp_sh.no_output_of_prior_pics_flag;
        m_sh_->slice_pic_parameter_set_id = temp_sh.slice_pic_parameter_set_id;
        m_sh_->dependent_slice_segment_flag = temp_sh.dependent_slice_segment_flag;
        m_sh_->slice_segment_address = temp_sh.slice_segment_address;
    }
    return false;
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