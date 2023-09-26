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

#ifndef ROCDECAPI
#if defined(_WIN32)
#define ROCDECAPI __stdcall       // for future: only linux is supported in this version
#else
#define ROCDECAPI
#endif
#endif

#pragma once
#include "hip/hip_runtime.h"

/*****************************************************************************************************/
//! \file rocdecode.h
//! rocDecode API provides video decoding interface to AMD GPU devices.
//! This file contains constants, structure definitions and function prototypes used for decoding.
/*****************************************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

/*********************************************************************************/
//! HANDLE pf rocDecDecoder
//! Used in subsequent API calls after rocDecCreateDecoder
/*********************************************************************************/

typedef void *rocDecDecoderHandle;


/*********************************************************************************/
//! \enum rocDecStatus
//! rocDecoder return status enums
//! These enums are used in all API calls to rocDecoder
/*********************************************************************************/

typedef enum rocDecStatus_enum{
    ROCDEC_DEVICE_INVALID = -1,
    ROCDEC_CONTEXT_INVALID = -2,
    ROCDEC_RUNTIME_ERROR  = -3,
    ROCDEC_OUTOF_MEMORY = -4,
    ROCDEC_INVALID_PARAMETER = -5,
    ROCDEC_NOT_IMPLEMENTED = -6,
    ROCDEC_NOT_INITIALIZED = -7,
    ROCDEC_NOT_SUPPORTED = -8,
    ROCDEC_SUCCESS = 0,
}rocDecStatus;

/*********************************************************************************/
//! \enum rocDecodeVideoCodec
//! Video codec enums
//! These enums are used in ROCDECODECREATEINFO and ROCDECODEVIDDECODECAPS structures
/*********************************************************************************/
typedef enum rocDecVideoCodec_enum {
    rocDecVideoCodec_MPEG1=0,                                         /**<  MPEG1             */
    rocDecVideoCodec_MPEG2,                                           /**<  MPEG2             */
    rocDecVideoCodec_MPEG4,                                           /**<  MPEG4             */
    rocDecVideoCodec_H264,                                            /**<  H264              */
    rocDecVideoCodec_HEVC,                                            /**<  HEVC              */
    rocDecVideoCodec_AV1,                                             /**<  AV1               */
    rocDecVideoCodec_VP8,                                             /**<  VP8               */
    rocDecVideoCodec_VP9,                                             /**<  VP9               */
    rocDecVideoCodec_JPEG,                                            /**<  JPEG              */
    rocDecVideoCodec_NumCodecs,                                       /**<  Max codecs        */
    // Uncompressed YUV
    rocDecVideoCodec_YUV420 = (('I'<<24)|('Y'<<16)|('U'<<8)|('V')),   /**< Y,U,V (4:2:0)      */
    rocDecVideoCodec_YV12   = (('Y'<<24)|('V'<<16)|('1'<<8)|('2')),   /**< Y,V,U (4:2:0)      */
    rocDecVideoCodec_NV12   = (('N'<<24)|('V'<<16)|('1'<<8)|('2')),   /**< Y,UV  (4:2:0)      */
    rocDecVideoCodec_YUYV   = (('Y'<<24)|('U'<<16)|('Y'<<8)|('V')),   /**< YUYV/YUY2 (4:2:2)  */
    rocDecVideoCodec_UYVY   = (('U'<<24)|('Y'<<16)|('V'<<8)|('Y'))    /**< UYVY (4:2:2)       */
} rocDecVideoCodec;

/*********************************************************************************/
//! \enum rocDecVideoSurfaceFormat
//! Video surface format enums used for output format of decoded output
//! These enums are used in RocdecDecoderCreateInfo structure
/*********************************************************************************/
typedef enum rocDecVideoSurfaceFormat_enum {
    rocDecVideoSurfaceFormat_NV12=0,          /**< Semi-Planar YUV [Y plane followed by interleaved UV plane]     */
    rocDecVideoSurfaceFormat_P016=1,          /**< 16 bit Semi-Planar YUV [Y plane followed by interleaved UV plane].
                                                 Can be used for 10 bit(6LSB bits 0), 12 bit (4LSB bits 0)      */
    rocDecVideoSurfaceFormat_YUV444=2,        /**< Planar YUV [Y plane followed by U and V planes]                */
    rocDecVideoSurfaceFormat_YUV444_16Bit=3,  /**< 16 bit Planar YUV [Y plane followed by U and V planes]. 
                                                 Can be used for 10 bit(6LSB bits 0), 12 bit (4LSB bits 0)      */
} rocDecVideoSurfaceFormat;

