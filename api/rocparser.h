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

#include "rocdecode.h"

/*!
 * \file
 * \brief The AMD rocParser Library.
 *
 * \defgroup group_amd_rocdecode rocDecode: AMD Decode API
 * \brief AMD The rocDECODE is a toolkit to decode videos and images using a hardware-accelerated video decoder on AMD’s GPUs.
 */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
/*!
 * \file
 * \brief The AMD rocDecode library.
 *
 * \defgroup group_rocparser API: APIs for Video Parser
 * \defgroup group_rocdec_struct
 */
/*********************************************************************************/
//! HANDLE pf rocDecDecoder
//! Used in subsequent API calls after rocDecCreateDecoder
/*********************************************************************************/

typedef void *RocdecVideoParser;

typedef uint64_t RocdecTimeStamp;

/**
 * @brief ROCDEC_VIDEO_FORMAT struct
 * @ingroup group_rocdec_struct
 * Used in Parser callback API
 */
typedef struct {
    rocDecVideoCodec codec;                   /**< OUT: Compression format          */
   /**
    * OUT: frame rate = numerator / denominator (for example: 30000/1001)
    */
    struct {
        /**< OUT: frame rate numerator   (0 = unspecified or variable frame rate) */
        uint32_t numerator;
        /**< OUT: frame rate denominator (0 = unspecified or variable frame rate) */
        uint32_t denominator;
    } frame_rate;
    uint8_t progressive_sequence;     /**< OUT: 0=interlaced, 1=progressive                                      */
    uint8_t bit_depth_luma_minus8;    /**< OUT: high bit depth luma. E.g, 2 for 10-bitdepth, 4 for 12-bitdepth   */
    uint8_t bit_depth_chroma_minus8;  /**< OUT: high bit depth chroma. E.g, 2 for 10-bitdepth, 4 for 12-bitdepth */
    uint8_t min_num_decode_surfaces;  /**< OUT: Minimum number of decode surfaces to be allocated for correct
                                                      decoding. The client can send this value in ulNumDecodeSurfaces.
                                                      This guarantees correct functionality and optimal video memory
                                                      usage but not necessarily the best performance, which depends on
                                                      the design of the overall application. The optimal number of
                                                      decode surfaces (in terms of performance and memory utilization)
                                                      should be decided by experimentation for each application, but it
                                                      cannot go below min_num_decode_surfaces.
                                                      If this value is used for ulNumDecodeSurfaces then it must be
                                                      returned to parser during sequence callback.                     */
    uint32_t coded_width;               /**< OUT: coded frame width in pixels                                      */
    uint32_t coded_height;              /**< OUT: coded frame height in pixels                                     */
   /**
    * area of the frame that should be displayed
    * typical example:
    * coded_width = 1920, coded_height = 1088
    * display_area = { 0,0,1920,1080 }
    */
    struct {
        int left;                           /**< OUT: left position of display rect    */
        int top;                            /**< OUT: top position of display rect     */
        int right;                          /**< OUT: right position of display rect   */
        int bottom;                         /**< OUT: bottom position of display rect  */
    } display_area;
    
    rocDecVideoChromaFormat chroma_format;    /**< OUT:  Chroma format                   */
    uint32_t bitrate;                   /**< OUT: video bitrate (bps, 0=unknown)   */
   /**
    * OUT: Display Aspect Ratio = x:y (4:3, 16:9, etc)
    */
    struct {
        int x;
        int y;
    } display_aspect_ratio;
    /**
    * Video Signal Description
    * Refer section E.2.1 (VUI parameters semantics) of H264 spec file
    */
    struct {
        uint8_t video_format          : 3; /**< OUT: 0-Component, 1-PAL, 2-NTSC, 3-SECAM, 4-MAC, 5-Unspecified     */
        uint8_t video_full_range_flag : 1; /**< OUT: indicates the black level and luma and chroma range           */
        uint8_t reserved_zero_bits    : 4; /**< Reserved bits                                                      */
        uint8_t color_primaries;           /**< OUT: chromaticity coordinates of source primaries                  */
        uint8_t transfer_characteristics;  /**< OUT: opto-electronic transfer characteristic of the source picture */
        uint8_t matrix_coefficients;       /**< OUT: used in deriving luma and chroma signals from RGB primaries   */
    } video_signal_description;
    uint32_t seqhdr_data_length;             /**< OUT: Additional bytes following (RocdecVideoFormatEx)                  */
} RocdecVideoFormat;

