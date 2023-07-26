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

__version__ = "1.0"
__status__ = "Shipping"

# Arguments
parser = argparse.ArgumentParser()
parser.add_argument('--directory', 	type=str, default='~/rocDecode-deps',
                    help='Setup home directory - optional (default:~/)')
parser.add_argument('--ffmpeg',    	type=str, default='yes',
                    help='FFMPEG V4.4.2 Installation (default:yes) [options:yes/no]')
parser.add_argument('--reinstall', 	type=str, default='no',
                    help='Remove previous setup and reinstall - optional (default:no) [options:yes/no]')

args = parser.parse_args()

setupDir = args.directory
ffmpegInstall = args.ffmpeg
reinstall = args.reinstall

if ffmpegInstall not in ('no', 'yes'):
    print(
        "ERROR: FFMPEG Install Option Not Supported - [Supported Options: no or yes]")
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

# Setup Directory for Deps
if setupDir == '~/rocDecode-deps':
    setupDir_deps = setupDir
else:
    setupDir_deps = setupDir+'/rocDecode-deps'

# setup directory path
deps_dir = os.path.expanduser(setupDir_deps)
deps_dir = os.path.abspath(deps_dir)

# setup for Linux
linuxSystemInstall = ''
linuxCMake = 'cmake'
linuxSystemInstall_check = ''
linuxFlag = ''
if "centos" in platfromInfo or "redhat" in platfromInfo:
    linuxSystemInstall = 'yum -y'
    linuxSystemInstall_check = '--nogpgcheck'
    if "centos-7" in platfromInfo or "redhat-7" in platfromInfo:
        linuxCMake = 'cmake3'
        os.system(linuxSystemInstall+' install cmake3')
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
    print("\nrocDecode Setup Supported on: Ubuntu 20/22; CentOS 7/8; RedHat 7/8; & SLES 15-SP2\n")
    exit()

# rocDecode Setup
print("\nrocDecode Setup on: "+platfromInfo+"\n")

if userName == 'root':
    os.system(linuxSystemInstall+' update')
    os.system(linuxSystemInstall+' install sudo')

# Delete previous install
if os.path.exists(deps_dir) and reinstall == 'yes':
    os.system('sudo -v')
    os.system('sudo rm -rf '+deps_dir)
    print("\nrocDecode Setup: Removing Previous Install -- "+deps_dir+"\n")

# Re-Install
if os.path.exists(deps_dir):
    print("\nrocDecode Setup: Re-Installing Libraries from -- "+deps_dir+"\n")
    if ffmpegInstall == 'yes':
        # FFMPEG
        if os.path.exists(deps_dir+'/FFmpeg-n4.4.2'):
            os.system('sudo -v')
            os.system('(cd '+deps_dir+'/FFmpeg-n4.4.2; sudo ' +
                      linuxFlag+' make install -j8)')

    print("\nrocDecode Dependencies Re-Installed with rocDecode-setup.py V-"+__version__+"\n")
    exit()
# Clean Install
else:
    print("\nrocDecode Dependencies Installation with rocDecode-setup.py V-"+__version__+"\n")
    os.system('mkdir '+deps_dir)
    # Create Build folder
    os.system('(cd '+deps_dir+'; mkdir build )')
    # install pre-reqs
    os.system('sudo -v')
    os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' ' +
              linuxSystemInstall_check+' install gcc cmake git wget unzip pkg-config inxi vainfo')

    # Get Installation Source
    if ffmpegInstall == 'yes':
        os.system(
            '(cd '+deps_dir+'; wget https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n4.4.2.zip && unzip n4.4.2.zip )')

    # Install ffmpeg
    if ffmpegInstall == 'yes':
        if "Ubuntu" in platfromInfo:
            os.system('sudo -v')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                      ' install autoconf automake build-essential git-core libass-dev libfreetype6-dev')
            os.system('sudo -v')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                      ' install libsdl2-dev libtool libva-dev libvdpau-dev libvorbis-dev libxcb1-dev')
            os.system('sudo -v')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                      ' install libxcb-shm0-dev libxcb-xfixes0-dev pkg-config texinfo zlib1g-dev')
            os.system('sudo -v')
            os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                      ' install nasm yasm libx264-dev libx265-dev libnuma-dev libfdk-aac-dev')
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
            elif "SLES" in platfromInfo:
                # FFMPEG-4 packages
                os.system(
                    'sudo zypper ar -cfp 90 \'https://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_Leap_$releasever/Essentials\' packman-essentials')
                os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +
                          ' install ffmpeg-4')

        # FFMPEG 4 from source -- for Ubuntu, CentOS 7, & RedHat 7
        if "Ubuntu" in platfromInfo or "centos-7" in platfromInfo or "redhat-7" in platfromInfo:
            os.system('sudo -v')
            os.system(
                '(cd '+deps_dir+'/FFmpeg-n4.4.2; sudo '+linuxFlag+' ldconfig )')
            os.system('(cd '+deps_dir+'/FFmpeg-n4.4.2; export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig/"; ./configure --enable-shared --disable-static)')
            os.system('(cd '+deps_dir+'/FFmpeg-n4.4.2; make -j8 )')
            os.system('sudo -v')
            os.system('(cd '+deps_dir+'/FFmpeg-n4.4.2; sudo ' +
                      linuxFlag+' make install )')

    print("\nrocDecode Dependencies Installed with rocDecode-setup.py V-"+__version__+"\n")