/**************************************************************************************************************/
//! \enum rocDecVideoChromaFormat
//! Chroma format enums
//! These enums are used in ROCDCODECREATEINFO and RocdecDecodeCaps structures
/**************************************************************************************************************/
typedef enum rocDecVideoChromaFormat_enum {
    rocDecVideoChromaFormat_Monochrome=0,  /**< MonoChrome */
    rocDecVideoChromaFormat_420,           /**< YUV 4:2:0  */
    rocDecVideoChromaFormat_422,           /**< YUV 4:2:2  */
    rocDecVideoChromaFormat_444            /**< YUV 4:4:4  */
} rocDecVideoChromaFormat;

/*************************************************************************/
//! \enum rocDecDecodeStatus
//! Decode status enums
//! These enums are used in RocdecGetDecodeStatus structure
/*************************************************************************/
typedef enum rocDecodeStatus_enum {
    rocDecodeStatus_Invalid         = 0,   // Decode status is not valid
    rocDecodeStatus_InProgress      = 1,   // Decode is in progress
    rocDecodeStatus_Success         = 2,   // Decode is completed without any errors
    // 3 to 7 enums are reserved for future use
    rocDecodeStatus_Error           = 8,   // Decode is completed with an error (error is not concealed)
    rocDecodeStatus_Error_Concealed = 9,   // Decode is completed with an error and error is concealed 
} rocDecDecodeStatus;

/**************************************************************************************************************/
//! \struct RocdecDecodeCaps;
//! This structure is used in rocDecGetDecoderCaps API
/**************************************************************************************************************/
typedef struct _RocdecDecodeCaps {
    rocDecVideoCodec          eCodecType;                 /**< IN: rocDecVideoCodec_XXX                                             */
    rocDecVideoChromaFormat   eChromaFormat;              /**< IN: rocDecVideoChromaFormat_XXX                                      */
    uint32_t              nBitDepthMinus8;            /**< IN: The Value "BitDepth minus 8"                                   */
    uint32_t              reserved1[3];               /**< Reserved for future use - set to zero                              */

    uint8_t             bIsSupported;               /**< OUT: 1 if codec supported, 0 if not supported                      */
    uint8_t             nNumDecoders;                 /**< OUT: Number of Decoders that can support IN params                   */
    uint16_t            nOutputFormatMask;          /**< OUT: each bit represents corresponding rocDecVideoSurfaceFormat enum */
    uint32_t              nMaxWidth;                  /**< OUT: Max supported coded width in pixels                           */
    uint32_t              nMaxHeight;                 /**< OUT: Max supported coded height in pixels                          */
    uint16_t            nMinWidth;                  /**< OUT: Min supported coded width in pixels                           */
    uint16_t            nMinHeight;                 /**< OUT: Min supported coded height in pixels                          */
    uint32_t              reserved2[6];              /**< Reserved for future use - set to zero                              */
} RocdecDecodeCaps;

/**************************************************************************************************************/
//! \struct RocdecDecoderCreateInfo
//! This structure is used in rocDecCreateDecoder API
/**************************************************************************************************************/
typedef struct _RocdecDecoderCreateInfo {
    uint32_t ulWidth;                /**< IN: Coded sequence width in pixels                                             */
    uint32_t ulHeight;               /**< IN: Coded sequence height in pixels                                            */
    uint32_t ulNumDecodeSurfaces;    /**< IN: Maximum number of internal decode surfaces                                 */
    rocDecVideoCodec CodecType;           /**< IN: rocDecVideoCodec_XXX                                                         */
    rocDecVideoChromaFormat ChromaFormat; /**< IN: rocDecVideoChromaFormat_XXX                                                  */
    uint32_t bitDepthMinus8;         /**< IN: The value "BitDepth minus 8"                                               */
    uint32_t ulIntraDecodeOnly;      /**< IN: Set 1 only if video has all intra frames (default value is 0). This will
                                             optimize video memory for Intra frames only decoding. The support is limited
                                             to specific codecs - H264, HEVC, VP9, the flag will be ignored for codecs which
                                             are not supported. However decoding might fail if the flag is enabled in case
                                             of supported codecs for regular bit streams having P and/or B frames.          */
    uint32_t ulMaxWidth;             /**< IN: Coded sequence max width in pixels used with reconfigure Decoder           */
    uint32_t ulMaxHeight;            /**< IN: Coded sequence max height in pixels used with reconfigure Decoder          */                                           
    /**
    * IN: area of the frame that should be copied
    */
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } roi_area;

    rocDecVideoSurfaceFormat OutputFormat;       /**< IN: rocDecVideoSurfaceFormat_XXX                                     */
    uint32_t ulTargetWidth;               /**< IN: Post-processed output width (Should be aligned to 2)           */
    uint32_t ulTargetHeight;              /**< IN: Post-processed output height (Should be aligned to 2)          */
    uint32_t ulNumOutputSurfaces;         /**< IN: Maximum number of output surfaces simultaneously mapped        */

    /**
    * IN: target rectangle in the output frame (for aspect ratio conversion)
    * if a null rectangle is specified, {0,0,ulTargetWidth,ulTargetHeight} will be used
    */
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } target_rect;

    uint32_t enableHistogram;             /**< IN: enable histogram output, if supported */
    uint32_t Reserved2[4];                /**< Reserved for future use - set to zero */
} RocdecDecoderCreateInfo;

