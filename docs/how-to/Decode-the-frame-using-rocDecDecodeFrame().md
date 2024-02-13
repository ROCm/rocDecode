## Decoding the frame using rocDecDecodeFrame()

After de-multiplexing and parsing, users can decode bitstream data containing a frame/field using hardware. 

Use the `rocDecDecodeFrame()` API to submit a new frame for hardware decoding. Underneath the driver, the Video Acceleration API (VA-API) is used to submit compressed picture data to the driver. The parser extracts all the necessary information from the bitstream and fills the "RocdecPicParams" struct, which is appropriate for the codec. The high-level `RocVideoDecoder` class connects the parser and decoder used for all the sample applications.

The `rocDecDecodeFrame()` call takes the decoder handle and the pointer to the `RocdecPicParams` structure and initiates the video decoding using VA-API.

