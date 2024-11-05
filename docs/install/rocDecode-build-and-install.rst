.. meta::
  :description: Build and install rocDecode with the source code
  :keywords: install, building, rocDecode, AMD, ROCm, source code, developer

********************************************************************
Building and installing rocDecode from source code
********************************************************************

These instructions are for building rocDecode from its source code. If you will not be contributing to the rocDecode code base or previewing features, :doc:`package installers <./rocDecode-package-install>` are available. 

Use the `rocDecode-setup.py <https://github.com/ROCm/rocDecode/blob/develop/rocDecode-setup.py>`_ setup script to install prerequisites:

.. code:: shell

  python3 rocDecode-setup.py  [--rocm_path ROCM_INSTALLATION_PATH; default=/opt/rocm]
                              [--runtime {ON|OFF}; default=ON]
                              [--developer {ON|OFF}; default=OFF]

.. note:: 

  Always run ``rocDecode-setup.py`` with ``--runtime ON``.  
  
  To use the rocDecode samples, set ``--developer`` to ``ON``.

Build and install rocDecode using the following commands:

.. code:: shell

  git clone https://github.com/ROCm/rocDecode.git
  cd rocDecode
  mkdir build && cd build
  cmake ../
  make -j8
  sudo make install

After installation, the rocDecode libraries will be copied to ``/opt/rocm/lib`` and the rocDecode header files will be copied to ``/opt/rocm/include/rocdecode``.

Build and install the rocDecode test module. This module is required to use the rocDecode samples, and can only be installed if ``rocDecode-setup.py`` was run with ``--developer ON``.

.. code:: shell

  mkdir rocdecode-test && cd rocdecode-test
  cmake /opt/rocm/share/rocdecode/test/
  ctest -VV

Run ``make test`` to test your build. To run the test with the verbose option, run ``make test ARGS="-VV"``. 

To create a package installer for rocDecode, run:

.. code:: shell

  sudo make package

