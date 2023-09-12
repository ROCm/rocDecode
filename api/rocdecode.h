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
    ROCDEC_NOT_SUPPORTED = -7,
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
//! These enums are used in rocDecDECODECREATEINFO structure
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
//! These enums are used in ROCDCODECREATEINFO and ROCDECDECODECAPS structures
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
//! These enums are used in ROCDECGETDECODESTATUS structure
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
//! \struct rocDecDECODECAPS;
//! This structure is used in rocDecGetDecoderCaps API
/**************************************************************************************************************/
typedef struct _ROCDECDECODECAPS {
    rocDecVideoCodec          eCodecType;                 /**< IN: rocDecVideoCodec_XXX                                             */
    rocDecVideoChromaFormat   eChromaFormat;              /**< IN: rocDecVideoChromaFormat_XXX                                      */
    unsigned int              nBitDepthMinus8;            /**< IN: The Value "BitDepth minus 8"                                   */
    unsigned int              reserved1[3];               /**< Reserved for future use - set to zero                              */

    unsigned char             bIsSupported;               /**< OUT: 1 if codec supported, 0 if not supported                      */
    unsigned char             nNumDecoders;                 /**< OUT: Number of Decoders that can support IN params                   */
    unsigned short            nOutputFormatMask;          /**< OUT: each bit represents corresponding rocDecVideoSurfaceFormat enum */
    unsigned int              nMaxWidth;                  /**< OUT: Max supported coded width in pixels                           */
    unsigned int              nMaxHeight;                 /**< OUT: Max supported coded height in pixels                          */
    unsigned short            nMinWidth;                  /**< OUT: Min supported coded width in pixels                           */
    unsigned short            nMinHeight;                 /**< OUT: Min supported coded height in pixels                          */
    unsigned int              reserved2[6];              /**< Reserved for future use - set to zero                              */
} ROCDECDECODECAPS;

/**************************************************************************************************************/
//! \struct ROCDECDECODECREATEINFO
//! This structure is used in rocDecCreateDecoder API
/**************************************************************************************************************/
typedef struct _ROCDECDECODECREATEINFO {
    unsigned long ulWidth;                /**< IN: Coded sequence width in pixels                                             */
    unsigned long ulHeight;               /**< IN: Coded sequence height in pixels                                            */
    unsigned long ulNumDecodeSurfaces;    /**< IN: Maximum number of internal decode surfaces                                 */
    rocDecVideoCodec CodecType;           /**< IN: rocDecVideoCodec_XXX                                                         */
    rocDecVideoChromaFormat ChromaFormat; /**< IN: rocDecVideoChromaFormat_XXX                                                  */
    unsigned long bitDepthMinus8;         /**< IN: The value "BitDepth minus 8"                                               */
    unsigned long ulIntraDecodeOnly;      /**< IN: Set 1 only if video has all intra frames (default value is 0). This will
                                             optimize video memory for Intra frames only decoding. The support is limited
                                             to specific codecs - H264, HEVC, VP9, the flag will be ignored for codecs which
                                             are not supported. However decoding might fail if the flag is enabled in case
                                             of supported codecs for regular bit streams having P and/or B frames.          */
    unsigned long ulMaxWidth;             /**< IN: Coded sequence max width in pixels used with reconfigure Decoder           */
    unsigned long ulMaxHeight;            /**< IN: Coded sequence max height in pixels used with reconfigure Decoder          */                                           

    /**
    * IN: area of the frame that should be copied
    */
    struct {
        short left;
        short top;
        short right;
        short bottom;
    } roi_area;

    rocDecVideoSurfaceFormat OutputFormat;       /**< IN: rocDecVideoSurfaceFormat_XXX                                     */
    unsigned long ulTargetWidth;               /**< IN: Post-processed output width (Should be aligned to 2)           */
    unsigned long ulTargetHeight;              /**< IN: Post-processed output height (Should be aligned to 2)          */
    unsigned long ulNumOutputSurfaces;         /**< IN: Maximum number of output surfaces simultaneously mapped        */

    /**
    * IN: target rectangle in the output frame (for aspect ratio conversion)
    * if a null rectangle is specified, {0,0,ulTargetWidth,ulTargetHeight} will be used
    */
    struct {
        short left;
        short top;
        short right;
        short bottom;
    } target_rect;

    unsigned long enableHistogram;             /**< IN: enable histogram output, if supported */
    unsigned long Reserved2[4];                /**< Reserved for future use - set to zero */
} ROCDECDECODECREATEINFO;

