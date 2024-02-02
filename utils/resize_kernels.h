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
#include <stdint.h>
#include <hip/hip_runtime.h>


/**
 * @brief Function to resize 420 YUV image
 * 
 * @param p_dst_Y  - Destination Y plane pointer
 * @param p_dst_U  - Destination U plane pointer
 * @param p_dst_V  - Destination V plane pointer
 * @param dst_pitch_Y   - Destination Pitch Y
 * @param dst_pitch_UV  - Destination Pitch UV
 * @param dst_width     
 * @param dst_height    
 * @param p_src_Y       
 * @param p_src_U       
 * @param p_src_V 
 * @param dst_pitch_Y 
 * @param dst_pitch_UV 
 * @param src_width 
 * @param dst_width 
 * @param b_nv12 
 */
void ResizeYUV420(uint8_t *p_dst_Y, uint8_t* p_dst_U, uint8_t* p_dst_V, int dst_pitch_Y, int dst_pitch_UV, int dst_width, int dst_height,
                uint8_t *p_src_Y,
                uint8_t* p_src_U,
                uint8_t* p_src_V, 
                int dst_pitch_Y,
                int dst_pitch_UV,
                int src_width,
                int dst_width,
                bool b_nv12);

/**
 * @brief The function to launch ResizeYUV HIP kernel
 * 
 * @param dp_dst    - dest pointer
 * @param dst_pitch - Pitch of the dst plane
 * @param dst_width - Width of the dst plane
 * @param dst_height - Height of the dst plane
 * @param dp_src     - source pointer
 * @param src_pitch  - source pitch
 * @param src_width  - source width
 * @param src_height - source height
 * @param b_resize_uv - to resize UV plance or not
 */
void ResizeYUVHipKernel(uint8_t *dp_dst, int dst_pitch, int dst_width, int dst_height, uint8_t *dp_src, int src_pitch, 
                                    int src_width, int src_height, bool b_resize_uv = false);
