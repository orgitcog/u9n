#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "TBB::tbb" for configuration "Release"
set_property(TARGET TBB::tbb APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(TBB::tbb PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbb.so.12.13"
  IMPORTED_SONAME_RELEASE "libtbb.so.12"
  )

list(APPEND _cmake_import_check_targets TBB::tbb )
list(APPEND _cmake_import_check_files_for_TBB::tbb "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbb.so.12.13" )

# Import target "TBB::tbbmalloc" for configuration "Release"
set_property(TARGET TBB::tbbmalloc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(TBB::tbbmalloc PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc.so.2.13"
  IMPORTED_SONAME_RELEASE "libtbbmalloc.so.2"
  )

list(APPEND _cmake_import_check_targets TBB::tbbmalloc )
list(APPEND _cmake_import_check_files_for_TBB::tbbmalloc "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc.so.2.13" )

# Import target "TBB::tbbmalloc_proxy" for configuration "Release"
set_property(TARGET TBB::tbbmalloc_proxy APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(TBB::tbbmalloc_proxy PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "TBB::tbbmalloc"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc_proxy.so.2.13"
  IMPORTED_SONAME_RELEASE "libtbbmalloc_proxy.so.2"
  )

list(APPEND _cmake_import_check_targets TBB::tbbmalloc_proxy )
list(APPEND _cmake_import_check_files_for_TBB::tbbmalloc_proxy "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libtbbmalloc_proxy.so.2.13" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