/*********************************************************************************************************/
//! \struct RocdecDecodeStatus
//! Struct for reporting decode status.
//! This structure is used in RocdecGetDecodeStatus API.
/*********************************************************************************************************/
typedef struct _RocdecDecodeStatus {
    rocDecDecodeStatus decodeStatus;
    uint32_t reserved[31];
    void *pReserved[8];
} RocdecDecodeStatus;

/****************************************************/
//! \struct RocdecReconfigureDecoderInfo
//! Struct for decoder reset
//! This structure is used in rocDecReconfigureDecoder() API
/****************************************************/
typedef struct _RocdecReconfigureDecoderInfo {
    uint32_t ulWidth;             /**< IN: Coded sequence width in pixels, MUST be < = ulMaxWidth defined at RocdecDecoderCreateInfo  */
    uint32_t ulHeight;            /**< IN: Coded sequence height in pixels, MUST be < = ulMaxHeight defined at RocdecDecoderCreateInfo  */
    uint32_t ulTargetWidth;       /**< IN: Post processed output width */
    uint32_t ulTargetHeight;      /**< IN: Post Processed output height */
    uint32_t ulNumDecodeSurfaces; /**< IN: Maximum number of internal decode surfaces */
    uint32_t reserved1[12];       /**< Reserved for future use. Set to Zero */
    /**
    * IN: Area of frame to be displayed. Use-case : Source Cropping
    */
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } roi_area;
    /**
    * IN: Target Rectangle in the OutputFrame. Use-case : Aspect ratio Conversion
    */
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } target_rect;
    uint32_t reserved2[11]; /**< Reserved for future use. Set to Zero */
} RocdecReconfigureDecoderInfo; 

/*********************************************************/
//! \struct RocdecH264Picture
//! H.264 Picture Entry
//! This structure is used in RocdecH264PicParams structure
/*********************************************************/
typedef struct _RocdecH264Picture {
    int PicIdx;                 /**< picture index of reference frame    */
    int FrameIdx;               /**< frame_num(int16_t-term) or LongTermFrameIdx(long-term)  */
    uint32_t RefFlags;      /**< See below for definitions  */
    int TopFieldOrderCnt;       /**< field order count of top field  */
    int BottomFieldOrderCnt;    /**< field order count of bottom field   */
} RocdecH264Picture;

/* flags in RocdecH264Picture could be OR of the following */
#define RocdecH264Picture_FLAGS_INVALID			          0x00000001
#define RocdecH264Picture_FLAGS_TOP_FIELD		          0x00000002
#define RocdecH264Picture_FLAGS_BOTTOM_FIELD		      0x00000004
#define RocdecH264Picture_FLAGS_SHORT_TERM_REFERENCE	0x00000008
#define RocdecH264Picture_FLAGS_LONG_TERM_REFERENCE	  0x00000010
#define RocdecH264Picture_FLAGS_NON_EXISTING		      0x00000020

/*********************************************************/
//! \struct RocdecHEVCPicture
//! HEVC Picture Entry
//! This structure is used in RocdecHevcPicParams structure
/*********************************************************/
typedef struct _RocdecHEVCPicture {
    int PicIdx;                 /**< reconstructed picture surface ID    */
    /** \brief picture order count.
     * in HEVC, POCs for top and bottom fields of same picture should
     * take different values.
     */
    int POC;
    uint32_t Flags;              /**< See below for definitions  */
    uint32_t Reserved[4];        /**< reserved for future; must be zero  */
} RocdecHEVCPicture;

/* flags in RocdecHEVCPicture could be OR of the following */
#define RocdecHEVCPicture_INVALID                 0x00000001
/** \brief indication of interlace scan picture.
 * should take same value for all the pictures in sequence.
 */
