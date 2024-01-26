# Samples

rocDecode samples

## [Video decode](videoDecode)

The video decode sample illustrates decoding a single packetized video stream using FFMPEG demuxer, video parser, and rocDecoder to get the individual decoded frames in YUV format. This sample cab ne configured with a device ID and optionally able to dump the output to a file. This sample uses the high level RocVideoDecoder class which connects both the video parser and Rocdecoder. This process repeats in a loop until all frames have been decoded.

## [Video decode fork](videoDecodeFork)

The video decode fork sample creates multiple processes which demux and decode the same video in parallel. The demuxer uses FFMPEG to get the individual frames which are then sent to the decoder APIs. The sample uses shared memory to keep count of the number of frames decoded in the different processes. Each child process needs to exit successfully for the sample to complete successfully.

This sample shows scaling in performance for `N` VCN engines as per GPU architecture.

## [Video decode memory](videoDecodeMem)

The video decode memory sample illustrates a way to pass the data chunk-by-chunk sequentially to the FFMPEG demuxer which are then decoded on AMD hardware using rocDecode library.

The sample provides a user class `FileStreamProvider` derived from the existing `VideoDemuxer::StreamProvider` to read a video file and fill the buffer owned by the demuxer. It then takes frames from this buffer for further parsing and decoding.

## [Video decode multi files](videoDecodeMultiFiles)

The video decode multiple files sample illustrates the use of providing a list of files as input to showcase the reconfigure option in rocDecode library. The input video files have to be of the same codec type to use the reconfigure option but can have different resolution or resize parameters.

The reconfigure option can be disabled by the user if needed. The input file is parsed line by line and data is stored in a queue. The individual video files are demuxed and decoded one after the other in a loop. Outpuot for each individual input file can also be stored if needed.

## [Video decode performance](videoDecodePerf)

This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded on AMD hardware using rocDecode library.

This sample uses multiple threads to decode the same input video parallely.

## [Video decode RGB](videoDecodeRGB)

This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded using rocDecode API and optionally color-converted using custom HIP kernels on AMD hardware. This sample converts decoded YUV output to one of the RGB or BGR formats(24bit, 32bit, 464bit) in a separate thread allowing to run both VCN hardware and compute engine in parallel.

This sample uses HIP kernels to showcase the color conversion.  Whenever a frame is ready after decoding, the `ColorSpaceConversionThread` is notified and can be used for post-processing.