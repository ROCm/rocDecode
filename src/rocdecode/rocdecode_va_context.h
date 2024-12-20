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

typedef enum {
    kSpx = 0, // Single Partition Accelerator
    kDpx = 1, // Dual Partition Accelerator
    kTpx = 2, // Triple Partition Accelerator
    kQpx = 3, // Quad Partition Accelerator
    kCpx = 4, // Core Partition Accelerator
} ComputePartition;

typedef struct {
    int num_devices;
    int device_id;
    int drm_fd;
    VADisplay va_display;
    hipDeviceProp_t hip_dev_prop;
    uint32_t num_dec_engines;
    int num_va_profiles;
    std::vector<VAProfile> va_profile_list; // supported profiles by the current GPU
    VAProfile va_profile; // current profile used
    VAConfigID va_config_id;
    bool config_attributes_probed;
    uint32_t rt_format_attrib;
    uint32_t output_format_mask;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t min_width;
    uint32_t min_height;
} VaContextInfo;

// The GpuVaContext singleton class providing access to the the GPU VA services
class GpuVaContext {
public:
    std::vector<VaContextInfo> va_contexts_;

    static GpuVaContext& GetInstance() {
        static GpuVaContext instance;
        return instance;
    }

    rocDecStatus GetVaContext(int device_id, uint32_t *va_ctx_id) {
        std::lock_guard<std::mutex> lock(mutex);
        bool found_existing = false;
        uint32_t va_ctx_idx = 0;
        if (!va_contexts_.empty()) {
            for (va_ctx_idx = 0; va_ctx_idx < va_contexts_.size(); va_ctx_idx++) {
                if (device_id == va_contexts_[va_ctx_idx].device_id) {
                    found_existing = true;
                    break;
                }
            }
        }
        if (found_existing) {
            *va_ctx_id = va_ctx_idx;
            return ROCDEC_SUCCESS;
        } else {
            va_contexts_.resize(va_contexts_.size() + 1);
            va_ctx_idx = va_contexts_.size() - 1;

            va_contexts_[va_ctx_idx].device_id = device_id;
            va_contexts_[va_ctx_idx].drm_fd = -1;
            va_contexts_[va_ctx_idx].va_display = 0;
            va_contexts_[va_ctx_idx].num_dec_engines = 1;
            va_contexts_[va_ctx_idx].va_profile = VAProfileNone;
            va_contexts_[va_ctx_idx].config_attributes_probed = false;

            rocDecStatus rocdec_status = ROCDEC_SUCCESS;
            rocdec_status = InitHIP(va_ctx_idx);
            if (rocdec_status != ROCDEC_SUCCESS) {
                ERR("Failed to initilize the HIP.");
                return rocdec_status;
            }

            std::string gcn_arch_name = va_contexts_[va_ctx_idx].hip_dev_prop.gcnArchName;
            std::size_t pos = gcn_arch_name.find_first_of(":");
            std::string gcn_arch_name_base = (pos != std::string::npos) ? gcn_arch_name.substr(0, pos) : gcn_arch_name;
            std::vector<int> visible_devices;
            GetVisibleDevices(visible_devices);

            int offset = 0;
            if (gcn_arch_name_base.compare("gfx942") == 0) {
                std::vector<ComputePartition> current_compute_partitions;
                GetCurrentComputePartition(current_compute_partitions);
                if (current_compute_partitions.empty()) {
                    //if the current_compute_partitions is empty then the default SPX mode is assumed.
                    if (va_contexts_[va_ctx_idx].device_id < visible_devices.size()) {
                        offset = visible_devices[va_contexts_[va_ctx_idx].device_id] * 7;
                    } else {
                        offset = va_contexts_[va_ctx_idx].device_id * 7;
                    }
                } else {
                    GetDrmNodeOffset(va_contexts_[va_ctx_idx].hip_dev_prop.name, va_contexts_[va_ctx_idx].device_id, visible_devices, current_compute_partitions, offset);
                }
            }

            std::string drm_node = "/dev/dri/renderD";
            if (va_contexts_[va_ctx_idx].device_id < visible_devices.size()) {
                drm_node += std::to_string(128 + offset + visible_devices[va_contexts_[va_ctx_idx].device_id]);
            } else {
                drm_node += std::to_string(128 + offset + va_contexts_[va_ctx_idx].device_id);
            }

            rocdec_status = InitVAAPI(va_ctx_idx, drm_node);
            if (rocdec_status != ROCDEC_SUCCESS) {
                ERR("Failed to initilize the VAAPI.");
                return rocdec_status;
            }

            amdgpu_device_handle dev_handle;
            uint32_t major_version = 0, minor_version = 0;
            if (amdgpu_device_initialize(va_contexts_[va_ctx_idx].drm_fd, &major_version, &minor_version, &dev_handle)) {
                ERR("GPU device initialization failed: " + drm_node);
                return ROCDEC_DEVICE_INVALID;
            }
            if (amdgpu_query_hw_ip_count(dev_handle, AMDGPU_HW_IP_VCN_DEC, &va_contexts_[va_ctx_idx].num_dec_engines)) {
                ERR("Failed to get the number of video decode engines.");
            }
            amdgpu_device_deinitialize(dev_handle);

            // Prob VA profiles
            va_contexts_[va_ctx_idx].num_va_profiles = vaMaxNumProfiles(va_contexts_[va_ctx_idx].va_display);
            va_contexts_[va_ctx_idx].va_profile_list.resize(va_contexts_[va_ctx_idx].num_va_profiles);
            CHECK_VAAPI(vaQueryConfigProfiles(va_contexts_[va_ctx_idx].va_display, va_contexts_[va_ctx_idx].va_profile_list.data(), &va_contexts_[va_ctx_idx].num_va_profiles));

            *va_ctx_id = va_ctx_idx;
            return ROCDEC_SUCCESS;
        }
    }

