# Using rocDecode API

All rocDecode APIs are exposed in the two header files: `rocdecode.h` and `rocparser.h`. These headers can be found under the `api` folder in this repository.

The samples use the `RocVideoDecoder` user class provided in `roc_video_dec.h` under the `utils` folder in this repository.

## Video parser (defined in rocparser.h)

A Video Parser is needed to extract and decode headers from the bitstream to organize the data into a structured format required for the hardware decoder. The parser plays an important role in video decoding as it controls the decoding and display of the individual frames/fields of a bitstream.

The parser object in `rocparser.h` has 3 main apis as described below

## Creating parser object using rocDecCreateVideoParser()

This API creates a video parser object for the Codec specified by the user. The API takes `max_num_decode_surfaces` which determines the DPB (Decoded Picture Buffer) size for decoding. When creating a parser object, the application must register certain callback functions with the driver which will be called from the parser during the decode.

* `pfn_sequence_callback` will be called when the parser encounters a new sequence header. The parser informs the user of the minimum number of surfaces needed by the parser's DPB for successful decoding of the bitstream. In addition, the caller can set additional parameters like `max_display_delay` to control the decoding and displaying of the frames.
* `pfn_decode_picture` callback function will be triggered when a picture is ready to be decoded.
* `pfn_display_picture` callback function will be triggered when a frame in display order is ready to be consumed by the caller. 
* `pfn_get_sei_msg` callback function will be triggered when a user SEI message is parsed by the parser and sent back to the caller.

## Parsing video data using rocDecParseVideoData()

Elementary stream video packets extracted from the demultiplexer are fed into the parser using the `rocDecParseVideoData()` API. During this call, the parser triggers the above callbacks as it encounters a new sequence header, gets a compressed frame/field data ready to be decoded, or when it is ready to display a frame. If any of the callbacks returns failure, it will be propagated back to the application so the decoding can be terminated gracefully.

## Destroying the parser using rocDecDestroyVideoParser()

The user needs to call `rocDecDestroyVideoParser()` to destroy the parser object and free up all allocated resources at the end of video decoding.

## Querying decode capabilities using rocDecGetDecoderCaps() (defined in rocdecode.h)

`rocDecGetDecoderCaps()` Allows users to query the capabilities of underlying hardware video decoder as different hardware will have different capabilities. Caps usually inform the user of the supported codecs, max. resolution, bit-depth, etc.

The following pseudo-code illustrates the use of this API. If any of the decoder caps are not supported, the application is supposed to handle the error appropriately.

    RocdecDecodeCaps decode_caps;
    memset(&decode_caps, 0, sizeof(decode_caps));
    decode_caps.codec_type = p_video_format->codec;
    decode_caps.chroma_format = p_video_format->chroma_format;
    decode_caps.bit_depth_minus_8 = p_video_format->bit_depth_luma_minus8;

    ROCDEC_API_CALL(rocDecGetDecoderCaps(&decode_caps));

    if(!decode_caps.is_supported) {
        ROCDEC_THROW("Rocdec:: Codec not supported on this GPU: ", ROCDEC_NOT_SUPPORTED);
        return 0;
    }

    if ((p_video_format->coded_width > decode_caps.max_width) ||
        (p_video_format->coded_height > decode_caps.max_height)) {

        std::ostringstream errorString;
        errorString << std::endl
                    << "Resolution          : " << p_video_format->coded_width << "x" << p_video_format->coded_height << std::endl
                    << "Max Supported (wxh) : " << decode_caps.max_width << "x" << decode_caps.max_height << std::endl
                    << "Resolution not supported on this GPU ";

        const std::string cErr = errorString.str();
        ROCDEC_THROW(cErr, ROCDEC_NOT_SUPPORTED);
        return 0;
    }

## Creating a Decoder using rocDecCreateDecoder()
Creates an instance of the hardware video decoder object and gives a handle to the user on successful creation. Refer to `RocDecoderCreateInfo` structure for information about parameters that are passed for creating the decoder. E.g. `RocDecoderCreateInfo::codec_type`  represents the codec type of the video. The decoder handle returned by the `rocDecCreateDecoder()` must be retained for the entire session of the decode because the handle is passed along with the other decoding APIs. In addition, users can inform display or crop dimensions along with this API. 

