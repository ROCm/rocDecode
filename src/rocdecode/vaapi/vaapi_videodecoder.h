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

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <mutex>
#include <algorithm>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif
#include <libdrm/amdgpu_drm.h>
#include <libdrm/amdgpu.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include "../../commons.h"
#include "../../../api/rocdecode.h"

#define CHECK_HIP(call) {\
    hipError_t hip_status = call;\
    if (hip_status != hipSuccess) {\
        std::cout << "HIP failure: " << #call << " failed with 'status: " << hipGetErrorName(hip_status) << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;\
        return ROCDEC_RUNTIME_ERROR;\
    }\
}

#define CHECK_VAAPI(call) {\
    VAStatus va_status = call;\
    if (va_status != VA_STATUS_SUCCESS) {\
        std::cout << "VAAPI failure: " << #call << " failed with status: " << std::hex << "0x" << va_status << std::dec << " = '" << vaErrorStr(va_status) << "' at " <<  __FILE__ << ":" << __LINE__ << std::endl;\
        return ROCDEC_RUNTIME_ERROR;\
    }\
}

#define INIT_SLICE_PARAM_LIST_NUM 16 // initial slice parameter buffer list size

typedef enum {
    kSpx = 0, // Single Partition Accelerator
    kDpx = 1, // Dual Partition Accelerator
    kTpx = 2, // Triple Partition Accelerator
    kQpx = 3, // Quad Partition Accelerator
    kCpx = 4, // Core Partition Accelerator
} ComputePartition;

class VaapiVideoDecoder {
public:
    VaapiVideoDecoder(RocDecoderCreateInfo &decoder_create_info);
    ~VaapiVideoDecoder();
    rocDecStatus InitializeDecoder(std::string device_name, std::string gcn_arch_name);
    rocDecStatus SubmitDecode(RocdecPicParams *pPicParams);
    rocDecStatus GetDecodeStatus(int pic_idx, RocdecDecodeStatus* decode_status);
    rocDecStatus ExportSurface(int pic_idx, VADRMPRIMESurfaceDescriptor &va_drm_prime_surface_desc);
    rocDecStatus SyncSurface(int pic_idx);
    rocDecStatus ReconfigureDecoder(RocdecReconfigureDecoderInfo *reconfig_params);

private:
    RocDecoderCreateInfo decoder_create_info_;
    int drm_fd_;
    VADisplay va_display_;
    VAProfile va_profile_;
    VAConfigAttrib va_config_attrib_;
    VAConfigID va_config_id_;
    VAContextID va_context_id_;
    std::vector<VASurfaceID> va_surface_ids_;

    VABufferID pic_params_buf_id_;
    VABufferID iq_matrix_buf_id_;
    std::vector<VABufferID> slice_params_buf_id_ = std::vector<VABufferID>(INIT_SLICE_PARAM_LIST_NUM, 0);
    uint32_t num_slices_;
    VABufferID slice_data_buf_id_;
    uint32_t slice_data_buf_size_;

    bool IsCodecConfigSupported(int device_id, rocDecVideoCodec codec_type, rocDecVideoChromaFormat chroma_format, uint32_t bit_depth_minus8, rocDecVideoSurfaceFormat output_format);
    rocDecStatus CreateDecoderConfig();
    rocDecStatus CreateSurfaces();
    rocDecStatus CreateContext();
    rocDecStatus DestroyDataBuffers();
};

// Jefftest
// The GpuVaContext singleton class providing access to the the GPU VA services
class GpuVaContext {
public:
    int num_devices_;
    int device_id_;
    int drm_fd_;
    VADisplay va_display_;
    hipDeviceProp_t hip_dev_prop_;
    uint32_t num_dec_engines_;
    int num_va_profiles_;
    std::vector<VAProfile> va_profile_list_; // supported profiles by the current GPU
    VAProfile va_profile_; // current profile used
    VAConfigID va_config_id_;
    uint32_t rt_format_attrib_;
    uint32_t output_format_mask_;
    uint32_t max_width_;
    uint32_t max_height_;
    uint32_t min_width_;
    uint32_t min_height_;

    static GpuVaContext& GetInstance() {
        printf("Get instance .....\n"); // Jefftest
        static GpuVaContext instance;
        return instance;
    }

