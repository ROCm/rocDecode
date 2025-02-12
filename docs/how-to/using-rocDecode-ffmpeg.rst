.. meta::
  :description: Using rocDecode with the FFMpeg decoder
  :keywords: parse video, parse, decode, video decoder, video decoding, rocDecode, AMD, ROCm, FFmpeg decoder

********************************************************************
Using rocDecode with the FFmpeg decoder
********************************************************************

rocDecode core APIs are available in the |apifolder|_ of the `rocDecode GitHub repository <https://github.com/ROCm/rocDecode>`_. Utility classes that call the core APIs are available in the |utilsfolder|_ of the repository. For information about the core APIs, see :doc:`Using the rocDecode core APIs <./using-rocdecode>`.

The `samples in the rocDecode GitHub repository <https://github.com/ROCm/rocDecode/tree/develop/samples>`_ use the utility classes. These classes are convenience classes that provide a high-level calls to the core APIs. 

A video stream can be decoded either with the FFmpeg decoder on CPU or with the rocDecode bitstream reader on GPU. For information about using the rocDecode bitstream reader, see :doc:`Using rocDecode with the bitstream decoder <./using-rocDecode-bitstream>`.

Import the ``roc_video_dec.h``, ``video_demuxer.h``, and ``ffmpeg_video_dec.h`` header files. These headers contain the convenience classes and functions for decoding and demultiplexing (demuxing) video.

.. code:: C++ 

  #include "roc_video_dec.h"
  #include "video_demuxer.h"
  #include "ffmpeg_video_dec.h"


Instantiate a ``VideoDemuxer`` with the path to the video file. Use the ``GetCodecId`` and ``GetBitDepth`` demuxer functions to obtain the video stream's codec and bit depth. The ``AVCodec2RocDecVideoCodec`` utility function converts the codec returned from the demuxer to its corresponding ``rocDecVideoCodec_enum`` value.

.. code:: C++

  VideoDemuxer *demuxer;
  demuxer = new VideoDemuxer(input_file_path.c_str());
  rocdec_codec_id = AVCodec2RocDecVideoCodec(demuxer->GetCodecID());
  bit_depth = demuxer->GetBitDepth();

Instantiate the video decoder using the codec ID and the bit depth.

From the |videodecode|_ sample:

.. code:: C++

  viddec = new FFMpegVideoDecoder(device_id, mem_type, rocdec_codec_id, b_force_zero_latency, p_crop_rect, b_extract_sei_messages, disp_delay);

The decoder constructor takes the following parameters:

.. list-table:: 
    :widths: 10 30 60 
    :header-rows: 1

    *   - Parameter
        - Type
        - Description 

    *   - ``device_id``
        - ``int``
        - The GPU device ID. Set it to 0 for the first device, 1 for the second device, 2 for the third device, and so on for each subsequent device.
    
    *   - ``out_mem_type``
        - ``OutputSurfaceMemoryType``
        - The memory type where the surface data, such as the decoded frames, resides. |br| |br| ``OUT_SURFACE_MEM_DEV_INTERNAL``: The surface data is stored internally on memory shared by the GPU and CPU. |br| |br| ``OUT_SURFACE_MEM_HOST_COPIED``: The surface data resides on the CPU. |br| |br| See :doc:`Surface data memory locations <../conceptual/rocDecode-memory-types>` for more information. 

    *   - ``codec``
        - ``rocDecVideoCodec``
        - The video file's codec ID extracted and converted to a ``rocDecVideoCodec_enum`` value using ``AVCodec2RocDecVideoCodec(demuxer->GetCodecID())``.

    *   - ``force_zero_latency``
        - ``bool``
        - Set to ``true`` to force flushing decoded frames for immediate display.

    *   - ``p_crop_rect``
        - ``const Rect *``
        - Defines the crop rectangle. Defaults to no crop rectangle.

    *   - ``extract_user_SEI_Message``
        - ``bool``
        - Set to ``true`` to extract Supplemental Enhancement Information (SEI) from the video stream.

    *   - ``disp_delay``
        - ``uint32_t``    
        - Delay displaying frames by this number of frames. Defaults to 0 with no delay in displaying the frames.

    *   - ``no_multithreading``
        - ``bool``    
        - Set to ``true`` to not use multithreading.

    *   - ``max_width``
        - ``int``    
        - Max width. Defaults to 0.

    *   - ``max_height``
        - ``int``  
        - Max height. Defaults to 0.

    *   - ``clk_rate``
        - ``uint32_t``    
        - Clock rate. Defaults to 1000.


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


