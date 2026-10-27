# SPQR lib usually requires linking to a blas and lapack library.
# It is up to the user of this module to find a BLAS and link to it.
#
# SPQR lib requires Cholmod, colamd and amd as well.
# FindCholmod.cmake can be used to find those packages before finding spqr
#
# This module first tries to find SPQR via its CMake config-mode package
# (shipped with SuiteSparse >= 7.0). If that fails, it falls back to a
# manual header/library search for compatibility with older installations.
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

if (SPQR_INCLUDES AND SPQR_LIBRARIES)
  set(SPQR_FIND_QUIETLY TRUE)
endif ()

# --- Try config-mode first (SuiteSparse >= 7.0) ---
if(NOT SPQR_INCLUDES OR NOT SPQR_LIBRARIES)
  find_package(SPQR CONFIG QUIET)
  if(SPQR_FOUND AND TARGET SuiteSparse::SPQR)
    get_target_property(_spqr_inc SuiteSparse::SPQR INTERFACE_INCLUDE_DIRECTORIES)
    if(_spqr_inc)
      set(SPQR_INCLUDES "${_spqr_inc}" CACHE PATH "SPQR include directory")
    endif()
    set(SPQR_LIBRARIES SuiteSparse::SPQR CACHE STRING "SPQR libraries")
    mark_as_advanced(SPQR_INCLUDES SPQR_LIBRARIES)
    return()
  endif()
endif()

# --- Fallback: manual search (SuiteSparse < 7.0 or no config package) ---
find_path(SPQR_INCLUDES
  NAMES
  SuiteSparseQR.hpp
  PATHS
  $ENV{SPQRDIR}
  ${INCLUDE_INSTALL_DIR}
  PATH_SUFFIXES
  suitesparse
  ufsparse
)

find_library(SPQR_LIBRARIES spqr $ENV{SPQRDIR} ${LIB_INSTALL_DIR})

if(SPQR_LIBRARIES)

  find_library(SUITESPARSE_LIBRARY SuiteSparse PATHS $ENV{SPQRDIR} ${LIB_INSTALL_DIR})
  if (SUITESPARSE_LIBRARY)
    set(SPQR_LIBRARIES ${SPQR_LIBRARIES} ${SUITESPARSE_LIBRARY})
  endif()

  find_library(CHOLMOD_LIBRARY cholmod PATHS $ENV{UMFPACK_LIBDIR} $ENV{UMFPACKDIR} ${LIB_INSTALL_DIR})
  if(CHOLMOD_LIBRARY)
    set(SPQR_LIBRARIES ${SPQR_LIBRARIES} ${CHOLMOD_LIBRARY})
  endif()

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SPQR DEFAULT_MSG SPQR_INCLUDES SPQR_LIBRARIES)

mark_as_advanced(SPQR_INCLUDES SPQR_LIBRARIES)
