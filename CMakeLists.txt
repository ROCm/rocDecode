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

project(rocdecode)
set(VERSION "0.4.0")
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "Library install directory")
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE STRING "Include install directory")
include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

find_program(MAKE_NSIS_EXE makensis)
find_program(RPMBUILD_EXE rpmbuild)
find_program(DPKG_EXE dpkg)

# ROCM Path
if(DEFINED ENV{ROCM_PATH})
  set(ROCM_PATH $ENV{ROCM_PATH} CACHE PATH "Default ROCm installation path")
elseif(ROCM_PATH)
  message("-- INFO:ROCM_PATH Set -- ${ROCM_PATH}")
else()
  set(ROCM_PATH /opt/rocm CACHE PATH "Default ROCm installation path")
endif()
# avoid setting the default installation path to /usr/local
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${ROCM_PATH} CACHE PATH "rocDecode default installation path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(DEFAULT_BUILD_TYPE "Release")
set(ENHANCED_MESSAGE ON)

if(ENHANCED_MESSAGE)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(Red "${Esc}[31m")
  set(Green "${Esc}[32m")
  set(Yellow "${Esc}[33m")
  set(Blue "${Esc}[34m")
  set(BoldBlue "${Esc}[1;34m")
  set(Magenta "${Esc}[35m")
  set(Cyan "${Esc}[36m")
  set(White "${Esc}[37m")
endif()
message("-- ${BoldBlue}rocDecode Version -- ${VERSION}${ColourReset}")
message("-- ${BoldBlue}rocDecode Install Path -- ${CMAKE_INSTALL_PREFIX}${ColourReset}")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${ROCM_PATH}/hip ${ROCM_PATH})
set(CMAKE_CXX_COMPILER ${ROCM_PATH}/llvm/bin/clang++)

# rocDecode Default Options
option(BUILD_WITH_AMD_ADVANCE "Build rocDecode for advanced AMD GPU Architecture" OFF)

# rocDecode Build Type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "rocDecode Default Build Type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()
if(CMAKE_BUILD_TYPE MATCHES Debug)
  # -O0 -- Don't Optimize output file 
  # -g  -- generate debugging information 
  # -Og -- Optimize for debugging experience rather than speed or size
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Og")
else()
  # -O3       -- Optimize output file 
  # -DNDEBUG  -- turn off asserts 
  # -fPIC     -- Generate position-independent code if possible
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -fPIC")
endif()
message("-- ${BoldBlue}rocDecode Build Type -- ${CMAKE_BUILD_TYPE}${ColourReset}")

set(DEFAULT_AMDGPU_TARGETS "gfx803;gfx900;gfx906;gfx908;gfx90a;gfx940;gfx1030;gfx1031;gfx1032")
if(BUILD_WITH_AMD_ADVANCE)
  set(DEFAULT_AMDGPU_TARGETS ${DEFAULT_AMDGPU_TARGETS} "gfx941")
endif()
set(AMDGPU_TARGETS "${DEFAULT_AMDGPU_TARGETS}" CACHE STRING "List of specific machine types for library to target")

find_package(HIP QUIET)
find_package(Libdrm QUIET)
find_package(Libva QUIET)
find_package(FFmpeg QUIET)

