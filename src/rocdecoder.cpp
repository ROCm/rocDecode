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

#include "rocdecoder.hpp"

ROCDecode::ROCDecode(int device_id) : num_devices_{0}, device_id_{device_id}, hip_stream_ {0},
    external_mem_handle_desc_{{}}, external_mem_buffer_desc_{{}}, va_display_{nullptr},
    va_profiles_{{}}, num_va_profiles_{-1}, yuv_dev_mem_{nullptr}, width_{0}, height_{0},
    chroma_height_{0}, surface_height_{0}, surface_width_{0}, num_chroma_planes_{0},
    surface_stride_{0}, surface_size_{0}, bit_depth_{8}, byte_per_pixel_{1},
    subsampling_{ROCDEC_FMT_YUV420}, out_image_info_{{}}, fp_out_{nullptr}, drm_fd_{-1} {

    if (!InitHIP(device_id_)) {
        std::cerr << "Failed to initilize the HIP" << std::endl;
        throw std::runtime_error("Failed to initilize the HIP");
    }
    InitDRMnodes();
    InitVAAPI();
}

ROCDecode::~ROCDecode() {
    if (hip_stream_) {
        hipError_t hip_status = hipSuccess;
        hip_status = hipStreamDestroy(hip_stream_);
        if (hip_status != hipSuccess) {
            std::cout << "ERROR: hipStream_Destroy failed! (" << hip_status << ")" << std::endl;
        }
    }
    if (drm_fd_ != -1) {
        close(drm_fd_);
    }
    if (va_display_) {
        vaTerminate(va_display_);
    }
}

bool ROCDecode::DecodeFrame(uint8_t *data, size_t size, int64_t pts) {
    //TODO implement the DecodeFrame function
    return false;
}

uint8_t *ROCDecode::GetFrame(int64_t *pts) {
    //TODO implement the GetFrame function
    return nullptr;
}

bool ROCDecode::ReleaseFrame(int64_t pts) {
    //TODO implement the ReleaseFrame function
    return false;
}

void ROCDecode::SaveImage(std::string output_file_name, void *dev_mem, OutputImageInfo *image_info, bool is_output_RGB) {
    uint8_t *hst_ptr = nullptr;
    uint64_t output_image_size = image_info->output_image_size_in_bytes;
    if (hst_ptr == nullptr) {
        hst_ptr = new uint8_t [output_image_size];
    }
    hipError_t hip_status = hipSuccess;
    hip_status = hipMemcpyDtoH((void *)hst_ptr, dev_mem, output_image_size);
    if (hip_status != hipSuccess) {
        std::cout << "ERROR: hipMemcpyDtoH failed! (" << hip_status << ")" << std::endl;
        delete [] hst_ptr;
        return;
    }

    // no RGB dump if the surface type is YUV400
    if (image_info->chroma_format == ROCDEC_FMT_YUV400 && is_output_RGB) {
        return;
    }
    uint8_t *tmp_hst_ptr = hst_ptr;
    if (fp_out_ == nullptr) {
        fp_out_ = fopen(output_file_name.c_str(), "wb");
    }
    if (fp_out_) {
        int img_width = image_info->output_width;
        int img_height = image_info->output_height;
        int output_image_stride =  image_info->output_h_stride;
        if (img_width * image_info->bytes_per_pixel == output_image_stride && img_height == image_info->output_v_stride) {
            fwrite(hst_ptr, 1, output_image_size, fp_out_);
        } else {
            uint32_t width = is_output_RGB ? image_info->output_width * 3 : image_info->output_width;
            if (image_info->bit_depth == 8) {
                for (int i = 0; i < image_info->output_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width, fp_out_);
                    tmp_hst_ptr += output_image_stride;
                }
                if (!is_output_RGB) {
                    // dump chroma
                    uint8_t *uv_hst_ptr = hst_ptr + output_image_stride * image_info->output_v_stride;
                    for (int i = 0; i < img_height >> 1; i++) {
                        fwrite(uv_hst_ptr, 1, width, fp_out_);
                        uv_hst_ptr += output_image_stride;
                    }
                }
            } else if (image_info->bit_depth > 8 &&  image_info->bit_depth <= 16 ) {
                for (int i = 0; i < img_height; i++) {
                    fwrite(tmp_hst_ptr, 1, width * image_info->bytes_per_pixel, fp_out_);
                    tmp_hst_ptr += output_image_stride;
                }
                if (!is_output_RGB) {
                    // dump chroma
                    uint8_t *uv_hst_ptr = hst_ptr + output_image_stride * image_info->output_v_stride;
                    for (int i = 0; i < img_height >> 1; i++) {
                        fwrite(uv_hst_ptr, 1, width * image_info->bytes_per_pixel, fp_out_);
                        uv_hst_ptr += output_image_stride;
                    }
                }
            }
        }
    }

    if (hst_ptr != nullptr) {
        delete [] hst_ptr;
        hst_ptr = nullptr;
        tmp_hst_ptr = nullptr;
    }
}

