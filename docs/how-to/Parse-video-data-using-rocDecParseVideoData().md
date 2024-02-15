## Parsing video data using rocDecParseVideoData()

Elementary stream video packets extracted from the de-multiplexer are fed into the parser using the `rocDecParseVideoData()` API. 

During this call, the parser triggers the [callbacks](https://github.com/ROCm/rocDecode/blob/c96c35d1469539596fa472c257462d87b474325b/docs/how-to/Create-parser-object-using-rocDecCreateVideoParser().md) as it encounters a new sequence header, gets a compressed frame/field data ready to be decoded, or when ready to display a frame. If any of the callbacks returns failure, it is propagated back to the application so the decoding can be terminated gracefully.