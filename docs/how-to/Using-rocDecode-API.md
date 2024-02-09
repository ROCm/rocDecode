# Using rocDecode API

All rocDecode APIs are exposed in the two header files: `rocdecode.h` and `rocparser.h`. These headers can be found under the `api` folder in this repository.

The samples use the `RocVideoDecoder` user class provided in `roc_video_dec.h` under the `utils` folder in this repository.

## Video parser (defined in rocparser.h)

A video parser is needed to extract and decode headers from the bitstream to organize the data into a structured format required for the hardware decoder. The parser plays an important role in video decoding as it controls the decoding and display of the individual frames/fields of a bitstream.

The parser object in `rocparser.h` has three main apis as described below,

- rocDecCreateVideoParser()
- rocDecParseVideoData()
- rocDecDestroyVideoParser()

