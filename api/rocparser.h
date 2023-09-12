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

typedef long long RocdecTimeStamp;

/**
 * @brief ROCDEC_VIDEO_FORMAT struct
 * @ingroup group_rocdec_struct
 * Used in Parser callback API
 */
typedef struct
{
    rocDecVideoCodec codec;                   /**< OUT: Compression format          */
   /**
    * OUT: frame rate = numerator / denominator (for example: 30000/1001)
    */
    struct {
        /**< OUT: frame rate numerator   (0 = unspecified or variable frame rate) */
        unsigned int numerator;
        /**< OUT: frame rate denominator (0 = unspecified or variable frame rate) */
        unsigned int denominator;
    } frame_rate;
    unsigned char progressive_sequence;     /**< OUT: 0=interlaced, 1=progressive                                      */
    unsigned char bit_depth_luma_minus8;    /**< OUT: high bit depth luma. E.g, 2 for 10-bitdepth, 4 for 12-bitdepth   */
    unsigned char bit_depth_chroma_minus8;  /**< OUT: high bit depth chroma. E.g, 2 for 10-bitdepth, 4 for 12-bitdepth */
    unsigned char min_num_decode_surfaces;  /**< OUT: Minimum number of decode surfaces to be allocated for correct
                                                      decoding. The client can send this value in ulNumDecodeSurfaces.
                                                      This guarantees correct functionality and optimal video memory
                                                      usage but not necessarily the best performance, which depends on
                                                      the design of the overall application. The optimal number of
                                                      decode surfaces (in terms of performance and memory utilization)
                                                      should be decided by experimentation for each application, but it
                                                      cannot go below min_num_decode_surfaces.
                                                      If this value is used for ulNumDecodeSurfaces then it must be
                                                      returned to parser during sequence callback.                     */
    unsigned int coded_width;               /**< OUT: coded frame width in pixels                                      */
    unsigned int coded_height;              /**< OUT: coded frame height in pixels                                     */
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
    unsigned int bitrate;                   /**< OUT: video bitrate (bps, 0=unknown)   */
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
        unsigned char video_format          : 3; /**< OUT: 0-Component, 1-PAL, 2-NTSC, 3-SECAM, 4-MAC, 5-Unspecified     */
        unsigned char video_full_range_flag : 1; /**< OUT: indicates the black level and luma and chroma range           */
        unsigned char reserved_zero_bits    : 4; /**< Reserved bits                                                      */
        unsigned char color_primaries;           /**< OUT: chromaticity coordinates of source primaries                  */
        unsigned char transfer_characteristics;  /**< OUT: opto-electronic transfer characteristic of the source picture */
        unsigned char matrix_coefficients;       /**< OUT: used in deriving luma and chroma signals from RGB primaries   */
    } video_signal_description;
    unsigned int seqhdr_data_length;             /**< OUT: Additional bytes following (ROCDECVIDEOFORMATEX)                  */
} ROCDECVIDEOFORMAT;

/****************************************************************/
//! \ingroup group_rocdec_struct
//! \struct ROCDECVIDEOFORMATEX
//! Video format including raw sequence header information
//! Used in rocDecCreateVideoParser API
/****************************************************************/
typedef struct
{
    ROCDECVIDEOFORMAT format;                 /**< OUT: ROCDECVIDEOFORMAT structure */
    unsigned int max_width;
    unsigned int max_height;
    unsigned char raw_seqhdr_data[1024];  /**< OUT: Sequence header data    */
} ROCDECVIDEOFORMATEX;

/*****************************************************************************/
//! \ingroup STRUCTS
//! \struct ROCDECSOURCEDATAPACKET
//! Data Packet
//! Used in rocDecParseVideoData API
//! IN for rocDecParseVideoData
/*****************************************************************************/
typedef struct _ROCDECSOURCEDATAPACKET
{
    unsigned long flags;            /**< IN: Combination of CUVID_PKT_XXX flags                              */
    unsigned long payload_size;     /**< IN: number of bytes in the payload (may be zero if EOS flag is set) */
    const unsigned char *payload;   /**< IN: Pointer to packet payload data (may be NULL if EOS flag is set) */
    RocdecTimeStamp pts;                   /**< IN: Presentation time stamp (10MHz clock), only valid if
                                             CUVID_PKT_TIMESTAMP flag is set                                 */
} ROCDECSOURCEDATAPACKET;


