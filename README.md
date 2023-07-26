# rocDecode
rocDecode is a high performance video decode SDK for AMD hardware

## Prerequisites:

* One of the supported GPUs by ROCm: [AMD Radeon&trade; Graphics](https://docs.amd.com/bundle/Hardware_and_Software_Reference_Guide/page/Hardware_and_Software_Support.html)
* Linux distribution
  + **Ubuntu** - `20.04` / `22.04`
* Install [ROCm5.5 or later](https://docs.amd.com)
  + **Note** - both graphics and rocm use-cases must be installed (i.e., sudo amdgpu-install --usecase=graphics,rocm).
* CMake 3.0 or later
* libva-dev 2.7 or later
* [FFMPEG n4.4.2 or later](https://github.com/FFmpeg/FFmpeg/releases/tag/n4.4.2)

* **Note** [vcnDECODE-setup.py](vcnDECODE-setup.py) script can be used for installing all the dependencies

## Build instructions:
Please follow the instructions below to build and install the vcndecode library.
```
 cd rocDecode
 mkdir build; cd build
 cmake ..
 make -j8
 sudo make install
```

## Samples:
The tool provides a few samples to decode videos [here](samples/). Please refer to the individual folders to build and run the samples.

## Docker:
Docker files to build vcnDECODE containers are available [here](docker/)