/****************************************************************/
//! \ingroup group_rocdec_struct
//! \struct RocdecVideoFormat
//! Video format including raw sequence header information
//! Used in rocDecCreateVideoParser API
/****************************************************************/
typedef struct {
    RocdecVideoFormat format;                 /**< OUT: RocdecVideoFormat structure */
    uint32_t max_width;
    uint32_t max_height;
    uint8_t raw_seqhdr_data[1024];  /**< OUT: Sequence header data    */
} RocdecVideoFormatEx;

/*****************************************************************************/
//! \ingroup STRUCTS
//! \struct RocdecSourceDataPacket
//! Data Packet
//! Used in rocDecParseVideoData API
//! IN for rocDecParseVideoData
/*****************************************************************************/
typedef struct _RocdecSourceDataPacket {
    uint32_t flags;            /**< IN: Combination of CUVID_PKT_XXX flags                              */
    uint32_t payload_size;     /**< IN: number of bytes in the payload (may be zero if EOS flag is set) */
    const uint8_t *payload;   /**< IN: Pointer to packet payload data (may be NULL if EOS flag is set) */
    RocdecTimeStamp pts;                   /**< IN: Presentation time stamp (10MHz clock), only valid if
                                             CUVID_PKT_TIMESTAMP flag is set                                 */
} RocdecSourceDataPacket;


/**********************************************************************************/
/*! \brief Timing Info struct
 * \ingroup group_rocdec_struct
 * \struct RocdecParserDispInfo
 * \Used in rocdecParseVideoData API with PFNVIDDISPLAYCALLBACK pfnDisplayPicture
 */
/**********************************************************************************/
typedef struct _RocdecParserDispInfo {
    int picture_index;          /**< OUT: Index of the current picture                                                         */
    int progressive_frame;      /**< OUT: 1 if progressive frame; 0 otherwise                                                  */
    int top_field_first;        /**< OUT: 1 if top field is displayed first; 0 otherwise                                       */
    int repeat_first_field;     /**< OUT: Number of additional fields (1=ivtc, 2=frame doubling, 4=frame tripling, 
                                     -1=unpaired field)                                                                        */
    RocdecTimeStamp pts;                /**< OUT: Presentation time stamp                                                              */
} RocdecParserDispInfo;

/**
 * @brief RocdecOperatingPointInfo struct
 * @ingroup group_rocdec_struct
 * Operating point information of scalable bitstream
 */
typedef struct _RocdecOperatingPointInfo {
    rocDecVideoCodec codec;
    union {
        struct {
            uint8_t  operating_points_cnt;
            uint8_t  reserved24_bits[3];
            uint16_t operating_points_idc[32];
        } av1;
        uint8_t CodecReserved[1024];
    };
} RocdecOperatingPointInfo;


/**********************************************************************************/
//! \ingroup STRUCTS
//! \struct RocdecSeiMessage;
//! Used in RocdecSeiMessageInfo structure
/**********************************************************************************/
typedef struct _RocdecSeiMessage {
    uint8_t sei_message_type; /**< OUT: SEI Message Type      */
    uint8_t reserved[3];
    uint32_t sei_message_size;  /**< OUT: SEI Message Size      */
} RocdecSeiMessage;


/**********************************************************************************/
//! \ingroup STRUCTS
//! \struct RocdecSeiMessageInfo
//! Used in rocDecParseVideoData API with PFNVIDSEIMSGCALLBACK pfnGetSEIMsg
/**********************************************************************************/
typedef struct _RocdecSeiMessageInfo {
    void *pSEIData;                 /**< OUT: SEI Message Data      */
    RocdecSeiMessage *pSEIMessage;      /**< OUT: SEI Message Info      */
    uint32_t sei_message_count; /**< OUT: SEI Message Count     */
    uint32_t picIdx;            /**< OUT: SEI Message Pic Index */
} RocdecSeiMessageInfo;