#define RocdecHEVCPicture_FIELD_PIC               0x00000002
/** \brief polarity of the field picture.
 * top field takes even lines of buffer surface.
 * bottom field takes odd lines of buffer surface.
 */
#define RocdecHEVCPicture_BOTTOM_FIELD            0x00000004
/** \brief Long term reference picture */
#define RocdecHEVCPicture_LONG_TERM_REFERENCE     0x00000008
/**
 * RocdecHEVCPicture_ST_CURR_BEFORE, RocdecHEVCPicture_RPS_ST_CURR_AFTER
 * and RocdecHEVCPicture_RPS_LT_CURR of any picture in ReferenceFrames[] should
 * be exclusive. No more than one of them can be set for any picture.
 * Sum of NumPocStCurrBefore, NumPocStCurrAfter and NumPocLtCurr
 * equals NumPocTotalCurr, which should be equal to or smaller than 8.
 * Application should provide valid values for both int16_t format and long format.
 * The pictures in DPB with any of these three flags turned on are referred by
 * the current picture.
 */
/** \brief RefPicSetStCurrBefore of HEVC spec variable
 * Number of ReferenceFrames[] entries with this bit set equals
 * NumPocStCurrBefore.
 */
#define RocdecHEVCPicture_RPS_ST_CURR_BEFORE      0x00000010
/** \brief RefPicSetStCurrAfter of HEVC spec variable
 * Number of ReferenceFrames[] entries with this bit set equals
 * NumPocStCurrAfter.
 */
#define RocdecHEVCPicture_RPS_ST_CURR_AFTER       0x00000020
/** \brief RefPicSetLtCurr of HEVC spec variable
 * Number of ReferenceFrames[] entries with this bit set equals
 * NumPocLtCurr.
 */
#define RocdecHEVCPicture_RPS_LT_CURR             0x00000040

/***********************************************************/
//! \struct RocdecJPEGPicParams placeholder
//! JPEG picture parameters
//! This structure is used in RocdecPicParams structure
/***********************************************************/
typedef struct _RocdecJPEGPicParams {
    int Reserved;
} RocdecJPEGPicParams;

/***********************************************************/
//! \struct RocdecMpeg2QMatrix
//! MPEG2 QMatrix
//! This structure is used in _RocdecMpeg2PicParams structure
/***********************************************************/
typedef struct _RocdecMpeg2QMatrix {
    int32_t load_intra_quantiser_matrix;
    int32_t load_non_intra_quantiser_matrix;
    int32_t load_chroma_intra_quantiser_matrix;
    int32_t load_chroma_non_intra_quantiser_matrix;
    uint8_t intra_quantiser_matrix[64];
    uint8_t non_intra_quantiser_matrix[64];
    uint8_t chroma_intra_quantiser_matrix[64];
    uint8_t chroma_non_intra_quantiser_matrix[64];
} RocdecMpeg2QMatrix;


/***********************************************************/
//! \struct RocdecMpeg2PicParams
//! MPEG2 picture parameters
//! This structure is used in RocdecMpeg2PicParams structure
/***********************************************************/
typedef struct _RocdecMpeg2PicParams {
    uint16_t horizontal_size;
    uint16_t vertical_size;
    uint32_t forward_reference_pic;       // surface_id for forward reference
    uint32_t backward_reference_picture;       // surface_id for backward reference
    /* meanings of the following fields are the same as in the standard */
    int32_t picture_coding_type;
    int32_t f_code; /* pack all four fcode into this */
    union {
        struct {
            uint32_t intra_dc_precision     : 2;
            uint32_t picture_structure      : 2;
            uint32_t top_field_first        : 1;
            uint32_t frame_pred_frame_dct       : 1;
            uint32_t concealment_motion_vectors : 1;
            uint32_t q_scale_type           : 1;
            uint32_t intra_vlc_format       : 1;
            uint32_t alternate_scan         : 1;
            uint32_t repeat_first_field     : 1;
            uint32_t progressive_frame      : 1;
            uint32_t is_first_field         : 1; // indicate whether the current field is the first field for field picture
        } bits;
        uint32_t value;
    } picture_coding_extension;

    RocdecMpeg2QMatrix q_matrix;
    uint32_t  Reserved[4];
} RocdecMpeg2PicParams;

