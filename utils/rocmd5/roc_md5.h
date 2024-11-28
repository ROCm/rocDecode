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

extern "C" {
#include "libavutil/md5.h"
#include "libavutil/mem.h"
}
#include "roc_video_dec.h"

/*!
 * \file
 * \brief The MD5 message digest generation utility.
 */

class MD5Generator {
public:
    MD5Generator() {};
    ~MD5Generator() {};

    /*! \brief Function to start MD5 calculation
     */
    void InitMd5();

    /*! \brief Function to update MD5 digest for a device data buffer
     *  \param [in] data_buf Pointer to the data buffer
     *  \param [in] buf_size Buffer info
     */
    void UpdateMd5ForDataBuffer(void *data_buf, int buf_size);

    /*! \brief Function to update MD5 digest for a decoded frame
    *  \param [in] surf_mem Pointer to surface memory
    *  \param [in] surf_info Surface info
    */
    void UpdateMd5ForFrame(void *surf_mem, OutputSurfaceInfo *surf_info);

    /*! \brief Function to complete MD5 calculation
     *  \param [out] digest Pointer to the 16 byte message digest
     */
    void FinalizeMd5(uint8_t **digest);

private:
    struct AVMD5 *md5_ctx_;
    uint8_t md5_digest_[16];
};