# ##############################################################################
# Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc.
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

cmake_minimum_required(VERSION 3.10)
# ROCM Path
if(DEFINED ENV{ROCM_PATH})
  set(ROCM_PATH $ENV{ROCM_PATH} CACHE PATH "Default ROCm installation path")
elseif(ROCM_PATH)
  message("-- INFO:ROCM_PATH Set -- ${ROCM_PATH}")
else()
  set(ROCM_PATH /opt/rocm CACHE PATH "Default ROCm installation path")
endif()
# Set AMD Clang as default compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS ON)
if (NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_C_COMPILER ${ROCM_PATH}/bin/amdclang)
  set(CMAKE_CXX_COMPILER ${ROCM_PATH}/bin/amdclang++)
endif()

# rocdecode-host Version
set(VERSION "1.0.0")

# Set Project Version and Language
project(rocdecode-host VERSION ${VERSION} LANGUAGES CXX)

# add cmake find Config locations
list(APPEND CMAKE_PREFIX_PATH ${ROCM_PATH}/lib/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../cmake)

find_package(HIP QUIET)
find_package(Threads QUIET)
find_package(rocprofiler-register QUIET)
find_package(FFmpeg QUIET)

if(HIP_FOUND AND Threads_FOUND AND FFMPEG_FOUND)
  # HIP
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} hip::host)
  # FFMPEG
  include_directories(${AVUTIL_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR})
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${FFMPEG_LIBRARIES})

  # rocprofiler
  if (rocprofiler-register_FOUND)
    set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} rocprofiler-register::rocprofiler-register)
  endif()

  # local include files
  include_directories(api rocdecode-host ../src/rocdecode ../src/parser)
  # source files
  file(GLOB_RECURSE SOURCES "./*.cpp")
  # rocdecode-host.so
  add_library(${PROJECT_NAME} SHARED ${SOURCES})

  target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARY_LIST})
  # FFMPEG multi-version support
  if(_FFMPEG_AVCODEC_VERSION VERSION_LESS_EQUAL 58.134.100) #FFmpeg version <= 4.4
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_GREATER_THAN_58_134=0)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_GREATER_THAN_60_31=0)
  elseif(_FFMPEG_AVCODEC_VERSION VERSION_GREATER 58.134.100 AND _FFMPEG_AVCODEC_VERSION VERSION_LESS_EQUAL 60.31.100) #FFmpeg version >= 4.4 && version < 6.1
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_GREATER_THAN_58_134=1)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_GREATER_THAN_60_31=0)
  else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_GREATER_THAN_58_134=0) #FFmpeg version >= 6.1
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_GREATER_THAN_60_31=1)
  endif() #FFMpeg version >= 7.1
  set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

  message("-- ${White}AMD ROCm ${PROJECT_NAME} -- CMAKE_CXX_FLAGS:${CMAKE_CXX_FLAGS}${ColourReset}")
  message("-- ${White}AMD ROCm ${PROJECT_NAME} -- Link Libraries: ${LINK_LIBRARY_LIST}${ColourReset}")

  # install rocdecode-host libs -- {ROCM_PATH}/lib
  set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
  install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime NAMELINK_SKIP)
  install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}-targets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev NAMELINK_ONLY)
  install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT asan)
  # install rocdecode include files -- {ROCM_PATH}/include/rocdecode
  install(FILES ${PROJECT_SOURCE_DIR}/../../api/rocdecode/rocdecode_host.h 
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME} COMPONENT dev)

else()
  message("-- ERROR!: ${PROJECT_NAME} excluded! please install all the dependencies and try again!")
  if (NOT HIP_FOUND)
      message(FATAL_ERROR "-- ERROR!: HIP Not Found! - please install ROCm and HIP!")
  endif()
  if (NOT Threads_FOUND)
      message(FATAL_ERROR "-- ERROR!: Threads Not Found! - please install Threads!")
  endif()
  if (NOT FFMPEG_FOUND)
      message(FATAL_ERROR "-- ERROR!: FFMPEG Not Found! - please install FFMPEG!")
  endif()
endif()