void ROCDecode::GetDeviceinfo(std::string &device_name, std::string &gcn_arch_name, int &pci_bus_id, int &pci_domain_id, int &pci_device_id, std::string &drm_node) {
    device_name = hip_dev_prop_.name;
    gcn_arch_name = hip_dev_prop_.gcnArchName;
    pci_bus_id = hip_dev_prop_.pciBusID;
    pci_domain_id = hip_dev_prop_.pciDomainID;
    pci_device_id = hip_dev_prop_.pciDeviceID;
    drm_node = drm_nodes_[device_id_];
}

void ROCDecode::GetDecoderCaps(ROCDECDECODECAPS &decoder_caps) {
    //TODO implement the GetDecoderCaps function

}

std::string ROCDecode::GetPixFmtName(RocDecImageFormat subsampling) {
    std::string fmt_name = "";
    switch (subsampling) {
        case ROCDEC_FMT_YUV420:
            fmt_name = "YUV420";
            break;
        case ROCDEC_FMT_YUV444:
            fmt_name = "YUV444";
            break;
        case ROCDEC_FMT_YUV422:
            fmt_name = "YUV422";
            break;
        case ROCDEC_FMT_YUV400:
            fmt_name = "YUV400";
            break;
       case ROCDEC_FMT_YUV420P10:
            fmt_name = "YUV420P10";
            break;
       case ROCDEC_FMT_YUV420P12:
            fmt_name = "YUV420P12";
            break;
        case ROCDEC_FMT_RGB:
            fmt_name = "RGB";
            break;
        default:
            std::cerr << "ERROR: subsampling format is not supported!" << std::endl;
    }
    return fmt_name;
}

std::string ROCDecode::GetCodecFmtName(rocDecVideoCodec codec_id) {
    std::string fmt_name = "";
    switch (codec_id) {
        case rocDecVideoCodec_MPEG1:
            fmt_name = "MPEG1";
            break;
        case rocDecVideoCodec_MPEG2:
            fmt_name = "MPEG1";
            break;
        case rocDecVideoCodec_MPEG4:
            fmt_name = "MPEG4";
            break;
       case rocDecVideoCodec_H264:
            fmt_name = "H264";
            break;
       case rocDecVideoCodec_HEVC:
            fmt_name = "HEVC";
            break;
        case rocDecVideoCodec_VP8:
            fmt_name = "VP8";
            break;
        case rocDecVideoCodec_VP9:
            fmt_name = "VP9";
            break;
        case rocDecVideoCodec_JPEG:
            fmt_name = "JPEG";
            break;
        case rocDecVideoCodec_AV1:
            fmt_name = "AV1";
            break;
        default:
            std::cerr << "ERROR: subsampling format is not supported!" << std::endl;
    }
    return fmt_name;
}

bool ROCDecode::GetOutputImageInfo(OutputImageInfo **image_info) {
    if (!width_ || !height_) {
        std::cerr << "ERROR: ROCDecoder is not intialized" << std::endl;
        return false;
    }
    *image_info = &out_image_info_;
    return true;
}

