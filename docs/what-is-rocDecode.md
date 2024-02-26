<head>
  <meta charset="UTF-8">
  <meta name="description" content="What is rocDecode?">
  <meta name="keywords" content="video decoding, rocDecode, AMD, ROCm">
</head>

# What is rocDecode?

AMD GPUs contain one or more media engines (VCNs) that provide fully accelerated, hardware-based
video decoding. Hardware decoders consume lower power than CPU-based decoders. Dedicated
hardware decoders offload decoding tasks from the CPU, boosting overall decoding throughput. With
proper power management, decoding on hardware decoders can lower the overall system power
consumption and improve decoding performance.

Using the rocDecode API, you can decode compressed video streams while keeping the resulting YUV
frames in video memory. With decoded frames in video memory, you can run video post-processing
using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. You can post-process video
frames using scaling or color conversion and augmentation kernels (on a GPU or host) in a format for
GPU/CPU-accelerated inferencing and training.

In addition, you can use the rocDecode API to create multiple instances of video decoder based on the
number of available VCNs in a GPU device. By configuring the decoder for a device, all available
VCNs can be used seamlessly for decoding a batch of video streams in parallel.

For more information, refer to the
[Video decoding pipeline](./conceptual/video-decoding-pipeline.md).
