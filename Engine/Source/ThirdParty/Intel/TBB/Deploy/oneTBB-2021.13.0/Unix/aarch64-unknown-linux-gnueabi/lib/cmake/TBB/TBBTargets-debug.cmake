#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "TBB::tbb" for configuration "Debug"
set_property(TARGET TBB::tbb APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(TBB::tbb PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbb_debug.so.12.13"
  IMPORTED_SONAME_DEBUG "libtbb_debug.so.12"
  )

list(APPEND _cmake_import_check_targets TBB::tbb )
list(APPEND _cmake_import_check_files_for_TBB::tbb "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbb_debug.so.12.13" )

# Import target "TBB::tbbmalloc" for configuration "Debug"
set_property(TARGET TBB::tbbmalloc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(TBB::tbbmalloc PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc_debug.so.2.13"
  IMPORTED_SONAME_DEBUG "libtbbmalloc_debug.so.2"
  )

list(APPEND _cmake_import_check_targets TBB::tbbmalloc )
list(APPEND _cmake_import_check_files_for_TBB::tbbmalloc "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc_debug.so.2.13" )

# Import target "TBB::tbbmalloc_proxy" for configuration "Debug"
set_property(TARGET TBB::tbbmalloc_proxy APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(TBB::tbbmalloc_proxy PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "TBB::tbbmalloc"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc_proxy_debug.so.2.13"
  IMPORTED_SONAME_DEBUG "libtbbmalloc_proxy_debug.so.2"
  )

list(APPEND _cmake_import_check_targets TBB::tbbmalloc_proxy )
list(APPEND _cmake_import_check_files_for_TBB::tbbmalloc_proxy "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc_proxy_debug.so.2.13" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
