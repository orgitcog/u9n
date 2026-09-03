@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set ENGINE_ROOT=%CD%\..\..\..\..\..\..\

set ZLIB_VERSION=1.3
set PATH_TO_ZLIB=%CD%\..\..\..\..\zlib\%ZLIB_VERSION%
set PATH_TO_ZLIB_SRC=%PATH_TO_ZLIB%\include
REM This is a dummy library to satisfy the find_package probe for zlib. This will not actually get linked 
set PATH_TO_ZLIB_LIB=%PATH_TO_ZLIB%\lib\Unix\x86_64-unknown-linux-gnu\Release\libz.a

set PNG_VERSION=libPNG-1.6.44
set PATH_TO_PNG=%CD%\..\..\..\..\libPNG\%PNG_VERSION%
set PATH_TO_PNG_SRC=%PATH_TO_PNG%
REM This is a dummy lib to satisfy the find_package probe for libpng. THis will not be linked 
set PATH_TO_PNG_LIB=%PATH_TO_PNG%\lib\Unix\x86_64-unknown-linux-gnu\Release\libpng.a

set FREETYPE_LINUX_VERSION=FreeType2-2.10.0
REM We pass these flags to strip out unused code and data in the release version of the libs 
REM set COMPILER_CFLAGS=-ffunction-sections -fdata-sections
set CMAKE_ADDITIONAL_ARGUMENTS=-DFT_WITH_ZLIB=ON
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DFT_WITH_PNG=ON
REM We do not want to use HB in FreeType 
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DFT_WITH_HARFBUZZ=OFF
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE
REM Uncomment this to output debug messages while building the library 
REM set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_FIND_DEBUG_MODE=ON
REM to create the fpic version of the file to have a smaller memory footprint 
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DCMAKE_POSITION_INDEPENDENT_CODE=ON
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DZLIB_INCLUDE_DIR="%PATH_TO_ZLIB_SRC%"
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DZLIB_LIBRARY="%PATH_TO_ZLIB_LIB%"
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DPNG_PNG_INCLUDE_DIR=%PATH_TO_PNG_SRC%
set CMAKE_ADDITIONAL_ARGUMENTS=%CMAKE_ADDITIONAL_ARGUMENTS% -DPNG_LIBRARY="%PATH_TO_PNG_LIB%"

set MAKE_TARGET=freetype

echo Creating FreeType2 libraries for x86_64-unknown-linux-gnu...
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetLib=FreeType2 -TargetLibVersion=%FREETYPE_LINUX_VERSION% -TargetConfigs=Debug+Release -LibOutputPath=lib -TargetArchitecture=x86_64-unknown-linux-gnu -CMakeGenerator=Makefile -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -MakeTarget=%MAKE_TARGET% -SkipCreateChangelist || exit /b

echo Creating FreeType2 libraries for aarch64-unknown-linux-gnueabi...
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetLib=FreeType2 -TargetLibVersion=%FREETYPE_LINUX_VERSION% -TargetConfigs=Debug+Release -LibOutputPath=lib -TargetArchitecture=aarch64-unknown-linux-gnueabi -CMakeGenerator=Makefile -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -MakeTarget=%MAKE_TARGET% -SkipCreateChangelist || exit /b



endlocal
