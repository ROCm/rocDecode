# rocDecode overview

AMD GPUs contain one or more media engines (VCNs) that provide fully accelerated hardware-based video decoding. Hardware decoders consume lower power than CPU-based decoders. Dedicated hardware decoders offload decoding tasks from the CPU, boosting overall decoding throughput. With proper power management, decoding on hardware decoders can lower the overall system power consumption and improve decoding performance.

This document describes AMD's rocDecode SDK,  which provides APIs, utilities, and samples, allowing the developers to access the video decoding features of VCNs easily. Furthermore, it allows interoperability with other compute engines on the GPU using VA-API/HIP interop. rocDecode API facilitates decoding of the compressed video streams and keeps the resulting YUV frames in video memory. With decoded frames in video memory, video post-processing can be executed using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. The video frames can further be post-processed using scaling/color-conversion and augmentation kernels (on GPU or host) and be in a format for GPU/CPU accelerated inferencing/training.

In addition, rocDecode API can be used to create multiple instances of video decoder based on the number of available VCN engines in a GPU device. By configuring the decoder for a device, all the available engines can be used seamlessly for decoding a batch of video streams in parallel.