/**
 * @brief Parser callbacks
 * \ The parser will call these synchronously from within rocDecParseVideoData(), whenever there is sequence change or a picture
 * \ is ready to be decoded and/or displayed. 
 * \ Return values from these callbacks are interpreted as below. If the callbacks return failure, it will be propagated by
 * \ rocDecParseVideoData() to the application.
 * \ Parser picks default operating point as 0 and outputAllLayers flag as 0 if PFNVIDOPPOINTCALLBACK is not set or return value is 
 * \ -1 or invalid operating point.
 * \ PFNVIDSEQUENCECALLBACK : 0: fail, 1: succeeded, > 1: override dpb size of parser (set by RocdecParserParams::ulMaxNumDecodeSurfaces
 * \ while creating parser)
 * \ PFNVIDDECODECALLBACK   : 0: fail, >=1: succeeded
 * \ PFNVIDDISPLAYCALLBACK  : 0: fail, >=1: succeeded
 * \ PFNVIDOPPOINTCALLBACK  : <0: fail, >=0: succeeded (bit 0-9: OperatingPoint, bit 10-10: outputAllLayers, bit 11-30: reserved)
 * \ PFNVIDSEIMSGCALLBACK   : 0: fail, >=1: succeeded
*/ 
typedef int (ROCDECAPI *PFNVIDSEQUENCECALLBACK)(void *, RocdecVideoFormat *);
typedef int (ROCDECAPI *PFNVIDDECODECALLBACK)(void *, RocdecPicParams *);
typedef int (ROCDECAPI *PFNVIDDISPLAYCALLBACK)(void *, RocdecParserDispInfo *);
//typedef int (ROCDECAPI *PFNVIDOPPOINTCALLBACK)(void *, RocdecOperatingPointInfo*);        // reserved for future (AV1 specific)
typedef int (ROCDECAPI *PFNVIDSEIMSGCALLBACK) (void *, RocdecSeiMessageInfo *);

/**
 * \brief The AMD rocDecode library.
 * \ingroup group_rocdec_struct
 * \Used in rocDecCreateVideoParser API
*/
typedef struct _RocdecParserParams {
    rocDecVideoCodec CodecType;                   /**< IN: rocDecVideoCodec_XXX                                                  */
    uint32_t ulMaxNumDecodeSurfaces;        /**< IN: Max # of decode surfaces (parser will cycle through these)          */
    uint32_t ulClockRate;                   /**< IN: Timestamp units in Hz (0=default=10000000Hz)                        */
    uint32_t ulErrorThreshold;              /**< IN: % Error threshold (0-100) for calling pfnDecodePicture (100=always 
                                                     IN: call pfnDecodePicture even if picture bitstream is fully corrupted) */
    uint32_t ulMaxDisplayDelay;             /**< IN: Max display queue delay (improves pipelining of decode with display)
                                                         0=no delay (recommended values: 2..4)                               */
    uint32_t bAnnexb : 1;                   /**< IN: AV1 annexB stream                                                   */
    uint32_t uReserved : 31;                /**< Reserved for future use - set to zero                                   */
    uint32_t uReserved1[4];                 /**< IN: Reserved for future use - set to 0                                  */
    void *pUserData;                            /**< IN: User data for callbacks                                             */
    PFNVIDSEQUENCECALLBACK pfnSequenceCallback; /**< IN: Called before decoding frames and/or whenever there is a fmt change */
    PFNVIDDECODECALLBACK pfnDecodePicture;      /**< IN: Called when a picture is ready to be decoded (decode order)         */
    PFNVIDDISPLAYCALLBACK pfnDisplayPicture;    /**< IN: Called whenever a picture is ready to be displayed (display order)  */
    //PFNVIDOPPOINTCALLBACK pfnGetOperatingPoint; /**< IN: Called from AV1 sequence header to get operating point of a AV1 
    //                                                     scalable bitstream                                                  */
    PFNVIDSEIMSGCALLBACK pfnGetSEIMsg;          /**< IN: Called when all SEI messages are parsed for particular frame        */
    void *pvReserved2[5];                       /**< Reserved for future use - set to NULL                                   */
    RocdecVideoFormatEx *pExtVideoInfo;             /**< IN: [Optional] sequence header data from system layer                   */
} RocdecParserParams;

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocDecodeStatus ROCDECAPI rocDecCreateVideoParser(RocdecVideoParser *pHandle, RocdecParserParams *pParams)
//! Create video parser object and initialize
/************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecCreateVideoParser(RocdecVideoParser *pHandle, RocdecParserParams *pParams);

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocDecodeStatus ROCDECAPI rocDecParseVideoData(RocdecVideoParser handle, RocdecSourceDataPacket *pPacket)
//! Parse the video data from source data packet in pPacket 
//! Extracts parameter sets like SPS, PPS, bitstream etc. from pPacket and 
//! calls back pfnDecodePicture with RocdecPicParams data for kicking of HW decoding
//! calls back pfnSequenceCallback with RocdecVideoFormat data for initial sequence header or when
//! the decoder encounters a video format change
//! calls back pfnDisplayPicture with RocdecParserDispInfo data to display a video frame
/************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecParseVideoData(RocdecVideoParser handle, RocdecSourceDataPacket *pPacket);

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocDecStatus ROCDECAPI rocDecDestroyVideoParser(RocdecVideoParser handle)
//! Destroy the video parser object
/************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecDestroyVideoParser(RocdecVideoParser handle);

#if defined(__cplusplus)
}
#endif /* __cplusplus */
