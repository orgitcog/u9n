#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "TBB::tbb" for configuration "Release"
set_property(TARGET TBB::tbb APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(TBB::tbb PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/VS2015/ARM64/lib/tbb12.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/ARM64/bin/tbb12.dll"
  )

list(APPEND _cmake_import_check_targets TBB::tbb )
list(APPEND _cmake_import_check_files_for_TBB::tbb "${_IMPORT_PREFIX}/VS2015/ARM64/lib/tbb12.lib" "${_IMPORT_PREFIX}/VS2015/ARM64/bin/tbb12.dll" )

# Import target "TBB::tbbmalloc" for configuration "Release"
set_property(TARGET TBB::tbbmalloc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(TBB::tbbmalloc PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/VS2015/ARM64/lib/tbbmalloc.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/ARM64/bin/tbbmalloc.dll"
  )

list(APPEND _cmake_import_check_targets TBB::tbbmalloc )
list(APPEND _cmake_import_check_files_for_TBB::tbbmalloc "${_IMPORT_PREFIX}/VS2015/ARM64/lib/tbbmalloc.lib" "${_IMPORT_PREFIX}/VS2015/ARM64/bin/tbbmalloc.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
