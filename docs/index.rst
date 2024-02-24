.. meta::
  :description: rocDecode documentation and API reference library
  :keywords: rocDecode, ROCm, API, documentation

.. _rocDecode:

********************************************************************
rocDecode SDK
********************************************************************

This rocDecode documentation describes AMD's rocDecode SDK, which provides APIs, utilities, and samples, allowing the developers to access the video decoding features of VCNs easily. Furthermore, it allows interoperability with other compute engines on the GPU using VA-API/HIP interop.

rocDecode API facilitates decoding of the compressed video streams and keeps the resulting YUV frames in video memory. With decoded frames in video memory, video post-processing can be executed using ROCm HIP, thereby avoiding unnecessary data copies via PCIe bus. The video frames can further be post-processed using scaling/color-conversion and augmentation kernels (on GPU or host) and be in a format for GPU/CPU accelerated inferencing/training.

The code is open and hosted at: https://github.com/ROCm/rocDecode

The rocDecode documentation is structured as follows:

The rocAL documentation is structured as follows:

.. grid:: 2
  :gutter: 3

  .. grid-item-card:: Installation

    * :doc:`rocDecode installation <install/install>`

  .. grid-item-card:: How-to

    * :doc:`Use rocDecode API <how-to/Use-rocDecode-API>`
    * :doc:`Create parser object <how-to/Create-parser-object-using-rocDecCreateVideoParser>`
    * :doc:`Parse video data <how-to/Parse-video-data-using-rocDecParseVideoData>`
    * :doc:`Query decode capabilities <how-to/Query-decode-capabilities-using-rocDecGetDecoderCaps>`
    * :doc:`Create a decoder <how-to/Create-a-decoder-using-rocDecCreateDecoder>`    
    * :doc:`Decode the frame <how-to/Decode-the-frame-using-rocDecDecodeFrame>`
    * :doc:`Query the decoding status <how-to/Query-the-decoding-status>`
    * :doc:`Prepare the decoded frame for further processing <how-to/Prepare-the-decoded-frame-for-further-processing>`
    * :doc:`Reconfigure the decoder <how-to/Reconfigure-the-decoder>`
    * :doc:`Destroy the decoder <how-to/Destroy-the-decoder>`
    * :doc:`Destroy the parser <how-to/Destroy-the-parser-using-rocDecDestroyVideoParser>`

  .. grid-item-card:: Reference

     * :doc:`rocDecode API reference <../doxygen/html/files>`  

  .. grid-item-card:: Samples

    * `rocDecode samples <https://github.com/ROCm/rocDecode/tree/develop/samples>`_

  


    
To contribute to the documentation, refer to `Contributing to ROCm  <https://rocm.docs.amd.com/en/latest/contribute/index.html>`_.

You can find licensing information on the `Licensing <https://rocm.docs.amd.com/en/latest/about/license.html>`_ page.

