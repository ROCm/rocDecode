## Reconfiguring the decoder
The `rocDecReconfigureDecoder()` can be called to reuse a single decoder for multiple clips or when the video resolution changes during the decode. The API currently supports resolution changes, resize parameter changes, and target area parameter changes for the same codec without having to destroy the ongoing decoder instance and create a new one. This can improve performance and reduce the overall latency. 

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.
* reconfig_params: The user needs to specify the parameters for the changes in `RocdecReconfigureDecoderInfo`. The width and height used for reconfiguration cannot exceed the values set for max_width and max_height defined at RocDecoderCreateInfo. If these values need to be changed, the session needs to be destroyed and recreated.

The call to the `rocDecReconfigureDecoder()` must be called during `RocdecParserParams::pfn_sequence_callback`.
