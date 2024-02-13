## Destroying the decoder
Users must call the `rocDecDestroyDecoder()` to destroy the session and free up resources.

The inputs to the API are:

* decoder_handle: A `RocDecoder` handler `rocDecDecoderHandle`.

The API returns a `RocdecDecodeStatus` value.
