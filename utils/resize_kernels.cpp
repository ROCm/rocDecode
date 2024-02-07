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

#include "resize_kernels.h"
#include "roc_video_dec.h"

template<typename YuvUnitx2>
static __global__ void ResizeHip(hipTextureObject_t tex_y, hipTextureObject_t tex_uv,
        uint8_t *p_dst, uint8_t *p_dst_uv, int pitch, int width, int height,
        float fx_scale, float fy_scale)
{
    int ix = blockIdx.x * blockDim.x + threadIdx.x,
        iy = blockIdx.y * blockDim.y + threadIdx.y;

    if (ix >= width / 2 || iy >= height / 2) {
        return;
    }

    int x = ix * 2, y = iy * 2;
    typedef decltype(YuvUnitx2::x) YuvUnit;
    const int MAX = (1 << (sizeof(YuvUnit) * 8)) - 1;
    *(YuvUnitx2 *)(p_dst + y * pitch + x * sizeof(YuvUnit)) = YuvUnitx2 {
        (YuvUnit)(tex2D<float>(tex_y, x / fx_scale, y / fy_scale) * MAX),
        (YuvUnit)(tex2D<float>(tex_y, (x + 1) / fx_scale, y / fy_scale) * MAX)
    };
    y++;
    *(YuvUnitx2 *)(p_dst + y * pitch + x * sizeof(YuvUnit)) = YuvUnitx2 {
        (YuvUnit)(tex2D<float>(tex_y, x / fx_scale, y / fy_scale) * MAX),
        (YuvUnit)(tex2D<float>(tex_y, (x + 1) / fx_scale, y / fy_scale) * MAX)
    };
    float2 uv = tex2D<float2>(tex_uv, ix / fx_scale, (height + iy) / fy_scale + 0.5f);
    *(YuvUnitx2 *)(p_dst_uv + iy * pitch + ix * 2 * sizeof(YuvUnit)) = YuvUnitx2{ (YuvUnit)(uv.x * MAX), (YuvUnit)(uv.y * MAX) };
}

template <typename YuvUnitx2>
static void Resize(unsigned char *p_dst, unsigned char* dp_dst_uv, int dst_pitch, int dst_width, int dst_height, 
                    unsigned char *p_src, int src_pitch, int src_width, int src_height) {
    hipResourceDesc res_desc = {};
    res_desc.resType = hipResourceTypePitch2D;
    res_desc.res.pitch2D.devPtr = p_src;
    res_desc.res.pitch2D.desc = hipCreateChannelDesc<decltype(YuvUnitx2::x)>();
    res_desc.res.pitch2D.width = src_width;
    res_desc.res.pitch2D.height = src_height;
    res_desc.res.pitch2D.pitchInBytes = src_pitch;

    hipTextureDesc tex_desc = {};
    tex_desc.filterMode = hipFilterModeLinear;
    tex_desc.readMode = hipReadModeNormalizedFloat;

    hipTextureObject_t tex_y=0;
    HIP_CALL(hipCreateTextureObject(&tex_y, &res_desc, &tex_desc, NULL));

    res_desc.res.pitch2D.desc = hipCreateChannelDesc<YuvUnitx2>();
    res_desc.res.pitch2D.width = src_width >> 1;
    res_desc.res.pitch2D.height = src_height * 3 / 2;

    hipTextureObject_t tex_uv=0;
    HIP_CALL(hipCreateTextureObject(&tex_uv, &res_desc, &tex_desc, NULL));

    ResizeHip<YuvUnitx2> << <dim3((dst_width + 31) / 32, (dst_height + 31) / 32), dim3(16, 16) >> >(tex_y, tex_uv, p_dst, dp_dst_uv,
        dst_pitch, dst_width, dst_height, 1.0f * dst_width / src_width, 1.0f * dst_height / src_height);

    HIP_CALL(hipDestroyTextureObject(tex_y));
    HIP_CALL(hipDestroyTextureObject(tex_uv));
}

void ResizeNv12(unsigned char *p_dst_nv12, int dst_pitch, int dst_width, int dst_height, unsigned char *p_dst_nv12, int src_pitch, int src_width, int src_height, unsigned char* p_dst_nv12_uv)
{
    unsigned char* dp_dst_uv = p_dst_nv12_uv ? p_dst_nv12_uv : p_dst_nv12 + (dst_pitch*dst_height);
    return Resize<uchar2>(p_dst_nv12, dp_dst_uv, dst_pitch, dst_width, dst_height, p_src_nv12, src_pitch, src_width, src_height);
}


void ResizeP016(unsigned char *p_dst_p016, int dst_pitch, int dst_width, int dst_height, unsigned char *p_src_p016, int src_pitch, int src_width, int src_height, unsigned char* p_src_p016_uv)
{
    unsigned char* dp_dst_uv = p_src_p016_uv ? p_src_p016_uv : p_dst_p016 + (dst_pitch*dst_height);
    return Resize<ushort2>(p_dst_p016, dp_dst_uv, dst_pitch, dst_width, dst_height, p_src_p016, src_pitch, src_width, src_height);
}

