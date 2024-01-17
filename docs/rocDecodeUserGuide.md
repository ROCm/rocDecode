## Contents

#### Chapter 1 Overview
    1.1 Supported Codecs
#### Chapter 2 rocDecode Hardware Capabalities
#### Chapter 3 Video Decoding Pipeline
#### Chapter 4 Using rocDecode API
    4.1 Video Parser
    4.2 Querying decode capabilities
    4.3 Creating a Decoder
    4.4 Decoding the frame
    4.5 Preparing the decoded frame for further processing
    4.6 Querying the decoding status
    4.7 Reconfiguring the decoder
    4.8 Destroying the decoder
#### Chapter 5 Samples
    5.1 Video Decode
    5.2 Video Decode Performance
    5.3 Video Decode Fork
    5.4 Video Decode Memory
    5.5 Video Decode Multiple Files
    5.6 Video Decode RGB

## Chapter 1 Overview
AMD GPUs contain one or more hardware decoders as separate engines (VCNs) that provide fully accelerated hardware based video decoding. Hardware decoders consume lower power than CPU based decoders. Dedicated hardware decoders offload decoding tasks from the CPU, boosting overall decoding throughput. And with proper power management, decoding on hardware decoders can lower the overall system power consumption and improve decoding performance.

This document describes AMD's rocDecode SDK library which provides APIs, utilities and samples, allowing the developers to access the video decoding features of VCNs easily. Further more, it allows interoperability with other compute engines on the GPU using VA-API/HIP interop. rocDecode API facilitates decoding of the compressed video streams and keeps the resulting YUV frames in video memory. With decoded frames in video memory, video post processing can be executed using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. The video frames can further be post-processed using scaling/color-conversion and augmentation kernels (on GPU or host) and be in a format for GPU/CPU accelerated inferencing/training.

In addition, rocDecode API can be used to create multiple instances of video decoder based on the number of available VCN engines available in a GPU device . By configuring the decoder for a device, all the available engines can be used seamlessly for decoding a batch of video streams in parallel.

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

## Chapter 3 Video Decoding Pipeline

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

All rocDecode APIs are exposed in the two header files: `rocdecode.h` and `rocparser.h`. These headers can be found under `api` folder in this repository. \
The samples use the `RocVideoDecoder` user class provided in `roc_video_dec.h` under the `utils` folder in this repository.

### 4.1 Video Parser (defined in [rocparser.h](../api/))
Video Parser is needed to required to extract and decode headers from the bitstream to organize the data into a structured format required for the hardware decoder. Parser plays an important role in the video decoding as it controls the decoding and display of the individual frames/fields of a bitstream.
The parser object in rocparser.h has 3 main apis as described below
#### 4.1.1 Creating Parser Object using rocDecCreateVideoParser()
This API creates a video parser object for the Codec specified by the user. The api takes "ulMaxNumDecodeSurfaces" which determines the DPB (Decoded Picture Buffer) size for decoding. When creating a parser object, application must register certain callback functions with the driver which will be called from the parser during the decode. 
- pfnSequenceCallback will be called when the parser encounters a new sequence header. The parser informs the user with the minimum number of surfaces needed by parser's DPB for successful decoding of the bitstream. In addition, the caller can set additional parameters like "ulMaxDisplayDelay" to control the decoding and displaying of the frames.
- "pfnDecodePicture" callback function will be triggered when a picture is ready to be decoded.
- "pfnDisplayPicture" callback function will be triggered when a frame in display order is ready to be consumed by the caller. 
- "pfnGetSEIMsg" callback function will be triggered when a user SEI message is parsed by the parser and send back to the caller.
#### 4.1.2 Parsing video data using rocDecParseVideoData()
Elementary stream video packets extracted from the demultiplexer are fed into the parser using the "rocDecParseVideoData()" API. During this call, the parser triggers the above callbacks as it encounters a new sequence header, got a compressed frame/field data ready to be decoded, or when it is ready to display a frame. If any of the callbacks returns failure, it will be propagated back to the application so the decoding can be termiated gracefully.
#### 4.1.3 Destroying the parser using rocDecDestroyVideoParser()
The user needs to call rocDecDestroyVideoParser() to destroy parser object and free up all allocated resources at the end of video decoding.