## Decoding the frame using rocDecDecodeFrame()
After de-muxing and parsing, the next step is to decode bitstream data containing a frame/field using hardware. `rocDecDecodeFrame()` API is to submit a new frame for hardware decoding. Underneath the driver, VA-API is used to submit compressed picture data to the driver. The parser extracts all the necessary information from the bitstream and fills the "RocdecPicParams" struct which is appropriate for the codec used. The high-level `RocVideoDecoder` class connects the parser and decoder which is used for all the sample applications.
The `rocDecDecodeFrame()` call takes the decoder handle and the pointer to RocdecPicParams structure and kicks off video decoding using VA-API.

## Preparing the decoded frame for further processing
The decoded frames can be used for further postprocessing using the `rocDecGetVideoFrame()` API call. The successful completion of `rocDecGetVideoFrame()` indicates that the decoding process is completed and the device memory pointer is inter-opped into ROCm HIP address space for further processing of the decoded frame in device memory. The caller will get all the necessary information on the output surface like YUV format, dimensions, pitch, etc. from this call. In the high-level `RocVideoDecoder` class, we provide 4 different surface type modes for the mapped surface as specified in OutputSurfaceMemoryType as explained below.

    typedef enum OutputSurfaceMemoryType_enum {
        OUT_SURFACE_MEM_DEV_INTERNAL = 0,      /**<  Internal interopped decoded surface memory **/
        OUT_SURFACE_MEM_DEV_COPIED = 1,        /**<  decoded output will be copied to a separate device memory **/
        OUT_SURFACE_MEM_HOST_COPIED = 2        /**<  decoded output will be copied to a separate host memory **/
        OUT_SURFACE_MEM_NOT_MAPPED = 3         /**<  decoded output is not available (interop won't be used): useful for decode only performance app*/
    } OutputSurfaceMemoryType;

If the mapped surface type is `OUT_SURFACE_MEM_DEV_INTERNAL`, the direct pointer to the decoded surface is given to the user. The user is supposed to trigger `rocDecUnMapVideoFrame()` using the `ReleaseFrame()` call of the RocVideoDecoder class. If the requested surface type `OUT_SURFACE_MEM_DEV_COPIED` or `OUT_SURFACE_MEM_HOST_COPIED`, the internal decoded frame will be copied to another buffer either in device memory or host memory. After that, it is immediately unmapped for re-use by the RocVideoDecoder class.
In all the cases, the user needs to call `rocDecUnMapVideoFrame()` to indicate that the frame is ready to be used for decoding again.
Please refer to the RocVideoDecoder class and samples for detailed use of these APIs.

## Querying the decoding status
The `rocDecGetDecodeStatus()` can be called at any time to query the status of the decoding for a given frame. A struct pointer `RocdecDecodeStatus*` will be filled and returned to the user.

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.
* pic_idx: An int value for the picIdx for which the user wants a status.
* decode_status: A pointer to `RocdecDecodeStatus` as a return value.

The API returns one of the following statuses:
* Invalid (0): Decode status is not valid.
* In Progress (1): Decoding is in progress.
* Success (2): Decoding was successful and no errors were returned.
* Error (8): The frame was corrupted, but the error was not concealed.
* Error Concealed (9): The frame was corrupted and the error was concealed.
* Displaying (10):  Decode is completed, displaying in progress.

## Reconfiguring the decoder
The `rocDecReconfigureDecoder()` can be called to reuse a single decoder for multiple clips or when the video resolution changes during the decode. The API currently supports resolution changes, resize parameter changes, and target area parameter changes for the same codec without having to destroy the ongoing decoder instance and create a new one. This can improve performance and reduce the overall latency. 

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.
* reconfig_params: The user needs to specify the parameters for the changes in `RocdecReconfigureDecoderInfo`. The width and height used for reconfiguration cannot exceed the values set for max_width and max_height defined at RocDecoderCreateInfo. If these values need to be changed, the session needs to be destroyed and recreated.

The call to the `rocDecReconfigureDecoder()` must be called during `RocdecParserParams::pfn_sequence_callback`.

## Destroying the decoder
The user needs to call the `rocDecDestroyDecoder()` to destroy the session and free up resources.

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.

The API returns a `RocdecDecodeStatus` value.