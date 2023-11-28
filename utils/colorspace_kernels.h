/*
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
*/

#pragma once
#include <stdint.h>
#include <hip/hip_runtime.h>

/*!
 * \file
 * \brief The AMD Color Space Standards for VCN Decode Library.
 *
 * \defgroup group_amd_vcn_colorspace colorSpace: AMD VCN Color Space API
 * \brief AMD The vcnDECODE Color Space API.
 */

typedef enum ColorSpaceStandard_ {
    ColorSpaceStandard_BT709 = 1,
    ColorSpaceStandard_Unspecified = 2,
    ColorSpaceStandard_Reserved = 3,
    ColorSpaceStandard_FCC = 4,
    ColorSpaceStandard_BT470 = 5,
    ColorSpaceStandard_BT601 = 6,
    ColorSpaceStandard_SMPTE240M = 7,
    ColorSpaceStandard_YCgCo = 8,
    ColorSpaceStandard_BT2020 = 9,
    ColorSpaceStandard_BT2020C = 10
} ColorSpaceStandard;

union BGR24 {
    uchar3 v;
    struct {
        uint8_t b, g, r;
    } c;
};

union RGB24 {
    uchar3 v;
    struct {
        uint8_t r, g, b;
    } c;
};

union BGR48 {
    ushort3 v;
    struct {
        uint16_t b, g, r;
    } c;
};

union RGB48 {
    ushort3 v;
    struct {
        uint16_t r, g, b;
    } c;
};

union BGRA32 {
    uint32_t d;
    uchar4 v;
    struct {
        uint8_t b, g, r, a;
    } c;
};

union RGBA32 {
    uint32_t d;
    uchar4 v;
    struct {
        uint8_t r, g, b, a;
    } c;
};

union BGRA64 {
    uint64_t d;
    ushort4 v;
    struct {
        uint16_t b, g, r, a;
    } c;
};

union RGBA64 {
    uint64_t d;
    ushort4 v;
    struct {
        uint16_t r, g, b, a;
    } c;
};

// color-convert hip kernel function definitions
template <class COLOR32>
void YUV444ToColor32(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR64>
void YUV444ToColor64(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR24>
void YUV444ToColor24(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR48>
void YUV444ToColor48(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);

template <class COLOR24>
void Nv12ToColor24(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR32>
void Nv12ToColor32(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR48>
void Nv12ToColor48(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR64>
void Nv12ToColor64(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR24>
void YUV444P16ToColor24(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR48>
void YUV444P16ToColor48(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR32>
void YUV444P16ToColor32(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR64>
void YUV444P16ToColor64(uint8_t *dpYUV444, int nPitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR32>
void P016ToColor32(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR64>
void P016ToColor64(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR24>
void P016ToColor24(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);
template <class COLOR48>
void P016ToColor48(uint8_t *dpP016, int nP016Pitch, uint8_t *dpBgr, int nBgrPitch, int nWidth, int nHeight, int nVPitch, int colStandard);

