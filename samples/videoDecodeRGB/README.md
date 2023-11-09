# Video Decode Sample
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded and optionally color-converted on AMD hardware using rocDecode API. This sample supports both YUV420 8-bit and 10-bit streams.

## Build and run the sample:
```
mkdir build
cd build
cmake ..
make -j
./videodecodergb -i <input video file - required> -o <optional; output path to save decoded YUV frames> -d <GPU device ID, 0 for the first device, 1 for the second device, etc> -of <optional: output format bgr, bgra, bgr48, bgr64 etc>
```