/*********************************************************************************************************/
//! \struct ROCDECGETDECODESTATUS
//! Struct for reporting decode status.
//! This structure is used in rocDecGetDecodeStatus API.
/*********************************************************************************************************/
typedef struct _ROCDECGETDECODESTATUS {
    rocDecDecodeStatus decodeStatus;
    unsigned int reserved[31];
    void *pReserved[8];
} ROCDECGETDECODESTATUS;

/****************************************************/
//! \struct rocDecRECONFIGUREDECODERINFO
//! Struct for decoder reset
//! This structure is used in rocDecReconfigureDecoder() API
/****************************************************/
typedef struct _ROCDECRECONFIGUREDECODERINFO {
    unsigned int ulWidth;             /**< IN: Coded sequence width in pixels, MUST be < = ulMaxWidth defined at ROCDECDECODECREATEINFO  */
    unsigned int ulHeight;            /**< IN: Coded sequence height in pixels, MUST be < = ulMaxHeight defined at ROCDECDECODECREATEINFO  */
    unsigned int ulTargetWidth;       /**< IN: Post processed output width */
    unsigned int ulTargetHeight;      /**< IN: Post Processed output height */
    unsigned int ulNumDecodeSurfaces; /**< IN: Maximum number of internal decode surfaces */
    unsigned int reserved1[12];       /**< Reserved for future use. Set to Zero */
    /**
    * IN: Area of frame to be displayed. Use-case : Source Cropping
    */
    struct {
        short left;
        short top;
        short right;
        short bottom;
    } roi_area;
    /**
    * IN: Target Rectangle in the OutputFrame. Use-case : Aspect ratio Conversion
    */
    struct {
        short left;
        short top;
        short right;
        short bottom;
    } target_rect;
    unsigned int reserved2[11]; /**< Reserved for future use. Set to Zero */
} ROCDECRECONFIGUREDECODERINFO; 

/*********************************************************/
//! \struct ROCDECH264PICTURE
//! H.264 Picture Entry
//! This structure is used in ROCDECH264PICPARAMS structure
/*********************************************************/
typedef struct _ROCDECH264PICTURE
{
    int PicIdx;                 /**< picture index of reference frame    */
    int FrameIdx;               /**< frame_num(short-term) or LongTermFrameIdx(long-term)  */
    unsigned int RefFlags;              /**< See below for definitions  */
    int TopFieldOrderCnt;               /**< field order count of top field  */
    int BottomFieldOrderCnt;            /**< field order count of bottom field   */
} ROCDECH264PICTURE;

/* flags in ROCDECH264PICTURE could be OR of the following */
#define ROCDECH264PICTURE_FLAGS_INVALID			        0x00000001
#define ROCDECH264PICTURE_FLAGS_TOP_FIELD		        0x00000002
#define ROCDECH264PICTURE_FLAGS_BOTTOM_FIELD		    0x00000004
#define ROCDECH264PICTURE_FLAGS_SHORT_TERM_REFERENCE	0x00000008
#define ROCDECH264PICTURE_FLAGS_LONG_TERM_REFERENCE	    0x00000010
#define ROCDECH264PICTURE_FLAGS_NON_EXISTING		    0x00000020

/*********************************************************/
//! \struct ROCDECHEVCPICTURE
//! HEVC Picture Entry
//! This structure is used in ROCDECHEVCPICPARAMS structure
/*********************************************************/
typedef struct _ROCDECHEVCPICTURE
{
    int PicIdx;                 /**< reconstructed picture surface ID    */
    /** \brief picture order count.
     * in HEVC, POCs for top and bottom fields of same picture should
     * take different values.
     */
    int POC;
    unsigned int Flags;              /**< See below for definitions  */
    unsigned int Reserved[4];        /**< reserved for future; must be zero  */
} ROCDECHEVCPICTURE;

