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

from datetime import datetime
from subprocess import Popen, PIPE
import argparse
import os
import shutil
import sys
import platform
import glob
import pandas as pd
from pathlib import Path

__license__ = "MIT"
__version__ = "1.0"
__status__ = "Shipping"


def shell(cmd):
    p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    output = p.communicate()[0][0:-1]
    return output


def write_formatted(output, f):
    f.write("````\n")
    f.write("%s\n\n" % output)
    f.write("````\n")


def strip_libtree_addresses(lib_tree):
    return lib_tree

def iter_files(path):
    for file_or_directory in path.rglob("*"):
        if file_or_directory.is_file():
            yield file_or_directory
            
# Import arguments
parser = argparse.ArgumentParser()
parser.add_argument('--rocDecode_directory',   type=str, default='',
                    help='The rocDecode Directory - required')
parser.add_argument('--gpu_device_id',      type=int, default=0,
                    help='The GPU device ID that will be used to run the test on it - optional (default:0 [range:0 - N-1] N = total number of available GPUs on a machine)')
parser.add_argument('--files_directory',    type=str, default='',
                    help='The path to a dirctory containing one or more supported files for decoding (e.g., mp4, mov, etc.) - required')
parser.add_argument('--sample_mode',          type=int, default=0,
                    help='The sample to run - optional (default:0 [range:0-1] 0: videoDecode, 1: videoDecodePerf)')
parser.add_argument('--num_threads',          type=int, default=1,
                    help='The number of threads is only for the videoDecodePerf sample (sample_mode = 1) - optional (default:1)')
parser.add_argument('--max_num_decoded_frames',          type=int, default=0,
                    help='The max number of decoded frames. Useful for partial decoding of a long stream. - optional (default:0, meaning no limit)')

args = parser.parse_args()

rocDecodeDirectory = args.rocDecode_directory
gpuDeviceID = args.gpu_device_id
filesDir = args.files_directory
filesDirPath = Path(filesDir)
sampleMode = args.sample_mode
numThreads = args.num_threads
maxNumFrames = args.max_num_decoded_frames

print("\nrunrocDecodeTests V"+__version__+"\n")

# rocDecode Application
scriptPath = os.path.dirname(os.path.realpath(__file__))
if sampleMode == 0:
    rocDecode_exe = rocDecodeDirectory+'/samples/videoDecode/build/videodecode'
    resultsPath = scriptPath+'/rocDecode_videoDecode_results'
elif sampleMode == 1:
    rocDecode_exe = rocDecodeDirectory+'/samples/videoDecodePerf/build/videodecodeperf'
    resultsPath = scriptPath+'/rocDecode_videoDecodePerf_results'
run_rocDecode_app = os.path.abspath(rocDecode_exe)
os.system('(mkdir -p ' +  resultsPath + ')')
if(os.path.isfile(run_rocDecode_app)):
    print("STATUS: rocDecode path - "+run_rocDecode_app+"\n")
else:
    print("\nERROR: rocDecode Executable Not Found\n")
    exit()

if os.path.exists(filesDir) and not os.path.isfile(filesDir):
    # Checking if the directory is empty or not
    if not os.listdir(filesDir):
        print("\nERROR: Empty directory - no videos to decode")
        exit()
else:
    print("\nERROR: The input directory path is either for a file or directory does not exist!")
    exit()

# Get cwd
cwd = os.getcwd()
if os.path.exists(resultsPath+'/rocDecode_output.log'):
    os.remove(resultsPath+'/rocDecode_output.log')

if os.path.exists(resultsPath+'/rocDecode_test_results.csv'):
    os.remove(resultsPath+'/rocDecode_test_results.csv')

if sampleMode == 0:
    for current_file in iter_files(filesDirPath):
        os.system(run_rocDecode_app+' -i '+str(current_file)+' -d '+str(gpuDeviceID)+' -f '+str(maxNumFrames)+' | tee -a '+resultsPath+'/rocDecode_output.log')
        print("\n\n")

    orig_stdout = sys.stdout
    sys.stdout = open(resultsPath+'/rocDecode_test_results.csv', 'a')
    echo_1 = 'File Name, Codec, Bit Depth, Total Frames, Average decoding time per frame (ms), Avg FPS'
    print(echo_1)
    sys.stdout = orig_stdout

    runAwk_csv = r'''awk '/info: Input file: / {filename=$4; next}
                        /info: Using GPU device 0 - AMD Radeon Graphics[gfx1030] on PCI bus 0d:00.0/{next}
                        /info: decoding started, please wait!/{next}
                        /Input Video Information/{next}
                        /\tCodec        : / {codec=$3; next}
                        /\tSequence     : /{next}
                        /\tCoded size   : /{next}
                        /\tDisplay area : /{next}
                        /\tChroma       : /{next}
                        /\tBit depth    : / {bitDepth=$4; next}
                        /Video Decoding Params:/{next}
                        /\tNum Surfaces : /{next}
                        /\tCrop         : /{next}
                        /\tResize       : /{next}
                        /^$/{next}
                        /info: Total pictures decoded: / {totalFrames=$5; next}
                        /info: avg decoding time per picture: /{timePerFrame=$7; next}
                        /info: avg decode FPS: / { printf("%s, %s, %d, %d, %f, %f\n", filename, codec, bitDepth, totalFrames, timePerFrame, $5) }' rocDecode_videoDecode_results/rocDecode_output.log >> rocDecode_videoDecode_results/rocDecode_test_results.csv'''
    os.system(runAwk_csv)
