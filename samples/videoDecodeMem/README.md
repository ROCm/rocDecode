# Video Decode Sample
This sample illustrates a way to pass the data chunk-by-chunk sequentially to the FFMPEG demuxer which are then decoded on AMD hardware using rocDecode library.

This sample supports both YUV420 8-bit and 10-bit streams.

## Prerequisites:

* Linux distribution
  + Ubuntu - `20.04` / `22.04`

* [ROCm supported hardware](https://rocm.docs.amd.com/en/latest/release/gpu_os_support.html)

* Install [ROCm 5.5 or later](https://rocmdocs.amd.com/en/latest/deploy/linux/installer/install.html) with `--usecase=graphics,rocm --no-32`

* rocDecode

* CMake `3.5` or later

* [FFMPEG](https://ffmpeg.org/about.html)
  ```
  sudo apt install ffmpeg
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
./videodecodemem -i <input video file [required]> 
              -o <output path to save decoded YUV frames [optional]> 
              -d <GPU device ID - 0:device 0 / 1:device 1/ ... [optional - default:0]>
              -z <force_zero_latency - Decoded frames will be flushed out for display immediately [optional]>
              -sei <extract SEI messages [optional]>
              -crop <crop rectangle for output (not used when using interopped decoded frame) [optional - default: 0,0,0,0]>
```