/***********************************************************/
//! \struct RocdecH264PicParams placeholder
//! H.264 picture parameters
//! This structure is used in RocdecH264PicParams structure
//! This structure is configured similar to VA-API VAPictureParameterBufferH264 structure
/***********************************************************/
typedef struct _RocdecH264PicParams {
    RocdecH264Picture cur_pic;
    RocdecH264Picture dpb[16];	/* in DPB */
    uint16_t picture_width_in_mbs_minus1;
    uint16_t picture_height_in_mbs_minus1;
    uint8_t bit_depth_luma_minus8;
    uint8_t bit_depth_chroma_minus8;
    uint8_t num_ref_frames;
    union {
        struct {
            uint32_t chroma_format_idc			: 2;
            uint32_t residual_colour_transform_flag		: 1;
            uint32_t gaps_in_frame_num_value_allowed_flag	: 1;
            uint32_t frame_mbs_only_flag			: 1;
            uint32_t mb_adaptive_frame_field_flag		: 1; 
            uint32_t direct_8x8_inference_flag		: 1;
            uint32_t MinLumaBiPredSize8x8			: 1; /* see A.3.3.2 */
            uint32_t log2_max_frame_num_minus4		: 4;
            uint32_t pic_order_cnt_type			: 2;
            uint32_t log2_max_pic_order_cnt_lsb_minus4	: 4;
            uint32_t delta_pic_order_always_zero_flag	: 1;
        } bits;
        uint32_t value;
    } sps_fields;
    union {
        struct {
            uint32_t entropy_coding_mode_flag	: 1;
            uint32_t weighted_pred_flag		: 1;
            uint32_t weighted_bipred_idc		: 2;
            uint32_t transform_8x8_mode_flag	: 1;
            uint32_t field_pic_flag			: 1;
            uint32_t constrained_intra_pred_flag	: 1;
            uint32_t pic_order_present_flag			: 1;
            uint32_t deblocking_filter_control_present_flag : 1;
            uint32_t redundant_pic_cnt_present_flag		: 1;
            uint32_t reference_pic_flag			: 1; /* nal_ref_idc != 0 */
        } bits;
        uint32_t value;
    } pps_fields;

    // FMO/ASO
    uint8_t num_slice_groups_minus1;
    uint8_t slice_group_map_type;
    uint16_t slice_group_change_rate_minus1;
    int8_t pic_init_qp_minus26;
    int8_t pic_init_qs_minus26;
    int8_t chroma_qp_index_offset;
    int8_t second_chroma_qp_index_offset;

    uint16_t frame_num;
    uint8_t num_ref_idx_l0_default_active_minus1;
    uint8_t num_ref_idx_l1_default_active_minus1;

    // Quantization Matrices (raster-order)
    uint8_t scaling_list_4x4[6][16];
    uint8_t scaling_list_8x8[2][64];
    // SVC/MVC : Not supported in this version
    // union
    // {
    //     ROCDECH264MVCEXT mvcext;
    //     ROCDECH264SVCEXT svcext;
    // };
    uint32_t  Reserved[12];
} RocdecH264PicParams;


/***********************************************************/
//! \struct RocdecHevcQMatrix
//! HEVC QMatrix
//! This structure is sent once per frame,
//! and only when scaling_list_enabled_flag = 1.
//! When sps_scaling_list_data_present_flag = 0, app still
//! needs to send in this structure with default matrix values.
//! This structure is used in RocdecHevcQMatrix structure
/***********************************************************/
typedef struct _RocdecHevcQMatrix {
    /**
     * \brief 4x4 scaling,
     * correspongs i = 0, MatrixID is in the range of 0 to 5,
     * inclusive. And j is in the range of 0 to 15, inclusive.
     */
    uint8_t                 ScalingList4x4[6][16];
    /**
     * \brief 8x8 scaling,
     * correspongs i = 1, MatrixID is in the range of 0 to 5,
     * inclusive. And j is in the range of 0 to 63, inclusive.
     */
    uint8_t                 ScalingList8x8[6][64];
    /**
     * \brief 16x16 scaling,
     * correspongs i = 2, MatrixID is in the range of 0 to 5,
     * inclusive. And j is in the range of 0 to 63, inclusive.
     */
    uint8_t                 ScalingList16x16[6][64];
    /**
     * \brief 32x32 scaling,
     * correspongs i = 3, MatrixID is in the range of 0 to 1,
     * inclusive. And j is in the range of 0 to 63, inclusive.
     */
    uint8_t                 ScalingList32x32[2][64];
    /**
     * \brief DC values of the 16x16 scaling lists,
     * corresponds to HEVC spec syntax
     * scaling_list_dc_coef_minus8[ sizeID - 2 ][ matrixID ] + 8
     * with sizeID = 2 and matrixID in the range of 0 to 5, inclusive.
     */
    uint8_t                 ScalingListDC16x16[6];
    /**
     * \brief DC values of the 32x32 scaling lists,
     * corresponds to HEVC spec syntax
     * scaling_list_dc_coef_minus8[ sizeID - 2 ][ matrixID ] + 8
     * with sizeID = 3 and matrixID in the range of 0 to 1, inclusive.
     */
    uint8_t                 ScalingListDC32x32[2];

} RocdecHevcQMatrix;

