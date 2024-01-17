# rocDecode User Guide

## Contents

#### Chapter 1 Overview
1.1 Supported Codecs
#### Chapter 2 Decoder Capabalities
#### Chapter 3 Decoder Pipeline
#### Chapter 4 Using rocDecode API
4.1 Video Parser Creation \
4.2 Querying decode capabilities \
4.3 Creating a Decoder \
4.4 Decoding the frame \
4.5 Preparing the decoded frame for further processing \
4.6 Getting data buffer \
4.7 Querying the decoding status \
4.8 Reconfiguring the decoder \
4.9 Destroying the decoder
#### Chapter 5 Samples
5.1 Video Decode \
5.2 Video Decode Performance \
5.3 Video Decode Fork \
5.4 Video Decode Memory \
5.5 Video Decode Multiple Files \
5.6 Video Decode RGB

## Chapter 1 Overview
AMD GPUs contain one or more hardware decoders as separate engines (VCNs) that provide fully accelerated hardware based video decoding. Hardware decoders consume lower power than CPU based decoders. Dedicated hardware decoders offload decoding tasks from CPU, boosting overall decoding throughput. And with proper power management, decoding on hardware decoders can lower the overall system power consumption.

This document describes AMDs rocDecode SDK which provides APIs, allowing the developers to access the video decoding features of VCNs and allows interoperability with other compute engines on the GPU. rocDecode API facilitates decoding of the compressed video streams and keeps the resulting YUV frames in video memory. With decoded frames in video memory, video post processing can 
be done using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. The video frames can further be post-processed using scaling/color-conversion and augmentation kernels (on GPU or host) and be in a format for GPU/CPU accelerated inferencing/training.

In addition, rocDecode API can be used to create multiple instances of video decoder based on the number of available VCN engines in a GPU. By configuring the decoder for a device, all the available engines can be used seamlessly for decoding a batch of video streams in parallel.

### 1.1 Supported Codecs
The codecs currently supported by rocDecode are:
* HEVC/H.265 (8 bit and 10 bit)

Future versions of the SDK will support:
* H.264/AVC (8 bit)
* AV1


## Chapter 2 rocDecode Hardware Capabilities

Table 1 shows the codec support and capabilities of the VCN for each GPU architecture supported by rocDecode.

|GPU Architecture                    |VCN Generation | Number of VCNs |H.265/HEVC | Max width, Max height - H.265 | H.264/AVC | Max width, Max height - H.264 |
| :---                               | :---          | :---           | :---      | :---                          | :---      | :---                      |
| gfx908 - MI1xx                     | VCN 2.5.0     | 2              | Yes       | 4096, 2176                    | No        | 4096, 2160                |
| gfx90a - MI2xx                     | VCN 2.6.0     | 2              | Yes       | 4096, 2176                    | No        | 4096, 2160                |
| gfx940, gfx942 - MI3xx             | VCN 3.0       | 3              | Yes       | 7680, 4320                    | No        | 4096, 2176               |
| gfx941 - MI3xx                     | VCN 3.0       | 4              | Yes       | 7680, 4320                    | No        | 4096, 2176               |
| gfx1030, gfx1031, gfx1032 - Navi2x | VCN 3.x       | 2              | Yes       | 7680, 4320                    | No        | 4096, 2176               |
| gfx1100, gfx1102 - Navi3x          | VCN 4.0       | 2              | Yes       | 7680, 4320                    | No        | 4096, 2176               |
| gfx1101 - Navi3x                   | VCN 4.0       | 1              | Yes       | 7680, 4320                    | No        | 4096, 2176               |

Table 1: Hardware video decoder capabilities

## Chapter 3 Decoder Pipeline

There are three main components ![Fig. 1](data/VideoDecoderPipeline.PNG)  in the rocDecode: Demuxer, Video Parser APIs, and Video Decode APIs.
The Demuxer is based on FFMPEG. The demuxer extracts a segment of video data and sends it to the Video Parser. The parser then extracts crucial information such as picture and slice parameters, which is then sent to the Decoder APIs. These APIs submit the information to the hardware for the decoding of a frame. This process repeats in a loop until all frames have been decoded.

