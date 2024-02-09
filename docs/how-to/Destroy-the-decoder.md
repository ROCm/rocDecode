## Destroying the decoder
The user needs to call the `rocDecDestroyDecoder()` to destroy the session and free up resources.

The inputs to the API are:
* decoder_handle: A rocdec decoder handler `rocDecDecoderHandle`.

The API returns a `RocdecDecodeStatus` value.
