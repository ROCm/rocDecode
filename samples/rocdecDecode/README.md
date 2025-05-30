
# rocdecDecode sample

The rocdec decode sample illustrates decoding of individual frames of video elementary stream data using the rocDecoder and rocDecodeHost low level api to get the individual decoded frames in YUV format. This sample can be configured with a device ID and optionally able to dump the output to a file. This sample directly uses low-level Rocdecoder/RocDecoderHost api. This sample only works with raw elementary video frame files, not with packetized data.

## Prerequisites:

* Install [rocDecode](../../README.md#build-and-install-instructions)

* [FFMPEG](https://ffmpeg.org/about.html) for rocDecodeHost

    * On `Ubuntu`

  ```shell
  sudo apt install libavcodec-dev libavformat-dev libavutil-dev
  ```
  
    * On `RHEL`/`SLES` - install ffmpeg development packages manually or use [rocDecode-setup.py](../../rocDecode-setup.py) script

## Build

```shell
mkdir rocdec_decode_sample && cd rocdec_decode_sample
cmake ../
make -j
```

## Run

```shell
./rocdecdecode -i <input video frame file or folder containing multiple frames [required]> -b <backend> -o <outfile>
              -o <output path to save decoded YUV frames [optional]> 
              -b <backend for the decoder - 0:device 1:host [optional - default:0]>
              -d <GPU device ID - 0:device 0 / 1:device 1/ ... [optional - default:0]>
              -f <Number of decoded frames - specify the number of pictures to be decoded [optional]>
              -crop <crop rectangle for output (not used when using interopped decoded frame) [optional - default: 0,0,0,0]>
              -m <output_surface_memory_type - decoded surface memory [optional - default: 0][0 : OUT_SURFACE_MEM_DEV_INTERNAL/ 1 : OUT_SURFACE_MEM_DEV_COPIED/ 2 : OUT_SURFACE_MEM_HOST_COPIED/3 : OUT_SURFACE_MEM_NOT_MAPPED]>
```
