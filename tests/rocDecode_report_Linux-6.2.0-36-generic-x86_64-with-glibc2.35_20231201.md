rocDecode app report
================================

Generated: 2023-12-01 03:14:36 

Platform: b'legolas.amd.com Legolas ' (b'10.217.69.44 172.17.0.1')
--------

````
b'System:\n  Host: Legolas Kernel: 6.2.0-36-generic x86_64 bits: 64 Desktop: N/A\n    Distro: Ubuntu 22.04.3 LTS (Jammy Jellyfish)'

````
````
b'CPU:\n  Info: 16-core model: AMD Ryzen Threadripper 1950X bits: 64 type: MT MCP MCM\n    cache: L2: 8 MiB\n  Speed (MHz): avg: 2253 min/max: 2200/3400 cores: 1: 2200 2: 2200 3: 2200\n    4: 2044 5: 2200 6: 2200 7: 2200 8: 2200 9: 4075 10: 2200 11: 2200 12: 2200\n    13: 2200 14: 2200 15: 1874 16: 2200 17: 2200 18: 2200 19: 3299 20: 3400\n    21: 2200 22: 1832 23: 2800 24: 2200 25: 2200 26: 2200 27: 2200 28: 2200\n    29: 2200 30: 2200 31: 2200 32: 0'

````
````
b'Graphics:\n  Device-1: AMD Navi 21 Pro-XTA [Radeon Pro W6900X] driver: amdgpu v: kernel\n  Device-2: AMD Vega 20 [Radeon VII] driver: amdgpu v: kernel\n  Display: server: X.org v: 1.21.1.4 driver: X: loaded: amdgpu,ati\n    unloaded: fbdev,modesetting,radeon,vesa gpu: amdgpu resolution: 3840x1143\n  OpenGL: renderer: Rasterizer v: 1.4 (2.1 Mesa 10.5.4)'

````
````
b'Machine:\n  Type: Desktop System: Micro-Star product: MS-7B09 v: 1.0\n    serial: <superuser required>\n  Mobo: Micro-Star model: X399 GAMING PRO CARBON AC (MS-7B09) v: 1.0\n    serial: <superuser required> UEFI: American Megatrends v: 1.A0\n    date: 07/24/2018'

````
````
b'Memory:\n  RAM: total: 125.65 GiB used: 9.88 GiB (7.9%)\n  RAM Report:\n    permissions: Unable to run dmidecode. Root privileges required.'

````


Benchmark Report
--------


| File Name                             | Codec      | Bit Depth | Total Frames | Average decoding time per frame (ms)   | Avg FPS        |
|---------------------------------------|------------|-----------|--------------|----------------------------------------|----------------|
|                             data1.img | H.265/HEVC | 10        | 9900         | 2.74514                                | 364.281        |


Dynamic Libraries Report
-----------------

````
b'\tlinux-vdso.so.1 (0x00007ffc2a480000)\n\tlibavcodec.so.58 => /usr/local/lib/libavcodec.so.58 (0x00007fcd25c00000)\n\tlibavformat.so.58 => /usr/local/lib/libavformat.so.58 (0x00007fcd25800000)\n\tlibavutil.so.56 => /usr/local/lib/libavutil.so.56 (0x00007fcd25400000)\n\tlibswscale.so.5 => /usr/local/lib/libswscale.so.5 (0x00007fcd27096000)\n\tlibrocdecode.so.0 => /opt/rocm/lib/librocdecode.so.0 (0x00007fcd27068000)\n\tlibamdhip64.so.5 => /opt/rocm/lib/libamdhip64.so.5 (0x00007fcd23800000)\n\tlibstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007fcd23400000)\n\tlibm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007fcd26f66000)\n\tlibgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007fcd26f46000)\n\tlibc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fcd23000000)\n\tlibswresample.so.3 => /usr/local/lib/libswresample.so.3 (0x00007fcd25be1000)\n\tliblzma.so.5 => /lib/x86_64-linux-gnu/liblzma.so.5 (0x00007fcd25bb6000)\n\tlibz.so.1 => /lib/x86_64-linux-gnu/libz.so.1 (0x00007fcd25b9a000)\n\tlibfdk-aac.so.2 => /lib/x86_64-linux-gnu/libfdk-aac.so.2 (0x00007fcd256cf000)\n\tlibx264.so.163 => /lib/x86_64-linux-gnu/libx264.so.163 (0x00007fcd22c00000)\n\tlibx265.so.199 => /lib/x86_64-linux-gnu/libx265.so.199 (0x00007fcd21c00000)\n\tlibva.so.2 => /lib/x86_64-linux-gnu/libva.so.2 (0x00007fcd25b69000)\n\tlibbz2.so.1.0 => /lib/x86_64-linux-gnu/libbz2.so.1.0 (0x00007fcd25b56000)\n\tlibva-drm.so.2 => /lib/x86_64-linux-gnu/libva-drm.so.2 (0x00007fcd26f3d000)\n\tlibva-x11.so.2 => /lib/x86_64-linux-gnu/libva-x11.so.2 (0x00007fcd25b4e000)\n\tlibvdpau.so.1 => /lib/x86_64-linux-gnu/libvdpau.so.1 (0x00007fcd25b48000)\n\tlibX11.so.6 => /lib/x86_64-linux-gnu/libX11.so.6 (0x00007fcd252c0000)\n\tlibdrm.so.2 => /opt/amdgpu/lib/x86_64-linux-gnu/libdrm.so.2 (0x00007fcd25b2e000)\n\tlibamd_comgr.so.2 => /opt/rocm/lib/libamd_comgr.so.2 (0x00007fcd19000000)\n\tlibhsa-runtime64.so.1 => /opt/rocm/lib/libhsa-runtime64.so.1 (0x00007fcd18c00000)\n\tlibnuma.so.1 => /lib/x86_64-linux-gnu/libnuma.so.1 (0x00007fcd25b21000)\n\t/lib64/ld-linux-x86-64.so.2 (0x00007fcd2712a000)\n\tlibXext.so.6 => /lib/x86_64-linux-gnu/libXext.so.6 (0x00007fcd25b0c000)\n\tlibXfixes.so.3 => /lib/x86_64-linux-gnu/libXfixes.so.3 (0x00007fcd25b04000)\n\tlibxcb.so.1 => /lib/x86_64-linux-gnu/libxcb.so.1 (0x00007fcd25ada000)\n\tlibtinfo.so.6 => /lib/x86_64-linux-gnu/libtinfo.so.6 (0x00007fcd25aa8000)\n\tlibelf.so.1 => /lib/x86_64-linux-gnu/libelf.so.1 (0x00007fcd25a8a000)\n\tlibdrm_amdgpu.so.1 => /opt/amdgpu/lib/x86_64-linux-gnu/libdrm_amdgpu.so.1 (0x00007fcd25a7a000)\n\tlibXau.so.6 => /lib/x86_64-linux-gnu/libXau.so.6 (0x00007fcd25a74000)\n\tlibXdmcp.so.6 => /lib/x86_64-linux-gnu/libXdmcp.so.6 (0x00007fcd25a6c000)\n\tlibbsd.so.0 => /lib/x86_64-linux-gnu/libbsd.so.0 (0x00007fcd256b7000)\n\tlibmd.so.0 => /lib/x86_64-linux-gnu/libmd.so.0 (0x00007fcd256aa000)'

````



---
**Copyright AMD ROCm rocDecode app 2023 -- run_rocDecode_tests.py V-1.0**