Steps in decoding video content for applications (available in rocDecode Toolkit)

1. Demultiplex the content into elementary stream packets (FFmpeg)
2. Parse the demultiplexed packets into video frames for the decoder provided by rocDecode API.
3. Decode compressed video frames into YUV frames using rocDecode API.
4. Wait for the decoding to finish.
5. Map the decoded YUV frame from amd-gpu context to HIP (using VAAPI-HIP under ROCm)
6. Execute HIP kernels in the mapped YUV frame (e.g., format conversion, scaling, object detection, classification etc.)
7. Un-map decoded HIP YUV frame.

The above steps are demonstrated in the [sample applications](../samples/) included in the repository.

## Chapter 4 Using rocDecode API

All rocDecode APIs are exposed in the two header files: `rocdecode.h` and `rocparser.h`. These headers can be found under the `api` folder in this repository. \ The samples use the `RocVideoDecoder` user class provided in `roc_video_dec.h` under the `utils` folder in this repository.
The following sections in this chapter explain components and steps required for decoding using rocDecode api

### 4.1 Video Parser Creation
Video Parser is needed to required to extract and decode headers from the bitstream to organize the data into a structured format required for the hardware decoder. Parser plays an important role in the video decoding as it controls the decoding and display of the individual frames/fields of a bitstream.
The parser object in rocparser.h has 3 main apis as described below
#### 4.1.1 Creating Parser Object using rocDecCreateVideoParser() [api](../api/)
This creates a video parser object for the Codec specified by the user. The api takes "ulMaxNumDecodeSurfaces" which is used to determine the DPB size for decoding. When creating a parser object, application must register certain callback functions with the driver which will be called from the parser during the decode. For e.g., pfnSequenceCallback will be called when the parser encounters a new sequence header. The parser informs the user with the minimum number of surfaces needed by parser's DPB for successful decoding of the bitstream. In addition, the caller can set additional parameters like "ulMaxDisplayDelay" to control the decoding and displaying of the frames.
"pfnDecodePicture" will be triggered when a picture is ready to be decoded.
"pfnDisplayPicture" will be triggered when a frame in display order is ready to be consumed by the caller. 
"pfnGetSEIMsg" will be triggered when a user SEI message is parsed by the parser and send back to the caller.
#### 4.1.2 Parsing video data using rocDecParseVideoData()
Elementary stream video packets extracted from the demultiplexer are fed into the parser using the "rocDecParseVideoData()" api. During this calls parser triggers the above callbacks as it encounters a new sequence header or got a compressed frame/field data ready to be decoded. If any of the callbacks returns failure, it will be propagated back to the application so the decoding can be termiated gracefully.
#### 4.1.3 Destroying the parser using rocDecDestroyVideoParser()
The user needs to call rocDecDestroyVideoParser() to destroy parser object and free up all allocated resources at the end of video decoding.

### 4.2 Querying decode capabilities using rocDecGetDecoderCaps() [api](../../api)
rocDecGetDecoderCaps() Allows users to query the capabilities of underlying hardware video decoder as different hardware will have different capabilities. Caps usually informs the user of the supported codecs, max. resolution, bit-depth etc. [See Table 1]

