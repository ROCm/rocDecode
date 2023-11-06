# rocDecode
rocDecode is a high performance video decode SDK for AMD hardware. rocDecode API lets developers access the video decoding features available on the GPU.

## Supported Codecs
* H.264 - 8 bit
* H.265 (HEVC) - 8 bit, 10 bit, and 12 bit

## Prerequisites:

* Linux distribution
  + Ubuntu - `20.04` / `22.04`

* [ROCm supported hardware](https://rocm.docs.amd.com/en/latest/release/gpu_os_support.html)

* Install [ROCm 5.5 or later](https://rocmdocs.amd.com/en/latest/deploy/linux/installer/install.html) with `--usecase=graphics,rocm --no-32`

* CMake `3.5` or later

* libva-dev `2.7` or later
  ```
  sudo apt install libva-dev
  ```

* libdrm-dev `2.4` or later
  ```
  sudo apt install libdrm-dev
  ```

* **Note** [rocDecode-setup.py](rocDecode-setup.py) script can be used for installing all the dependencies

## Build instructions:
Please follow the instructions below to build and install the rocDecode library.

```
 cd rocDecode
 mkdir build; cd build
 cmake ..
 make -j8
 sudo make install
```

* run tests - Requires `FFMPEG` install
  ```
  make test
  ```
  **NOTE:** run tests with verbose option `make test ARGS="-VV"`

* make package
  ```
  sudo make test package
  ```

## Verify Installation

The installer will copy

* Libraries into /opt/rocm/lib
* Header files into /opt/rocm/include/rocdecode
* Samples folder into /opt/rocm/share/rocdecode
* Documents folder into /opt/rocm/share/doc/rocdecode

Build and run sample

```
mkdir rocdecode-sample && cd rocdecode-sample
cmake /opt/rocm/share/rocdecode/samples/videoDecode/
make -j8
./videodecode -i /opt/rocm/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
```

## Samples:
The tool provides a few samples to decode videos [here](samples/). Please refer to the individual folders to build and run the samples.

### Prerequisites

* [FFMPEG](https://ffmpeg.org/about.html) - required to run sample applications & make test
  ```
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libswscale-dev
  ```

## Docker:
Docker files to build rocDecode containers are available [here](docker/)

## Documentation

Run the steps below to build documentation locally.

* Doxygen 
```
doxygen .Doxyfile
```

## Tested configurations

* Linux distribution
  + Ubuntu - `20.04` / `22.04`
* ROCm: 
  + rocm-core - `5.6.1.50601-93`
  + amdgpu-core - `1:5.6.50601-1649308`
* FFMPEG - `4.2.7` / `4.4.2-0`
* libva-dev - `2.7.0-2` / `2.14.0-1`
* libdrm-dev - `2.4.107` / `2.4.113`
* rocDecode Setup Script - `V1.2`