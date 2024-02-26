# rocDecode quick-start installation

To install the rocDecode runtime with minimum requirements, follow these steps:

1. Install core ROCm components (ROCm 6.1.0 or later) using the
   {doc}`native package manager<rocm:how-to/native-install/index>` installation instructions.

    * Register repositories
    * Register kernel-mode driver
    * Register ROCm packages
    * Install kernel driver (`amdgpu-dkms`)--only required on bare metal install. Docker runtime uses the
        base `dkms` package irrespective of the version installed.

2. Install rocDecode runtime package. rocDecode only provides the `librocdecode.so` library (the
    runtime package only installs the required core dependencies).

    ::::{tab-set}

    :::{tab-item} Ubuntu

    ```shell
        sudo apt install rocdecode
    ```

    :::

    :::{tab-item} RHEL

    ```shell
    sudo yum install rocdecode
    ```

    :::

    :::{tab-item} SLES

    ```shell
    sudo zypper install rocdecode
    ```

    :::
    ::::
