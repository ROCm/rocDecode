FROM ubuntu:22.04

# install base dependencies
RUN apt-get update -y
#RUN apt-get dist-upgrade -y
RUN DEBIAN_FRONTEND=noninteractive apt-get -y install gcc g++ cmake pkg-config git apt-utils sudo vainfo dialog libstdc++-12-dev

# install ROCm
RUN DEBIAN_FRONTEND=noninteractive apt-get -y install initramfs-tools libnuma-dev wget keyboard-configuration && \
        wget https://repo.radeon.com/amdgpu-install/6.1/ubuntu/jammy/amdgpu-install_6.1.60100-1_all.deb && \
        sudo apt-get install ./amdgpu-install_6.1.60100-1_all.deb && \
        sudo amdgpu-install -y --usecase=multimediasdk,rocm --no-32

WORKDIR /workspace

# clone rocDecode and build
RUN git clone https://github.com/ROCm/rocDecode.git && \
        cd rocDecode && mkdir build && cd build && cmake .. && make -j8 && sudo make install