elif sampleMode == 1:
    for current_file in iter_files(filesDirPath):
        os.system(run_rocDecode_app+' -i '+str(current_file)+' -t '+str(numThreads)+' -f '+str(maxNumFrames)+' | tee -a '+resultsPath+'/rocDecode_output.log')
        print("\n\n")

    orig_stdout = sys.stdout
    sys.stdout = open(resultsPath+'/rocDecode_test_results.csv', 'a')
    echo_1 = 'File Name, Num Threads, Codec, Bit Depth, Total Frames, Average decoding time per frame (ms), Avg FPS'
    print(echo_1)
    sys.stdout = orig_stdout

    runAwk_csv = r'''awk '/info: Input file: / {filename=$4; next}
                        /info: Number of threads: / {numThreads=$5; next}
                        /info: Using GPU device 0 - AMD Radeon Graphics[gfx1030] on PCI bus 0d:00.0/{next}
                        /info: decoding started, please wait!/{next}
                        /Input Video Information/{next}
                        /\tCodec        : / {codec=$3; next}
                        /\tSequence     : /{next}
                        /\tCoded size   : /{next}
                        /\tDisplay area : /{next}
                        /\tChroma       : /{next}
                        /\tBit depth    : / {bitDepth=$4; next}
                        /Video Decoding Params:/{next}
                        /\tNum Surfaces : /{next}
                        /\tCrop         : /{next}
                        /\tResize       : /{next}
                        /^$/{next}
                        /info: Total pictures decoded: / {totalFrames=$5; next}
                        /info: avg decoding time per picture: /{timePerFrame=$7; next}
                        /info: avg decode FPS: / { printf("%s, %d, %s, %d, %d, %f, %f\n", filename, numThreads, codec, bitDepth, totalFrames, timePerFrame, $5) }' rocDecode_videoDecodePerf_results/rocDecode_output.log >> rocDecode_videoDecodePerf_results/rocDecode_test_results.csv'''
    sys.stdout = orig_stdout
    os.system(runAwk_csv)

# get data
platform_name = platform.platform()
platform_name_fq = shell('hostname --all-fqdns')
platform_ip = shell('hostname -I')[0:-1]  # extra trailing space

file_dtstr = datetime.now().strftime("%Y%m%d")
reportFilename = 'rocDecode_report_%s_%s.md' % (platform_name, file_dtstr)
report_dtstr = datetime.now().strftime("%Y-%m-%d %H:%M:%S %Z")
sys_info = shell('inxi -c0 -S')
cpu_info = shell('inxi -c0 -C')
gpu_info = shell('inxi -c0 -G')
memory_info = shell('inxi -c 0 -m')
board_info = shell('inxi -c0 -M')

lib_tree = shell('ldd '+run_rocDecode_app)
lib_tree = strip_libtree_addresses(lib_tree)

# Load the data
df = pd.read_csv(resultsPath+'/rocDecode_test_results.csv')
# Generate the markdown table
print(df.to_markdown(index=False))

# Write Report
with open(reportFilename, 'w') as f:
    f.write("rocDecode app report\n")
    f.write("================================\n")
    f.write("\n")

    f.write("Generated: %s\n" % report_dtstr)
    f.write("\n")

    f.write("Platform: %s (%s)\n" % (platform_name_fq, platform_ip))
    f.write("--------\n")
    f.write("\n")

    write_formatted(sys_info, f)
    write_formatted(cpu_info, f)
    write_formatted(gpu_info, f)
    write_formatted(board_info, f)
    write_formatted(memory_info, f)

    f.write("\n\nBenchmark Report\n")
    f.write("--------\n")
    f.write("\n")
    f.write("\n")
    f.write(df.to_markdown(index=False))
    f.write("\n")
    f.write("\n")
    f.write("Dynamic Libraries Report\n")
    f.write("-----------------\n")
    f.write("\n")
    write_formatted(lib_tree, f)
    f.write("\n")

    f.write(
        "\n\n---\n**Copyright (c) 2023 - 2024 AMD ROCm rocDecode app -- run_rocDecode_tests.py V-"+__version__+"**\n")
    f.write("\n")
    # report file
    reportFileDir = os.path.abspath(reportFilename)
    print("\nSTATUS: Output Report File - "+reportFileDir)

print("\nrun_rocDecode_tests.py completed - V"+__version__+"\n")