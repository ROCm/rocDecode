[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

<p align="center"><img width="70%" src="docs/data/AMD_rocDecode_Logo.png" /></p>

rocDecode SDK is a high-performance video decode SDK for AMD GPUs. Using the rocDecode API,
you can access the video decoding features available on your GPU.

## Tested configurations

* Linux
  * Ubuntu - `20.04` / `22.04`
  * RHEL - `8` / `9`
  * SLES - `15-SP4`
* ROCm:
  * rocm-core - `6.1.0.60100-28`
  * amdgpu-core - `1:6.1.60100-1731559`
* FFmpeg - `4.2.7` / `4.4.2-0`
* rocDecode Setup Script - `V1.4`

## Supported codecs

* H.265 (HEVC) - 8 bit, and 10 bit

## Prerequisites

* Linux distribution
  * Ubuntu - `20.04` / `22.04`
  * RHEL - `8` / `9`
  * SLES - `15-SP4`

* [ROCm-supported hardware](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/reference/system-requirements.html)
  * **NOTE:** `gfx908` or higher required

* Install ROCm `6.1.0` or later with
  [amdgpu-install](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/how-to/amdgpu-install.html)
  * Run: `--usecase=multimediasdk,rocm --no-32`
  * **NOTE:** To install rocDecode with minimum requirements, follow the instructions [here](https://github.com/ROCm/rocDecode/wiki#how-can-i-install-rocdecode-runtime-with-minimum-requirements)

### To build from source

* CMake `3.5` or later

  ```shell
  sudo apt install cmake
  ```

* [pkg-config](https://en.wikipedia.org/wiki/Pkg-config)

  ```shell
  sudo apt install pkg-config
  ```

* [FFmpeg](https://ffmpeg.org/about.html) runtime and headers - for tests and samples

  ```shell
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
  ```

>[!NOTE]
> All package installs are shown with the `apt` package manager. Use the appropriate package
> manager for your operating system.

* Ubuntu 22.04 - Install `libstdc++-12-dev`

  ```shell
  sudo apt install libstdc++-12-dev
  ```

#### Prerequisites setup script for Linux


For your convenience, we provide the setup script,
[rocDecode-setup.py](https://github.com/ROCm/rocDecode/blob/develop/rocDecode-setup.py),
which installs all required dependencies.

```shell
  python rocDecode-setup.py  --rocm_path [ ROCm Installation Path - optional (default:/opt/rocm)]
                             --developer [ Setup Developer Options - optional (default:ON) [options:ON/OFF]]
```

>[!NOTE]
>Run this script only once.

### rocDecode package install

To install rocDecode runtime, development, and test packages, run the line of code for your operating
system.

>[!NOTE]
> Package install auto installs all dependencies.

* Ubuntu

  ```shell
  sudo apt install rocdecode rocdecode-dev rocdecode-test
  ```

* RHEL

  ```shell
  sudo yum install rocdecode rocdecode-devel rocdecode-test
  ```

* SLES

  ```shell
  sudo zypper install rocdecode rocdecode-devel rocdecode-test
  ```

* Runtime package - `rocdecode` only provides the rocdecode library `librocdecode.so`
* Development package - `rocdecode-dev`/`rocdecode-devel` provides the library, header files, and samples
* Test package - `rocdecode-test` provides ctest to verify installation

### Source build and install

To build rocDecode from source, run:

```shell
git clone https://github.com/ROCm/rocDecode.git
cd rocDecode
mkdir build && cd build
cmake ../
make -j8
sudo make install
```

* Run tests (this requires `FFMPEG` dev install):

  ```shell
  make test
  ```

  To run tests with verbose option, use `make test ARGS="-VV"`.

* Make package:

  ```shell
  sudo make package
  ```

## Verify installation

The installer copies:

* Libraries into `/opt/rocm/lib`
* Header files into `/opt/rocm/include/rocdecode`
* Samples folder into `/opt/rocm/share/rocdecode`
* Documents folder into `/opt/rocm/share/doc/rocdecode`

>[!NOTE]
> FFmpeg dev install is required to run samples and tests.

* To verify your installation using a sample application, run:

  ```shell
  mkdir rocdecode-sample && cd rocdecode-sample
  cmake /opt/rocm/share/rocdecode/samples/videoDecode/
  make -j8
  ./videodecode -i /opt/rocm/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
  ```

* To verify your installation using the `rocdecode-test` package, run:

  ```shell
  mkdir rocdecode-test && cd rocdecode-test
  cmake /opt/rocm/share/rocdecode/test/
  ctest -VV
  ```

  This test package installs the CTest module.

## Samples

You can access samples to decode your videos in our
[GitHub repository](https://github.com/ROCm/rocDecode/tree/develop/samples). Refer to the
individual folders to build and run the samples.

[FFmpeg](https://ffmpeg.org/about.html) is required for sample applications and `make test`. To install
FFmpeg, refer to the instructions listed for your operating system:

* Ubuntu:

  ```shell
  sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev
  ```

* `RHEL`/`SLES`:

  Install ffmpeg development packages manually or use `rocDecode-setup.py` script

## Docker

You can find rocDecode Docker containers in our
[GitHub repository](https://github.com/ROCm/rocDecode/tree/develop/docker).

## Documentation

Run the following code to build our documentation locally.

```shell
cd docs
pip3 install -r sphinx/requirements.txt
python3 -m sphinx -T -E -b html -d _build/doctrees -D language=en . _build/html
```

For more information on documentation builds, refer to the
{doc}`Building documentation <rocm:contribute/building>` page.
