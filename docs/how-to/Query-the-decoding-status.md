## Querying the decoding status
After submitting a frame for decoding, `rocDecGetDecodeStatus() can be called to query the status of the decoding for a given frame. A struct pointer `RocdecDecodeStatus*` is filled and returned to the user.

The inputs to the API are:
* decoder_handle: A rocDecoder handler `rocDecDecoderHandle`.
* pic_idx: An int value for the picIdx for which the user wants a status.
* decode_status: A pointer to `RocdecDecodeStatus` as a return value.

The API returns one of the following statuses:
* Invalid (0): Decode status is not valid.
* In Progress (1): Decoding is in progress.
* Success (2): Decoding was successful and no errors were returned.
* Error (8): The frame was corrupted, but the error was not concealed.
* Error Concealed (9): The frame was corrupted and the error was concealed.
* Displaying (10):  Decode is completed, displaying in progress.
