[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

<p align="center"><img width="70%" src="docs/data/AMD_rocDecode_Logo.png" /></p>

rocDecode is a high-performance video decode SDK for AMD GPUs. Using the rocDecode API, you can
access the video decoding features available on your GPU.

> [!NOTE]
> The published documentation, including installation steps, is available at [rocDecode](https://rocm.docs.amd.com/projects/rocDecode/en/latest/index.html) in an organized, easy-to-read format, with search and a table of contents. The documentation source files reside in the `rocDecode/docs` folder of this repository. As with all ROCm projects, the documentation is open source. For more information on contributing to the documentation, see [Contribute to ROCm documentation](https://rocm.docs.amd.com/en/latest/contribute/contributing.html).

After installation, the rocDecode files are found in the following locations:

* Libraries: `/opt/rocm/lib`
* Header files: `/opt/rocm/include/rocdecode`
* Samples folder: `/opt/rocm/share/rocdecode`
* Documents folder: `/opt/rocm/share/doc/rocdecode`

You can verify your installation using either the `rocdecode-test` package or by running a sample.

  ```shell
  mkdir rocdecode-test && cd rocdecode-test
  cmake /opt/rocm/share/rocdecode/test/
  ctest -VV
  ```

  ```shell
  mkdir rocdecode-sample && cd rocdecode-sample
  cmake /opt/rocm/share/rocdecode/samples/videoDecode/
  make -j8
  ./videodecode -i /opt/rocm/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
  ```

Samples are available in the [samples directory of this GitHub repository](https://github.com/ROCm/rocDecode/tree/develop/samples). Refer to the
individual folders to build and run the samples.

You can run unit tests using `make test`. To run unit tests with the verbose option, use `make test ARGS="-VV"`

  ```shell
  make test ARGS-"-VV"
  ```

You can find rocDecode Docker containers in [the `docker` directory of this GitHub repository](https://github.com/ROCm/rocDecode/tree/develop/docker).