/***********************************************************/
//! \struct RocdecHevcPicParams
//! HEVC picture parameters
//! This structure is used in RocdecHevcPicParams structure
/***********************************************************/
typedef struct _RocdecHevcPicParams {
    RocdecHEVCPicture cur_pic;
    RocdecHEVCPicture dpb[15];	/* in DPB */
    uint16_t picture_width_in_luma_samples;
    uint16_t picture_height_in_luma_samples;
    union {
        struct {
            /** following flags have same syntax and semantic as those in HEVC spec */
            uint32_t        chroma_format_idc                           : 2;
            uint32_t        separate_colour_plane_flag                  : 1;
            uint32_t        pcm_enabled_flag                            : 1;
            uint32_t        scaling_list_enabled_flag                   : 1;
            uint32_t        transform_skip_enabled_flag                 : 1;
            uint32_t        amp_enabled_flag                            : 1;
            uint32_t        strong_intra_smoothing_enabled_flag         : 1;
            uint32_t        sign_data_hiding_enabled_flag               : 1;
            uint32_t        constrained_intra_pred_flag                 : 1;
            uint32_t        cu_qp_delta_enabled_flag                    : 1;
            uint32_t        weighted_pred_flag                          : 1;
            uint32_t        weighted_bipred_flag                        : 1;
            uint32_t        transquant_bypass_enabled_flag              : 1;
            uint32_t        tiles_enabled_flag                          : 1;
            uint32_t        entropy_coding_sync_enabled_flag            : 1;
            uint32_t        pps_loop_filter_across_slices_enabled_flag  : 1;
            uint32_t        loop_filter_across_tiles_enabled_flag       : 1;
            uint32_t        pcm_loop_filter_disabled_flag               : 1;
            /** set based on sps_max_num_reorder_pics of current temporal layer. */
            uint32_t        NoPicReorderingFlag                         : 1;
            /** picture has no B slices */
            uint32_t        NoBiPredFlag                                : 1;

            uint32_t        ReservedBits                                : 11;
        } bits;
        uint32_t            value;
    } pps_fields;

    /** SPS fields: the following parameters have same syntax with those in HEVC spec */
    uint8_t                 sps_max_dec_pic_buffering_minus1;       /**< IN: DPB size for current temporal layer */
    uint8_t                 bit_depth_luma_minus8;
    uint8_t                 bit_depth_chroma_minus8;
    uint8_t                 pcm_sample_bit_depth_luma_minus1;
    uint8_t                 pcm_sample_bit_depth_chroma_minus1;
    uint8_t                 log2_min_luma_coding_block_size_minus3;
    uint8_t                 log2_diff_max_min_luma_coding_block_size;
    uint8_t                 log2_min_transform_block_size_minus2;
    uint8_t                 log2_diff_max_min_transform_block_size;
    uint8_t                 log2_min_pcm_luma_coding_block_size_minus3;
    uint8_t                 log2_diff_max_min_pcm_luma_coding_block_size;
    uint8_t                 max_transform_hierarchy_depth_intra;
    uint8_t                 max_transform_hierarchy_depth_inter;
    int8_t                  init_qp_minus26;
    uint8_t                 diff_cu_qp_delta_depth;
    int8_t                  pps_cb_qp_offset;
    int8_t                  pps_cr_qp_offset;
    uint8_t                 log2_parallel_merge_level_minus2;
    uint8_t                 num_tile_columns_minus1;
    uint8_t                 num_tile_rows_minus1;
    /**
     * when uniform_spacing_flag equals 1, application should populate
     * column_width_minus[], and row_height_minus1[] with approperiate values.
     */
    uint16_t                column_width_minus1[19];
    uint16_t                row_height_minus1[21];

    uint32_t                slice_parsing_fields;       /**< IN: Needed only for Short Slice Format */

    /** following parameters have same syntax with those in HEVC spec */
    uint8_t                 log2_max_pic_order_cnt_lsb_minus4;
    uint8_t                 num_int16_t_term_ref_pic_sets;
    uint8_t                 num_long_term_ref_pic_sps;
    uint8_t                 num_ref_idx_l0_default_active_minus1;
    uint8_t                 num_ref_idx_l1_default_active_minus1;
    int8_t                  pps_beta_offset_div2;
    int8_t                  pps_tc_offset_div2;
    uint8_t                 num_extra_slice_header_bits;
    /**
     * \brief number of bits that structure
     * int16_t_term_ref_pic_set( num_int16_t_term_ref_pic_sets ) takes in slice
     * segment header when int16_t_term_ref_pic_set_sps_flag equals 0.
     * if int16_t_term_ref_pic_set_sps_flag equals 1, the value should be 0.
     * the bit count is calculated after emulation prevention bytes are removed
     * from bit streams.
     * This variable is used for accelorater to skip parsing the
     * int16_t_term_ref_pic_set( num_int16_t_term_ref_pic_sets ) structure.
     */
    uint32_t                st_rps_bits;

    RocdecHevcQMatrix      q_matrix;
    uint32_t                Reserved[16];
} RocdecHevcPicParams;

