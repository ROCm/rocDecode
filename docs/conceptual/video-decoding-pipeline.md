# Video decoding pipeline

There are three main components in rocDecode, as shown below, 

- Demuxer - Demuxer is based on FFmpeg, a leading multimedia framework. For more information, refer to https://ffmpeg.org/about.html.
- Video Parser APIs - 
- Video Decode APIs - 

<p align="center"><img width="70%" src="../data/VideoDecoderPipeline.PNG" /></p>

Demuxer is based on FFmpeg, a leading multimedia framework. For more information, refer to https://ffmpeg.org/about.html.

Demuxer extracts a segment of the video data and sends it to the Video Parser. The parser then extracts crucial information, such as picture and slice parameters, sent to the Decoder APIs. The APIs, then, submit the information to the hardware to decode a frame. This process repeats in a loop until all frames have been decoded.

Steps in decoding video content for applications (available in rocDecode Toolkit)

1. Demultiplex the content into elementary stream packets (FFmpeg)
2. Parse the demultiplexed packets into video frames for the decoder provided by rocDecode API.
3. Decode compressed video frames into YUV frames using rocDecode API.
4. Wait for the decoding to finish.
5. Map the decoded YUV frame from amd-gpu context to HIP (using VAAPI-HIP under ROCm)
6. Execute HIP kernels in the mapped YUV frame (e.g., format conversion, scaling, object detection, classification, etc.)
7. Un-map decoded HIP YUV frame.

The above steps are demonstrated in the sample applications included in the repository.

For samples, refer to the [samples](https://github.com/ROCm/rocDecode/tree/develop/samples) directory. 