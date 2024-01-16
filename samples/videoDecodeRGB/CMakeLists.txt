################################################################################
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
################################################################################

cmake_minimum_required (VERSION 3.5)
project(videodecodergb)
set(CMAKE_CXX_STANDARD 17)

# ROCM Path
if(DEFINED ENV{ROCM_PATH})
  set(ROCM_PATH $ENV{ROCM_PATH} CACHE PATH "Default ROCm installation path")
elseif(ROCM_PATH)
  message("-- INFO:ROCM_PATH Set -- ${ROCM_PATH}")
else()
  set(ROCM_PATH /opt/rocm CACHE PATH "Default ROCm installation path")
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../cmake)
list(APPEND CMAKE_PREFIX_PATH ${ROCM_PATH}/hip ${ROCM_PATH})
set(CMAKE_CXX_COMPILER ${ROCM_PATH}/llvm/bin/clang++)

set(DEFAULT_AMDGPU_TARGETS "gfx908;gfx90a;gfx940;gfx1030;gfx1031;gfx1032;gfx1100;gfx1101;gfx1102")
if(BUILD_WITH_AMD_ADVANCE)
  set(DEFAULT_AMDGPU_TARGETS ${DEFAULT_AMDGPU_TARGETS} "gfx941;gfx942")
endif()
set(AMDGPU_TARGETS "${DEFAULT_AMDGPU_TARGETS}" CACHE STRING "List of specific machine types for library to target")
find_package(HIP QUIET)
find_package(FFmpeg QUIET)
# find rocDecode
find_library(ROCDECODE_LIBRARY NAMES rocdecode HINTS {ROCM_PATH}/lib)
find_path(ROCDECODE_INCLUDE_DIR NAMES rocdecode.h PATHS /opt/rocm/include/rocdecode {ROCM_PATH}/include/rocdecode)

if(ROCDECODE_LIBRARY AND ROCDECODE_INCLUDE_DIR)
    set(ROCDECODE_FOUND TRUE)
    message("-- ${White}Using rocDecode -- \n\tLibraries:${ROCDECODE_LIBRARY} \n\tIncludes:${ROCDECODE_INCLUDE_DIR}${ColourReset}")
endif()

if(HIP_FOUND AND FFMPEG_FOUND AND ROCDECODE_FOUND)
    # HIP
    set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} hip::device)
    # FFMPEG
    include_directories(${AVUTIL_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR})
    set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${FFMPEG_LIBRARIES})
    # rocDecode and utils
    include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../../utils ${ROCDECODE_INCLUDE_DIR})
    set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${ROCDECODE_LIBRARY})
    list(APPEND SOURCES ${PROJECT_SOURCE_DIR} 
                        videodecrgb.cpp 
                        ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/rocvideodecode/roc_video_dec.cpp 
                        ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/colorspace_kernels.cpp)

    add_executable(${PROJECT_NAME} ${SOURCES})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
    target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARY_LIST})
else()
    message("-- ERROR!: ${PROJECT_NAME} excluded! please install all the dependencies and try again!")
    if (NOT HIP_FOUND)
        message(FATAL_ERROR "-- ERROR!: HIP Not Found! - please install ROCm and HIP!")
    endif()
    if (NOT FFMPEG_FOUND)
        message(FATAL_ERROR "-- ERROR!: FFMPEG Not Found! - please install FFMPEG!")
    endif()
    if (NOT ROCDECODE_FOUND)
        message(FATAL_ERROR "-- ERROR!: rocDecode Not Found! - please install rocDecode!")
    endif()
endif()
