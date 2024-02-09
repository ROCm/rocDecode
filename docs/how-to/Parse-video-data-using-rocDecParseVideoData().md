## Parsing video data using rocDecParseVideoData()

Elementary stream video packets extracted from the demultiplexer are fed into the parser using the `rocDecParseVideoData()` API. During this call, the parser triggers the above callbacks as it encounters a new sequence header, gets a compressed frame/field data ready to be decoded, or when it is ready to display a frame. If any of the callbacks returns failure, it will be propagated back to the application so the decoding can be terminated gracefully.