/***********************************************************/
//! \struct RocdecVc1PicParams placeholder
//! JPEG picture parameters
//! This structure is used in RocdecVc1PicParams structure
/***********************************************************/
typedef struct _RocdecVc1PicParams {
    int Reserved;
} RocdecVc1PicParams;


/******************************************************************************************/
//! \struct _RocdecPicParams
//! Picture parameters for decoding
//! This structure is used in rocDecDecodePicture API
//! IN  for rocDecDecodePicture
/******************************************************************************************/
typedef struct _RocdecPicParams {
    int PicWidth;                         /**< IN: Coded frame width                                        */
    int PicHeight;                        /**< IN: Coded frame height                                       */
    int CurrPicIdx;                        /**< IN: Output index of the current picture                       */
    int field_pic_flag;                    /**< IN: 0=frame picture, 1=field picture                          */
    int bottom_field_flag;                 /**< IN: 0=top field, 1=bottom field (ignored if field_pic_flag=0) */
    int second_field;                      /**< IN: Second field of a complementary field pair                */
    // Bitstream data
    uint32_t nBitstreamDataLen;        /**< IN: Number of bytes in bitstream data buffer                  */
    const uint8_t *pBitstreamData;   /**< IN: Ptr to bitstream data for this picture (slice-layer)      */
    uint32_t nNumSlices;               /**< IN: Number of slices in this picture                          */
    const uint32_t *pSliceDataOffsets; /**< IN: nNumSlices entries, contains offset of each slice within 
                                                        the bitstream data buffer                             */
    int ref_pic_flag;                      /**< IN: This picture is a reference picture                       */
    int intra_pic_flag;                    /**< IN: This picture is entirely intra coded                      */
    uint32_t Reserved[30];             /**< Reserved for future use                                       */
    // IN: Codec-specific data
    union {
        RocdecMpeg2PicParams mpeg2;         /**< Also used for MPEG-1 */
        RocdecH264PicParams  h264;
        RocdecHevcPicParams  hevc;
        RocdecVc1PicParams   vc1;
        RocdecJPEGPicParams  jpeg;
        uint32_t CodecReserved[256];
    } CodecSpecific;

} RocdecPicParams;

/******************************************************/
//! \struct RocdecProcParams
//! Picture parameters for postprocessing
//! This structure is used in rocDecMapVideoFrame API
/******************************************************/
typedef struct _RocdecProcParams
{
    int progressive_frame;                        /**< IN: Input is progressive (deinterlace_mode will be ignored)                */
    int top_field_first;                          /**< IN: Input frame is top field first (1st field is top, 2nd field is bottom) */
    uint32_t reserved_flags[2];                  /**< Reserved for future use (set to zero)                                      */

    // The fields below are used for raw YUV input
    uint64_t raw_input_dptr;                  /**< IN: Input HIP device ptr for raw YUV extensions                               */
    uint32_t raw_input_pitch;                 /**< IN: pitch in bytes of raw YUV input (should be aligned appropriately)      */
    uint32_t raw_input_format;                /**< IN: Input YUV format (rocDecVideoCodec_enum)                                 */
    uint64_t raw_output_dptr;                 /**< IN: Output HIP device mem ptr for raw YUV extensions                              */
    uint32_t raw_output_pitch;                /**< IN: pitch in bytes of raw YUV output (should be aligned appropriately)     */
    uint32_t raw_output_format;               /**< IN: Output YUV format (rocDecVideoCodec_enum)                                 */
    hipStream_t output_hstream;               /**< IN: stream object used by rocDecMapVideoFrame                               */
    uint32_t Reserved[16];                    /**< Reserved for future use (set to zero)                                      */
} RocdecProcParams;


