.. meta::
  :description: Using rocDecode with the bitstream decoder
  :keywords: parse video, parse, decode, video decoder, video decoding, rocDecode, AMD, ROCm, bitstream decoder

********************************************************************
Using rocDecode with the bitstream decoder
********************************************************************

rocDecode core APIs are available in the |apifolder|_ of the `rocDecode GitHub repository <https://github.com/ROCm/rocDecode>`_. Utility classes that call the core APIs are available in the |utilsfolder|_ of the repository. For information about the core APIs, see :doc:`Using the rocDecode core APIs <./using-rocdecode>`.

The `samples in the rocDecode GitHub repository <https://github.com/ROCm/rocDecode/tree/develop/samples>`_ use the utility classes. These classes are convenience classes that provide a high-level calls to the core APIs. 

A video stream can be decoded either with the rocDecode bitstream reader on GPU or with the FFmpeg decoder on CPU. For information about using the FFmpeg decoder, see :doc:`Using rocDecode with the FFmpeg decoder <./using-rocDecode-ffmpeg>`.

Import the ``roc_bitstream_reader.h`` and ``roc_video_dec.h`` header files. These headers contain the convenience classes and functions that use the core APIs.

.. code:: C++ 

    #include "roc_bitstream_reader.h"
    #include "roc_video_dec.h"

Use the ``rocDecCreateBitstreamReader`` to get a handle to a bitstream reader. The video file's bit depth and codec ID will be returned in the ``rocDecCreateBitstreamReader`` parameters.

From the |videodecoderaw|_ sample:

.. code:: C++

  RocdecBitstreamReader bs_reader = nullptr;
  rocDecVideoCodec rocdec_codec_id;
  int bit_depth;
  if (rocDecCreateBitstreamReader(&bs_reader, input_file_path.c_str()) != ROCDEC_SUCCESS) {
    std::cerr << "Failed to create the bitstream reader." << std::endl;
    return 1;
  }
        
Get the extracted codec type using ``rocDecGetBitstreamCodecType`` and get the extracted bit depth using ``rocDecGetBitstreamBitDepth``. Verify that the codec and bit depth are supported. 

From the ``videodecoderaw.cpp`` sample:


.. code:: C++
        
        if (rocDecGetBitstreamCodecType(bs_reader, &rocdec_codec_id) != ROCDEC_SUCCESS) {
            std::cerr << "Failed to get stream codec type." << std::endl;
             return 1;
        }
        if (rocdec_codec_id >= rocDecVideoCodec_NumCodecs) {
            std::cerr << "Unsupported stream file type or codec type by the bitstream reader. Exiting." << std::endl;
            return 1;
        }
        if (rocDecGetBitstreamBitDepth(bs_reader, &bit_depth) != ROCDEC_SUCCESS) {
            std::cerr << "Failed to get stream bit depth." << std::endl;
            return 1;
        }

Instantiate the decoder. The decoder constructor takes the following parameters:

.. list-table:: 
    :widths: 10 30 60
    :header-rows: 1

    *   - Parameter
        - Type
        - Description 

    *   - ``device_id``
        - ``int``
        - The GPU device ID. |br| |br| Set it to 0 for the first device, 1 for the second device, 2 for the third device, and so on for each subsequent device.
    
    *   - ``out_mem_type``
        - ``OutputSurfaceMemoryType``
        - The memory type where the surface data, such as the decoded frames, resides. |br| |br| ``OUT_SURFACE_MEM_DEV_INTERNAL``: The surface data is stored internally on memory shared by the GPU and CPU. |br| |br| ``OUT_SURFACE_MEM_DEV_COPIED``: The surface data resides on the GPU. |br| |br| ``OUT_SURFACE_MEM_HOST_COPIED``: The surface data resides on the CPU. |br| |br| See :doc:`Surface data memory locations <../conceptual/rocDecode-memory-types>` for more information.

    *   - ``codec``
        - ``rocDecVideoCodec``
        - The video file's codec ID extracted using ``rocDecGetBitstreamCodecType``.

    *   - ``force_zero_latency``
        - ``bool``
        - Set to ``true`` to flush decoded frames in order for immediate display.

    *   - ``p_crop_rect``
        - ``const Rect *``
        - The rectangle to use for cropping. Defaults to no cropping.

    *   - ``extract_user_SEI_Message``
        - ``bool``
        - Set to ``true`` to extract Supplemental Enhancement Information (SEI) from the video stream.

    *   - ``disp_delay``
        - ``uint32_t``    
        - Delay the display by this number of frames. Defaults to 0 with no delay in displaying the frames.

    *   - ``max_width``
        - ``int``    
        - Max width. Defaults to 0.

    *   - ``max_height``
        - ``int``  
        - Max height. Defaults to 0.

    *   - ``clk_rate``
        - ``uint32_t``    
        - Clock rate. Defaults to 1000.