From ``videodecode.cpp``:

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

In the decode loop, demultiplex the video stream before calling ``DecodeFrame``. Release the frames with ``ReleaseFrames`` once processing is done. 

.. code:: C++

  demuxer->Demux(&pvideo, &n_video_bytes, &pts);
  
  n_frame_returned = viddec->DecodeFrame(pvideo, n_video_bytes, pkg_flags, pts, &decoded_pics);

  for (int i = 0; i < n_frame_returned; i++) {
    viddec->ReleaseFrame(pts);


The demuxer will demultiplex frames sequentially starting at the beginning of the stream. To start the demultiplexing and decoding process from a different frame, create a demuxer seek context that specifies a seek criteria and a seek mode. 

The seek criteria describes whether the demuxer needs to seek to a specific frame or seek to a specific timestamp. The seek mode indicates whether the demuxer should seek to the exact frame or to the previous key frame. 

The seek criteria is defined by the ``SeekCriteriaEnum`` enum and the seek mode is defined by the ``SeekModeEnum`` enum. Both the ``SeekCriteriaEnum`` and the ``SeekModeEnum`` are defined in ``video_demuxer.h``.

Set the seek criteria to ``SEEK_CRITERIA_FRAME_NUM`` to seek to a frame or to ``SEEK_CRITERIA_TIME_STAMP`` to seek to a timestamp. Set the seek mode to ``SEEK_MODE_EXACT_FRAME`` to seek to the exact frame or to ``SEEK_MODE_PREV_KEY_FRAME`` to seek to the previous key frame.

From ``videodecode.cpp``:

.. code:: C++

  VideoSeekContext video_seek_ctx;
  [...]
  do {
    [...] 
    if (seek_criteria == 1 && first_frame) {
      // use VideoSeekContext class to seek to given frame number
      video_seek_ctx.seek_frame_ = seek_to_frame;
      video_seek_ctx.seek_crit_ = SEEK_CRITERIA_FRAME_NUM;            
      video_seek_ctx.seek_mode_ = (seek_mode ? SEEK_MODE_EXACT_FRAME : SEEK_MODE_PREV_KEY_FRAME);
      demuxer->Seek(video_seek_ctx, &pvideo, &n_video_bytes);
      pts = video_seek_ctx.out_frame_pts_;
      std::cout << "info: Number of frames that were decoded during seek - " << video_seek_ctx.num_frames_decoded_ << std::endl;
      first_frame = false;
    } else if (seek_criteria == 2 && first_frame) {
      // use VideoSeekContext class to seek to given timestamp
      video_seek_ctx.seek_frame_ = seek_to_frame;
      video_seek_ctx.seek_crit_ = SEEK_CRITERIA_TIME_STAMP;
      video_seek_ctx.seek_mode_ = (seek_mode ? SEEK_MODE_EXACT_FRAME : SEEK_MODE_PREV_KEY_FRAME);
      demuxer->Seek(video_seek_ctx, &pvideo, &n_video_bytes);
      pts = video_seek_ctx.out_frame_pts_;
      std::cout << "info: Duration of frame found after seek - " << video_seek_ctx.out_frame_duration_ << " ms" << std::endl;
      first_frame = false;
    } else {
      demuxer->Demux(&pvideo, &n_video_bytes, &pts);
    }    
    [...]
    n_frame_returned = viddec->DecodeFrame(pvideo, n_video_bytes, pkg_flags, pts, &decoded_pics);
    [...]
    for (int i = 0; i < n_frame_returned; i++) {
      [...]
      viddec->ReleaseFrame(pts);
    }
    [...]
  } while (n_video_bytes);


Delete the demuxer once decoding is done.

.. code:: C++

  delete demuxer;

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

.. |br| raw:: html

      </br>
