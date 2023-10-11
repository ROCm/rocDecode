# ##############################################################################
# Copyright (c) 2023 Advanced Micro Devices, Inc.
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

# videoDecode
add_test(
  NAME
    video_decode
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/videoDecode"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecode"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecode"
            -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H264.mp4
)

add_test(NAME video_decode-H265 
              COMMAND videodecode
              -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H265.mp4
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/videoDecode)

# videoDecodeFork
add_test(
  NAME
    video_decode_fork
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/videoDecodeFork"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodeFork"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodefork"
            -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H264.mp4
)

add_test(NAME video_decode_fork-H265 
              COMMAND videodecodefork
              -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H265.mp4
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/videoDecodeFork)

# videoDecodePerf
add_test(
  NAME
    video_decode_perf
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/videoDecodePerf"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodePerf"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodeperf"
            -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H264.mp4
)

add_test(NAME video_decode_perf-H265 
              COMMAND videodecodeperf
              -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H265.mp4
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/videoDecodePerf)

# videoDecodeRGB
add_test(
  NAME
    video_decode_rgb
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/videoDecodeRGB"
                              "${CMAKE_CURRENT_BINARY_DIR}/videoDecodeRGB"
            --build-generator "${CMAKE_GENERATOR}"
            --test-command "videodecodergb"
            -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H264.mp4
)

add_test(NAME video_decode_rgb-H265 
              COMMAND videodecodergb
              -i ${CMAKE_SOURCE_DIR}/data/videos/AMD_driving_virtual_20-H265.mp4
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/videoDecodeRGB)