/* flags in ROCDECHEVCPICTURE could be OR of the following */
#define ROCDECHEVCPICTURE_INVALID                 0x00000001
/** \brief indication of interlace scan picture.
 * should take same value for all the pictures in sequence.
 */
#define ROCDECHEVCPICTURE_FIELD_PIC               0x00000002
/** \brief polarity of the field picture.
 * top field takes even lines of buffer surface.
 * bottom field takes odd lines of buffer surface.
 */
#define ROCDECHEVCPICTURE_BOTTOM_FIELD            0x00000004
/** \brief Long term reference picture */
#define ROCDECHEVCPICTURE_LONG_TERM_REFERENCE     0x00000008
/**
 * ROCDECHEVCPICTURE_ST_CURR_BEFORE, ROCDECHEVCPICTURE_RPS_ST_CURR_AFTER
 * and ROCDECHEVCPICTURE_RPS_LT_CURR of any picture in ReferenceFrames[] should
 * be exclusive. No more than one of them can be set for any picture.
 * Sum of NumPocStCurrBefore, NumPocStCurrAfter and NumPocLtCurr
 * equals NumPocTotalCurr, which should be equal to or smaller than 8.
 * Application should provide valid values for both short format and long format.
 * The pictures in DPB with any of these three flags turned on are referred by
 * the current picture.
 */
/** \brief RefPicSetStCurrBefore of HEVC spec variable
 * Number of ReferenceFrames[] entries with this bit set equals
 * NumPocStCurrBefore.
 */
#define ROCDECHEVCPICTURE_RPS_ST_CURR_BEFORE      0x00000010
/** \brief RefPicSetStCurrAfter of HEVC spec variable
 * Number of ReferenceFrames[] entries with this bit set equals
 * NumPocStCurrAfter.
 */
#define ROCDECHEVCPICTURE_RPS_ST_CURR_AFTER       0x00000020
/** \brief RefPicSetLtCurr of HEVC spec variable
 * Number of ReferenceFrames[] entries with this bit set equals
 * NumPocLtCurr.
 */
#define ROCDECHEVCPICTURE_RPS_LT_CURR             0x00000040

/***********************************************************/
//! \struct ROCDECJPEGPICPARAMS placeholder
//! JPEG picture parameters
//! This structure is used in ROCDECPICPARAMS structure
/***********************************************************/
typedef struct _ROCDECJPEGPICPARAMS {
    int Reserved;
} ROCDECJPEGPICPARAMS;

/***********************************************************/
//! \struct ROCDECMPEG2QMATRIX
//! MPEG2 QMatrix
//! This structure is used in _ROCDECMPEG2PICPARAMS structure
/***********************************************************/
typedef struct _ROCDECMPEG2QMATRIX {
    int32_t load_intra_quantiser_matrix;
    int32_t load_non_intra_quantiser_matrix;
    int32_t load_chroma_intra_quantiser_matrix;
    int32_t load_chroma_non_intra_quantiser_matrix;
    uint8_t intra_quantiser_matrix[64];
    uint8_t non_intra_quantiser_matrix[64];
    uint8_t chroma_intra_quantiser_matrix[64];
    uint8_t chroma_non_intra_quantiser_matrix[64];
}ROCDECMPEG2QMATRIX;


/***********************************************************/
//! \struct ROCDECMPEG2PICPARAMS
//! MPEG2 picture parameters
//! This structure is used in ROCDECMPEG2PICPARAMS structure
/***********************************************************/
typedef struct _ROCDECMPEG2PICPARAMS {
    uint16_t horizontal_size;
    uint16_t vertical_size;
    unsigned int forward_reference_pic;       // surface_id for forward reference
    unsigned int backward_reference_picture;       // surface_id for backward reference
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

    ROCDECMPEG2QMATRIX q_matrix;
    uint32_t  Reserved[4];
} ROCDECMPEG2PICPARAMS;

