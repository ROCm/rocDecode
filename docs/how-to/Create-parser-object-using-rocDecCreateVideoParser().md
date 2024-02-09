
## Creating parser object using rocDecCreateVideoParser()

This API creates a video parser object for the Codec specified by the user. The API takes `max_num_decode_surfaces` which determines the DPB (Decoded Picture Buffer) size for decoding. When creating a parser object, the application must register certain callback functions with the driver which will be called from the parser during the decode.

* `pfn_sequence_callback` will be called when the parser encounters a new sequence header. The parser informs the user of the minimum number of surfaces needed by the parser's DPB for successful decoding of the bitstream. In addition, the caller can set additional parameters like `max_display_delay` to control the decoding and displaying of the frames.
* `pfn_decode_picture` callback function will be triggered when a picture is ready to be decoded.
* `pfn_display_picture` callback function will be triggered when a frame in display order is ready to be consumed by the caller. 
* `pfn_get_sei_msg` callback function will be triggered when a user SEI message is parsed by the parser and sent back to the caller.
