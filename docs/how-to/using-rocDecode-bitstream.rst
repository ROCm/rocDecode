.. meta::
  :description: Using the rocDecode bitstream reader API
  :keywords: rocDecode, AMD, ROCm, bitstream decoder

********************************************************************
Using the rocDecode bitstream reader API
********************************************************************

The rocDecode bitstream reader API is intended to be used as a testing tool for the decoder. The bitstream reader API extracts and parses coded picture data from an elementary video stream for the decoder to consume.

.. note::

    The bitstream reader API can only be used with elementary video streams and IVF container files.


The |videodecoderaw|_ sample demonstrates how to use the bitstream reader API. It demonstrates how to create a bitstream reader and use it to extract picture data and pass it to the decoder:

.. code:: C++

    RocdecBitstreamReader bs_reader = nullptr;
    rocDecVideoCodec rocdec_codec_id;
    int bit_depth;
    if (rocDecCreateBitstreamReader(&bs_reader, input_file_path.c_str()) != ROCDEC_SUCCESS) {
        std::cerr << "Failed to create the bitstream reader." << std::endl;
        return 1;
    }
    [...]
    # Decode loop:
    do { 
        if (rocDecGetBitstreamPicData(bs_reader, &pvideo, &n_video_bytes, &pts) != ROCDEC_SUCCESS) {
            std::cerr << "Failed to get picture data." << std::endl;
            return 1;
        }
        [...]    
        n_frame_returned = viddec.DecodeFrame(pvideo, n_video_bytes, pkg_flags, pts, &decoded_pics);
    }
  
        
The example also demonstrates other available bitstream APIs. For example, APIs for getting the bit depth and codec of the stream:

.. code:: C++
        
        if (rocDecGetBitstreamCodecType(bs_reader, &rocdec_codec_id) != ROCDEC_SUCCESS) {
            std::cerr << "Failed to get stream codec type." << std::endl;
             return 1;
        }
        [...]
        if (rocDecGetBitstreamBitDepth(bs_reader, &bit_depth) != ROCDEC_SUCCESS) {
            std::cerr << "Failed to get stream bit depth." << std::endl;
            return 1;
        }


.. note:: 
    
    ``rocDecDestroyBitstreamReader`` must always be called to destroy the bitstream reader once processing is complete.


.. |videodecode| replace:: ``videodecode.cpp``
.. _videodecode: https://github.com/ROCm/rocDecode/tree/develop/samples/videoDecode/videodecode.cpp

.. |videodecoderaw| replace:: ``videodecoderaw.cpp``
.. _videodecoderaw: https://github.com/ROCm/rocDecode/tree/develop/samples/videoDecodeRaw

.. |common| replace:: ``common.h``
.. _common: https://github.com/ROCm/rocDecode/blob/develop/samples/common.h

.. |apifolder| replace:: ``api`` folder
.. _apifolder: https://github.com/ROCm/rocDecode/tree/develop/api

.. |utilsfolder| replace:: ``utils`` folder
.. _utilsfolder: https://github.com/ROCm/rocDecode/tree/develop/utils


.. |reconfig_struct| replace:: ``ReconfigParams_t``
.. _reconfig_struct: https://rocm.docs.amd.com/projects/rocDecode/en/latest/doxygen/html/structReconfigParams__t.html