    rocDecStatus Initialize(int device_id) {
        printf("Initialize(): device_id = %d, initialized_ = %d\n", device_id, initialized_); // Jefftest
        if ( initialized_ && device_id != device_id_) {
            CHECK_VAAPI(vaTerminate(va_display_));
            initialized_ = false;
        }
        if (!initialized_) {
            std::lock_guard<std::mutex> lock(mutex);
            device_id_ = device_id;
            rocDecStatus rocdec_status = ROCDEC_SUCCESS;
            rocdec_status = InitHIP(device_id_);
            if (rocdec_status != ROCDEC_SUCCESS) {
                ERR("Failed to initilize the HIP.");
                return rocdec_status;
            }

            std::cout << hip_dev_prop_.name << std::endl; // Jefftest
            std::cout << hip_dev_prop_.gcnArchName << std::endl; // Jefftest
            std::string gcn_arch_name = hip_dev_prop_.gcnArchName;
            std::size_t pos = gcn_arch_name.find_first_of(":");
            std::string gcn_arch_name_base = (pos != std::string::npos) ? gcn_arch_name.substr(0, pos) : gcn_arch_name;
            std::vector<int> visible_devices;
            GetVisibleDevices(visible_devices);
            std::cout << visible_devices.size() << std::endl; // Jefftest

            int offset = 0;
            if (gcn_arch_name_base.compare("gfx942") == 0) {
                std::vector<ComputePartition> current_compute_partitions;
                GetCurrentComputePartition(current_compute_partitions);
                if (current_compute_partitions.empty()) {
                    //if the current_compute_partitions is empty then the default SPX mode is assumed.
                    if (device_id_ < visible_devices.size()) {
                        offset = visible_devices[device_id_] * 7;
                    } else {
                        offset = device_id_ * 7;
                    }
                } else {
                    GetDrmNodeOffset(hip_dev_prop_.name, device_id_, visible_devices, current_compute_partitions, offset);
                }
            }

            std::string drm_node = "/dev/dri/renderD";
            if (device_id_ < visible_devices.size()) {
                drm_node += std::to_string(128 + offset + visible_devices[device_id_]);
            } else {
                drm_node += std::to_string(128 + offset + device_id_);
            }
            std::cout << "drm_node = " << drm_node << std::endl; // Jefftest

            rocdec_status = InitVAAPI(drm_node);
            if (rocdec_status != ROCDEC_SUCCESS) {
                ERR("Failed to initilize the VAAPI.");
                return rocdec_status;
            }

            amdgpu_device_handle dev_handle;
            uint32_t major_version = 0, minor_version = 0;
            if (amdgpu_device_initialize(drm_fd_, &major_version, &minor_version, &dev_handle)) {
                ERR("GPU device initialization failed: " + drm_node);
                return ROCDEC_DEVICE_INVALID;
            }
            if (amdgpu_query_hw_ip_count(dev_handle, AMDGPU_HW_IP_VCN_DEC, &num_dec_engines_)) {
                ERR("Failed to get the number of video decode engines.");
            }
            printf("num_dec_engines_ = %d ....\n", num_dec_engines_); // Jefftest
            amdgpu_device_deinitialize(dev_handle);

            // Prob VA profiles
            num_va_profiles_ = vaMaxNumProfiles(va_display_);
            std::cout << "num_va_profiles_ = " << num_va_profiles_ << std::endl; // Jefftest
            va_profile_list_.resize(num_va_profiles_);
            CHECK_VAAPI(vaQueryConfigProfiles(va_display_, va_profile_list_.data(), &num_va_profiles_));
            std::cout << "num_va_profiles_ = " << num_va_profiles_ << std::endl; // Jefftest


            initialized_ = true;
        }
        return ROCDEC_SUCCESS;
    }

