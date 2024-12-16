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
#include "vaapi_videodecoder.h"

// The RocDecVcnCodecSpec singleton class for providing access to the the vcn_spec_table
class RocDecVcnCodecSpec {
public:
    static RocDecVcnCodecSpec& GetInstance() {
        static RocDecVcnCodecSpec instance;
        return instance;
    }
    rocDecStatus GetDecoderCaps(RocdecDecodeCaps *pdc) {
        // Jefftest
        GpuVaContext& va_ctx = GpuVaContext::GetInstance();
        va_ctx.Initialize(pdc->device_id);
        if (va_ctx.CheckDecCapForCodecType(pdc) != ROCDEC_SUCCESS) {
            ERR("Failed to obtain decoder capabilities from driver.");
            return ROCDEC_DEVICE_INVALID;
        } else {
            return ROCDEC_SUCCESS;
        }
    }
    bool IsCodecConfigSupported(int device_id, rocDecVideoCodec codec_type, rocDecVideoChromaFormat chroma_format, uint32_t bit_depth_minus8, rocDecVideoSurfaceFormat output_format) {
        RocdecDecodeCaps decode_caps;
        decode_caps.device_id = device_id;
        decode_caps.codec_type = codec_type;
        decode_caps.chroma_format = chroma_format;
        decode_caps.bit_depth_minus_8 = bit_depth_minus8;
        if((rocDecGetDecoderCaps(&decode_caps) != ROCDEC_SUCCESS) || (decode_caps.is_supported == false) || ((decode_caps.output_format_mask & (1 << output_format)) == 0)) {
            return false;
        } else {
            return true;
        }
    }
private:
    bool initialized_;
    uint32_t num_dec_engines_ = 1;
    // Jefftest std::vector<CodecSpec> decode_cap_list_{0};
    std::mutex mutex;
    RocDecVcnCodecSpec() {
        initialized_ = false;
    }
    RocDecVcnCodecSpec(const RocDecVcnCodecSpec&) = delete;
    RocDecVcnCodecSpec& operator = (const RocDecVcnCodecSpec) = delete;
    ~RocDecVcnCodecSpec() = default;
};