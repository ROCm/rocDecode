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

#define CHECK_VAAPI(call) {\
    VAStatus va_status = call;\
    if (va_status != VA_STATUS_SUCCESS) {\
        std::cout << "VAAPI failure: " << #call << " failed with status: " << std::hex << "0x" << va_status << std::dec << " = '" << vaErrorStr(va_status) << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;\
        return ROCDEC_RUNTIME_ERROR;\
    }\
}

// The CodecSpec struct contains information for an individual codec (e.g., rocDecVideoCodec_HEVC)
struct CodecSpec {
    rocDecVideoCodec codec_type;
    std::vector<rocDecVideoChromaFormat> chroma_format;
    int max_bit_depth;
    uint16_t output_format_mask;
    uint32_t max_width;
    uint32_t max_height;
    uint16_t min_width;
    uint16_t min_height;
};

// The RocDecVcnCodecSpec singleton class for providing access to the the vcn_spec_table
class RocDecVcnCodecSpec {
public:
    static RocDecVcnCodecSpec& GetInstance() {
        static RocDecVcnCodecSpec instance;
        return instance;
    }
    rocDecStatus ProbeHwDecodeCapabilities() {
        std::string drm_node = "/dev/dri/renderD128"; // look at device_id 0
        int drm_fd = open(drm_node.c_str(), O_RDWR);
        if (drm_fd < 0) {
            ERR("Failed to open drm node." + drm_node);
            return ROCDEC_DEVICE_INVALID;
        }
        VADisplay va_display = vaGetDisplayDRM(drm_fd);
        if (!va_display) {
            ERR("Failed to create va_display.");
            return ROCDEC_DEVICE_INVALID;
        }
        int major_version = 0, minor_version = 0;
        CHECK_VAAPI(vaInitialize(va_display, &major_version, &minor_version));

        int num_profiles = 0;
        std::vector<VAProfile> profile_list;
        num_profiles = vaMaxNumProfiles(va_display);
        profile_list.resize(num_profiles);
        CHECK_VAAPI(vaQueryConfigProfiles(va_display, profile_list.data(), &num_profiles));

        // To simplify, merge all profile attributes into one codec type.
        rocDecVideoCodec codec_type;
        rocDecVideoChromaFormat chroma_format;
        int bit_depth;
        for (int i = 0; i < num_profiles; i++) {
            bool interested = false;
            bit_depth = 8;
            switch (profile_list[i]) {
                case VAProfileH264Main:
                case VAProfileH264High:
                case VAProfileH264ConstrainedBaseline:
                    codec_type = rocDecVideoCodec_AVC;
                    chroma_format = rocDecVideoChromaFormat_420;
                    interested = true;
                    break;

                case VAProfileHEVCMain10:
                    bit_depth = 10;
                case VAProfileHEVCMain:
                    codec_type = rocDecVideoCodec_HEVC;
                    chroma_format = rocDecVideoChromaFormat_420;
                    interested = true;
                    break;

                case VAProfileAV1Profile0:
                    codec_type = rocDecVideoCodec_AV1;
                    chroma_format = rocDecVideoChromaFormat_420;
                    bit_depth = 10; // both 8 and 10 bit
                    interested = true;
                    break;

                default:
                    break;
            }

            if (interested) {
                int j = 0;
                for (j = 0; j < decode_cap_list_.size(); j++) {
                    if (decode_cap_list_[j].codec_type == codec_type) {
                        break;
                    }
                }
                if (decode_cap_list_.size() == 0 || (decode_cap_list_.size() && j == decode_cap_list_.size())) {
                    decode_cap_list_.resize(decode_cap_list_.size() + 1, {});
                }
                decode_cap_list_[j].codec_type = codec_type;
                if (decode_cap_list_[j].max_bit_depth < bit_depth) {
                    decode_cap_list_[j].max_bit_depth = bit_depth;
                }
                auto it_chroma_format = std::find(decode_cap_list_[j].chroma_format.begin(), decode_cap_list_[j].chroma_format.end(), chroma_format);
                if (it_chroma_format == decode_cap_list_[j].chroma_format.end()) {
                    decode_cap_list_[j].chroma_format.resize(decode_cap_list_[j].chroma_format.size() + 1);
                    decode_cap_list_[j].chroma_format[decode_cap_list_[j].chroma_format.size() - 1] = chroma_format;
                }

                VAConfigAttrib va_config_attrib;
                VAConfigID va_config_id;
                unsigned int attr_count;
                std::vector<VASurfaceAttrib> attr_list;
                va_config_attrib.type = VAConfigAttribRTFormat;
                CHECK_VAAPI(vaGetConfigAttributes(va_display, profile_list[i], VAEntrypointVLD, &va_config_attrib, 1));
                CHECK_VAAPI(vaCreateConfig(va_display, profile_list[i], VAEntrypointVLD, &va_config_attrib, 1, &va_config_id));
                CHECK_VAAPI(vaQuerySurfaceAttributes(va_display, va_config_id, 0, &attr_count));
                attr_list.resize(attr_count);
                CHECK_VAAPI(vaQuerySurfaceAttributes(va_display, va_config_id, attr_list.data(), &attr_count));
                for (int k = 0; k < attr_count; k++) {
                    switch (attr_list[k].type) {
                    case VASurfaceAttribPixelFormat:
                    {
                        switch (attr_list[k].value.value.i) {
                            case VA_FOURCC_NV12:
                                decode_cap_list_[j].output_format_mask |= 1 << rocDecVideoSurfaceFormat_NV12;
                                break;
                            case VA_FOURCC_P016:
                                decode_cap_list_[j].output_format_mask |= 1 << rocDecVideoSurfaceFormat_P016;
                                break;
                            default:
                                break;
                        }
                    }
                        break;
                    case VASurfaceAttribMinWidth:
                        if (decode_cap_list_[j].min_width == 0 || (decode_cap_list_[j].min_width > 0 && decode_cap_list_[j].min_width > attr_list[k].value.value.i)) {
                            decode_cap_list_[j].min_width = attr_list[k].value.value.i;
                        }
                        break;
                    case VASurfaceAttribMinHeight:
                        if (decode_cap_list_[j].min_height == 0 || (decode_cap_list_[j].min_height > 0 && decode_cap_list_[j].min_height > attr_list[k].value.value.i)) {
                            decode_cap_list_[j].min_height = attr_list[k].value.value.i;
                        }
                        break;
                    case VASurfaceAttribMaxWidth:
                        if (decode_cap_list_[j].max_width < attr_list[k].value.value.i) {
                            decode_cap_list_[j].max_width = attr_list[k].value.value.i;
                        }
                        break;
                    case VASurfaceAttribMaxHeight:
                        if (decode_cap_list_[j].max_height < attr_list[k].value.value.i) {
                            decode_cap_list_[j].max_height = attr_list[k].value.value.i;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        initialized_ = true;
        return ROCDEC_SUCCESS;
    }
    rocDecStatus GetDecoderCaps(RocdecDecodeCaps *pdc) {
        if (!initialized_) {
            if (ProbeHwDecodeCapabilities() != ROCDEC_SUCCESS) {
                ERR("Failed to obtain decoder capabilities from driver.");
                return ROCDEC_DEVICE_INVALID;
            }
        }
        std::lock_guard<std::mutex> lock(mutex);
        int i;
        for (i = 0; i < decode_cap_list_.size(); i++) {
            if (decode_cap_list_[i].codec_type == pdc->codec_type) {
                break;
            }
        }
        if (i < decode_cap_list_.size()) {
            auto it_chroma_format = std::find(decode_cap_list_[i].chroma_format.begin(), decode_cap_list_[i].chroma_format.end(), pdc->chroma_format);
            if (it_chroma_format != decode_cap_list_[i].chroma_format.end() && (pdc->bit_depth_minus_8 + 8) <= decode_cap_list_[i].max_bit_depth) {
                pdc->is_supported = 1;
                pdc->output_format_mask = decode_cap_list_[i].output_format_mask;
                pdc->max_width = decode_cap_list_[i].max_width;
                pdc->max_height = decode_cap_list_[i].max_height;
                pdc->min_width = decode_cap_list_[i].min_width;
                pdc->min_height = decode_cap_list_[i].min_height;
                return ROCDEC_SUCCESS;
            } else {
                return ROCDEC_NOT_SUPPORTED;
            }
        } else {
            return ROCDEC_NOT_SUPPORTED;
        }
    }
    bool IsCodecConfigSupported(rocDecVideoCodec codec_type, rocDecVideoChromaFormat chroma_format, uint32_t bit_depth_minus8, rocDecVideoSurfaceFormat output_format) {
        if (!initialized_) {
            if (ProbeHwDecodeCapabilities() != ROCDEC_SUCCESS) {
                ERR("Failed to obtain decoder capabilities from driver.");
                return ROCDEC_DEVICE_INVALID;
            }
        }
        std::lock_guard<std::mutex> lock(mutex);
        int i;
        for (i = 0; i < decode_cap_list_.size(); i++) {
            if (decode_cap_list_[i].codec_type == codec_type) {
                break;
            }
        }
        if (i < decode_cap_list_.size()) {
            auto it_chroma_format = std::find(decode_cap_list_[i].chroma_format.begin(), decode_cap_list_[i].chroma_format.end(), chroma_format);
            if (it_chroma_format != decode_cap_list_[i].chroma_format.end() && (bit_depth_minus8 + 8) <= decode_cap_list_[i].max_bit_depth) {
                return decode_cap_list_[i].output_format_mask & 1 << (static_cast<int>(output_format));
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
private:
    bool initialized_;
    std::vector<CodecSpec> decode_cap_list_{0};
    std::mutex mutex;
    RocDecVcnCodecSpec() {
        initialized_ = false;
    }
    RocDecVcnCodecSpec(const RocDecVcnCodecSpec&) = delete;
    RocDecVcnCodecSpec& operator = (const RocDecVcnCodecSpec) = delete;
    ~RocDecVcnCodecSpec() = default;
};