[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

<p align="center"><img width="70%" src="https://raw.githubusercontent.com/ROCm/rocDecode/master/docs/data/AMD_rocDecode_Logo.png" /></p>

rocDecode is a high performance video decode SDK for AMD GPUs. rocDecode API lets developers access the video decoding features available on the GPU.

## Supported codecs

* H.265 (HEVC) - 8 bit, and 10 bit

## Prerequisites

* Linux distribution
  + Ubuntu - `20.04` / `22.04`
  + RHEL - `8` / `9`
  + SLES - `15-SP4`

* [ROCm supported hardware](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/reference/system-requirements.html)

* Install ROCm `6.1.0` or later with [amdgpu-install](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/how-to/amdgpu-install.html) with `--usecase=multimediasdk,rocm --no-32`
  + **NOTE:** To install rocdecode with minimum requirements follow instructions [here](https://github.com/ROCm/rocDecode/wiki#how-can-i-install-rocdecode-runtime-with-minimum-requirements)

### To build from source

* CMake `3.5` or later

* [pkg-config](https://en.wikipedia.org/wiki/Pkg-config)

* [FFMPEG](https://ffmpeg.org/about.html) runtime and headers - for tests and samples

**NOTE:** Ubuntu 22.04 - Install `libstdc++-12-dev`
```shell
sudo apt install libstdc++-12-dev
```
#### Prerequisites setup script for Linux
For the convenience of the developer, we provide the setup script [rocDecode-setup.py](rocDecode-setup.py) which will install all the dependencies required by this project.

**Usage:**
```shell
  python rocDecode-setup.py  --rocm_path [ ROCm Installation Path - optional (default:/opt/rocm)]
                             --developer [ Setup Developer Options - optional (default:ON) [options:ON/OFF]]
```
**NOTE:** This script only needs to be executed once.

## Build and install instructions

### Package install

Install rocDecode runtime, development, and test packages. 
* Runtime package - `rocdecode` only provides the rocdecode library `librocdecode.so`
* Development package - `rocdecode-dev`/`rocdecode-devel` provides the library, header files, and samples
* Test package - `rocdecode-test` provides ctest to verify installation
**NOTE:** Package install will auto install all dependencies.

* Install packages on `Ubuntu`
```shell
sudo apt install rocdecode rocdecode-dev rocdecode-test
```
* Install packages on `RHEL`
```shell
sudo yum install rocdecode rocdecode-devel rocdecode-test
```
* Install packages on `SLES`
```shell
sudo zypper install rocdecode rocdecode-devel rocdecode-test
```

### Source build and install

```shell
git clone https://github.com/ROCm/rocDecode.git
cd rocDecode
mkdir build && cd build
cmake ../
make -j8
sudo make install
```

* run tests - Requires `FFMPEG` dev install

  ```shell
  make test
  ```
  **NOTE:** run tests with verbose option `make test ARGS="-VV"`

* make package
  
  ```shell
  sudo make package
  ```

## Verify installation

The installer will copy

* Libraries into `/opt/rocm/lib`
* Header files into `/opt/rocm/include/rocdecode`
* Samples folder into `/opt/rocm/share/rocdecode`
* Documents folder into `/opt/rocm/share/doc/rocdecode`

**NOTE:** FFMPEG dev install required to run samples and tests

### Verify with sample application

```shell
mkdir rocdecode-sample && cd rocdecode-sample
cmake /opt/rocm/share/rocdecode/samples/videoDecode/
make -j8
./videodecode -i /opt/rocm/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
```

### Verify with rocdecode-test package

Test package will install ctest module to test rocdecode. Follow below steps to test packge install

```shell
mkdir rocdecode-test && cd rocdecode-test
cmake /opt/rocm/share/rocdecode/test/
ctest -VV
```

## Samples

The tool provides a few samples to decode videos [here](samples/). Please refer to the individual folders to build and run the samples.

### Sample prerequisites

* [FFMPEG](https://ffmpeg.org/about.html) - required to run sample applications & make test

  * On `Ubuntu`
  ```shell
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
  ```
  * On `RHEL`/`SLES` - install ffmpeg development packages manually or use `rocDecode-setup.py` script

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
