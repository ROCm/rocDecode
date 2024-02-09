# Video decode fork sample

The video decode fork sample creates multiple processes that demux and decode the same video in parallel. The demuxer uses FFMPEG to get the individual frames which are then sent to the decoder APIs. The sample uses shared memory to keep count of the number of frames decoded in the different processes. Each child process needs to exit successfully for the sample to complete successfully.

This sample shows scaling in performance for `N` VCN engines as per GPU architecture.

## Prerequisites:

* Install [rocDecode](../../README.md#build-and-install-instructions)

* [FFMPEG](https://ffmpeg.org/about.html)

    * On `Ubuntu`

  ```shell
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
  ```
  
    * On `RHEL`/`SLES` - install ffmpeg development packages manually or use [rocDecode-setup.py](../../rocDecode-setup.py) script

## Build

```shell
mkdir video_decode_fork_sample && cd video_decode_fork_sample
cmake ../
make -j
```

## Run

```shell
./videodecodefork -i <input video file [required]> 
                  -f <Number of forks ( >= 1) [optional; default:4]>
                  -d <Device ID (>= 0) [optional - default:0]>
                  -z <force_zero_latency - Decoded frames will be flushed out for display immediately [optional]>
                  -m <output_surface_memory_type - decoded surface memory [optional - default: 0][0 : OUT_SURFACE_MEM_DEV_INTERNAL/ 1 : OUT_SURFACE_MEM_DEV_COPIED/ 2 : OUT_SURFACE_MEM_HOST_COPIED]>
```