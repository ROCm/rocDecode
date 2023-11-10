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

#include "vaapi_videodecoder.h"

VaapiVideoDecoder::VaapiVideoDecoder(RocDecoderCreateInfo &decoder_create_info) : decoder_create_info_{decoder_create_info},
    drm_fd_{-1}, va_display_{0}, va_config_attrib_{{}}, va_config_id_{0}, va_profile_ {VAProfileNone}, va_context_id_{0}, va_surface_ids_{{}} {};

VaapiVideoDecoder::~VaapiVideoDecoder() {
    if (drm_fd_ != -1) {
        close(drm_fd_);
    }
    if (va_display_) {
        vaDestroySurfaces(va_display_, va_surface_ids_.data(), va_surface_ids_.size());
        if (va_context_id_)
            vaDestroyContext(va_display_, va_context_id_);
        if (va_config_id_)
            vaDestroyConfig(va_display_, va_config_id_);
        vaTerminate(va_display_);
    }
}

rocDecStatus VaapiVideoDecoder::InitializeDecoder(std::string gcn_arch_name) {
    rocDecStatus rocdec_status = ROCDEC_SUCCESS;

    //Before initializing the VAAPI, first check to see if the requested codec config is supported
    RocDecVcnCodecSpec& vcn_codec_spec = RocDecVcnCodecSpec::GetInstance();
    if (!vcn_codec_spec.IsCodecConfigSupported(gcn_arch_name, decoder_create_info_.CodecType, decoder_create_info_.ChromaFormat,
        decoder_create_info_.bitDepthMinus8, decoder_create_info_.OutputFormat)) {
        ERR("ERROR: the codec config combination is not supported!");
        return ROCDEC_NOT_SUPPORTED;
    }
    // There are 8 renderDXXX per physical device on gfx940 and gfx941
    int num_render_cards_per_device = ((gcn_arch_name.compare("gfx940") == 0) ||
                                       (gcn_arch_name.compare("gfx941") == 0)) ? 8 : 1;
    std::string drm_node = "/dev/dri/renderD" + std::to_string(128 + decoder_create_info_.deviceid * num_render_cards_per_device);
    rocdec_status = InitVAAPI(drm_node);
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to initilize the VAAPI!" + TOSTR(rocdec_status));
        return rocdec_status;
    }
    rocdec_status = CreateDecoderConfig();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to create a VAAPI decoder configuration" + TOSTR(rocdec_status));
        return rocdec_status;
    }
    rocdec_status = CreateSurfaces();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to create VAAPI surfaces " + TOSTR(rocdec_status));
        return rocdec_status;
    }
    rocdec_status = CreateContext();
    if (rocdec_status != ROCDEC_SUCCESS) {
        ERR("ERROR: Failed to create a VAAPI context " + TOSTR(rocdec_status));
        return rocdec_status;
    }
    return rocdec_status;
}

