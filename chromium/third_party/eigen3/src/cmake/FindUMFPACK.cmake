# Umfpack lib usually requires linking to a blas library.
# It is up to the user of this module to find a BLAS and link to it.
#
# This module first tries to find UMFPACK via its CMake config-mode package
# (shipped with SuiteSparse >= 7.0). If that fails, it falls back to a
# manual header/library search for compatibility with older installations.
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

if (UMFPACK_INCLUDES AND UMFPACK_LIBRARIES)
  set(UMFPACK_FIND_QUIETLY TRUE)
endif ()

# --- Try config-mode first (SuiteSparse >= 7.0) ---
if(NOT UMFPACK_INCLUDES OR NOT UMFPACK_LIBRARIES)
  find_package(UMFPACK CONFIG QUIET)
  if(UMFPACK_FOUND AND TARGET SuiteSparse::UMFPACK)
    get_target_property(_umfpack_inc SuiteSparse::UMFPACK INTERFACE_INCLUDE_DIRECTORIES)
    if(_umfpack_inc)
      set(UMFPACK_INCLUDES "${_umfpack_inc}" CACHE PATH "UMFPACK include directory")
    endif()
    set(UMFPACK_LIBRARIES SuiteSparse::UMFPACK CACHE STRING "UMFPACK libraries")
    mark_as_advanced(UMFPACK_INCLUDES UMFPACK_LIBRARIES)
    return()
  endif()
endif()

# --- Fallback: manual search (SuiteSparse < 7.0 or no config package) ---
find_path(UMFPACK_INCLUDES
  NAMES
  umfpack.h
  PATHS
  $ENV{UMFPACKDIR}
  ${INCLUDE_INSTALL_DIR}
  PATH_SUFFIXES
  suitesparse
  ufsparse
)

find_library(UMFPACK_LIBRARIES umfpack PATHS $ENV{UMFPACKDIR} ${LIB_INSTALL_DIR})

if(UMFPACK_LIBRARIES)

  if(NOT UMFPACK_LIBDIR)
    get_filename_component(UMFPACK_LIBDIR ${UMFPACK_LIBRARIES} PATH)
  endif()

  find_library(COLAMD_LIBRARY colamd PATHS ${UMFPACK_LIBDIR} $ENV{UMFPACKDIR} ${LIB_INSTALL_DIR})
  if(COLAMD_LIBRARY)
    set(UMFPACK_LIBRARIES ${UMFPACK_LIBRARIES} ${COLAMD_LIBRARY})
  endif ()

  find_library(AMD_LIBRARY amd PATHS ${UMFPACK_LIBDIR} $ENV{UMFPACKDIR} ${LIB_INSTALL_DIR})
  if(AMD_LIBRARY)
    set(UMFPACK_LIBRARIES ${UMFPACK_LIBRARIES} ${AMD_LIBRARY})
  endif ()

  find_library(SUITESPARSE_LIBRARY SuiteSparse PATHS ${UMFPACK_LIBDIR} $ENV{UMFPACKDIR} ${LIB_INSTALL_DIR})
  if(SUITESPARSE_LIBRARY)
    set(UMFPACK_LIBRARIES ${UMFPACK_LIBRARIES} ${SUITESPARSE_LIBRARY})
  endif ()

  find_library(CHOLMOD_LIBRARY cholmod PATHS $ENV{UMFPACK_LIBDIR} $ENV{UMFPACKDIR} ${LIB_INSTALL_DIR})
  if(CHOLMOD_LIBRARY)
    set(UMFPACK_LIBRARIES ${UMFPACK_LIBRARIES} ${CHOLMOD_LIBRARY})
  endif()

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UMFPACK DEFAULT_MSG
                                  UMFPACK_INCLUDES UMFPACK_LIBRARIES)

mark_as_advanced(UMFPACK_INCLUDES UMFPACK_LIBRARIES AMD_LIBRARY COLAMD_LIBRARY CHOLMOD_LIBRARY SUITESPARSE_LIBRARY)
