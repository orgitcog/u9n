#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "embree" for configuration "Debug"
set_property(TARGET embree APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(embree PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/VS2015/x64/lib/embree4_d.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/VS2015/x64/bin/embree4_d.dll"
  )

list(APPEND _cmake_import_check_targets embree )
list(APPEND _cmake_import_check_files_for_embree "${_IMPORT_PREFIX}/VS2015/x64/lib/embree4_d.lib" "${_IMPORT_PREFIX}/VS2015/x64/bin/embree4_d.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
