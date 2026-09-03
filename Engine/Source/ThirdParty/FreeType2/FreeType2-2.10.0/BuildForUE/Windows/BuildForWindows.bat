@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal
REM Note that this needs to be run from a developer command prompt as admin 
set ENGINE_ROOT=%CD%\..\..\..\..\..\..\

set ZLIB_VERSION=1.3
set PATH_TO_ZLIB=%CD%\..\..\..\..\zlib\%ZLIB_VERSION%
set PATH_TO_ZLIB_SRC=%PATH_TO_ZLIB%\include
REM This is a dummy library to satisfy the find_package probe for zlib. This will not actually get linked 
set PATH_TO_ZLIB_LIB=%PATH_TO_ZLIB%\lib\Win64\Release\zlibstatic.lib

set PNG_VERSION=libPNG-1.6.44
set PATH_TO_PNG=%CD%\..\..\..\..\libPNG\%PNG_VERSION%
set PATH_TO_PNG_SRC=%PATH_TO_PNG%
REM This is a dummy lib to satisfy the find_package probe for libpng. THis will not be linked 
set PATH_TO_PNG_LIB=%PATH_TO_PNG%\lib\Win64\x64\Release\libpng.lib

set FREETYPE_WINDOWS_VERSION=FreeType2-2.10.0
set CMAKE_ADDITIONAL_ARGUMENTS=-DFT_WITH_ZLIB=ON
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DFT_WITH_PNG=ON
REM We do not want to use HB in FreeType 
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DFT_WITH_HARFBUZZ=OFF
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE
REM Uncomment this to output debug messages while building the library 
REM set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_FIND_DEBUG_MODE=ON
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DZLIB_INCLUDE_DIR="%PATH_TO_ZLIB_SRC%"
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DZLIB_LIBRARY="%PATH_TO_ZLIB_LIB%"
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DPNG_PNG_INCLUDE_DIR=%PATH_TO_PNG_SRC%
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DPNG_LIBRARY="%PATH_TO_PNG_LIB%"

set MAKE_TARGET=freetype


echo Creating FreeType2 libraries for x64...
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetLib=FreeType2 -TargetLibVersion=%FREETYPE_WINDOWS_VERSION% -TargetConfigs=Debug+Release -LibOutputPath=lib -TargetArchitecture=x64  -CMakeGenerator=VS2022 -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -MakeTarget=%MAKE_TARGET% -SkipCreateChangelist || exit /b

echo Creating FreeType2 libraries for arm64...
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetLib=FreeType2 -TargetLibVersion=%FREETYPE_WINDOWS_VERSION% -TargetConfigs=Debug+Release -LibOutputPath=lib -TargetArchitecture=ARM64 -CMakeGenerator=VS2022 -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -MakeTarget=%MAKE_TARGET% -SkipCreateChangelist || exit /b



endlocal