The following pseudo-code illustrates the use of this api. If any of the decoder caps is not supported, the application is supposed to handle the error appropriately.

    RocdecDecodeCaps decode_caps;
    memset(&decode_caps, 0, sizeof(decode_caps));
    decode_caps.eCodecType = p_video_format->codec;
    decode_caps.eChromaFormat = p_video_format->chroma_format;
    decode_caps.nBitDepthMinus8 = p_video_format->bit_depth_luma_minus8;

    ROCDEC_API_CALL(rocDecGetDecoderCaps(&decode_caps));

    if(!decode_caps.bIsSupported) {
        ROCDEC_THROW("Rocdec:: Codec not supported on this GPU: ", ROCDEC_NOT_SUPPORTED);
        return 0;
    }

    if ((p_video_format->coded_width > decode_caps.nMaxWidth) ||
        (p_video_format->coded_height > decode_caps.nMaxHeight)) {

        std::ostringstream errorString;
        errorString << std::endl
                    << "Resolution          : " << p_video_format->coded_width << "x" << p_video_format->coded_height << std::endl
                    << "Max Supported (wxh) : " << decode_caps.nMaxWidth << "x" << decode_caps.nMaxHeight << std::endl
                    << "Resolution not supported on this GPU ";

        const std::string cErr = errorString.str();
        ROCDEC_THROW(cErr, ROCDEC_NOT_SUPPORTED);
        return 0;
    }

### 4.3 Creating a Decoder using rocDecCreateDecoder() [api](../../api)
Creates an instance of the hardware video decoder object and gives a handle to the user on successful creation. Refer to RocDecoderCreateInfo [structure](../../api/rocdecode.h) for information about parameters which are passed for creating the decoder. For e.g. RocDecoderCreateInfo::CodecType  represents the codec type of the video. The decoder handle returned by the rocDecCreateDecoder() must be retained for the entire session of the decode because the handle needs to be passed along with the other decoding apis. In addition, user can inform display or roi dimensions along with this API. 

### 4.4 Decoding the frame using rocDecDecodeFrame() [api](../../api)
After de-muxing and parsing, the next step is to decode bitstream data contaiing a frame/field using hardware. rocDecDecodeFrame()API is to submit a new frame for hardware decoding. Underneath the driver VA-API is used to submit compressed picture data to the driver. The parser extracts all the necessary information from the bitstream and fill "RocdecPicParams" struct which is appropriate for the codec used. The high level [RocVideoDecoder](../utils/rocvideodecode/) class connects the parser and decoder which is used for all the sample applications.
The rocDecDecodeFrame() call takes decoder handle and the pointer to RocdecPicParams structure and kicks off video decoding using VA-API.

### 4.5 Preparing the decoded frame for further processing
The decoded frames can be used further postprocessing using the rocDecMapVideoFrame() and rocDecUnMapVideoFrame() API calls. The successful completion of rocDecMapVideoFrame() indicates that the decoding process is completed and the device memory pointer is interopped to ROCm HIP address space for further processing of the decoded frame in device memory. The caller will get all the necessary information of the output surface like YUV format, dimensions, pitch etc. from this call. In the high level [RocVideoDecoder](../utils/rocvideodecode/) class, we provide 3 different surface tppe modes for the mapped surface as specified in OutputSurfaceMemoryType as explained below.

  typedef enum OutputSurfaceMemoryType_enum {
      OUT_SURFACE_MEM_DEV_INTERNAL = 0,      /**<  Internal interopped decoded surface memory(original mapped decoded surface) */
      OUT_SURFACE_MEM_DEV_COPIED = 1,        /**<  decoded output will be copied to a separate device memory (the user doesn't need to call release) **/
      OUT_SURFACE_MEM_HOST_COPIED = 2        /**<  decoded output will be copied to a separate host memory (the user doesn't need to call release) **/
  } OutputSurfaceMemoryType;

If the mapped surface type is "OUT_SURFACE_MEM_DEV_INTERNAL", the direct pointer to the decoded surface is given to the user. The user is supposed to trigger rocDecUnMapVideoFrame() using "ReleaseFrame()" call of RocVideoDecoder class. If the requested surface type OUT_SURFACE_MEM_DEV_COPIED or OUT_SURFACE_MEM_HOST_COPIED, the internal decoded frame will be copied to another buffer either in device memory and host memory. After that it is immediately unmapped for re-use by the RRocVideoDecoder class.
In all the cases, the user needs to call rocDecUnMapVideoFrame() to indicate that the frame is ready to be used for decoding again.
Please refer to RocVideoDecoder class and [samples](../samples/) for detailed use of these APIs.
