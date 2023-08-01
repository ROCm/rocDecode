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

#pragma once

#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <map>
#include <vector>
#include <queue>
#include <mutex>
#include <iostream>
#include <sstream>
#include <string>
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <va/va_drm.h>
#include <hip/hip_runtime.h>
#include "../api/rocdecode.h"

#define MAX_VA_PROFILES 36

static inline int align(int value, int alignment) {
   return (value + alignment - 1) & ~(alignment - 1);
}

typedef struct DecFrameBufferType {
    VASurfaceID va_surface_id;                              /**< VASurfaceID for the decoded frame buffer */
    hipExternalMemory_t hip_ext_mem;                        /**< interop hip memory for the decoded surface */
    VADRMPRIMESurfaceDescriptor va_drm_prime_surface_desc;  /**< DRM surface descriptor */
} DecFrameBuffer;

typedef enum {
    ROCDEC_FMT_YUV420 = 0,
    ROCDEC_FMT_YUV444 = 1,
    ROCDEC_FMT_YUV422 = 2,
    ROCDEC_FMT_YUV400 = 3,
    ROCDEC_FMT_YUV420P10 = 4,
    ROCDEC_FMT_YUV420P12 = 5,
    ROCDEC_FMT_RGB = 6,
    ROCDEC_FMT_MAX = 7
} RocDecImageFormat;

typedef struct OutputImageInfoType {
    uint32_t output_width;               /**< Output width of decoded image*/
    uint32_t output_height;              /**< Output height of decoded image*/
    uint32_t output_h_stride;            /**< Output horizontal stride in bytes of luma plane, chroma hstride can be inferred based on chromaFormat*/
    uint32_t output_v_stride;            /**< Output vertical stride in number of columns of luma plane,  chroma vstride can be inferred based on chromaFormat */
    uint32_t bytes_per_pixel;            /**< Output BytesPerPixel of decoded image*/
    uint32_t bit_depth;                  /**< Output BitDepth of the image*/
    uint64_t output_image_size_in_bytes; /**< Output Image Size in Bytes; including both luma and chroma planes*/ 
    RocDecImageFormat chroma_format;      /**< Chroma format of the decoded image*/
} OutputImageInfo;

class ROCDecode {
    public:
       ROCDecode(int device_id = 0);
       ~ROCDecode();
       bool DecodeFrame(uint8_t *data, size_t size, int64_t pts = 0);
       uint8_t* GetFrame(int64_t *pts);
       bool ReleaseFrame(int64_t pts);
       void SaveImage(std::string output_file_name, void* dev_mem, OutputImageInfo* image_info, bool is_output_RGB = 0);
       void GetDeviceinfo(std::string &device_name, std::string &gcn_arch_name, int &pci_bus_id, int &pci_domain_id, int &pci_device_id, std::string &drm_node);
       void GetDecoderCaps(ROCDECDECODECAPS &decoder_caps);
       std::string GetPixFmtName(RocDecImageFormat subsampling);
       uint32_t GetWidth() { assert(width_); return width_;}
       uint32_t GetHeight() { assert(height_); return height_; }
       uint32_t GetBitDepth() { assert(bit_depth_); return bit_depth_; }
       uint32_t GetBytePerPixel() { assert(byte_per_pixel_); return byte_per_pixel_; }
       size_t GetSurfaceSize() { assert(surface_size_); return surface_size_; }
       uint32_t GetSurfaceStride() { assert(surface_stride_); return surface_stride_; }
       RocDecImageFormat GetSubsampling() { return subsampling_; }
       int GetSurfaceWidth() { assert(surface_width_); return surface_width_;}
       int GetSurfaceHeight() { assert(surface_height_); return surface_height_;}
       std::string GetCodecFmtName(rocDecVideoCodec codec_id);
       bool GetOutputImageInfo(OutputImageInfo **image_info);

    private:
       bool InitHIP(int device_id);
       void InitDRMnodes();
       bool InitVAAPI();
       bool QueryVaProfiles();
       bool GetImageSizeHintInternal(RocDecImageFormat subsampling, const uint32_t output_width, const uint32_t output_height,
            uint32_t *output_stride, size_t *output_image_size);
       int num_devices_;
       int device_id_;
       hipDeviceProp_t hip_dev_prop_;
       hipStream_t hip_stream_;
       hipExternalMemoryHandleDesc external_mem_handle_desc_;
       hipExternalMemoryBufferDesc external_mem_buffer_desc_;
       VADisplay va_display_;
       VAProfile va_profiles_[MAX_VA_PROFILES];
       int num_va_profiles_;
       void *yuv_dev_mem_;
       uint32_t width_;
       uint32_t height_;
       uint32_t chroma_height_;
       uint32_t surface_height_;
       uint32_t surface_width_;
       uint32_t num_chroma_planes_;
       uint32_t num_components_;
       uint32_t surface_stride_;
       size_t surface_size_;
       uint32_t bit_depth_;
       uint32_t byte_per_pixel_;
       RocDecImageFormat subsampling_;
       OutputImageInfo out_image_info_;
       std::vector<std::string> drm_nodes_;
       std::mutex mutex_;
       std::queue<DecFrameBuffer *> dec_frame_q_;
       std::vector<VASurfaceID> dec_buffer_pool_;
       FILE *fp_out_;
       int drm_fd_;
};