# rocDecode User Guide

## Contents

#### Chapter 1 Overview
1.1 Supported Codecs
#### Chapter 2 Decoder Capabalities
#### Chapter 3 Decoder Pipeline
#### Chapter 4 Using rocDecode API
4.1 Video Parser
4.2 Querying decode capabilities
4.3 Creating a Decoder
4.4 Decoding the frame/field
4.5 Preparing the decoded frame for further processing
4.6 Getting histogram data buffer
4.7 Querying the decoding status
4.8 Reconfiguring the decoder
4.9 Destroying the decoder

## Chapter 1 Overview
AMD GPUs contain one or more hardware decoders as separate engines that provide fully accelerated hardware based video decoding. Hardware decoders consume lower power than CPU decoders. 

This document talks about AMDs rocDecode SDK which provides APIs, allowing the developers to access the video decoding features of VCNs and allows interoperability with other compute engines on the GPU. rocDecode API facilitates decoding of the compressed video streams and keeps the resulting YUV frames in video memory. With decoded frames in video memory, video post processing can 
be done using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. The video frames can further be post-processed using scaling/color-conversion and augmentation kernels (on GPU or host) and be in a formatfor GPU/CPU accelerated inferencing/training.

In addition, rocDecode API can be used to create multiple instances of video decoder based on the number of available VCN engines in a GPU. By configuring the decoder for a device, all the available engines can be used seamlessly for decoding a batch of video streams in parallel.

### 1.1 Supported Codecs
The codecs currently supported by rocDecode are:
* HEVC/H.265

Future versions of the SDK will support:
* H264/AVC (8 bit and 10 bit)
* AV1


## Chapter 2 Decoder Capabilities

Table 1 shows the codec support and capabilities of the VCN for each GPU architecture.

|GPU Architecture        |VCN Generation | MPEG-2 | H.264/AVC | H.265/HEVC | AV1  | VP8/VP9 | JPEG |
| :---                   | :---          | :---   | :---      | :---       | :--- | :---    | :--- |
| Raven/Picasso          | VCN 1.0       | Yes    | Yes       | Yes        | No   | Yes     | Yes  |
| Navi 1x                | VCN 2.0       | Yes    | Yes       | Yes        | No   | Yes     | Yes  |
| Navi21, Navi22, Navi23 | VCN 3.0       | Yes    | Yes       | Yes        | No   | Yes     | Yes  |
| Navi24                 | VCN3.0.33     | No     | Yes       | Yes        | No   | Yes     | No   |
| Van Gogh               | VCN 3.1       | Yes    | Yes       | Yes        | No   | Yes     | No   |
| Rembrandt, Mendocino   | VCN 3.1.1     | No     | Yes       | Yes        | No   | Yes     | No   |
| Raphael, Dragon Range  | VCN 3.1.2     | No     | Yes       | Yes        | No   | No      | Yes  |
| Navi 3x, Phoenix       | VCN 4.0       | No     | Yes       | Yes        | No   | Yes     | Yes  |
Table 1: HArdware video decoder capabilities

## Chapter 3 Decoder Pipeline

The hardware video decoding pipeline consists of three major components ![Fig. 1](data/VideoDecoderPipeline.PNG) – Demuxer, Parser, and VCN engines accessible using rocDecode API. The Demuxer and Parser components are external tools that convert compressed frames into a format that can be used by the rocDecode API.

Steps in decoding video content for applications (available in rocDecode Toolkit)

1. Demultiplex the content into elementary stream packets (FFmpeg)
2. Parse the demultiplexed packets into video frames for the decoder provided by rocDecode API.
3. Decode compressed video frames into YUV frames using rocDecode API.
4. Wait for the decoding to finish.
5. Map the decoded YUV frame from amd-gpu context to HIP (using VAAPI-HIP under ROCm)
6. Execute HIP kernels in the mapped YUV frame (e.g., format conversion, scaling, object detection, classification etc.)
7. Un-map decoded HIP YUV frame.

The above steps are demonstrated in the [sample applications](../samples/) included in the repository.

#### Chapter 4 Using rocDecode API




