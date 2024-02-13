# Video decoding pipeline

There are three main components in rocDecode, as shown below, 

- Demuxer - Demuxer is based on FFmpeg, a leading multimedia framework. For more information, refer to https://ffmpeg.org/about.html.
- Video Parser APIs 
- Video Decode APIs
  

<p align="center"><img width="70%" src="../data/VideoDecoderPipeline.PNG" /></p>

The workflow is as follows,

1. Demuxer extracts a segment of the video data and sends it to the Video parser.
2. The Video parser extracts crucial information, such as picture and slice parameters, sends it to Decoder APIs.
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
