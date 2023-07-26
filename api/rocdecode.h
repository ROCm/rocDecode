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
    ROCDEC_RUNTIME_ERROR  = 3,
    ROCDEC_OUTOF_MEMORY = -4,
    ROCDEC_INVALID_PARAMETER = -5,
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

/***********************************************************/
//! \struct ROCDECJPEGPICPARAMS placeholder
//! JPEG picture parameters
//! This structure is used in ROCDECPICPARAMS structure
/***********************************************************/
typedef struct _ROCDECJPEGPICPARAMS {
    int Reserved;
} ROCDECJPEGPICPARAMS;

/***********************************************************/
//! \struct ROCDECMPEG2PICPARAMS placeholder
//! JPEG picture parameters
//! This structure is used in ROCDECMPEG2PICPARAMS structure
/***********************************************************/
typedef struct _ROCDECMPEG2PICPARAMS {
    int Reserved;
} ROCDECMPEG2PICPARAMS;

/***********************************************************/
//! \struct ROCDECH264PICPARAMS placeholder
//! H.264 picture parameters
//! This structure is used in ROCDECH264PICPARAMS structure
/***********************************************************/
typedef struct _ROCDECH264PICPARAMS {
    int Reserved;
} ROCDECH264PICPARAMS;

/***********************************************************/
//! \struct ROCDECHEVCPICPARAMS placeholder
//! HEVC picture parameters
//! This structure is used in ROCDECHEVCPICPARAMS structure
/***********************************************************/
typedef struct _ROCDECHEVCPICPARAMS {  
    int Reserved;
} ROCDECHEVCPICPARAMS;

/***********************************************************/
//! \struct ROCDECVC1PICPARAMS placeholder
//! VC1 picture parameters
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

/**********************************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocdecGetDecoderCaps(ROCDECDECODECAPS *pdc)
//! Queries decode capabilities of AMD's VCN decoder based on CodecType, ChromaFormat and BitDepthMinus8 parameters.
//! 1. Application fills IN parameters CodecType, ChromaFormat and BitDepthMinus8 of ROCDECDECODECAPS structure
//! 2. On calling rocdecGetDecoderCaps, driver fills OUT parameters if the IN parameters are supported
//!    If IN parameters passed to the driver are not supported by AMD-VCN-HW, then all OUT params are set to 0.
/**********************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecGetDecoderCaps(ROCDECDECODECAPS *pdc);

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
//! API returns CUDA_ERROR_NOT_SUPPORTED error code for unsupported GPU or codec.
/************************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecGetDecodeStatus(rocDecDecoderHandle hDecoder, int nPicIdx, ROCDECGETDECODESTATUS* pDecodeStatus);

/*********************************************************************************************************/
//! \fn rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, ROCDECRECONFIGUREDECODERINFO *pDecReconfigParams)
//! Used to reuse single decoder for multiple clips. Currently supports resolution change, resize params 
//! params, target area params change for same codec. Must be called during ROCDECPARSERPARAMS::pfnSequenceCallback 
/*********************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecReconfigureDecoder(rocDecDecoderHandle hDecoder, ROCDECRECONFIGUREDECODERINFO *pDecReconfigParams);


#ifdef  __cplusplus
}
#endif