/***********************************************************/
//! \struct ROCDECH264PICPARAMS placeholder
//! H.264 picture parameters
//! This structure is used in ROCDECH264PICPARAMS structure
//! This structure is configured similar to VA-API VAPictureParameterBufferH264 structure
/***********************************************************/
typedef struct _ROCDECH264PICPARAMS {
    ROCDECH264PICTURE cur_pic;
    ROCDECH264PICTURE dpb[16];	/* in DPB */
    unsigned short picture_width_in_mbs_minus1;
    unsigned short picture_height_in_mbs_minus1;
    unsigned char bit_depth_luma_minus8;
    unsigned char bit_depth_chroma_minus8;
    unsigned char num_ref_frames;
    union {
        struct {
            unsigned int chroma_format_idc			: 2;
            unsigned int residual_colour_transform_flag		: 1;
            unsigned int gaps_in_frame_num_value_allowed_flag	: 1;
            unsigned int frame_mbs_only_flag			: 1;
            unsigned int mb_adaptive_frame_field_flag		: 1; 
            unsigned int direct_8x8_inference_flag		: 1;
            unsigned int MinLumaBiPredSize8x8			: 1; /* see A.3.3.2 */
            unsigned int log2_max_frame_num_minus4		: 4;
            unsigned int pic_order_cnt_type			: 2;
            unsigned int log2_max_pic_order_cnt_lsb_minus4	: 4;
            unsigned int delta_pic_order_always_zero_flag	: 1;
        } bits;
        unsigned int value;
    } sps_fields;
    union {
        struct {
            unsigned int entropy_coding_mode_flag	: 1;
            unsigned int weighted_pred_flag		: 1;
            unsigned int weighted_bipred_idc		: 2;
            unsigned int transform_8x8_mode_flag	: 1;
            unsigned int field_pic_flag			: 1;
            unsigned int constrained_intra_pred_flag	: 1;
            unsigned int pic_order_present_flag			: 1;
            unsigned int deblocking_filter_control_present_flag : 1;
            unsigned int redundant_pic_cnt_present_flag		: 1;
            unsigned int reference_pic_flag			: 1; /* nal_ref_idc != 0 */
        } bits;
        unsigned int value;
    } pps_fields;

    // FMO/ASO
    unsigned char num_slice_groups_minus1;
    unsigned char slice_group_map_type;
    unsigned short slice_group_change_rate_minus1;
    signed char pic_init_qp_minus26;
    signed char pic_init_qs_minus26;
    signed char chroma_qp_index_offset;
    signed char second_chroma_qp_index_offset;

    unsigned short frame_num;
    unsigned char num_ref_idx_l0_default_active_minus1;
    unsigned char num_ref_idx_l1_default_active_minus1;

    // Quantization Matrices (raster-order)
    unsigned char scaling_list_4x4[6][16];
    unsigned char scaling_list_8x8[2][64];
    // SVC/MVC : Not supported in this version
    // union
    // {
    //     ROCDECH264MVCEXT mvcext;
    //     ROCDECH264SVCEXT svcext;
    // };
    unsigned int  Reserved[12];
} ROCDECH264PICPARAMS;


/***********************************************************/
//! \struct ROCDEC_HEVCQMATRIX
//! HEVC QMatrix
//! This structure is sent once per frame,
//! and only when scaling_list_enabled_flag = 1.
//! When sps_scaling_list_data_present_flag = 0, app still
//! needs to send in this structure with default matrix values.
//! This structure is used in ROCDEC_HEVCQMATRIX structure
/***********************************************************/
typedef struct _ROCDEC_HEVCQMATRIX {
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

}ROCDEC_HEVCQMATRIX;

