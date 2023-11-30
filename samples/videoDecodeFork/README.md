# Video Decode Sample
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded on AMD hardware using rocDecode library. 

This sample uses fork() to create multiple processes to decode the same input video parallely.

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
./videodecodefork -i <input video file [required]> 
                  -f <Number of forks ( >= 1) [optional; default:4]>
                  -d <Device ID (>= 0) [optional - default:0]>
                  -z <force_zero_latency - Decoded frames will be flushed out for display immediately [optional]>
                  -m <output_surface_memory_type - decoded surface memory [optional - default: 0][0 : OUT_SURFACE_MEM_DEV_INTERNAL/ 1 : OUT_SURFACE_MEM_DEV_COPIED/ 2 : OUT_SURFACE_MEM_HOST_COPIED]>
```