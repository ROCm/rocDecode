## Querying decode capabilities using rocDecGetDecoderCaps() (defined in rocdecode.h)

The `rocDecGetDecoderCaps()` API allows users to query the capabilities of underlying hardware video decoder as different hardware will have different capabilities. Decoder capabilities usually inform the user of the supported codecs, max. resolution, bit-depth, and others.

The following pseudo-code illustrates the use of this API. The application handles the error appropriately for non-supported decoder capabilities.


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
