#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "embree" for configuration "Debug"
set_property(TARGET embree APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(embree PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libembree4_d.so.4.3.3"
  IMPORTED_SONAME_DEBUG "libembree4_d.so.4"
  )

list(APPEND _cmake_import_check_targets embree )
list(APPEND _cmake_import_check_files_for_embree "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libembree4_d.so.4.3.3" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
