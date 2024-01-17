# rocDecode User Guide

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
    4.6 Getting data buffer
    4.7 Querying the decoding status
    4.8 Reconfiguring the decoder
    4.9 Destroying the decoder
#### Chapter 5 Samples
    5.1 Video Decode
    5.2 Video Decode Performance
    5.3 Video Decode Fork
    5.4 Video Decode Memory
    5.5 Video Decode Multiple Files
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

### 4.7 Querying the decoding status
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

### 4.8 Reconfiguring the decoder
The `rocDecReconfigureDecoder()` can be called to reuse a single decoder for multiple clips. The API currently supports resolution changes, resize parameter changes, target area parameter changes for the same codec without having to destroy the ongoing decoder instance and creating a new one. This saves time and reduces latency. 

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.
* reconfig_params: The user needs to specify the parameters for the changes in `RocdecReconfigureDecoderInfo`. The ulWidth and ulHeight used for reconfiguration cannot exceed the values set for ulMaxWidth and ulMaxHeight defined at RocDecoderCreateInfo. If these values need to be changed, the session needs to be destroyed and recreated.

The call to the `rocDecReconfigureDecoder()` must be called during `RocdecParserParams::pfnSequenceCallback`.

### 4.9 Destroying the decoder
The user needs to call the `rocDecDestroyDecoder()` to destroy the session and free up resources.

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.

The API returns a `RocdecDecodeStatus` value.

## Chapter 5 Samples
The rocDecode repository provides samples to show different use cases. These can be found under the `samples` directory in this repository.

### 5.1 Video Decode

The video decode sample illustrates the use of the FFMPEG demuxer to get the individual frames and send it to the Video Parser. The parser then extracts crucial information such as picture and slice parameters, which is then sent to the Decoder APIs. These APIs submit the information to the hardware for the decoding of a frame. The sample also dumps the output YUV file for the user to view if requested. This inidividual frame is then released. This process repeats in a loop until all frames have been decoded.

Instructions to run the sample can be found [here.](../samples/videoDecode/)

### 5.2 Video Decode Performance
The video decode performance sample creates multiple threads which demux and decode the same video in parallel. The demuxer uses FFMPEG to get the individual frames which are then sent to the decoder APIs. The sample waites for all the threads to complete execution to caluculate the toal FPS and total time taken. 

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
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded using rocDecode API and optionally color-converted using custom HIP kernels on AMD hardware. This sample converts decoded YUV output to one of the RGB or BGR formats(24bit, 32bit, 464bit).

This sample uses HIP kernels to showcase the color conversion. The color conversion and decoding are run on separate threads to hide latency. Whenever a frame is ready after decoding, the `ColorSpaceConversionThread` is notified and can be used for post-processing.

Instructions to run the sample can be found [here.](../samples/videoDecodeRGB/)

