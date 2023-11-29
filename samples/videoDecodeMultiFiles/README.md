# Video Decode Multi Files Sample
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded on AMD hardware using VAAPI.

This sample supports both YUV420 8-bit and 10-bit streams.

This sample takes multiple files as a list and decodes each of them one after the other.

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

* Example input file list - example.txt

```
infile input1.[mp4/mov...] [required]
outfile output1.yuv [optional]
z 0 [optional]
sei 0 [optional]
crop l,t,r,b [optional]
infile input2.[mp4/mov...] [optional]
outfile output2.yuv [optional]
...
...
```

```
./videodecodefiles -i <input file list[required - example.txt]>
              -d <GPU device ID - 0:device 0 / 1:device 1/ ... [optional - default:0]>
```