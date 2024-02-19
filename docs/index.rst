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

.. grid:: 2
  :gutter: 3

  .. grid-item-card:: Installation

    * :ref:`Install`

  .. grid-item-card:: How-to

    * :ref:`Using rocDecode API`
    * :ref:`Create parser object using rocDecCreateVideoParser`
    * :ref:`Parse video data using rocDecParseVideoData`
    * :ref:`Destroy the parser using rocDecDestroyVideoParser`
    * :ref:`Query decode capabilities using rocDecGetDecoderCaps`
    * :ref:`Create a Decoder using rocDecCreateDecoder`
    * :ref:`Decode the frame using rocDecDecodeFrame`
    * :ref:`Prepare the decoded frame for further processing`
    * :ref:`Query the decoding status`
    * :ref:`Reconfigure the decoder`
    * :ref:`Destroy the decoder`

  .. grid-item-card:: Reference

    * `rocDecode Header Files <https://rocm.docs.amd.com/projects/rocDecode/en/latest/doxygen/html/files.html>`_

  .. grid-item-card:: Samples

    * :ref:`samples`

To contribute to the documentation refer to `Contributing to ROCm  <https://rocm.docs.amd.com/en/latest/contribute/index.html>`_.

You can find licensing information on the `Licensing <https://rocm.docs.amd.com/en/latest/about/license.html>`_ page.
