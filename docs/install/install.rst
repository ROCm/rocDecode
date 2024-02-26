.. meta::
  :description: Install rocDecode
  :keywords: install, rocDecode, AMD, ROCm

********************************************************************
Installation
********************************************************************

rocDecode SDK is a high-performance video decode SDK for AMD GPUs. Using the rocDecode API,
you can access the video decoding features available on your GPU.

Tested configurations
========================================

* Linux

  * Ubuntu: 20.04/22.04
  * RHEL: 8/9
  * SLES: 15-SP4

* ROCm

  * rocm-core: 5.6.1.50601-93
  * amdgpu-core: 1:5.6.50601-1649308

* FFmpeg: 4.2.7/4.4.2-0

* rocDecode Setup Script: V1.4

Supported codecs
========================================

H.265 (HEVC) - 8 bit, and 10 bit

Prerequisites
========================================

* Linux distribution

  * Ubuntu: 20.04/22.04
  * RHEL: 8/9
  * SLES: 15-SP4

* `ROCm-supported hardware <https://rocm.docs.amd.com/projects/install-on-linux/en/latest/reference/system-requirements.html>`_
  (``gfx908`` or higher is required)

* Install ROCm 6.1.0 or later with
  `amdgpu-install <https://rocm.docs.amd.com/projects/install-on-linux/en/latest/how-to/amdgpu-install.html>`_

  * Run: ``--usecase=multimediasdk,rocm --no-32``
  * To install rocDecode with minimum requirements, follow the :doc:`quick-start instructions <./quick-start>`

Build from source
========================================

* CMake `3.5` or later

  .. code:: shell

   sudo apt install cmake

* `pkg-config <https://en.wikipedia.org/wiki/Pkg-config>`_

  .. code:: shell

    sudo apt install pkg-config


* `FFmpeg <https://ffmpeg.org/about.html>`_ runtime and headers - for tests and samples

  .. code:: shell

    sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev


.. note::
  All package installs are shown with the ``apt`` package manager. Use the appropriate package
  manager for your operating system.

* Ubuntu 22.04: Install ``libstdc++-12-dev``

  .. code:: shell

    sudo apt install libstdc++-12-dev


Prerequisites setup script for Linux
------------------------------------------------------------------------------------------------------------

For your convenience, we provide the setup script,
`rocDecode-setup.py <https://github.com/ROCm/rocDecode/blob/develop/rocDecode-setup.py>`_,
which installs all required dependencies. Run this script only once.

.. code:: shell

  python rocDecode-setup.py  --rocm_path [ ROCm Installation Path - optional (default:/opt/rocm)]
                            --developer [ Setup Developer Options - optional (default:ON) [options:ON/OFF]]


rocDecode package install
------------------------------------------------------------------------------------------------------------

To install rocDecode runtime, development, and test packages, run the line of code for your operating
system.

.. note::

  Package install auto installs all dependencies.

.. tab-set::

  .. tab-item:: Ubuntu

    .. code:: shell

      sudo apt install rocdecode rocdecode-dev rocdecode-test

  .. tab-item:: RHEL

    .. code:: shell

      sudo yum install rocdecode rocdecode-devel rocdecode-test

  .. tab-item:: SLES

    .. code:: shell

      sudo zypper install rocdecode rocdecode-devel rocdecode-test

* Runtime package - `rocdecode` only provides the rocdecode library `librocdecode.so`
* Development package - `rocdecode-dev`/`rocdecode-devel` provides the library, header files, and samples
* Test package - `rocdecode-test` provides ctest to verify installation

Source build and install
------------------------------------------------------------------------------------------------------------

To build rocDecode from source, run:

.. code:: shell

  git clone https://github.com/ROCm/rocDecode.git
  cd rocDecode
  mkdir build && cd build
  cmake ../
  make -j8
  sudo make install

Run tests (this requires FFmpeg dev install):

.. code:: shell

  make test

To run tests with verbose option, use ``make test ARGS="-VV"``.

Make package:

.. code:: shell

  sudo make package

Verify installation
========================================

The installer copies:

* Libraries into ``/opt/rocm/lib``
* Header files into ``/opt/rocm/include/rocdecode``
* Samples folder into ``/opt/rocm/share/rocdecode``
* Documents folder into ``/opt/rocm/share/doc/rocdecode``

.. note::

  FFmpeg dev install is required to run samples and tests.

To verify your installation using a sample application, run:

.. code:: shell

  mkdir rocdecode-sample && cd rocdecode-sample
  cmake /opt/rocm/share/rocdecode/samples/videoDecode/
  make -j8
  ./videodecode -i /opt/rocm/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4

To verify your installation using the ``rocdecode-test`` package, run:

.. code:: shell

  mkdir rocdecode-test && cd rocdecode-test
  cmake /opt/rocm/share/rocdecode/test/
  ctest -VV

This test package installs the CTest module.

Samples
========================================

You can access samples to decode your videos in our
`GitHub repository <https://github.com/ROCm/rocDecode/tree/develop/samples>`_. Refer to the
individual folders to build and run the samples.

`FFmpeg <https://ffmpeg.org/about.html>`_ is required for sample applications and ``make test``. To
install FFmpeg, refer to the instructions listed for your operating system:

.. tab-set::

  .. tab-item:: Ubuntu

    .. code:: shell

      sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev

  .. tab-item:: RHEL

    Install FFmpeg development packages manually or use the
    `rocDecode-setup.py <https://github.com/ROCm/rocDecode/blob/develop/rocDecode-setup.py>`_
    script


  .. tab-item:: SLES

    Install FFmpeg development packages manually or use the
    `rocDecode-setup.py <https://github.com/ROCm/rocDecode/blob/develop/rocDecode-setup.py>`_
    script

Docker
========================================

You can find rocDecode Docker containers in our
`GitHub repository <https://github.com/ROCm/rocDecode/tree/develop/docker>`_.

Documentation
========================================

Run the following code to build our documentation locally.

.. code:: shell

  cd docs
  pip3 install -r sphinx/requirements.txt
  python3 -m sphinx -T -E -b html -d _build/doctrees -D language=en . _build/html

For more information on documentation builds, refer to the
:doc:`Building documentation <rocm:contribute/building>` page.

rocDecode hardware capabilities
===================================================

The following table shows the codec support and capabilities of the VCN for each supported GPU
architecture.

.. csv-table::
  :header: "GPU Architecture", "VCN Generation", "Number of VCNs", "H.265/HEVC", "Max width, Max height - H.265", "H.264/AVC", "Max width, Max height - H.264"

  "gfx908 - MI1xx", "VCN 2.5.0", "2", "Yes", "4096, 2176", "No", "4096, 2160"
  "gfx90a - MI2xx", "VCN 2.6.0", "2", "Yes", "4096, 2176", "No", "4096, 2160"
  "gfx940, gfx942 - MI3xx", "VCN 3.0", "3", "Yes", "7680, 4320", "No", "4096, 2176"
  "gfx941 - MI3xx", "VCN 3.0", "4", "Yes", "7680, 4320", "No", "4096, 2176"
  "gfx1030, gfx1031, gfx1032 - Navi2x", "VCN 3.x", "2", "Yes", "7680, 4320", "No", "4096, 2176"
  "gfx1100, gfx1102 - Navi3x", "VCN 4.0", "2", "Yes", "7680, 4320", "No", "4096, 2176"
  "gfx1101 - Navi3x", "VCN 4.0", "1", "Yes", "7680, 4320", "No", "4096, 2176"