rocDecStatus VaapiVideoDecoder::InitVAAPI(std::string drm_node) {
    drm_fd_ = open(drm_node.c_str(), O_RDWR);
    if (drm_fd_ < 0) {
        ERR("ERROR: failed to open drm node " + drm_node);
        return ROCDEC_NOT_INITIALIZED;
    }
    va_display_ = vaGetDisplayDRM(drm_fd_);
    vaSetInfoCallback(va_display_, NULL, NULL);
    int major_version = 0, minor_version = 0;
    CHECK_VAAPI(vaInitialize(va_display_, &major_version, &minor_version));
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::CreateDecoderConfig() {
    switch (decoder_create_info_.CodecType) {
        case rocDecVideoCodec_HEVC:
            if (decoder_create_info_.bitDepthMinus8 == 0) {
                va_profile_ = VAProfileHEVCMain;
            } else if (decoder_create_info_.bitDepthMinus8 == 2) {
                va_profile_ = VAProfileHEVCMain10;
            }
            break;
        case rocDecVideoCodec_H264:
            va_profile_ = VAProfileH264Main;
            break;
        default:
            ERR("ERROR: the codec type is not supported!");
            return ROCDEC_NOT_SUPPORTED;
    }
    va_config_attrib_.type = VAConfigAttribRTFormat;
    CHECK_VAAPI(vaGetConfigAttributes(va_display_, va_profile_, VAEntrypointVLD, &va_config_attrib_, 1));
    CHECK_VAAPI(vaCreateConfig(va_display_, va_profile_, VAEntrypointVLD, &va_config_attrib_, 1, &va_config_id_));
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::CreateSurfaces() {
    if (decoder_create_info_.ulNumDecodeSurfaces < 1) {
        ERR("ERROR: invalid number of decode surfaces ");
        return ROCDEC_INVALID_PARAMETER;
    }
    va_surface_ids_.resize(decoder_create_info_.ulNumDecodeSurfaces);
    uint8_t surface_format;
    switch (decoder_create_info_.ChromaFormat) {
        case rocDecVideoChromaFormat_Monochrome:
            surface_format = VA_RT_FORMAT_YUV400;
            break;
        case rocDecVideoChromaFormat_420:
            surface_format = VA_RT_FORMAT_YUV420;
            break;
        case rocDecVideoChromaFormat_422:
            surface_format = VA_RT_FORMAT_YUV422;
            break;
        case rocDecVideoChromaFormat_444:
            surface_format = VA_RT_FORMAT_YUV444;
            break;
        default:
            ERR("ERROR: the surface type is not supported!");
            return ROCDEC_NOT_SUPPORTED;
    }

    CHECK_VAAPI(vaCreateSurfaces(va_display_, surface_format, decoder_create_info_.ulWidth,
        decoder_create_info_.ulHeight, va_surface_ids_.data(), va_surface_ids_.size(), nullptr, 0));

    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::CreateContext() {
    CHECK_VAAPI(vaCreateContext(va_display_, va_config_id_, decoder_create_info_.ulWidth, decoder_create_info_.ulHeight,
        VA_PROGRESSIVE, va_surface_ids_.data(), va_surface_ids_.size(), &va_context_id_));
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::SubmitDecode(RocdecPicParams *pPicParams) {
    // Todo copy pic param, slice param, IQ matrix and slice data from RocdecPicParams to VAAPI struct buffers, then submit to VAAPI driver.
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::GetDecodeStatus(int pic_idx, RocdecDecodeStatus *decode_status) {
    VASurfaceStatus va_surface_status;
    if (pic_idx >= va_surface_ids_.size() || decode_status == nullptr) {
        return ROCDEC_INVALID_PARAMETER;
    }
    CHECK_VAAPI(vaQuerySurfaceStatus(va_display_, va_surface_ids_[pic_idx], &va_surface_status));
    switch (va_surface_status) {
        case VASurfaceRendering:
            decode_status->decodeStatus = rocDecodeStatus_InProgress;
            break;
        case VASurfaceReady:
            decode_status->decodeStatus = rocDecodeStatus_Success;
            break;
        case VASurfaceDisplaying:
            decode_status->decodeStatus = rocDecodeStatus_Displaying;
            break;
        default:
           decode_status->decodeStatus = rocDecodeStatus_Invalid;
    }
    return ROCDEC_SUCCESS;
}

rocDecStatus VaapiVideoDecoder::ExportSurface(int pic_idx, VADRMPRIMESurfaceDescriptor &va_drm_prime_surface_desc) {
    if (pic_idx >= va_surface_ids_.size()) {
        return ROCDEC_INVALID_PARAMETER;
    }
    CHECK_VAAPI(vaSyncSurface(va_display_, va_surface_ids_[pic_idx]));
    CHECK_VAAPI(vaExportSurfaceHandle(va_display_, va_surface_ids_[pic_idx],
                VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                VA_EXPORT_SURFACE_READ_ONLY |
                VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                &va_drm_prime_surface_desc));

   return ROCDEC_SUCCESS;
}