    rocDecStatus CheckDecCapForCodecType(RocdecDecodeCaps *dec_cap) {
        if (dec_cap == nullptr) {
            ERR("Null decode capability struct pointer.");
            return ROCDEC_INVALID_PARAMETER;
        }
        std::lock_guard<std::mutex> lock(mutex);
        rocDecStatus rocdec_status = ROCDEC_SUCCESS;
        if (!initialized_) {
            rocdec_status = Initialize(dec_cap->device_id);
            if (rocdec_status != ROCDEC_SUCCESS) {
                ERR("Failed to initilize.");
                return rocdec_status;
            }
        }

        dec_cap->is_supported = 1; // init value
        VAProfile va_profile = VAProfileNone;
        switch (dec_cap->codec_type) {
            case rocDecVideoCodec_HEVC:
                if (dec_cap->bit_depth_minus_8 == 0) {
                    va_profile = VAProfileHEVCMain;
                } else if (dec_cap->bit_depth_minus_8 == 2) {
                    va_profile = VAProfileHEVCMain10;
                }
                break;
            case rocDecVideoCodec_AVC:
                va_profile = VAProfileH264Main;
                break;
            case rocDecVideoCodec_VP9:
                if (dec_cap->bit_depth_minus_8 == 0) {
                    va_profile = VAProfileVP9Profile0;
                } else if (dec_cap->bit_depth_minus_8 == 2) {
                    va_profile = VAProfileVP9Profile2;
                }
                break;
            case rocDecVideoCodec_AV1:
            #if VA_CHECK_VERSION(1,6,0)
                va_profile = VAProfileAV1Profile0;
            #else
                va_profile = static_cast<VAProfile>(32); // VAProfileAV1Profile0;
            #endif
                break;
            default:
                dec_cap->is_supported = 0;
                return ROCDEC_SUCCESS;
        }

        int i;
        for (i = 0; i < num_va_profiles_; i++) {
            if (va_profile_list_[i] == va_profile) {
                break;
            }
        }
        if (i == num_va_profiles_) {
            dec_cap->is_supported = 0;
            return ROCDEC_SUCCESS;
        }

        // Check if the config attributes of the profile have been probed before
        //if (config_attributes_probed_ == false)
        if (va_profile != va_profile_ || config_attributes_probed_ == false) {
            va_profile_ = va_profile;

            std::cout << "Create VA config .... " << std::endl; // Jefftest
            VAConfigAttrib va_config_attrib;
            unsigned int attr_count;
            std::vector<VASurfaceAttrib> attr_list;
            va_config_attrib.type = VAConfigAttribRTFormat;
            CHECK_VAAPI(vaGetConfigAttributes(va_display_, va_profile_, VAEntrypointVLD, &va_config_attrib, 1));
            rt_format_attrib_ = va_config_attrib.value;

            CHECK_VAAPI(vaCreateConfig(va_display_, va_profile_, VAEntrypointVLD, &va_config_attrib, 1, &va_config_id_));
            CHECK_VAAPI(vaQuerySurfaceAttributes(va_display_, va_config_id_, 0, &attr_count));
            attr_list.resize(attr_count);
            CHECK_VAAPI(vaQuerySurfaceAttributes(va_display_, va_config_id_, attr_list.data(), &attr_count));
            output_format_mask_ = 0;
            CHECK_VAAPI(vaDestroyConfig(va_display_, va_config_id_));
            for (int k = 0; k < attr_count; k++) {
                switch (attr_list[k].type) {
                case VASurfaceAttribPixelFormat: {
                    switch (attr_list[k].value.value.i) {
                        case VA_FOURCC_NV12:
                            output_format_mask_ |= 1 << rocDecVideoSurfaceFormat_NV12;
                            break;
                        case VA_FOURCC_P016:
                            output_format_mask_ |= 1 << rocDecVideoSurfaceFormat_P016;
                            break;
                        default:
                            break;
                    }
                }
                    break;
                case VASurfaceAttribMinWidth:
                    min_width_ = attr_list[k].value.value.i;
                    break;
                case VASurfaceAttribMinHeight:
                    min_height_ = attr_list[k].value.value.i;
                    break;
                case VASurfaceAttribMaxWidth:
                    max_width_ = attr_list[k].value.value.i;
                    break;
                case VASurfaceAttribMaxHeight:
                    max_height_ = attr_list[k].value.value.i;
                    break;
                default:
                    break;
                }
            }
            config_attributes_probed_ = true;
        }

        // Check chroma format
        switch (dec_cap->chroma_format) {
            case rocDecVideoChromaFormat_Monochrome: {
                if ((rt_format_attrib_ & VA_RT_FORMAT_YUV400) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case rocDecVideoChromaFormat_420: {
                if ((rt_format_attrib_ & (VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV420_10 | VA_RT_FORMAT_YUV420_12)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case rocDecVideoChromaFormat_422: {
                if ((rt_format_attrib_ & (VA_RT_FORMAT_YUV422 | VA_RT_FORMAT_YUV422_10 | VA_RT_FORMAT_YUV422_12)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case rocDecVideoChromaFormat_444: {
                if ((rt_format_attrib_ & (VA_RT_FORMAT_YUV444 | VA_RT_FORMAT_YUV444_10 | VA_RT_FORMAT_YUV444_12)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            default: {
                dec_cap->is_supported = 0;
                return ROCDEC_SUCCESS;
            }
        }
        // Check bit depth
        switch (dec_cap->bit_depth_minus_8) {
            case 0: {
                if ((rt_format_attrib_ & (VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV422 | VA_RT_FORMAT_YUV444 | VA_RT_FORMAT_YUV400)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case 2: {
                if ((rt_format_attrib_ & (VA_RT_FORMAT_YUV420_10 | VA_RT_FORMAT_YUV422_10 | VA_RT_FORMAT_YUV444_10)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case 4: {
                if ((rt_format_attrib_ & (VA_RT_FORMAT_YUV420_12 | VA_RT_FORMAT_YUV422_12 | VA_RT_FORMAT_YUV444_12)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            default: {
                dec_cap->is_supported = 0;
                return ROCDEC_SUCCESS;
            }
        }

        dec_cap->num_decoders = num_dec_engines_;
        dec_cap->output_format_mask = output_format_mask_;
        dec_cap->max_width = max_width_;
        dec_cap->max_height = max_height_;
        dec_cap->min_width = min_width_;
        dec_cap->min_height = min_height_;
        // Jefftest
        std::cout << "devicde_id = " << (int)dec_cap->device_id << ", codec_type = " << dec_cap->codec_type << ", chroma_format = " << dec_cap->chroma_format << ", bit_depth_minus_8 = " << dec_cap->bit_depth_minus_8 << ", is_supported = " << (int)dec_cap->is_supported << ", num_decoders = " << (int)dec_cap->num_decoders << ", output_format_mask = " << dec_cap->output_format_mask << ", max_width = " << dec_cap->max_width << ", max_height = " << dec_cap->max_height << ", min_width = " << dec_cap->min_width << ", min_height = " << dec_cap->min_height << std::endl;

        return ROCDEC_SUCCESS;
    }
private:
    bool initialized_;
    std::mutex mutex;
    bool config_attributes_probed_;

    GpuVaContext() : initialized_{false}, drm_fd_{-1}, num_dec_engines_{1}, va_profile_{VAProfileNone}, config_attributes_probed_{false} {
        printf("Private construction .... \n"); // Jefftest
    }
    GpuVaContext(const GpuVaContext&) = delete;
    GpuVaContext& operator = (const GpuVaContext) = delete;
    ~GpuVaContext() = default;

    rocDecStatus InitHIP(int device_id) {
        CHECK_HIP(hipGetDeviceCount(&num_devices_));
        if (num_devices_ < 1) {
            ERR("Didn't find any GPU.");
            return ROCDEC_DEVICE_INVALID;
        }
        if (device_id >= num_devices_) {
            ERR("ERROR: the requested device_id is not found! ");
            return ROCDEC_DEVICE_INVALID;
        }   
        CHECK_HIP(hipSetDevice(device_id));
        CHECK_HIP(hipGetDeviceProperties(&hip_dev_prop_, device_id));
        return ROCDEC_SUCCESS;
    }

    rocDecStatus InitVAAPI(std::string drm_node) {
        std::cout << "InitVAAPI() new .........." << std::endl; // Jefftest
        drm_fd_ = open(drm_node.c_str(), O_RDWR);
        if (drm_fd_ < 0) {
            ERR("Failed to open drm node." + drm_node);
            return ROCDEC_NOT_INITIALIZED;
        }
        va_display_ = vaGetDisplayDRM(drm_fd_);
        if (!va_display_) {
            ERR("Failed to create va_display_.");
            return ROCDEC_NOT_INITIALIZED;
        }
        vaSetInfoCallback(va_display_, NULL, NULL);
        int major_version = 0, minor_version = 0;
        CHECK_VAAPI(vaInitialize(va_display_, &major_version, &minor_version));
        return ROCDEC_SUCCESS;
    }

    void GetVisibleDevices(std::vector<int>& visible_devices_vetor) {
        char *visible_devices = std::getenv("HIP_VISIBLE_DEVICES");
        if (visible_devices != nullptr) {
            char *token = std::strtok(visible_devices,",");
            while (token != nullptr) {
                visible_devices_vetor.push_back(std::atoi(token));
                token = std::strtok(nullptr,",");
            }
            std::sort(visible_devices_vetor.begin(), visible_devices_vetor.end());
        }
    }

    void GetCurrentComputePartition(std::vector<ComputePartition> &current_compute_partitions) {
        std::string search_path = "/sys/devices/";
        std::string partition_file = "current_compute_partition";
        std::error_code ec;
        if (fs::exists(search_path)) {
            for (auto it = fs::recursive_directory_iterator(search_path, fs::directory_options::skip_permission_denied); it != fs::recursive_directory_iterator(); ) {
                try {
                    if (it->path().filename() == partition_file) {
                        std::ifstream file(it->path());
                        if (file.is_open()) {
                            std::string partition;
                            std::getline(file, partition);
                            if (partition.compare("SPX") == 0 || partition.compare("spx") == 0) {
                                current_compute_partitions.push_back(kSpx);
                            } else if (partition.compare("DPX") == 0 || partition.compare("dpx") == 0) {
                                current_compute_partitions.push_back(kDpx);
                            } else if (partition.compare("TPX") == 0 || partition.compare("tpx") == 0) {
                                current_compute_partitions.push_back(kTpx);
                            } else if (partition.compare("QPX") == 0 || partition.compare("qpx") == 0) {
                                current_compute_partitions.push_back(kQpx);
                            } else if (partition.compare("CPX") == 0 || partition.compare("cpx") == 0) {
                                current_compute_partitions.push_back(kCpx);
                            }
                            file.close();
                        }
                    }
                    ++it;
                } catch (fs::filesystem_error& e) {
                    it.increment(ec);
                }
            }
        }
    }

    void GetDrmNodeOffset(std::string device_name, uint8_t device_id, std::vector<int>& visible_devices, std::vector<ComputePartition> &current_compute_partitions, int &offset) {
        if (!current_compute_partitions.empty()) {
            switch (current_compute_partitions[0]) {
                case kSpx:
                    if (device_id < visible_devices.size()) {
                        offset = visible_devices[device_id] * 7;
                    } else {
                        offset = device_id * 7;
                    }
                    break;
                case kDpx:
                    if (device_id < visible_devices.size()) {
                        offset = (visible_devices[device_id] / 2) * 6;
                    } else {
                        offset = (device_id / 2) * 6;
                    }
                    break;
                case kTpx:
                    // Please note that although there are only 6 XCCs per socket on MI300A,
                    // there are two dummy render nodes added by the driver.
                    // This needs to be taken into account when creating drm_node on each socket in TPX mode.
                    if (device_id < visible_devices.size()) {
                        offset = (visible_devices[device_id] / 3) * 5;
                    } else {
                        offset = (device_id / 3) * 5;
                    }
                    break;
                case kQpx:
                    if (device_id < visible_devices.size()) {
                        offset = (visible_devices[device_id] / 4) * 4;
                    } else {
                        offset = (device_id / 4) * 4;
                    }
                    break;
                case kCpx:
                    // Please note that both MI300A and MI300X have the same gfx_arch_name which is
                    // gfx942. Therefore we cannot use the gfx942 to identify MI300A.
                    // instead use the device name and look for MI300A
                    // Also, as explained aboe in the TPX mode section, we need to be taken into account
                    // the extra two dummy nodes when creating drm_node on each socket in CPX mode as well.
                    std::string mi300a = "MI300A";
                    size_t found_mi300a = device_name.find(mi300a);
                    if (found_mi300a != std::string::npos) {
                        if (device_id < visible_devices.size()) {
                            offset = (visible_devices[device_id] / 6) * 2;
                        } else {
                            offset = (device_id / 6) * 2;
                        }
                    }
                    break;
            }
        }
    }
};