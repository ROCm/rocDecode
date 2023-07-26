FROM ubuntu:20.04

# install base dependencies
RUN apt-get update -y
#RUN apt-get dist-upgrade -y
RUN DEBIAN_FRONTEND=noninteractive apt-get -y install gcc g++ cmake pkg-config git apt-utils sudo vainfo dialog

RUN DEBIAN_FRONTEND=noninteractive dpkg --add-architecture i386

# install ROCm
RUN DEBIAN_FRONTEND=noninteractive apt-get -y install initramfs-tools libnuma-dev wget keyboard-configuration && \
        wget https://repo.radeon.com/amdgpu-install/5.5/ubuntu/focal/amdgpu-install_5.5.50500-1_all.deb && \
        sudo apt-get install ./amdgpu-install_5.5.50500-1_all.deb && \
        sudo amdgpu-install -y --usecase=graphics,rocm

# install FFMPEG, and other dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get -y install autoconf automake build-essential cmake git-core libass-dev libfreetype6-dev libsdl2-dev libtool libva-dev \
        libvdpau-dev libvorbis-dev libxcb1-dev libxcb-shm0-dev libxcb-xfixes0-dev pkg-config texinfo wget zlib1g-dev \
        nasm yasm libx264-dev libx265-dev libnuma-dev libfdk-aac-dev unzip && \
        wget https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n4.4.2.zip && unzip n4.4.2.zip && cd FFmpeg-n4.4.2/ && sudo ldconfig && \
        export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig/" && \
        ./configure --enable-shared --disable-static && \
        make -j8 && sudo make install && cd

ENV LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib64/:/usr/local/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"

WORKDIR /workspace

