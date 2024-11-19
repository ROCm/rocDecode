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

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include "../commons.h"
#include "../../api/rocdecode.h"


// The CodecSpec struct contains information for an individual codec (e.g., rocDecVideoCodec_HEVC)
struct CodecSpec {
    std::vector<rocDecVideoChromaFormat> chroma_format;
    std::vector<int> bitdepth_minus8;
    uint16_t output_format_mask;
    uint32_t max_width;
    uint32_t max_height;
    uint16_t min_width;
    uint16_t min_height;
};

// The VcnCodecsSpec struct contains information for all supported codecs and number of vcn instances per device
struct VcnCodecsSpec {
    std::unordered_map<rocDecVideoCodec, CodecSpec> codecs_spec;
    uint8_t num_decoders;
};

// The RocDecVcnCodecSpec singleton class for providing access to the the vcn_spec_table
class RocDecVcnCodecSpec {
public:
    static RocDecVcnCodecSpec& GetInstance() {
        static RocDecVcnCodecSpec instance;
        return instance;
    }
    rocDecStatus GetDecoderCaps(std::string gcn_arch_name, RocdecDecodeCaps *pdc) {
        std::lock_guard<std::mutex> lock(mutex);
        std::size_t pos = gcn_arch_name.find_first_of(":");
        std::string gcn_arch_name_base = (pos != std::string::npos) ? gcn_arch_name.substr(0, pos) : gcn_arch_name;
        auto it = vcn_spec_table.find(gcn_arch_name_base);
        if (it != vcn_spec_table.end()) {
            const VcnCodecsSpec& vcn_spec = it->second;
            auto it1 = vcn_spec.codecs_spec.find(pdc->codec_type);
            if (it1 != vcn_spec.codecs_spec.end()) {
                const CodecSpec& codec_spec = it1->second;
                auto it_chroma_format = std::find(codec_spec.chroma_format.begin(), codec_spec.chroma_format.end(), pdc->chroma_format);
                auto it_bitdepth_minus8 = std::find(codec_spec.bitdepth_minus8.begin(), codec_spec.bitdepth_minus8.end(), pdc->bit_depth_minus_8);
                if (it_chroma_format != codec_spec.chroma_format.end() && it_bitdepth_minus8 != codec_spec.bitdepth_minus8.end()) {
                    pdc->is_supported = 1;
                    pdc->num_decoders = vcn_spec.num_decoders;
                    pdc->output_format_mask = codec_spec.output_format_mask;
                    pdc->max_width = codec_spec.max_width;
                    pdc->max_height = codec_spec.max_height;
                    pdc->min_width = codec_spec.min_width;
                    pdc->min_height = codec_spec.min_height;
                    return ROCDEC_SUCCESS;
                } else {
                    return ROCDEC_NOT_SUPPORTED;
                }
            } else {
                return ROCDEC_NOT_SUPPORTED;
            }
        } else {
            ERR("Didn't find the decoder capability for " + gcn_arch_name + " GPU!");
            return ROCDEC_NOT_IMPLEMENTED;
        }
    }
    bool IsCodecConfigSupported(std::string gcn_arch_name, rocDecVideoCodec codec_type, rocDecVideoChromaFormat chroma_format, uint32_t bit_depth_minus8, rocDecVideoSurfaceFormat output_format) {
        std::lock_guard<std::mutex> lock(mutex);
        std::size_t pos = gcn_arch_name.find_first_of(":");
        std::string gcn_arch_name_base = (pos != std::string::npos) ? gcn_arch_name.substr(0, pos) : gcn_arch_name;
        auto it = vcn_spec_table.find(gcn_arch_name_base);
        if (it != vcn_spec_table.end()) {
            const VcnCodecsSpec& vcn_spec = it->second;
            auto it1 = vcn_spec.codecs_spec.find(codec_type);
            if (it1 != vcn_spec.codecs_spec.end()) {
                const CodecSpec& codec_spec = it1->second;
                auto it_chroma_format = std::find(codec_spec.chroma_format.begin(), codec_spec.chroma_format.end(), chroma_format);
                auto it_bitdepth_minus8 = std::find(codec_spec.bitdepth_minus8.begin(), codec_spec.bitdepth_minus8.end(), bit_depth_minus8);
                if (it_chroma_format != codec_spec.chroma_format.end() && it_bitdepth_minus8 != codec_spec.bitdepth_minus8.end()) {
                    return (codec_spec.output_format_mask & (static_cast<int>(output_format) + 1));
                } else {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
private:
    std::unordered_map<std::string, VcnCodecsSpec> vcn_spec_table;
    std::mutex mutex;
    RocDecVcnCodecSpec() {
        //vcn lookup table format:
        //{"gcn_arch_name1",{{{codec1, {{chroma_format1_for_codec1, chroma_format2_for_codec1, ...}, {bit_depth1_minus8_for_codec1, bit_depth2_minus8_for_codec1, ...}, output_format_mask_for_codec1, max_width_for_codec1, max_height_for_codec1, min_width_for_codec1, min_height_for_codec1}},
        //                    {codec2, {{chroma_format1_for_codec2, chroma_format2_for_codec2, ...}, {bit_depth1_minus8_for_codec2, bit_depth2_minus8_for_codec2, ...}, output_format_mask_for_codec2, max_width_for_codec2, max_height_for_codec2, min_width_for_codec2, min_height_for_codec2}}}
        //                    , vcn_instances_for_gcn_arch_name1}},
        // av1 is available only on VCN3.0 and above
        vcn_spec_table = {
            {"gfx908",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2160, 64, 64}}}, 2}},
            {"gfx90a",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2160, 64, 64}}}, 2}},
            {"gfx942",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 3}},
            {"gfx1030",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            {"gfx1031",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            {"gfx1032",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            {"gfx1100",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            {"gfx1101",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 1}},
            {"gfx1102",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            {"gfx1200",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            {"gfx1201",{{{rocDecVideoCodec_HEVC, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 7680, 4320, 64, 64}}, {rocDecVideoCodec_AVC, {{rocDecVideoChromaFormat_420}, {0}, 1, 4096, 2176, 64, 64}}, {rocDecVideoCodec_AV1, {{rocDecVideoChromaFormat_420}, {0, 2}, 3, 8192, 4352, 64, 64}}}, 2}},
            };
    }
    RocDecVcnCodecSpec(const RocDecVcnCodecSpec&) = delete;
    RocDecVcnCodecSpec& operator = (const RocDecVcnCodecSpec) = delete;
    ~RocDecVcnCodecSpec() = default;
};