/**********************************************************************************/
/*! \brief Timing Info struct
 * \ingroup group_rocdec_struct
 * \struct ROCDECPARSERDISPINFO
 * \Used in rocdecParseVideoData API with PFNVIDDISPLAYCALLBACK pfnDisplayPicture
 */
/**********************************************************************************/
typedef struct _ROCDECPARSERDISPINFO
{
    int picture_index;          /**< OUT: Index of the current picture                                                         */
    int progressive_frame;      /**< OUT: 1 if progressive frame; 0 otherwise                                                  */
    int top_field_first;        /**< OUT: 1 if top field is displayed first; 0 otherwise                                       */
    int repeat_first_field;     /**< OUT: Number of additional fields (1=ivtc, 2=frame doubling, 4=frame tripling, 
                                     -1=unpaired field)                                                                        */
    RocdecTimeStamp pts;                /**< OUT: Presentation time stamp                                                              */
} ROCDECPARSERDISPINFO;

/**
 * @brief ROCDECOPERATINGPOINTINFO struct
 * @ingroup group_rocdec_struct
 * Operating point information of scalable bitstream
 */
typedef struct _ROCDECOPERATINGPOINTINFO
{
    rocDecVideoCodec codec;
    union 
    {
        struct
        {
            unsigned char  operating_points_cnt;
            unsigned char  reserved24_bits[3];
            unsigned short operating_points_idc[32];
        } av1;
        unsigned char CodecReserved[1024];
    };
} ROCDECOPERATINGPOINTINFO;


/**********************************************************************************/
//! \ingroup STRUCTS
//! \struct ROCDECSEIMESSAGE;
//! Used in ROCDECSEIMESSAGEINFO structure
/**********************************************************************************/
typedef struct _ROCDECSEIMESSAGE
{
    unsigned char sei_message_type; /**< OUT: SEI Message Type      */
    unsigned char reserved[3];
    unsigned int sei_message_size;  /**< OUT: SEI Message Size      */
} ROCDECSEIMESSAGE;


/**********************************************************************************/
//! \ingroup STRUCTS
//! \struct ROCDECSEIMESSAGEINFO
//! Used in rocDecParseVideoData API with PFNVIDSEIMSGCALLBACK pfnGetSEIMsg
/**********************************************************************************/
typedef struct _ROCDECSEIMESSAGEINFO
{
    void *pSEIData;                 /**< OUT: SEI Message Data      */
    ROCDECSEIMESSAGE *pSEIMessage;      /**< OUT: SEI Message Info      */
    unsigned int sei_message_count; /**< OUT: SEI Message Count     */
    unsigned int picIdx;            /**< OUT: SEI Message Pic Index */
} ROCDECSEIMESSAGEINFO;

/**
 * @brief Parser callbacks
 * \ The parser will call these synchronously from within rocDecParseVideoData(), whenever there is sequence change or a picture
 * \ is ready to be decoded and/or displayed. 
 * \ Return values from these callbacks are interpreted as below. If the callbacks return failure, it will be propagated by
 * \ rocDecParseVideoData() to the application.
 * \ Parser picks default operating point as 0 and outputAllLayers flag as 0 if PFNVIDOPPOINTCALLBACK is not set or return value is 
 * \ -1 or invalid operating point.
 * \ PFNVIDSEQUENCECALLBACK : 0: fail, 1: succeeded, > 1: override dpb size of parser (set by ROCDECPARSERPARAMS::ulMaxNumDecodeSurfaces
 * \ while creating parser)
 * \ PFNVIDDECODECALLBACK   : 0: fail, >=1: succeeded
 * \ PFNVIDDISPLAYCALLBACK  : 0: fail, >=1: succeeded
 * \ PFNVIDOPPOINTCALLBACK  : <0: fail, >=0: succeeded (bit 0-9: OperatingPoint, bit 10-10: outputAllLayers, bit 11-30: reserved)
 * \ PFNVIDSEIMSGCALLBACK   : 0: fail, >=1: succeeded
*/ 
typedef int (ROCDECAPI *PFNVIDSEQUENCECALLBACK)(void *, ROCDECVIDEOFORMAT *);
typedef int (ROCDECAPI *PFNVIDDECODECALLBACK)(void *, ROCDECPICPARAMS *);
typedef int (ROCDECAPI *PFNVIDDISPLAYCALLBACK)(void *, ROCDECPARSERDISPINFO *);
typedef int (ROCDECAPI *PFNVIDOPPOINTCALLBACK)(void *, ROCDECOPERATINGPOINTINFO*);        // reserved for future (AV1 specific)
typedef int (ROCDECAPI *PFNVIDSEIMSGCALLBACK) (void *, ROCDECSEIMESSAGEINFO *);

