# KLU lib usually requires linking to a blas library.
# It is up to the user of this module to find a BLAS and link to it.
#
# This module first tries to find KLU via its CMake config-mode package
# (shipped with SuiteSparse >= 7.0). If that fails, it falls back to a
# manual header/library search for compatibility with older installations.

if (KLU_INCLUDES AND KLU_LIBRARIES)
  set(KLU_FIND_QUIETLY TRUE)
endif ()

# --- Try config-mode first (SuiteSparse >= 7.0) ---
if(NOT KLU_INCLUDES OR NOT KLU_LIBRARIES)
  find_package(KLU CONFIG QUIET)
  if(KLU_FOUND AND TARGET SuiteSparse::KLU)
    get_target_property(_klu_inc SuiteSparse::KLU INTERFACE_INCLUDE_DIRECTORIES)
    if(_klu_inc)
      set(KLU_INCLUDES "${_klu_inc}" CACHE PATH "KLU include directory")
    endif()
    set(KLU_LIBRARIES SuiteSparse::KLU CACHE STRING "KLU libraries")
    mark_as_advanced(KLU_INCLUDES KLU_LIBRARIES)
    return()
  endif()
endif()

# --- Fallback: manual search (SuiteSparse < 7.0 or no config package) ---
find_path(KLU_INCLUDES
  NAMES
  klu.h
  PATHS
  $ENV{KLUDIR}
  ${INCLUDE_INSTALL_DIR}
  PATH_SUFFIXES
  suitesparse
  ufsparse
)

find_library(KLU_LIBRARIES klu PATHS $ENV{KLUDIR} ${LIB_INSTALL_DIR})

if(KLU_LIBRARIES)

  if(NOT KLU_LIBDIR)
    get_filename_component(KLU_LIBDIR ${KLU_LIBRARIES} PATH)
  endif()

  find_library(COLAMD_LIBRARY colamd PATHS ${KLU_LIBDIR} $ENV{KLUDIR} ${LIB_INSTALL_DIR})
  if(COLAMD_LIBRARY)
    set(KLU_LIBRARIES ${KLU_LIBRARIES} ${COLAMD_LIBRARY})
  endif ()

  find_library(AMD_LIBRARY amd PATHS ${KLU_LIBDIR} $ENV{KLUDIR} ${LIB_INSTALL_DIR})
  if(AMD_LIBRARY)
    set(KLU_LIBRARIES ${KLU_LIBRARIES} ${AMD_LIBRARY})
  endif ()

  find_library(BTF_LIBRARY btf PATHS $ENV{KLU_LIBDIR} $ENV{KLUDIR} ${LIB_INSTALL_DIR})
  if(BTF_LIBRARY)
    set(KLU_LIBRARIES ${KLU_LIBRARIES} ${BTF_LIBRARY})
  endif()

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KLU DEFAULT_MSG
                                  KLU_INCLUDES KLU_LIBRARIES)

mark_as_advanced(KLU_INCLUDES KLU_LIBRARIES AMD_LIBRARY COLAMD_LIBRARY BTF_LIBRARY)