### 4.2 Querying decode capabilities using rocDecGetDecoderCaps() (defined in [rocdecode.h](../api/))
rocDecGetDecoderCaps() Allows users to query the capabilities of underlying hardware video decoder as different hardware will have different capabilities. Caps usually informs the user of the supported codecs, max. resolution, bit-depth etc. [See Table 1]

The following pseudo-code illustrates the use of this API. If any of the decoder caps is not supported, the application is supposed to handle the error appropriately.

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
Creates an instance of the hardware video decoder object and gives a handle to the user on successful creation. Refer to RocDecoderCreateInfo [structure](../../api/rocdecode.h) for information about parameters which are passed for creating the decoder. For e.g. RocDecoderCreateInfo::CodecType  represents the codec type of the video. The decoder handle returned by the rocDecCreateDecoder() must be retained for the entire session of the decode because the handle is passed along with the other decoding apis. In addition, user can inform display or crop dimensions along with this API. 

### 4.4 Decoding the frame using rocDecDecodeFrame() [api](../../api)
After de-muxing and parsing, the next step is to decode bitstream data contaiing a frame/field using hardware. rocDecDecodeFrame()API is to submit a new frame for hardware decoding. Underneath the driver VA-API is used to submit compressed picture data to the driver. The parser extracts all the necessary information from the bitstream and fill "RocdecPicParams" struct which is appropriate for the codec used. The high level [RocVideoDecoder](../utils/rocvideodecode/) class connects the parser and decoder which is used for all the sample applications.
The rocDecDecodeFrame() call takes decoder handle and the pointer to RocdecPicParams structure and kicks off video decoding using VA-API.

### 4.5 Preparing the decoded frame for further processing
The decoded frames can be used further postprocessing using the rocDecMapVideoFrame() and rocDecUnMapVideoFrame() API calls. The successful completion of rocDecMapVideoFrame() indicates that the decoding process is completed and the device memory pointer is interopped to ROCm HIP address space for further processing of the decoded frame in device memory. The caller will get all the necessary information of the output surface like YUV format, dimensions, pitch etc. from this call. In the high level [RocVideoDecoder](../utils/rocvideodecode/) class, we provide 3 different surface tppe modes for the mapped surface as specified in OutputSurfaceMemoryType as explained below.

    typedef enum OutputSurfaceMemoryType_enum {
        OUT_SURFACE_MEM_DEV_INTERNAL = 0,      /**<  Internal interopped decoded surface memory **/
        OUT_SURFACE_MEM_DEV_COPIED = 1,        /**<  decoded output will be copied to a separate device memory **/
        OUT_SURFACE_MEM_HOST_COPIED = 2        /**<  decoded output will be copied to a separate host memory **/
    } OutputSurfaceMemoryType;

If the mapped surface type is "OUT_SURFACE_MEM_DEV_INTERNAL", the direct pointer to the decoded surface is given to the user. The user is supposed to trigger rocDecUnMapVideoFrame() using "ReleaseFrame()" call of RocVideoDecoder class. If the requested surface type OUT_SURFACE_MEM_DEV_COPIED or OUT_SURFACE_MEM_HOST_COPIED, the internal decoded frame will be copied to another buffer either in device memory or host memory. After that it is immediately unmapped for re-use by the RocVideoDecoder class.
In all the cases, the user needs to call rocDecUnMapVideoFrame() to indicate that the frame is ready to be used for decoding again.
Please refer to RocVideoDecoder class and [samples](../samples/) for detailed use of these APIs.

### 4.6 Querying the decoding status
The `rocDecGetDecodeStatus()` can be called at any time to query the status of the decoding for given frame. A struct pointer `RocdecDecodeStatus*` will be filled and returned to the user.

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.
* pic_idx: An int value for the picIdx for which the user wants a status.
* decode_status: A pointer to `RocdecDecodeStatus` as return value.

The API returns one of the following statuses:
* Invalid (0): Decode status is not valid.
* In Progress (1): Decoding is in progress.
* Success (2): Decoding was successful and no errors were returned.
* Error (8): The frame was corrupted, but the error was not concealed.
* Error Concealed (9): The frame was corrupted and the error was concealed.
* Displaying (10):  Decode is completed, displaying in progress.

