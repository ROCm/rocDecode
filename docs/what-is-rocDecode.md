# rocDecode SDK overview

AMD GPUs contain one or more media engines (VCNs) that provide fully accelerated hardware-based video decoding. Hardware decoders consume lower power than CPU-based decoders. Dedicated hardware decoders offload decoding tasks from the CPU, boosting overall decoding throughput. With proper power management, decoding on hardware decoders can lower the overall system power consumption and improve decoding performance.

This document describes AMD's rocDecode SDK,  which provides APIs, utilities, and samples, allowing the developers to access the video decoding features of VCNs easily. Furthermore, it allows interoperability with other compute engines on the GPU using VA-API/HIP interop. rocDecode API facilitates decoding of the compressed video streams and keeps the resulting YUV frames in video memory. With decoded frames in video memory, video post-processing can be executed using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. The video frames can further be post-processed using scaling/color-conversion and augmentation kernels (on GPU or host) and be in a format for GPU/CPU accelerated inferencing/training.

In addition, rocDecode API can be used to create multiple instances of video decoder based on the number of available VCN engines in a GPU device. By configuring the decoder for a device, all the available engines can be used seamlessly for decoding a batch of video streams in parallel.

## Video decoding pipeline

There are three main components in rocDecode SDK, as shown below, 

- Demuxer - Demuxer is based on FFmpeg, a leading multimedia framework. For more information, refer to https://ffmpeg.org/about.html.
- Video Parser APIs 
- Video Decode APIs
  
  
<img src="VideoDecoderPipelinetest.png" alt="isolated" width="400"/>


The workflow is as follows,

1. Demuxer extracts a segment of the video data and sends it to the Video parser.
2. The Video Parser extracts crucial information, such as picture and slice parameters, sends it to Decoder APIs.
3. Picture and Slice parameters are submitted to the hardware to decode a frame using VA-API.

This process repeats in a loop until all frames have been decoded.

Steps in decoding video content for applications (available in the rocDecode Toolkit)

1. Demultiplex the content into elementary stream packets (FFmpeg)
2. Parse the demultiplexed packets into video frames for the decoder provided by rocDecode API.
3. Decode compressed video frames into YUV frames using rocDecode API.
4. Wait for the decoding to finish.
5. Get the decoded YUV frame from amd-gpu context to HIP (using VAAPI-HIP interop under ROCm).
6. Execute HIP kernels in the mapped YUV frame. For example, format conversion, scaling, object detection, classification, and others.
7. Release decoded frame.

**Note**: YUV is a color space that represents images using luminance Y for brightness and two chrominance components U and V for color information.

The above steps are demonstrated in the sample applications included in the repository. For samples, refer to the [samples](https://github.com/ROCm/rocDecode/tree/develop/samples) directory. 