/***********************************************************/
//! \struct ROCDECHEVCPICPARAMS
//! HEVC picture parameters
//! This structure is used in ROCDECHEVCPICPARAMS structure
/***********************************************************/
typedef struct _ROCDECHEVCPICPARAMS {
    ROCDECHEVCPICTURE cur_pic;
    ROCDECHEVCPICTURE dpb[15];	/* in DPB */
    unsigned short picture_width_in_luma_samples;
    unsigned short picture_height_in_luma_samples;
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
    uint8_t                 num_short_term_ref_pic_sets;
    uint8_t                 num_long_term_ref_pic_sps;
    uint8_t                 num_ref_idx_l0_default_active_minus1;
    uint8_t                 num_ref_idx_l1_default_active_minus1;
    int8_t                  pps_beta_offset_div2;
    int8_t                  pps_tc_offset_div2;
    uint8_t                 num_extra_slice_header_bits;
    /**
     * \brief number of bits that structure
     * short_term_ref_pic_set( num_short_term_ref_pic_sets ) takes in slice
     * segment header when short_term_ref_pic_set_sps_flag equals 0.
     * if short_term_ref_pic_set_sps_flag equals 1, the value should be 0.
     * the bit count is calculated after emulation prevention bytes are removed
     * from bit streams.
     * This variable is used for accelorater to skip parsing the
     * short_term_ref_pic_set( num_short_term_ref_pic_sets ) structure.
     */
    uint32_t                st_rps_bits;

    ROCDEC_HEVCQMATRIX      q_matrix;
    uint32_t                Reserved[16];
} ROCDECHEVCPICPARAMS;

/***********************************************************/
//! \struct ROCDECVC1PICPARAMS placeholder
//! JPEG picture parameters
//! This structure is used in ROCDECVC1PICPARAMS structure
/***********************************************************/
typedef struct _ROCDECVC1PICPARAMS {
    int Reserved;
} ROCDECVC1PICPARAMS;


/******************************************************************************************/
//! \struct _ROCDECPICPARAMS
//! Picture parameters for decoding
//! This structure is used in rocDecDecodePicture API
//! IN  for rocDecDecodePicture
/******************************************************************************************/
typedef struct _ROCDECPICPARAMS {
    int PicWidth;                         /**< IN: Coded frame width                                        */
    int PicHeight;                        /**< IN: Coded frame height                                       */
    int CurrPicIdx;                        /**< IN: Output index of the current picture                       */
    int field_pic_flag;                    /**< IN: 0=frame picture, 1=field picture                          */
    int bottom_field_flag;                 /**< IN: 0=top field, 1=bottom field (ignored if field_pic_flag=0) */
    int second_field;                      /**< IN: Second field of a complementary field pair                */
    // Bitstream data
    unsigned int nBitstreamDataLen;        /**< IN: Number of bytes in bitstream data buffer                  */
    const unsigned char *pBitstreamData;   /**< IN: Ptr to bitstream data for this picture (slice-layer)      */
    unsigned int nNumSlices;               /**< IN: Number of slices in this picture                          */
    const unsigned int *pSliceDataOffsets; /**< IN: nNumSlices entries, contains offset of each slice within 
                                                        the bitstream data buffer                             */
    int ref_pic_flag;                      /**< IN: This picture is a reference picture                       */
    int intra_pic_flag;                    /**< IN: This picture is entirely intra coded                      */
    unsigned int Reserved[30];             /**< Reserved for future use                                       */
    // IN: Codec-specific data
    union {
        ROCDECMPEG2PICPARAMS mpeg2;         /**< Also used for MPEG-1 */
        ROCDECH264PICPARAMS  h264;
        ROCDECHEVCPICPARAMS  hevc;
        ROCDECVC1PICPARAMS   vc1;
        ROCDECJPEGPICPARAMS  jpeg;
        unsigned int CodecReserved[256];
    } CodecSpecific;

} ROCDECPICPARAMS;

