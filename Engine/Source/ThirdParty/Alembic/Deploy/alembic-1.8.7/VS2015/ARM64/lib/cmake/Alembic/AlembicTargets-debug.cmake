#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Alembic::Alembic" for configuration "Debug"
set_property(TARGET Alembic::Alembic APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Alembic::Alembic PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/VS2015/ARM64/lib/Alembic_d.lib"
  )

list(APPEND _cmake_import_check_targets Alembic::Alembic )
list(APPEND _cmake_import_check_files_for_Alembic::Alembic "${_IMPORT_PREFIX}/VS2015/ARM64/lib/Alembic_d.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