if(HIP_FOUND AND FFMPEG_FOUND AND Libva_FOUND AND Libdrm_FOUND)

  # HIP
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} hip::device)
  # LibDRM
  include_directories(${LIBDRM_INCLUDE_DIR})
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${LIBDRM_LIBRARY})
  # LibVA
  include_directories(${LIBVA_INCLUDE_DIR})
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${LIBVA_LIBRARY})
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${LIBVA_DRM_LIBRARY})
  # FFMPEG
  include_directories(${AVUTIL_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR}
                      ${SWSCALE_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR})
  set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${FFMPEG_LIBRARIES})

  # local include files
  include_directories(api src/rocdecode src/parser src/rocdecode/vaapi)
  # source files
  file(GLOB_RECURSE SOURCES "./src/*.cpp")
  # rocdecode.so
  add_library(${PROJECT_NAME} SHARED ${SOURCES})

  if(_FFMPEG_AVCODEC_VERSION VERSION_LESS_EQUAL 58.134.100)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_58_134=1)
  endif()

  if(_FFMPEG_AVCODEC_VERSION VERSION_LESS_EQUAL 58.54.100)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_58_54=0)
  else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVCODEC_58_54=1)
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
  target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARY_LIST})

  install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES api/rocdecode.h api/rocparser.h utils/video_demuxer.hpp utils/rocvideodecode/roc_video_dec.h
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

  message("-- ${White}AMD ROCm rocDecode -- CMAKE_CXX_FLAGS:${CMAKE_CXX_FLAGS}${ColourReset}")
  message("-- ${White}AMD ROCm rocDecode -- Link Libraries: ${LINK_LIBRARY_LIST}${ColourReset}")

  enable_testing()
  include(CTest)
  add_subdirectory(samples)

  # set package information
  set(CPACK_PACKAGE_VERSION ${VERSION})
  set(CPACK_PACKAGE_LICENSE "AMD")
  set(CPACK_PACKAGE_CONTACT "rocDecode Support <mivisionx.support@amd.com>")
  set(CPACK_PACKAGE_VENDOR "AMD ROCm")
  set(CPACK_PACKAGE_GROUP "Development/Tools")
  set(CPACK_PACKAGE_HOMEPAGE "https://github.com/ROCmSoftwarePlatform/rocDecode")

  if(ENABLE_ASAN_PACKAGING)
    set(CPACK_PACKAGE_NAME "rocDecode-asan")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "AMD rocDecode address sanitizer libraries")
    set(CPACK_RPM_PACKAGE_SUMMARY "AMD rocDecode address sanitizer libraries")
  else()
    set(CPACK_PACKAGE_NAME "rocDecode")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "AMD rocDecode hardware-accelerated video decoder on AMD’s GPUs.
                 rocDecode is a toolkit to decode videos and images using a hardware-accelerated video \
                decoder on AMD’s GPUs.")
    set(CPACK_RPM_PACKAGE_SUMMARY "AMD rocDecode hardware-accelerated video decoder on AMD’s GPUs.")
  endif()

  # set license information
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

  # NOTE: ASAN package requires address sanitized libraries and license file
  # BUILD_DEV will be set to OFF for ASAN biulds.
  if(ENABLE_ASAN_PACKAGING)
    install(FILES ${CPACK_RESOURCE_FILE_LICENSE}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/rocdecode-asan)
  else()
    install(FILES ${CPACK_RESOURCE_FILE_LICENSE}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/rocdecode)
  endif()
  if(DEFINED ENV{ROCM_LIBPATCH_VERSION})
    set(CPACK_PACKAGE_VERSION
        "${CPACK_PACKAGE_VERSION}.$ENV{ROCM_LIBPATCH_VERSION}")
  endif()

  set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
  set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
  set(CPACK_DEBIAN_PACKAGE_RELEASE "local")
  set(CPACK_RPM_PACKAGE_RELEASE "local")

  if(DEFINED ENV{CPACK_DEBIAN_PACKAGE_RELEASE})
    set(CPACK_DEBIAN_PACKAGE_RELEASE $ENV{CPACK_DEBIAN_PACKAGE_RELEASE})
  endif()
  if(DEFINED ENV{CPACK_RPM_PACKAGE_RELEASE})
    set(CPACK_RPM_PACKAGE_RELEASE $ENV{CPACK_RPM_PACKAGE_RELEASE})
  endif()

  # Set the dependent packages
  set(rocDecode_DEBIAN_PACKAGE_LIST
      "vainfo, libva-dev, libdrm-dev, ffmpeg, libavcodec-dev, libavformat-dev, libswscale-dev")
  set(rocDecode_RPM_PACKAGE_LIST
      "libva-devel, libdrm-devel, ffmpeg, libavcodec-devel, libavformat-devel, libswscale-devel")
  # set dependency to ROCm if set to TRUE, default to OFF
  set(ROCM_DEP_ROCMCORE OFF CACHE BOOL "Set rocm-core dependency")
  if(ROCM_DEP_ROCMCORE)
    if(ENABLE_ASAN_PACKAGING)
      set(_rocm_core_pkg "rocm-core-asan")
    else()
      set(_rocm_core_pkg "rocm-core")
    endif()
    set(rocDecode_PACKAGE_REQS "${rocDecode_PACKAGE_REQS}, ${_rocm_core_pkg}")

    set(rocDecode_DEBIAN_PACKAGE_LIST "${rocDecode_PACKAGE_REQS}, ${rocDecode_DEBIAN_PACKAGE_LIST}")
    set(rocDecode_RPM_PACKAGE_LIST "${rocDecode_PACKAGE_REQS}, ${rocDecode_RPM_PACKAGE_LIST}")
  endif()

  set(CPACK_DEBIAN_PACKAGE_DEPENDS "${rocDecode_DEBIAN_PACKAGE_LIST}")
  set(CPACK_RPM_PACKAGE_REQUIRES "${rocDecode_RPM_PACKAGE_LIST}")

  # '%{?dist}' breaks manual builds on debian systems due to empty Provides
  execute_process(
    COMMAND rpm --eval %{?dist}
    RESULT_VARIABLE PROC_RESULT
    OUTPUT_VARIABLE EVAL_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(PROC_RESULT EQUAL "0" AND NOT EVAL_RESULT STREQUAL "")
    string(APPEND CPACK_RPM_PACKAGE_RELEASE "%{?dist}")
  endif()

  set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CPACK_PACKAGE_HOMEPAGE})
  set(CPACK_RPM_PACKAGE_URL ${CPACK_PACKAGE_HOMEPAGE})
  set(CPACK_RPM_PACKAGE_AUTOREQPROV "no")

  set(CPACK_GENERATOR "TGZ;ZIP")
  if(EXISTS ${MAKE_NSIS_EXE})
    list(APPEND CPACK_GENERATOR "NSIS")
  endif()
  if(EXISTS ${RPMBUILD_EXE})
    list(APPEND CPACK_GENERATOR "RPM")
    message("-- ${White}AMD ROCm rocDecode -- CPACK_RPM_PACKAGE_REQUIRES: ${CPACK_RPM_PACKAGE_REQUIRES}${ColourReset}")
  endif()
  if(EXISTS ${DPKG_EXE})
    list(APPEND CPACK_GENERATOR "DEB")
    message("-- ${White}AMD ROCm rocDecode -- CPACK_DEBIAN_PACKAGE_DEPENDS: ${CPACK_DEBIAN_PACKAGE_DEPENDS}${ColourReset}")
  endif()

  include(CPack)
else()
  if(NOT HIP_FOUND)
    message(FATAL_ERROR "-- ERROR!: HIP Not Found! - please install ROCm and HIP!")
  endif()
  if(NOT FFMPEG_FOUND)
    message(FATAL_ERROR "-- ERROR!: FFMPEG Not Found! - please install FFMPEG!")
  endif()
  if(NOT Libva_FOUND)
    message(FATAL_ERROR "-- ERROR!: libva-dev Not Found - please install libva-dev!")
  endif()
  if(NOT Libdrm_FOUND)
    message(FATAL_ERROR "-- ERROR!: libdrm-dev Not Found - please install libdrm-dev!")
  endif()
endif()
