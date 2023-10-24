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

find_library(LIBVA_LIBRARY NAMES va)
find_path(LIBVA_INCLUDE_DIR NAMES va/va.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libva DEFAULT_MSG LIBVA_INCLUDE_DIR LIBVA_LIBRARY)
mark_as_advanced(LIBVA_INCLUDE_DIR LIBVA_LIBRARY)

if(Libva_FOUND)
  if(NOT TARGET Libva::va)
    add_library(Libva::va UNKNOWN IMPORTED)
    set_target_properties(Libva::va PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LIBVA_INCLUDE_DIR}"
        IMPORTED_LOCATION "${LIBVA_LIBRARY}")
  endif()
endif()