static __global__ void Scale(hipTextureObject_t tex_src, uint8_t *p_dst, int pitch, int width, 
                            int height, float fx_scale, float fy_scale) {
    int x = blockIdx.x * blockDim.x + threadIdx.x,
        y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height)
    {
        return;
    }

    *(unsigned char*)(p_dst + (y * pitch) + x) = (unsigned char)(fminf((tex2D<float>(tex_src, x * fx_scale, y * fy_scale)) * 255.0f, 255.0f));
}

static __global__ void ScaleUV(hipTextureObject_t tex_src, uint8_t *p_dst, int pitch, int width,
                                int height, float fx_scale, float fy_scale) {
    int x = blockIdx.x * blockDim.x + threadIdx.x,
        y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height)
    {
        return;
    }

    float2 uv = tex2D<float2>(tex_src, x * fx_scale, y * fy_scale);
    uchar2 dst_uv = uchar2{ (unsigned char)(fminf(uv.x * 255.0f, 255.0f)), (unsigned char)(fminf(uv.y * 255.0f, 255.0f)) };

    *(uchar2*)(p_dst + (y * pitch) + 2 * x) = dst_uv;
}


void ResizeHipLaunchKernel(uint8_t *dp_dst, int dst_pitch, int dst_width, int dst_height, uint8_t *dp_src, int src_pitch, 
                                    int src_width, int src_height, bool b_resize_uv = false) {
    hipResourceDesc res_desc = {};
    res_desc.resType = hipResourceTypePitch2D;
    res_desc.res.pitch2D.devPtr = dp_src;
    res_desc.res.pitch2D.desc = b_resize_uv ? hipCreateChannelDesc<uchar2>() : hipCreateChannelDesc<unsigned char>();
    res_desc.res.pitch2D.width = src_width;
    res_desc.res.pitch2D.height = src_height;
    res_desc.res.pitch2D.pitchInBytes = src_pitch;

    hipTextureDesc tex_desc = {};
    tex_desc.filterMode = hipFilterModeLinear;
    tex_desc.readMode = hipReadModeNormalizedFloat;

    tex_desc.addressMode[0] = hipAddressModeClamp;
    tex_desc.addressMode[1] = hipAddressModeClamp;
    tex_desc.addressMode[2] = hipAddressModeClamp;

    hipTextureObject_t tex_src = 0;
    HIP_CALL(hipCreateTextureObject(&tex_src, &res_desc, &tex_desc, NULL));

    dim3 blockSize(16, 16, 1);
    dim3 gridSize(((uint32_t)dst_width + blockSize.x - 1) / blockSize.x, ((uint32_t)dst_height + blockSize.y - 1) / blockSize.y, 1);

    if (bUVPlane)
    {
        Scale_uv << <gridSize, blockSize >> >(tex_src, dp_dst,
            dst_pitch, dst_width, dst_height, 1.0f * src_width / dst_width, 1.0f * src_height / dst_height);
    }
    else
    {
        Scale << <gridSize, blockSize >> >(tex_src, dp_dst,
            dst_pitch, dst_width, dst_height, 1.0f * src_width / dst_width, 1.0f * src_height / dst_height);
    }

    HIP_CALL(hipGetLastError());
    HIP_CALL(hipDestroyTextureObject(tex_src));

}

void ResizeYUV420(uint8_t *p_dst_Y, 
                uint8_t* p_dst_U, 
                uint8_t* p_dst_V, 
                int dst_pitch_Y, 
                int dst_pitch_UV, 
                int dst_width, 
                int dst_height,
                uint8_t *p_src_Y,
                uint8_t* p_src_U,
                uint8_t* p_src_V, 
                int src_pitch_Y,
                int src_pitch_UV,
                int src_width,
                int src_height,
                bool b_nv12) {

    int uv_width_dst = (dst_width + 1) >> 1;
    int uv_height_dst = (dst_width + 1) >> 1;
    int uv_width_src = (src_width + 1) >> 1;
    int uv_height_src = (src_height + 1) >> 1;

    // Scale Y plane
    ResizeHipLaunchKernel(p_dst_Y, dst_pitch_Y, dst_width, dst_height, p_src_Y, src_pitch_Y, src_width, src_height);
    if (b_nv12) {
        ResizeHipLaunchKernel(p_dst_U, dst_pitch_UV, uv_width_dst, uv_height_dst, p_src_U, src_pitch_UV, uv_width_src, uv_height_src, b_nv12);
    } else {
        ResizeHipLaunchKernel(p_dst_U, dst_pitch_UV, uv_width_dst, uv_height_dst, p_src_U, src_pitch_UV, uv_width_src, uv_height_src);
        ResizeHipLaunchKernel(p_dst_V, dst_pitch_UV, uv_width_dst, uv_height_dst, p_src_V, src_pitch_UV, uv_width_src, uv_height_src);
    }
}

