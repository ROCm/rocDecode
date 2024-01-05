# rocDecode

rocDecode is a high performance video decode SDK for AMD GPUs. rocDecode API lets developers access the video decoding features available on the GPU.

## Supported Codecs

* H.265 (HEVC) - 8 bit, and 10 bit

## Prerequisites

* Linux distribution
  + Ubuntu - `20.04` / `22.04`
  + RHEL - `8` / `9`
  + SLES - `15-SP4`

* [ROCm supported hardware](https://rocm.docs.amd.com/en/latest/release/gpu_os_support.html)

* Install [ROCm](https://rocm.docs.amd.com/en/latest/)
  + [ROCm 6.1.0 or later](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/how-to/native-install/index.html)
    * Install packages on `Ubuntu`
      ```shell
      sudo apt install rocm-hip-runtime-dev mesa-amdgpu-multimedia-devel
      ```
    * Install packages on `RHEL` / `SLES`
      ```shell
      sudo yum install rocm-hip-runtime-devel mesa-amdgpu-multimedia-devel
      ```

* CMake `3.5` or later

* libstdc++-12-dev

  ```shell
  sudo apt install libstdc++-12-dev
  ```

### Prerequisites setup script for Linux
For the convenience of the developer, we provide the setup script [rocDecode-setup.py](rocDecode-setup.py) which will install all the dependencies required by this project.

**Usage:**
```shell
  python rocDecode-setup.py  --rocm_path [ ROCm Installation Path - optional (default:/opt/rocm)]
                             --developer [ Setup Developer Options - optional (default:ON) [options:ON/OFF]]
```
**NOTE:** This script only needs to be executed once.

## Build instructions

Please follow the instructions below to build and install the rocDecode library.

```shell
 cd rocDecode
 mkdir build; cd build
 cmake ..
 make -j8
 sudo make install
```

* run tests - Requires `FFMPEG` install

  ```shell
  make test
  ```

  **NOTE:** run tests with verbose option `make test ARGS="-VV"`

* make package
  
  ```shell
  sudo make test package
  ```

## Verify Installation

The installer will copy

* Libraries into `/opt/rocm/lib`
* Header files into `/opt/rocm/include/rocdecode`
* Samples folder into `/opt/rocm/share/rocdecode`
* Documents folder into `/opt/rocm/share/doc/rocdecode`

Build and run sample

```shell
mkdir rocdecode-sample && cd rocdecode-sample
cmake /opt/rocm/share/rocdecode/samples/videoDecode/
make -j8
./videodecode -i /opt/rocm/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
```
**NOTE:** FFMPEG install required to run samples

## Samples

The tool provides a few samples to decode videos [here](samples/). Please refer to the individual folders to build and run the samples.

### Sample Prerequisites

* [FFMPEG](https://ffmpeg.org/about.html) - required to run sample applications & make test

  ```shell
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
  ```

## Docker

Docker files to build rocDecode containers are available [here](docker/)

## Documentation

Run the steps below to build documentation locally.

* Sphinx

```shell
cd docs
pip3 install -r sphinx/requirements.txt
python3 -m sphinx -T -E -b html -d _build/doctrees -D language=en . _build/html
```

* Doxygen

```shell
doxygen .Doxyfile
```

## Tested configurations

* Linux distribution
  * Ubuntu - `20.04` / `22.04`
  * RHEL - `8` / `9`
  * SLES - `15-SP4`
* ROCm:
  * rocm-core - `5.6.1.50601-93`
  * amdgpu-core - `1:5.6.50601-1649308`
* FFMPEG - `4.2.7` / `4.4.2-0`
* rocDecode Setup Script - `V1.4`