### 4.7 Reconfiguring the decoder
The `rocDecReconfigureDecoder()` can be called to reuse a single decoder for multiple clips or when the video resolution changes during the decode. The API currently supports resolution changes, resize parameter changes, target area parameter changes for the same codec without having to destroy the ongoing decoder instance and creating a new one. This can improve performance and reduces the overall latency. 

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.
* reconfig_params: The user needs to specify the parameters for the changes in `RocdecReconfigureDecoderInfo`. The ulWidth and ulHeight used for reconfiguration cannot exceed the values set for ulMaxWidth and ulMaxHeight defined at RocDecoderCreateInfo. If these values need to be changed, the session needs to be destroyed and recreated.

The call to the `rocDecReconfigureDecoder()` must be called during `RocdecParserParams::pfnSequenceCallback`.

### 4.8 Destroying the decoder
The user needs to call the `rocDecDestroyDecoder()` to destroy the session and free up resources.

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.

The API returns a `RocdecDecodeStatus` value.

## Chapter 5 Samples
The rocDecode repository provides samples to show different use cases. These can be found under the `samples` directory in this repository.

### 5.1 Video Decode

The video decode sample illustrates decoding a single packetized video stream using FFMPEG demuxer, video parser, and rocDecoder to get the individual decoded frames in YUV format. This sample cab ne configured with a device ID and optionally able to dump the output to a file. This sample uses the high level RocVideoDecoder class which connects both the video parser and Rocdecoder. This process repeats in a loop until all frames have been decoded.

Instructions to run the sample can be found [here.](../samples/videoDecode/)

### 5.2 Video Decode Performance
The video decode performance sample creates multiple threads each of which uses a seperate demuxer and decoder instance to decode the same video in parallel. The demuxer uses FFMPEG to get the individual frames which are then sent to the decoder APIs. The sample waites for all the threads to complete execution to caluculate the toal FPS and total time taken for decoding multiple streams in parallel. 

This sample shows scaling in performance for `N` VCN engines as per GPU architecture.

Instructions to run the sample can be found [here.](../samples/videoDecodePerf/)


### 5.3 Video Decode Fork
The video decode fork sample creates multiple processes which demux and decode the same video in parallel. The demuxer uses FFMPEG to get the individual frames which are then sent to the decoder APIs. The sample uses shared memory to keep count of the number of frames decoded in the different processes. Each child process needs to exit successfully for the sample to complete successfully.

This sample shows scaling in performance for `N` VCN engines as per GPU architecture.

Instructions to run the sample can be found [here.](../samples/videoDecodeFork/)

### 5.4 Video Decode Memory
The video decode memory sample illustrates a way to pass the data chunk-by-chunk sequentially to the FFMPEG demuxer which are then decoded on AMD hardware using rocDecode library.

The sample provides a user class `FileStreamProvider` derived from the existing `VideoDemuxer::StreamProvider` to read a video file and fill the buffer owned by the demuxer. It then takes frames from this buffer for further parsing and decoding.

Instructions to run the sample can be found [here.](../samples/videoDecodeMem/)

### 5.5 Video Decode Multiple Files
The video decode multiple files sample illustrates the use of providing a list of files as input to showcase the reconfigure option in rocDecode library. The input video files have to be of the same codec type to use the reconfigure option but can have different resolution or resize parameters.

The reconfigure option can be disabled by the user if needed. The input file is parsed line by line and data is stored in a queue. The individual video files are demuxed and decoded one after the other in a loop. Outpuot for each individual input file can also be stored if needed.

Instructions to run the sample can be found [here.](../samples/videoDecodeMultiFiles/)

### 5.6 Video Decode RGB
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded using rocDecode API and optionally color-converted using custom HIP kernels on AMD hardware. This sample converts decoded YUV output to one of the RGB or BGR formats(24bit, 32bit, 464bit) in a separate thread allowing to run both VCN hardware and compute engine in parallel.

This sample uses HIP kernels to showcase the color conversion.  Whenever a frame is ready after decoding, the `ColorSpaceConversionThread` is notified and can be used for post-processing.

Instructions to run the sample can be found [here.](../samples/videoDecodeRGB/)