/******************************************************/
//! \struct ROCDECPROCPARAMS
//! Picture parameters for postprocessing
//! This structure is used in rocDecMapVideoFrame API
/******************************************************/
typedef struct _ROCDECPROCPARAMS
{
    int progressive_frame;                        /**< IN: Input is progressive (deinterlace_mode will be ignored)                */
    int top_field_first;                          /**< IN: Input frame is top field first (1st field is top, 2nd field is bottom) */
    unsigned int reserved_flags[2];                  /**< Reserved for future use (set to zero)                                      */

    // The fields below are used for raw YUV input
    unsigned long long raw_input_dptr;            /**< IN: Input HIP device ptr for raw YUV extensions                               */
    unsigned int raw_input_pitch;                 /**< IN: pitch in bytes of raw YUV input (should be aligned appropriately)      */
    unsigned int raw_input_format;                /**< IN: Input YUV format (rocDecVideoCodec_enum)                                 */
    unsigned long long raw_output_dptr;           /**< IN: Output HIP device mem ptr for raw YUV extensions                              */
    unsigned int raw_output_pitch;                /**< IN: pitch in bytes of raw YUV output (should be aligned appropriately)     */
    unsigned int raw_output_format;                /**< IN: Output YUV format (rocDecVideoCodec_enum)                                 */
    hipStream_t output_hstream;                   /**< IN: stream object used by rocDecMapVideoFrame                               */
    unsigned int Reserved[16];                    /**< Reserved for future use (set to zero)                                      */
} ROCDECPROCPARAMS;


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
//! \fn rocDecStatus ROCDECAPI rocDecCreateDecoder(rocDecDecoderHandle *phDecoder, ROCDECDECODECREATEINFO *pdci)
//! Create the decoder object based on pdci. A handle to the created decoder is returned
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecCreateDecoder(rocDecDecoderHandle *phDecoder, ROCDECDECODECREATEINFO *pdci);

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDestroyDecoder(rocDecDecoderHandle hDecoder)
//! Destroy the decoder object
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecDestroyDecoder(rocDecDecoderHandle hDecoder);

/**********************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocdecGetDecoderCaps(ROCDECDECODECAPS *pdc)
//! Queries decode capabilities of AMD's VCN decoder based on CodecType, ChromaFormat and BitDepthMinus8 parameters.
//! 1. Application fills IN parameters CodecType, ChromaFormat and BitDepthMinus8 of ROCDECDECODECAPS structure
//! 2. On calling rocdecGetDecoderCaps, driver fills OUT parameters if the IN parameters are supported
//!    If IN parameters passed to the driver are not supported by AMD-VCN-HW, then all OUT params are set to 0.
/**********************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecGetDecoderCaps(rocDecDecoderHandle hDecoder, ROCDECDECODECAPS *pdc);

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecDecodeFrame(rocDecDecoderHandle hDecoder, ROCDECPICPARAMS *pPicParams)
//! Decodes a single picture
//! Submits the frame for HW decoding 
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecDecodeFrame(rocDecDecoderHandle hDecoder, ROCDECPICPARAMS *pPicParams);

/************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, ROCDECGETDECODESTATUS* pDecodeStatus);
//! Get the decode status for frame corresponding to nPicIdx
//! API is currently supported for HEVC, H264 and JPEG codecs.
//! API returns ROCDEC_NOT_SUPPORTED error code for unsupported GPU or codec.
/************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, ROCDECGETDECODESTATUS* pDecodeStatus);

/*********************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, ROCDECRECONFIGUREDECODERINFO *pDecReconfigParams)
//! Used to reuse single decoder for multiple clips. Currently supports resolution change, resize params 
//! params, target area params change for same codec. Must be called during ROCDECPARSERPARAMS::pfnSequenceCallback 
/*********************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, ROCDECRECONFIGUREDECODERINFO *pDecReconfigParams);

/************************************************************************************************************************/
//! \fn extern rocDecStatus ROCDECAPI rocDecMapVideoFrame(rocDecDecoderHandle hDecoder, int nPicIdx,
//!                                           unsigned int *pDevMemPtr, unsigned int *pHorizontalPitch,
//!                                           ROCDECPROCPARAMS *pVidPostprocParams);
//! Post-process and map video frame corresponding to nPicIdx for use in HIP. Returns HIP device pointer and associated
//! pitch(horizontal stride) of the video frame. Returns device memory pointers for each plane (Y, U and V) seperately
/************************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecMapVideoFrame(rocDecDecoderHandle hDecoder, int nPicIdx,
                                           void *pDevMemPtr[3], unsigned int *pHorizontalPitch[3],
                                           ROCDECPROCPARAMS *pVidPostprocParams);

/*****************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecUnMapVideoFrame(rocDecDecoderHandle hDecoder, void *pMappedDevPtr)
//! Unmap a previously mapped video frame with the associated mapped raw pointer (pMappedDevPtr) 
/*****************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecUnMapVideoFrame(rocDecDecoderHandle hDecoder, void *pMappedDevPtr);

#ifdef  __cplusplus
}
#endif
