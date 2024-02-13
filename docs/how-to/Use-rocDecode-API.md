# Using rocDecode API

All rocDecode APIs are exposed in the two header files: `rocdecode.h` and `rocparser.h`. Users can find these header files in the `api` folder in the rocDecode repository.

The samples use the `RocVideoDecoder` user class provided in `roc_video_dec.h` in the `utils` folder in the rocDecode repository.

## Video parser (defined in rocparser.h)

A video parser is needed to extract and decode headers from the bitstream to organize the data into a structured format required for the hardware decoder. The parser is critical in video decoding, as it controls the decoding and display of a bitstream's individual frames/fields.

The parser object in `rocparser.h` has three main APIs as described below,

- rocDecCreateVideoParser()
- rocDecParseVideoData()
- rocDecDestroyVideoParser()