.. |br| raw:: html

      </br>

From ``videodecoderaw.cpp``:

.. code:: C++

    RocVideoDecoder viddec(device_id, mem_type, rocdec_codec_id, b_force_zero_latency, p_crop_rect, b_extract_sei_messages, disp_delay);

``RocVideoDecoder`` will create a parser and a decoder, and initialize HIP on the device. 

The decoder is reused when there is a change to the video resolution without a change in the codec. When the video stream resolution changes, the existing frame buffer is deleted along with any decoded frames that are still within it, and the decoder is reconfigured for the new resolution.

To prevent the remaining frames from being deleted with the frame buffer, a callback function can be defined to save or post-process the remaining frames. 

The |reconfig_struct|_ struct is used to store information on how to handle the reconfiguration. A callback, a user-defined flush mode, and a user-defined struct are passed to ``ReconfigParams_t``. The reconfiguration parameters are then passed to the decoder using ``SetReconfigParams``.

From |common|_ in the rocDecode samples:

.. code:: C++

    typedef enum ReconfigFlushMode_enum {
        RECONFIG_FLUSH_MODE_NONE = 0,               /**<  Just flush to get the frame count */
        RECONFIG_FLUSH_MODE_DUMP_TO_FILE = 1,       /**<  The remaining frames will be dumped to file in this mode */
        RECONFIG_FLUSH_MODE_CALCULATE_MD5 = 2,      /**<  Calculate the MD5 of the flushed frames */
    } ReconfigFlushMode;

    // this struct is used by videodecode and videodecodeMultiFiles to dump last frames to file
    typedef struct ReconfigDumpFileStruct_t {
        bool b_dump_frames_to_file;
        std::string output_file_name;
        void *md5_generator_handle;
    } ReconfigDumpFileStruct;


From ``videodecoderaw.cpp``:

.. code:: C++

        reconfig_params.p_fn_reconfigure_flush = ReconfigureFlushCallback;
        reconfig_user_struct.b_dump_frames_to_file = dump_output_frames;
        reconfig_user_struct.output_file_name = output_file_path;
        if (dump_output_frames) {
            reconfig_params.reconfig_flush_mode = RECONFIG_FLUSH_MODE_DUMP_TO_FILE;
        } else {
            reconfig_params.reconfig_flush_mode = RECONFIG_FLUSH_MODE_NONE;
        }
        reconfig_params.p_reconfig_user_struct = &reconfig_user_struct;

        viddec.SetReconfigParams(&reconfig_params);


The reconfiguration parameters need to be defined prior to entering the decoding loop.

In the decoding loop, pass the bitstream reader to ``rocDecGetBitstreamPicData`` to read the video stream. Pass the video stream and its size to ``DecodeFrame``. The decoded frame can then be further processed. 

Once processing is done on the frame, call ``ReleaseFrame`` to release the frame.

.. code:: C++

        do {
            auto start_time = std::chrono::high_resolution_clock::now();
            if (rocDecGetBitstreamPicData(bs_reader, &pvideo, &n_video_bytes, &pts) != ROCDEC_SUCCESS) {
                std::cerr << "Failed to get picture data." << std::endl;
                return 1;
            }
            // Treat 0 bitstream size as end of stream indicator
            if (n_video_bytes == 0) {
                pkg_flags |= ROCDEC_PKT_ENDOFSTREAM;
            }
            n_frame_returned = viddec.DecodeFrame(pvideo, n_video_bytes, pkg_flags, pts, &decoded_pics);

            if (!n_frame && !viddec.GetOutputSurfaceInfo(&surf_info)) {
                std::cerr << "Error: Failed to get Output Surface Info!" << std::endl;
                break;
            }
            for (int i = 0; i < n_frame_returned; i++) {
                pframe = viddec.GetFrame(&pts);
                if (dump_output_frames && mem_type != OUT_SURFACE_MEM_NOT_MAPPED) {
                    viddec.SaveFrameToFile(output_file_path, pframe, surf_info);
                }
                // release frame
                viddec.ReleaseFrame(pts);
            }
        
        } while (n_video_bytes);

Use ``rocDecDestroyBitstreamReader`` to destroy the bitstream reader once decoding is complete.


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
