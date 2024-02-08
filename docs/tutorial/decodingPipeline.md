# Video decoding pipeline

<p align="center"><img width="70%" src="https://raw.githubusercontent.com/ROCm/rocDecode/master/docs/data/VideoDecoderPipeline.PNG" /></p>

There are three main components in the rocDecode shown in the figure above: Demuxer, Video Parser APIs, and Video Decode APIs.
The Demuxer is based on FFMPEG. The demuxer extracts a segment of video data and sends it to the Video Parser. The parser then extracts crucial information such as picture and slice parameters, which is then sent to the Decoder APIs. These APIs submit the information to the hardware for the decoding of a frame. This process repeats in a loop until all frames have been decoded.

Steps in decoding video content for applications (available in rocDecode Toolkit)

1. Demultiplex the content into elementary stream packets (FFmpeg)
2. Parse the demultiplexed packets into video frames for the decoder provided by rocDecode API.
3. Decode compressed video frames into YUV frames using rocDecode API.
4. Wait for the decoding to finish.
5. Map the decoded YUV frame from amd-gpu context to HIP (using VAAPI-HIP under ROCm)
6. Execute HIP kernels in the mapped YUV frame (e.g., format conversion, scaling, object detection, classification etc.)
7. Un-map decoded HIP YUV frame.

The above steps are demonstrated in the [sample applications](../samples/README.md) included in the repository.