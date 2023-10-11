# Video Decode Performance Sample with Fork()
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded on AMD hardware using VAAPI. This sample supports both YUV420 8-bit and 10-bit streams. This sample uses fork() to create multiple processes to decode the same input video parallely.

## Build and run the sample:
```
mkdir build
cd build
cmake ..
make -j
./videodecodefork -i <input video file - required> -t <optional; number of threads. default: 4>
```
