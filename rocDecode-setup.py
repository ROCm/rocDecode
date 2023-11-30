# Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import os
import sys
import argparse
import platform
if sys.version_info[0] < 3:
    import commands
else:
    import subprocess

__copyright__ = "Copyright 2023, AMD ROCm rocDecode"
__version__ = "1.3"
__email__ = "mivisionx.support@amd.com"
__status__ = "Shipping"

# Arguments
parser = argparse.ArgumentParser()
parser.add_argument('--rocm_path', 	type=str, default='/opt/rocm',
                    help='ROCm Installation Path - optional (default:/opt/rocm) - ROCm Installation Required')
parser.add_argument('--developer', 	type=str, default='ON',
                    help='Setup Developer Options - optional (default:ON) [options:ON/OFF]')

args = parser.parse_args()
developerInstall = args.developer.upper()

ROCM_PATH = args.rocm_path

if "ROCM_PATH" in os.environ:
    ROCM_PATH = os.environ.get('ROCM_PATH')
print("\nROCm PATH set to -- "+ROCM_PATH+"\n")

# check ROCm installation
if os.path.exists(ROCM_PATH):
    print("\nROCm Installation Found -- "+ROCM_PATH+"\n")
    os.system('echo ROCm Info -- && '+ROCM_PATH+'/bin/rocminfo')
else:
    print(
        "WARNING: If ROCm installed, set ROCm Path with \"--rocm_path\" option for full installation [Default:/opt/rocm]\n")
    print("ERROR: rocDecode Setup requires ROCm install\n")
    exit(-1)

if developerInstall not in ('OFF', 'ON'):
    print(
        "ERROR: Developer Option Not Supported - [Supported Options: OFF or ON]\n")
    exit()

# get platfrom info
platfromInfo = platform.platform()

# sudo requirement check
sudoLocation = ''
userName = ''
if sys.version_info[0] < 3:
    status, sudoLocation = commands.getstatusoutput("which sudo")
    if sudoLocation != '/usr/bin/sudo':
        status, userName = commands.getstatusoutput("whoami")
else:
    status, sudoLocation = subprocess.getstatusoutput("which sudo")
    if sudoLocation != '/usr/bin/sudo':
        status, userName = subprocess.getstatusoutput("whoami")

# setup for Linux
linuxSystemInstall = ''
linuxCMake = 'cmake'
linuxSystemInstall_check = ''
linuxFlag = ''
if "centos" in platfromInfo or "redhat" in platfromInfo or os.path.exists('/usr/bin/yum'):
    linuxSystemInstall = 'yum -y'
    linuxSystemInstall_check = '--nogpgcheck'
    if "centos-7" in platfromInfo or "redhat-7" in platfromInfo:
        linuxCMake = 'cmake3'
        os.system(linuxSystemInstall+' install cmake3')
    if not "centos" in platfromInfo or not "redhat" in platfromInfo:
        platfromInfo = platfromInfo+'-redhat'
elif "Ubuntu" in platfromInfo or os.path.exists('/usr/bin/apt-get'):
    linuxSystemInstall = 'apt-get -y'
    linuxSystemInstall_check = '--allow-unauthenticated'
    linuxFlag = '-S'
    if not "Ubuntu" in platfromInfo:
        platfromInfo = platfromInfo+'-Ubuntu'
elif os.path.exists('/usr/bin/zypper'):
    linuxSystemInstall = 'zypper -n'
    linuxSystemInstall_check = '--no-gpg-checks'
    platfromInfo = platfromInfo+'-SLES'
else:
    print("\nrocDecode Setup on "+platfromInfo+" is unsupported\n")
    print("\nrocDecode Setup Supported on: Ubuntu 20/22; CentOS 7/8; RedHat 8/9; & SLES 15 SP4\n")
    exit(-1)

# rocDecode Setup
print("\nrocDecode Setup on: "+platfromInfo+"\n")
print("\nrocDecode Dependencies Installation with rocDecode-setup.py V-"+__version__+"\n")

if userName == 'root':
    os.system(linuxSystemInstall+' update')
    os.system(linuxSystemInstall+' install sudo')

# install pre-reqs
os.system('sudo -v')
os.system(linuxSystemInstall+' update')
os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' ' +
          linuxSystemInstall_check+' install gcc cmake git wget unzip pkg-config inxi')

# rocDecode RunTime Requirements
if "Ubuntu" in platfromInfo:
    os.system('sudo -v')
    os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
              ' install vainfo libva-dev libdrm-dev libstdc++-12-dev')
else:
    os.system('sudo -v')
    os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
              ' install libva-devel libdrm-devel')

# rocDecode Dev Requirements
if developerInstall == 'ON':
    if "Ubuntu" in platfromInfo:
        os.system('sudo -v')
        os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install ffmpeg libavcodec-dev libavformat-dev libavutil-dev')
    else:
        os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
              ' install autoconf automake bzip2 bzip2-devel freetype-devel')
        os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
              ' install gcc-c++ libtool make pkgconfig zlib-devel')
        # Nasm
        os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install nasm')
        if "centos-7" in platfromInfo or "redhat-7" in platfromInfo:
            # Yasm
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install http://repo.okay.com.mx/centos/7/x86_64/release/okay-release-1-1.noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' --enablerepo=extras install epel-release')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install yasm')
            # libx264 & libx265
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install libx264-devel libx265-devel')
            # libfdk_aac
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install https://forensics.cert.org/cert-forensics-tools-release-el7.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' --enablerepo=forensics install fdk-aac')
            # libASS
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install libass-devel')
            os.system('sudo -v')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install ffmpeg')
        elif "centos-8" in platfromInfo or "redhat-8" in platfromInfo:
            # el8 x86_64 packages
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install https://download1.rpmfusion.org/free/el/rpmfusion-free-release-8.noarch.rpm https://download1.rpmfusion.org/nonfree/el/rpmfusion-nonfree-release-8.noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install http://mirror.centos.org/centos/8/PowerTools/x86_64/os/Packages/SDL2-2.0.10-2.el8.x86_64.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install ffmpeg ffmpeg-devel')
        elif "centos-9" in platfromInfo or "redhat-9" in platfromInfo:
            # el8 x86_64 packages
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install install https://dl.fedoraproject.org/pub/epel/epel-next-release-latest-9.noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install install --nogpgcheck https://mirrors.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install https://mirrors.rpmfusion.org/nonfree/el/rpmfusion-nonfree-release-$(rpm -E %rhel).noarch.rpm')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install ffmpeg ffmpeg-free-devel')
        elif "SLES" in platfromInfo:
            # FFMPEG-4 packages
            os.system(
            'sudo zypper ar -cfp 90 \'https://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_Leap_$releasever/Essentials\' packman-essentials')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                  ' install ffmpeg-4')

print("\nrocDecode Dependencies Installed with rocDecode-setup.py V-"+__version__+"\n")
