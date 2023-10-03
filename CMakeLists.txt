################################################################################
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
################################################################################

cmake_minimum_required (VERSION 3.0)
project(rocdecode)
set(VERSION "0.2.0")

set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "Library install directory")
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE STRING "Include install directory")
set(CMAKE_CXX_STANDARD 17)
set(ROCM_PATH /opt/rocm CACHE PATH "Default ROCm installation path")

# avoid setting the default installation path to /usr/local
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${ROCM_PATH} CACHE PATH "rocDecode default installation path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${ROCM_PATH}/hip ${ROCM_PATH})
set(CMAKE_CXX_COMPILER ${ROCM_PATH}/llvm/bin/clang++)

set(DEFAULT_AMDGPU_TARGETS "gfx803;gfx900;gfx906;gfx908;gfx90a;gfx940;gfx1030;gfx1031;gfx1032")
set(AMDGPU_TARGETS "${DEFAULT_AMDGPU_TARGETS}" CACHE STRING "List of specific machine types for library to target")

find_package(HIP QUIET)
find_package(Libva QUIET)

if(HIP_FOUND AND Libva_FOUND)
    include_directories(api
                        src/rocdecode
                        src/parser
                        src/rocdecode/vaapi
                        )       
    file(GLOB_RECURSE SOURCES "./src/*.cpp")
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
    target_link_libraries(${PROJECT_NAME} Libva::va Libva::va_drm hip::device)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES api/rocdecode.h api/rocparser.h utils/video_demuxer.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

else()
    if (NOT HIP_FOUND)
        message(FATAL_ERROR "-- ERROR!: HIP Not Found! - please install ROCm and HIP!")
    endif()
    if (NOT Libva_FOUND)
        message(FATAL_ERROR "-- ERROR!: libva-dev Not Found - please install libva-dev!")
    endif()
    message("-- ERROR!: ${PROJECT_NAME} excluded! please install all the dependencies and try again!")
endif()