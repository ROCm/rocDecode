# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.
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
import traceback
if sys.version_info[0] < 3:
    import commands
else:
    import subprocess

__copyright__ = "Copyright (c) 2023 - 2024, AMD ROCm rocDecode"
__version__ = "2.0.0"
__email__ = "mivisionx.support@amd.com"
__status__ = "Shipping"

# error check calls
def ERROR_CHECK(call):
    status = call
    if(status != 0):
        print('ERROR_CHECK failed with status:'+str(status))
        traceback.print_stack()
        exit(status)

# Arguments
parser = argparse.ArgumentParser()
parser.add_argument('--rocm_path', 	type=str, default='/opt/rocm',
                    help='ROCm Installation Path - optional (default:/opt/rocm) - ROCm Installation Required')
parser.add_argument('--developer', 	type=str, default='OFF',
                    help='Setup Developer Options - optional (default:OFF) [options:ON/OFF]')

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

# check os version
os_info_data = 'NOT Supported'
if os.path.exists('/etc/os-release'):
    with open('/etc/os-release', 'r') as os_file:
        os_info_data = os_file.read().replace('\n', ' ')
        os_info_data = os_info_data.replace('"', '')

# setup for Linux
linuxSystemInstall = ''
linuxCMake = 'cmake'
linuxSystemInstall_check = ''
linuxFlag = ''
sudoValidateOption= '-v'
if "centos" in os_info_data or "redhat" in os_info_data:
    linuxSystemInstall = 'yum -y'
    linuxSystemInstall_check = '--nogpgcheck'
    if "VERSION_ID=7" in os_info_data:
        linuxCMake = 'cmake3'
        platfromInfo = platfromInfo+'-redhat-7'
    elif "VERSION_ID=8" in os_info_data:
        platfromInfo = platfromInfo+'-redhat-8'
    elif "VERSION_ID=9" in os_info_data:
        platfromInfo = platfromInfo+'-redhat-9'
    else:
        platfromInfo = platfromInfo+'-redhat-centos-undefined-version'
elif "Ubuntu" in os_info_data:
    linuxSystemInstall = 'apt-get -y'
    linuxSystemInstall_check = '--allow-unauthenticated'
    linuxFlag = '-S'
    if "VERSION_ID=20" in os_info_data:
        platfromInfo = platfromInfo+'-Ubuntu-20'
    elif "VERSION_ID=22" in os_info_data:
        platfromInfo = platfromInfo+'-Ubuntu-22'
    elif "VERSION_ID=24" in os_info_data:
        platfromInfo = platfromInfo+'-Ubuntu-24'
    else:
        platfromInfo = platfromInfo+'-Ubuntu-undefined-version'
elif "SLES" in os_info_data:
    linuxSystemInstall = 'zypper -n'
    linuxSystemInstall_check = '--no-gpg-checks'
    platfromInfo = platfromInfo+'-SLES'
elif "Mariner" in os_info_data:
    linuxSystemInstall = 'tdnf -y'
    linuxSystemInstall_check = '--nogpgcheck'
    platfromInfo = platfromInfo+'-Mariner'
else:
    print("\nrocDecode Setup on "+platfromInfo+" is unsupported\n")
    print("\nrocDecode Setup Supported on: Ubuntu 20/22, RedHat 8/9, & SLES 15\n")
    exit(-1)

# rocDecode Setup
print("\nrocDecode Setup on: "+platfromInfo+"\n")
print("\nrocDecode Dependencies Installation with rocDecode-setup.py V-"+__version__+"\n")

if userName == 'root':
    ERROR_CHECK(os.system(linuxSystemInstall+' update'))
    ERROR_CHECK(os.system(linuxSystemInstall+' install sudo'))

# source install - common package dependencies
commonPackages = [
    'gcc',
    'cmake',
    'git',
    'wget',
    'unzip',
    'pkg-config',
    'inxi',
    'rocm-hip-runtime'
]

# Debian packages
coreDebianPackages = [
    'rocm-hip-runtime-dev',
    'libva2',
    'libva-dev',
    'libdrm-amdgpu1',
    'mesa-amdgpu-va-drivers',
    'vainfo'
]
coreDebianU22Packages = [
    'libstdc++-12-dev'
]
ffmpegDebianPackages = [
    'ffmpeg',
    'libavcodec-dev',
    'libavformat-dev',
    'libavutil-dev'
]

# RPM Packages
libvaNameRPM = "libva"
libvaUtilsNameRPM = "libva-utils"
if "SLES" in os_info_data or "Mariner" in os_info_data:
    libvaNameRPM = "libva2"
if "Mariner" in os_info_data:
    libvaUtilsNameRPM = "libva2" #TBD - no utils package available 
coreRPMPackages = [
    'rocm-hip-runtime-devel',
    str(libvaNameRPM),
    'libva-devel',
    'libdrm-amdgpu',
    'mesa-amdgpu-va-drivers',
    'mesa-amdgpu-dri-drivers',
    str(libvaUtilsNameRPM)
]

# update
ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +' '+linuxSystemInstall_check+' update'))

# common packages
ERROR_CHECK(os.system('sudo '+sudoValidateOption))
for i in range(len(commonPackages)):
    ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
            ' '+linuxSystemInstall_check+' install '+ commonPackages[i]))

# rocDecode Core - LibVA Requirements
ERROR_CHECK(os.system('sudo '+sudoValidateOption))
if "Ubuntu" in platfromInfo:
    for i in range(len(coreDebianPackages)):
        ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
                ' '+linuxSystemInstall_check+' install '+ coreDebianPackages[i]))
    if "VERSION_ID=22" in os_info_data:
        for i in range(len(coreDebianU22Packages)):
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
                    ' '+linuxSystemInstall_check+' install '+ coreDebianU22Packages[i]))
else:
    for i in range(len(coreRPMPackages)):
        ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
                ' '+linuxSystemInstall_check+' install '+ coreRPMPackages[i]))

# rocDecode Dev Requirements
ERROR_CHECK(os.system('sudo '+sudoValidateOption))
if developerInstall == 'ON':
    if "Ubuntu" in platfromInfo:
        for i in range(len(ffmpegDebianPackages)):
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
                    ' '+linuxSystemInstall_check+' install '+ ffmpegDebianPackages[i]))
    else:
        if "centos-8" in platfromInfo or "redhat-8" in platfromInfo:
            # el8 x86_64 packages
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install https://download1.rpmfusion.org/free/el/rpmfusion-free-release-8.noarch.rpm https://download1.rpmfusion.org/nonfree/el/rpmfusion-nonfree-release-8.noarch.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install http://mirror.centos.org/centos/8/PowerTools/x86_64/os/Packages/SDL2-2.0.10-2.el8.x86_64.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install ffmpeg ffmpeg-devel'))
        elif "centos-9" in platfromInfo or "redhat-9" in platfromInfo:
            # el8 x86_64 packages
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install https://dl.fedoraproject.org/pub/epel/epel-next-release-latest-9.noarch.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install --nogpgcheck https://mirrors.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install https://mirrors.rpmfusion.org/nonfree/el/rpmfusion-nonfree-release-$(rpm -E %rhel).noarch.rpm'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install ffmpeg ffmpeg-free-devel'))
        elif "SLES" in platfromInfo:
            # FFMPEG-4 packages
            ERROR_CHECK(os.system(
            'sudo zypper ar -cfp 90 \'https://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_Leap_$releasever/Essentials\' packman-essentials'))
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                ' install ffmpeg-4'))

print("\nrocDecode Dependencies Installed with rocDecode-setup.py V-"+__version__+"\n")
