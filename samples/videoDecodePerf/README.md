# Video Decode Sample
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded on AMD hardware using VAAPI.

This sample supports both YUV420 8-bit and 10-bit streams. 

This sample uses multiple threads to decode the same input video parallely.

## Prerequisites:

* Linux distribution
  + Ubuntu - `20.04` / `22.04`

* [ROCm supported hardware](https://rocm.docs.amd.com/en/latest/release/gpu_os_support.html)

* Install [ROCm 5.5 or later](https://rocmdocs.amd.com/en/latest/deploy/linux/installer/install.html) with `--usecase=graphics,rocm --no-32`

* rocDecode

* CMake `3.5` or later

* [FFMPEG](https://ffmpeg.org/about.html)
  ```
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libswscale-dev
  ```

## Build
```
mkdir build
cd build
cmake ../
make -j
```
# Run 
```
./videodecodeperf -i <input video file [required]> 
                  -t <number of threads [optional - default:4]>
```