/***********************************************************************************************************/
//! ROCVIDEO_DECODER
//!
//! In order to minimize decode latencies, there should be always at least enough pictures (min 2) in the decode
//! queue at any time, in order to make sure that all VCN decode engines are always busy.
//!
//! Overall data flow:
//!  - rocdecGetDecoderCaps(...)
//!  - rocdecCreateDecoder(...)
//!  - For each picture:
//!    + rocdecDecodePicture(N)        /* N is determined based on available HW decode engines in the system */
//!    + rocdecMapVideoFrame(N-4)
//!    + do some processing in HIP
//!    + rocdecUnmapVideoFrame(N-4)
//!    + rocdecDecodePicture(N+1)
//!    + rocdecMapVideoFrame(N-3)
//!    + ...
//!  - rocdecDestroyDecoder(...)
//!
//! NOTE:
//! - There is a limit to how many pictures can be mapped simultaneously (ulNumOutputSurfaces)
//! - rocdecDecodePicture may block the calling thread if there are too many pictures pending
//!   in the decode queue
/***********************************************************************************************************/

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecCreateDecoder(rocDecDecoderHandle *phDecoder, RocdecDecoderCreateInfo *pdci)
//! Create the decoder object based on pdci. A handle to the created decoder is returned
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecCreateDecoder(rocDecDecoderHandle *phDecoder, RocdecDecoderCreateInfo *pdci);

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDestroyDecoder(rocDecDecoderHandle hDecoder)
//! Destroy the decoder object
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecDestroyDecoder(rocDecDecoderHandle hDecoder);

/**********************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocdecGetDecoderCaps(RocdecDecodeCaps *pdc)
//! Queries decode capabilities of AMD's VCN decoder based on CodecType, ChromaFormat and BitDepthMinus8 parameters.
//! 1. Application fills IN parameters CodecType, ChromaFormat and BitDepthMinus8 of RocdecDecodeCaps structure
//! 2. On calling rocdecGetDecoderCaps, driver fills OUT parameters if the IN parameters are supported
//!    If IN parameters passed to the driver are not supported by AMD-VCN-HW, then all OUT params are set to 0.
/**********************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecGetDecoderCaps(rocDecDecoderHandle hDecoder, RocdecDecodeCaps *pdc);

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDecodeFrame(rocDecDecoderHandle hDecoder, RocdecPicParams *pPicParams)
//! Decodes a single picture
//! Submits the frame for HW decoding 
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecDecodeFrame(rocDecDecoderHandle hDecoder, RocdecPicParams *pPicParams);

/************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, RocdecDecodeStatus* pDecodeStatus);
//! Get the decode status for frame corresponding to nPicIdx
//! API is currently supported for HEVC, H264 and JPEG codecs.
//! API returns ROCDEC_NOT_SUPPORTED error code for unsupported GPU or codec.
/************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, RocdecDecodeStatus* pDecodeStatus);

/*********************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, RocdecReconfigureDecoderInfo *pDecReconfigParams)
//! Used to reuse single decoder for multiple clips. Currently supports resolution change, resize params 
//! params, target area params change for same codec. Must be called during RocdecParserParams::pfnSequenceCallback 
/*********************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, RocdecReconfigureDecoderInfo *pDecReconfigParams);

/************************************************************************************************************************/
//! \fn extern rocDecStatus ROCDECAPI rocDecMapVideoFrame(rocDecDecoderHandle hDecoder, int nPicIdx,
//!                                           uint32_t *pDevMemPtr, uint32_t *pHorizontalPitch,
//!                                           RocdecProcParams *pVidPostprocParams);
//! Post-process and map video frame corresponding to nPicIdx for use in HIP. Returns HIP device pointer and associated
//! pitch(horizontal stride) of the video frame. Returns device memory pointers for each plane (Y, U and V) seperately
/************************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecMapVideoFrame(rocDecDecoderHandle hDecoder, int nPicIdx,
                                           void *pDevMemPtr[3], uint32_t *pHorizontalPitch[3],
                                           RocdecProcParams *pVidPostprocParams);

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecUnMapVideoFrame(rocDecDecoderHandle hDecoder, void *pMappedDevPtr)
//! Unmap a previously mapped video frame with the associated mapped raw pointer (pMappedDevPtr) 
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecUnMapVideoFrame(rocDecDecoderHandle hDecoder, void *pMappedDevPtr);

#ifdef  __cplusplus
}
#endif
