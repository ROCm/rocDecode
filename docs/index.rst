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

.. grid:: 1 2 2 3
  :gutter: 3

.. grid-item-card:: Installation

    * :ref:`install`   

   
.. grid-item-card:: How-to

    * :ref:`Use-rocDecode-API`
    * :ref:`Create-parser-object-using-rocDecCreateVideoParser`
    * :ref:`Parse-video-data-using-rocDecParseVideoData`
    * :ref:`Query-decode-capabilities-using-rocDecGetDecoderCaps`
    * :ref:`Create a decoder using rocDecCreateVideoParser`
    * :ref:`Decode-the-frame-using-rocDecDecodeFrame`
    * :ref:`Query-the-decoding-status`
    * :ref:`Prepare-the-decoded-frame-for-further-processing`
    * :ref:`Reconfigure-the-decoder`
    * :ref:`Destroy-the-decoder`
    * :ref:`Destroy-the-parser-using-rocDecDestroyVideoParser`   

  .. grid-item-card::   :ref: `API reference <api-reference>`

     * :doc:`API library <../doxygen/html/index>`  

  .. grid-item-card:: Samples

    * `Samples <https://github.com/ROCm/rocDecode/tree/develop/samples>`_

To contribute to the documentation refer to `Contributing to ROCm  <https://rocm.docs.amd.com/en/latest/contribute/index.html>`_.

You can find licensing information on the `Licensing <https://rocm.docs.amd.com/en/latest/about/license.html>`_ page.