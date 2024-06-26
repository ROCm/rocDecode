# ##############################################################################
# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# ##############################################################################
cmake_minimum_required(VERSION 3.5)

project(rocdecode-test)

# make test with CTest
enable_testing()
include(CTest)

# ROCM Path
if(DEFINED ENV{ROCM_PATH})
  set(ROCM_PATH $ENV{ROCM_PATH} CACHE PATH "${White}${PROJECT_NAME}: Default ROCm installation path${ColourReset}")
elseif(ROCM_PATH)
  message("-- ${White}${PROJECT_NAME} :ROCM_PATH Set -- ${ROCM_PATH}${ColourReset}")
else()
  set(ROCM_PATH /opt/rocm CACHE PATH "${White}${PROJECT_NAME}: Default ROCm installation path${ColourReset}")
endif()

# find rocDecode
find_library(ROCDECODE_LIBRARY NAMES rocdecode HINTS ${ROCM_PATH}/lib)
find_path(ROCDECODE_INCLUDE_DIR NAMES rocdecode.h PATHS /opt/rocm/include/rocdecode ${ROCM_PATH}/include/rocdecode)

if(ROCDECODE_LIBRARY AND ROCDECODE_INCLUDE_DIR)
    set(ROCDECODE_FOUND TRUE)
    message("-- ${White}Using rocDecode -- \n\tLibraries:${ROCDECODE_LIBRARY} \n\tIncludes:${ROCDECODE_INCLUDE_DIR}${ColourReset}")
endif()

if(NOT ROCDECODE_FOUND)
    message("-- ${Yellow}${PROJECT_NAME} requires rocDecode. Install rocDecode before running CTests")
endif()

# videoDecode
add_test(
  NAME
    video_decode-H265
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${ROCM_PATH}/share/rocdecode/samples/videoDecode"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecode"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecode"
            -i ${ROCM_PATH}/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
)

# videoDecodeMem
add_test(
  NAME
    video_decodeMem-H265
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${ROCM_PATH}/share/rocdecode/samples/videoDecodeMem"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodeMem"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodemem"
            -i ${ROCM_PATH}/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
)

# videoDecodeMem
add_test(
  NAME
    video_decodePerf-H265
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${ROCM_PATH}/share/rocdecode/samples/videoDecodePerf"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodePerf"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodeperf"
            -i ${ROCM_PATH}/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4
)

# videoDecodeRGB
add_test(
  NAME
    video_decodeRGB-H265
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${ROCM_PATH}/share/rocdecode/samples/videoDecodeRGB"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodeRGB"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodergb"
            -i ${ROCM_PATH}/share/rocdecode/video/AMD_driving_virtual_20-H265.mp4 -of rgb
)

# videoDecode H264
add_test(
  NAME
    video_decode-H264
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${ROCM_PATH}/share/rocdecode/samples/videoDecode"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecode"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecode"
            -i ${ROCM_PATH}/share/rocdecode/video/AMD_driving_virtual_20-H264.mp4
)

# videoDecodeBatch
add_test(
  NAME
    video_decodeBatch
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${ROCM_PATH}/share/rocdecode/samples/videoDecodeBatch"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodeBatch"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodebatch"
            -i ${ROCM_PATH}/share/rocdecode/video/ -t 2
)
