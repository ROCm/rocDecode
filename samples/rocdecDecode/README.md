
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
              -d <GPU device ID - 0:device 0 / 1:device 1/ ... [optional - default:0]>
              -b <backend for the decoder - 0:device 1:host [optional - default:0]>
              -c <codec - 0 : HEVC, 1 : H264, 2: AV1, 4: VP9, 5: VP8, 6: MJPEG [optional; default: 0]>
              -n <Number of iteration - specify the number of iterations for performance evaluation [optional; default: 1]>
              -m <output_surface_memory_type - decoded surface memory [optional - default: 0][0 : OUT_SURFACE_MEM_DEV_INTERNAL/ 1 : OUT_SURFACE_MEM_DEV_COPIED/ 2 : OUT_SURFACE_MEM_HOST_COPIED/3 : OUT_SURFACE_MEM_NOT_MAPPED]>
```