/**
 * \brief The AMD rocDecode library.
 * \ingroup group_rocdec_struct
 * \Used in rocDecCreateVideoParser API
*/
typedef struct _ROCDECPARSERPARAMS
{
    rocDecVideoCodec CodecType;                   /**< IN: rocDecVideoCodec_XXX                                                  */
    unsigned int ulMaxNumDecodeSurfaces;        /**< IN: Max # of decode surfaces (parser will cycle through these)          */
    unsigned int ulClockRate;                   /**< IN: Timestamp units in Hz (0=default=10000000Hz)                        */
    unsigned int ulErrorThreshold;              /**< IN: % Error threshold (0-100) for calling pfnDecodePicture (100=always 
                                                     IN: call pfnDecodePicture even if picture bitstream is fully corrupted) */
    unsigned int ulMaxDisplayDelay;             /**< IN: Max display queue delay (improves pipelining of decode with display)
                                                         0=no delay (recommended values: 2..4)                               */
    unsigned int bAnnexb : 1;                   /**< IN: AV1 annexB stream                                                   */
    unsigned int uReserved : 31;                /**< Reserved for future use - set to zero                                   */
    unsigned int uReserved1[4];                 /**< IN: Reserved for future use - set to 0                                  */
    void *pUserData;                            /**< IN: User data for callbacks                                             */
    PFNVIDSEQUENCECALLBACK pfnSequenceCallback; /**< IN: Called before decoding frames and/or whenever there is a fmt change */
    PFNVIDDECODECALLBACK pfnDecodePicture;      /**< IN: Called when a picture is ready to be decoded (decode order)         */
    PFNVIDDISPLAYCALLBACK pfnDisplayPicture;    /**< IN: Called whenever a picture is ready to be displayed (display order)  */
    PFNVIDOPPOINTCALLBACK pfnGetOperatingPoint; /**< IN: Called from AV1 sequence header to get operating point of a AV1 
                                                         scalable bitstream                                                  */
    PFNVIDSEIMSGCALLBACK pfnGetSEIMsg;          /**< IN: Called when all SEI messages are parsed for particular frame        */
    void *pvReserved2[5];                       /**< Reserved for future use - set to NULL                                   */
    ROCDECVIDEOFORMATEX *pExtVideoInfo;             /**< IN: [Optional] sequence header data from system layer                   */
} ROCDECPARSERPARAMS;

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocDecodeStatus ROCDECAPI rocDecCreateVideoParser(RocdecVideoParser *pHandle, ROCDECPARSERPARAMS *pParams)
//! Create video parser object and initialize
/************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecCreateVideoParser(RocdecVideoParser *pHandle, ROCDECPARSERPARAMS *pParams);

/************************************************************************************************/
//! \ingroup FUNCTS
//! \fn rocDecodeStatus ROCDECAPI rocDecParseVideoData(RocdecVideoParser handle, ROCDECSOURCEDATAPACKET *pPacket)
//! Parse the video data from source data packet in pPacket 
//! Extracts parameter sets like SPS, PPS, bitstream etc. from pPacket and 
//! calls back pfnDecodePicture with ROCDECPICPARAMS data for kicking of HW decoding
//! calls back pfnSequenceCallback with ROCDECVIDEOFORMAT data for initial sequence header or when
//! the decoder encounters a video format change
//! calls back pfnDisplayPicture with ROCDECPARSERDISPINFO data to display a video frame
/************************************************************************************************/
extern rocDecStatus ROCDECAPI rocDecParseVideoData(RocdecVideoParser handle, ROCDECSOURCEDATAPACKET *pPacket);

#if defined(__cplusplus)
}
#endif /* __cplusplus */