bool ROCDecode::InitHIP(int device_id)
{
    hipError_t hip_status = hipSuccess;
    hip_status = hipGetDeviceCount(&num_devices_);
    if (hip_status != hipSuccess) {
        std::cerr << "ERROR: hipGetDeviceCount failed! (" << hip_status << ")" << std::endl;
        return false;
    }
    if (num_devices_ < 1) {
        std::cerr << "ERROR: didn't find any GPU!" << std::endl;
        return false;
    }
    if (device_id >= num_devices_) {
        std::cerr << "ERROR: the requested device_id is not found! " << std::endl;
        return false;
    }
    hip_status = hipSetDevice(device_id);
    if (hip_status != hipSuccess) {
        std::cerr << "ERROR: hipSetDevice(" << device_id << ") failed! (" << hip_status << ")" << std::endl;
        return false;
    }
    hip_status = hipGetDeviceProperties(&hip_dev_prop_, device_id);
    if (hip_status != hipSuccess) {
        std::cerr << "ERROR: hipGetDeviceProperties for device (" << device_id << ") failed! (" << hip_status << ")" << std::endl;
        return false;
    }
    hip_status = hipStreamCreate(&hip_stream_);
    if (hip_status != hipSuccess) {
        std::cerr << "ERROR: hipStream_Create failed! (" << hip_status << ")" << std::endl;
        return false;
    }
    return true;
}

void ROCDecode::InitDRMnodes() {
    // build the DRM render node names
    for (int i = 0; i < num_devices_; i++) {
        drm_nodes_.push_back("/dev/dri/renderD" + std::to_string(128 + i));
    }
}

bool ROCDecode::InitVAAPI() {
    if (drm_nodes_.size() == 0) {
        std::cerr << "VAAPI initialization failed!" << std::endl;
        return false;
    }
    drm_fd_ = open(drm_nodes_[device_id_].c_str(), O_RDWR);
    if (drm_fd_ < 0) {
        std::cerr << "ERROR: failed to open drm node " << drm_nodes_[device_id_].c_str() << std::endl;
        return false;
    }
    va_display_ = vaGetDisplayDRM(drm_fd_);
    if (!va_display_) {
        std::cerr << "error: vaGetDisplayDRM failed! " << std::endl;
        return false;
    }

    vaSetInfoCallback(va_display_, NULL, NULL);

    VAStatus va_status;
    int major_version = 0, minor_version = 0;
    va_status = vaInitialize(va_display_, &major_version, &minor_version);
    if (va_status != VA_STATUS_SUCCESS) {
        std::cerr << "ERROR: vaInitialize failed! " << std::endl;
        return false;
    }
    return true;
}

bool ROCDecode::QueryVaProfiles() {
    if (va_display_ == nullptr) {
        if (!InitVAAPI()) {
            return false;
        }
    }
    if (num_va_profiles_ > -1) {
        //already queried available profiles
        return true;
    }
    num_va_profiles_ = vaMaxNumProfiles(va_display_);
    VAStatus va_status;
    va_status = vaQueryConfigProfiles(va_display_, va_profiles_, &num_va_profiles_);
    if (va_status != VA_STATUS_SUCCESS) {
        std::cerr << "ERROR: vaQueryConfigProfiles failed! " << std::endl;
        return false;
    }
    return true;
}

bool ROCDecode::GetImageSizeHintInternal(RocDecImageFormat subsampling, const uint32_t width, const uint32_t height, uint32_t *output_stride, size_t *output_image_size) {
    if ( output_stride == nullptr || output_image_size == nullptr) {
        std::cerr << "invalid input parameters!" << std::endl;
        return false;
    }
    int aligned_height = 0;
    switch (subsampling) {
        case ROCDEC_FMT_YUV420:
            *output_stride = align(width, 256);
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * (aligned_height + (aligned_height >> 1));
             break;
        case ROCDEC_FMT_YUV444:
            *output_stride = align(width, 256);
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * aligned_height * 3;
            break;
        case ROCDEC_FMT_YUV400:
            *output_stride = align(width, 256);
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * aligned_height;
            break;
        case ROCDEC_FMT_RGB:
            *output_stride = align(width, 256) * 3;
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * aligned_height;
            break;
        case ROCDEC_FMT_YUV420P10:
            *output_stride = align(width, 128) * 2;
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * (aligned_height + (aligned_height >> 1));
            break;
        case ROCDEC_FMT_YUV420P12:
            *output_stride = align(width, 128) * 2;
            aligned_height = align(height, 16);
            *output_image_size = *output_stride * (aligned_height + (aligned_height >> 1));
            break;
        default:
            std::cerr << "ERROR: "<< GetPixFmtName(subsampling) <<" is not supported! " << std::endl;
            return false;
    }

    return true;

}
