# rocDecode changelog

Documentation for rocDecode is available at
[https://rocm.docs.amd.com/projects/rocDecode/en/latest/](https://rocm.docs.amd.com/projects/rocDecode/en/latest/)

## rocDecode 0.7.0

### Additions

* Clang - Default CXX compiler

### Optimizations

* Setup Script - Build and runtime install options

### Changes

* CTest - Core tests for make test and package test

### Fixes

* Sample - Bugfix for videoDecodeBatch

### Tested configurations

* Linux
  * Ubuntu - `20.04` / `22.04`
  * RHEL - `8` / `9`
  * SLES - `15 SP5`
* ROCm:
  * rocm-core - `6.2.0.60200-66`
  * amdgpu-core - `1:6.2.60200-2009582`
* libva-dev - `2.7.0-2` / `2.14.0-1`
* mesa-amdgpu-va-drivers - `1:24.2.0.60200-2009582`
* mesa-amdgpu-dri-drivers - `24.1.0.60200`
* FFmpeg - `4.2.7` / `4.4.2-0`
* rocDecode Setup Script - `V2.2.0`


## rocDecode 0.6.0

### Additions

* FFMPEG V5.X Support
* Mariner - Build Support

### Optimizations

* Setup Script - Error Check install

### Changes

* Dependencies - Updates to core dependencies
* LibVA Headers - Use public headers
* mesa-amdgpu-va-drivers - RPM Package available on RPM from ROCm 6.2

### Fixes

* Package deps
* RHEL/SLES - Additional required packages `mesa-amdgpu-dri-drivers libdrm-amdgpu`

### Tested configurations

* Linux
  * Ubuntu - `20.04` / `22.04`
  * RHEL - `8` / `9`
* ROCm:
  * rocm-core - `6.1.0.60100-64`
  * amdgpu-core - `1:6.1.60100-1741643`
* libva-dev - `2.7.0-2` / `2.14.0-1`
* mesa-amdgpu-va-drivers - `1:24.1.0`
* mesa-amdgpu-dri-drivers - `24.1.0.60200`
* FFmpeg - `4.2.7` / `4.4.2-0`
* rocDecode Setup Script - `V2.1.0`

## rocDecode 0.5.0

### Changes

* Changed setup updates
* Added AMDGPU package support
* Optimized package dependencies
* Updated README

### Fixes

* Minor bug fix and updates

### Tested configurations

* Linux distribution
  * Ubuntu - `20.04` / `22.04`
* ROCm:
  * rocm-core - `6.1.0.60100-28`
  * amdgpu-core - `1:6.1.60100-1731559`
* FFMPEG - `4.2.7` / `4.4.2-0`
* rocDecode Setup Script - `V1.4`

## rocDecode 0.4.0

### Changes

* Added CTest - Tests for install verification
* Added Doxygen - Support for API documentation
* Changed setup updates
* Optimized CMakeList Cleanup
* Added README

### Fixes

* Minor bug fix and updates

### Tested configurations

* Linux distribution
  * Ubuntu - `20.04` / `22.04`
* ROCm:
  * rocm-core - `5.6.1.50601-93`
  * amdgpu-core - `1:5.6.50601-1649308`
* FFMPEG - `4.2.7` / `4.4.2-0`
* libva-dev - `2.7.0-2` / `2.14.0-1`
* rocDecode Setup Script - `V1.1`