    rocDecStatus GetVaDisplay(uint32_t va_ctx_id, VADisplay *va_display) {
        if (va_ctx_id >= va_contexts_.size()) {
            ERR("Invalid VA context Id.");
            *va_display = 0;
            return ROCDEC_INVALID_PARAMETER;
        } else {
            *va_display = va_contexts_[va_ctx_id].va_display;
            return ROCDEC_SUCCESS;
        }
    }

    rocDecStatus CheckDecCapForCodecType(RocdecDecodeCaps *dec_cap) {
        if (dec_cap == nullptr) {
            ERR("Null decode capability struct pointer.");
            return ROCDEC_INVALID_PARAMETER;
        }
        rocDecStatus rocdec_status = ROCDEC_SUCCESS;
        uint32_t va_ctx_id;
        rocdec_status = GetVaContext(dec_cap->device_id, &va_ctx_id);
        if (rocdec_status != ROCDEC_SUCCESS) {
            ERR("Failed to initilize.");
            return rocdec_status;
        }

        std::lock_guard<std::mutex> lock(mutex);
        dec_cap->is_supported = 1; // init value
        VAProfile va_profile = VAProfileNone;
        switch (dec_cap->codec_type) {
            case rocDecVideoCodec_HEVC: {
                if (dec_cap->bit_depth_minus_8 == 0) {
                    va_profile = VAProfileHEVCMain;
                } else if (dec_cap->bit_depth_minus_8 == 2) {
                    va_profile = VAProfileHEVCMain10;
                }
                break;
            }
            case rocDecVideoCodec_AVC: {
                va_profile = VAProfileH264Main;
                break;
            }
            case rocDecVideoCodec_VP9: {
                if (dec_cap->bit_depth_minus_8 == 0) {
                    va_profile = VAProfileVP9Profile0;
                } else if (dec_cap->bit_depth_minus_8 == 2) {
                    va_profile = VAProfileVP9Profile2;
                }
                break;
            }
            case rocDecVideoCodec_AV1: {
            #if VA_CHECK_VERSION(1,6,0)
                va_profile = VAProfileAV1Profile0;
            #else
                va_profile = static_cast<VAProfile>(32); // VAProfileAV1Profile0;
            #endif
                break;
            }
            default: {
                dec_cap->is_supported = 0;
                return ROCDEC_SUCCESS;
            }
        }

        int i;
        for (i = 0; i < va_contexts_[va_ctx_id].num_va_profiles; i++) {
            if (va_contexts_[va_ctx_id].va_profile_list[i] == va_profile) {
                break;
            }
        }
        if (i == va_contexts_[va_ctx_id].num_va_profiles) {
            dec_cap->is_supported = 0;
            return ROCDEC_SUCCESS;
        }

        // Check if the config attributes of the profile have been probed before
        if (va_profile != va_contexts_[va_ctx_id].va_profile || va_contexts_[va_ctx_id].config_attributes_probed == false) {
            va_contexts_[va_ctx_id].va_profile = va_profile;

            VAConfigAttrib va_config_attrib;
            unsigned int attr_count;
            std::vector<VASurfaceAttrib> attr_list;
            va_config_attrib.type = VAConfigAttribRTFormat;
            CHECK_VAAPI(vaGetConfigAttributes(va_contexts_[va_ctx_id].va_display, va_contexts_[va_ctx_id].va_profile, VAEntrypointVLD, &va_config_attrib, 1));
            va_contexts_[va_ctx_id].rt_format_attrib = va_config_attrib.value;

            CHECK_VAAPI(vaCreateConfig(va_contexts_[va_ctx_id].va_display, va_contexts_[va_ctx_id].va_profile, VAEntrypointVLD, &va_config_attrib, 1, &va_contexts_[va_ctx_id].va_config_id));
            CHECK_VAAPI(vaQuerySurfaceAttributes(va_contexts_[va_ctx_id].va_display, va_contexts_[va_ctx_id].va_config_id, 0, &attr_count));
            attr_list.resize(attr_count);
            CHECK_VAAPI(vaQuerySurfaceAttributes(va_contexts_[va_ctx_id].va_display, va_contexts_[va_ctx_id].va_config_id, attr_list.data(), &attr_count));
            va_contexts_[va_ctx_id].output_format_mask = 0;
            CHECK_VAAPI(vaDestroyConfig(va_contexts_[va_ctx_id].va_display, va_contexts_[va_ctx_id].va_config_id));
            for (int k = 0; k < attr_count; k++) {
                switch (attr_list[k].type) {
                case VASurfaceAttribPixelFormat: {
                    switch (attr_list[k].value.value.i) {
                        case VA_FOURCC_NV12:
                            va_contexts_[va_ctx_id].output_format_mask |= 1 << rocDecVideoSurfaceFormat_NV12;
                            break;
                        case VA_FOURCC_P016:
                            va_contexts_[va_ctx_id].output_format_mask |= 1 << rocDecVideoSurfaceFormat_P016;
                            break;
                        default:
                            break;
                    }
                }
                    break;
                case VASurfaceAttribMinWidth:
                    va_contexts_[va_ctx_id].min_width = attr_list[k].value.value.i;
                    break;
                case VASurfaceAttribMinHeight:
                    va_contexts_[va_ctx_id].min_height = attr_list[k].value.value.i;
                    break;
                case VASurfaceAttribMaxWidth:
                    va_contexts_[va_ctx_id].max_width = attr_list[k].value.value.i;
                    break;
                case VASurfaceAttribMaxHeight:
                    va_contexts_[va_ctx_id].max_height = attr_list[k].value.value.i;
                    break;
                default:
                    break;
                }
            }
            va_contexts_[va_ctx_id].config_attributes_probed = true;
        }

        // Check chroma format
        switch (dec_cap->chroma_format) {
            case rocDecVideoChromaFormat_Monochrome: {
                if ((va_contexts_[va_ctx_id].rt_format_attrib & VA_RT_FORMAT_YUV400) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case rocDecVideoChromaFormat_420: {
                if ((va_contexts_[va_ctx_id].rt_format_attrib & (VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV420_10 | VA_RT_FORMAT_YUV420_12)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case rocDecVideoChromaFormat_422: {
                if ((va_contexts_[va_ctx_id].rt_format_attrib & (VA_RT_FORMAT_YUV422 | VA_RT_FORMAT_YUV422_10 | VA_RT_FORMAT_YUV422_12)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case rocDecVideoChromaFormat_444: {
                if ((va_contexts_[va_ctx_id].rt_format_attrib & (VA_RT_FORMAT_YUV444 | VA_RT_FORMAT_YUV444_10 | VA_RT_FORMAT_YUV444_12)) == 0) {
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
                if ((va_contexts_[va_ctx_id].rt_format_attrib & (VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV422 | VA_RT_FORMAT_YUV444 | VA_RT_FORMAT_YUV400)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case 2: {
                if ((va_contexts_[va_ctx_id].rt_format_attrib & (VA_RT_FORMAT_YUV420_10 | VA_RT_FORMAT_YUV422_10 | VA_RT_FORMAT_YUV444_10)) == 0) {
                    dec_cap->is_supported = 0;
                    return ROCDEC_SUCCESS;
                }
                break;
            }
            case 4: {
                if ((va_contexts_[va_ctx_id].rt_format_attrib & (VA_RT_FORMAT_YUV420_12 | VA_RT_FORMAT_YUV422_12 | VA_RT_FORMAT_YUV444_12)) == 0) {
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

        dec_cap->num_decoders = va_contexts_[va_ctx_id].num_dec_engines;
        dec_cap->output_format_mask = va_contexts_[va_ctx_id].output_format_mask;
        dec_cap->max_width = va_contexts_[va_ctx_id].max_width;
        dec_cap->max_height = va_contexts_[va_ctx_id].max_height;
        dec_cap->min_width = va_contexts_[va_ctx_id].min_width;
        dec_cap->min_height = va_contexts_[va_ctx_id].min_height;
        return ROCDEC_SUCCESS;
    }

private:
    std::mutex mutex;

    GpuVaContext() {};
    GpuVaContext(const GpuVaContext&) = delete;
    GpuVaContext& operator = (const GpuVaContext) = delete;
    ~GpuVaContext() {
        for (int i = 0; i < va_contexts_.size(); i++) {
            if (va_contexts_[i].va_display) {
                if (vaTerminate(va_contexts_[i].va_display) != VA_STATUS_SUCCESS) {
                    ERR("Failed to termiate VA");
                }
            }
        }
    };

    rocDecStatus InitHIP(int va_ctx_idx) {
        CHECK_HIP(hipGetDeviceCount(&va_contexts_[va_ctx_idx].num_devices));
        if (va_contexts_[va_ctx_idx].num_devices < 1) {
            ERR("Didn't find any GPU.");
            return ROCDEC_DEVICE_INVALID;
        }
        if (va_contexts_[va_ctx_idx].device_id >= va_contexts_[va_ctx_idx].num_devices) {
            ERR("ERROR: the requested device_id is not found! ");
            return ROCDEC_DEVICE_INVALID;
        }   
        CHECK_HIP(hipSetDevice(va_contexts_[va_ctx_idx].device_id));
        CHECK_HIP(hipGetDeviceProperties(&va_contexts_[va_ctx_idx].hip_dev_prop, va_contexts_[va_ctx_idx].device_id));
        return ROCDEC_SUCCESS;
    }

    rocDecStatus InitVAAPI(int va_ctx_idx, std::string drm_node) {
        va_contexts_[va_ctx_idx].drm_fd = open(drm_node.c_str(), O_RDWR);
        if (va_contexts_[va_ctx_idx].drm_fd < 0) {
            ERR("Failed to open drm node." + drm_node);
            return ROCDEC_NOT_INITIALIZED;
        }
        va_contexts_[va_ctx_idx].va_display = vaGetDisplayDRM(va_contexts_[va_ctx_idx].drm_fd);
        if (!va_contexts_[va_ctx_idx].va_display) {
            ERR("Failed to create VA display.");
            return ROCDEC_NOT_INITIALIZED;
        }
        vaSetInfoCallback(va_contexts_[va_ctx_idx].va_display, NULL, NULL);
        int major_version = 0, minor_version = 0;
        CHECK_VAAPI(vaInitialize(va_contexts_[va_ctx_idx].va_display, &major_version, &minor_version));
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