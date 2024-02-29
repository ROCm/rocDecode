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

find_library(LIBDRM_LIBRARY NAMES drm HINTS /opt/amdgpu/lib/x86_64-linux-gnu /opt/amdgpu/lib64 NO_DEFAULT_PATH)
find_path(LIBDRM_INCLUDE_DIR NAMES drm.h PATHS /opt/amdgpu/include/libdrm NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libdrm DEFAULT_MSG LIBDRM_INCLUDE_DIR LIBDRM_LIBRARY)
mark_as_advanced(LIBDRM_INCLUDE_DIR LIBDRM_LIBRARY)

if(Libdrm_FOUND)
  if(NOT TARGET Libdrm::drm)
    add_library(Libdrm::drm UNKNOWN IMPORTED)
    set_target_properties(Libdrm::drm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LIBDRM_INCLUDE_DIR}"
        IMPORTED_LOCATION "${LIBDRM_LIBRARY}")
  endif()
  message("-- ${White}Using Libdrm -- \n\tLibraries:${LIBDRM_LIBRARY} \n\tIncludes:${LIBDRM_INCLUDE_DIR}${ColourReset}")
else()
  if(Libdrm_FIND_REQUIRED)
    message(FATAL_ERROR "{Red}FindLibdrm -- Libdrm NOT FOUND${ColourReset}")
  endif()
endif()