REM Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set LIBPNG_VERSION=1.6.44
set ZLIB_VERSION=1.3

rem x64 builds
call :build x64 Release "CMAKE_C_FLAGS_RELEASE:STRING=/Z7 /MD /O2 /Ob2 /DNDEBUG /Qvec" "Win64\Release\zlibstatic.lib"       || exit /b 1
call :build x64 Debug   "CMAKE_C_FLAGS_DEBUG:STRING=/Z7 /MDd /Ob0 /Od"                 "Win64\Release\zlibstatic.lib"       || exit /b 1

rem arm64 builds
call :build arm64 Release "CMAKE_C_FLAGS_RELEASE:STRING=/Z7 /MD /O2 /Ob2 /DNDEBUG /Qvec" "Win64\arm64\Release\zlibstatic.lib" || exit /b 1
call :build arm64 Debug   "CMAKE_C_FLAGS_DEBUG:STRING=/Z7 /MDd /Ob0 /Od"                 "Win64\arm64\Release\zlibstatic.lib" || exit /b 1

rem copy lib files from build folders

mkdir "libPNG-%LIBPNG_VERSION%\lib\Win64\x64\Release"
mkdir "libPNG-%LIBPNG_VERSION%\lib\Win64\x64\Debug"
mkdir "libPNG-%LIBPNG_VERSION%\lib\Win64\arm64\Release"
mkdir "libPNG-%LIBPNG_VERSION%\lib\Win64\arm64\Debug"

copy "build-x64\libpng16_static.lib"    "libPNG-%LIBPNG_VERSION%\lib\Win64\x64\Release\libpng.lib"
copy "build-x64\libpng16_staticd.lib"   "libPNG-%LIBPNG_VERSION%\lib\Win64\x64\Debug\libpng.lib"

copy "build-arm64\libpng16_static.lib"  "libPNG-%LIBPNG_VERSION%\lib\Win64\arm64\Release\libpng.lib"
copy "build-arm64\libpng16_staticd.lib" "libPNG-%LIBPNG_VERSION%\lib\Win64\arm64\Debug\libpng.lib"

rem copy generated include to source folder
copy "build-x64\pnglibconf.h" "libPNG-%LIBPNG_VERSION%"


echo Done!
goto :eof


:build
setlocal

REM This build script relies on VS2022 being present with clang support installed
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -host_arch=amd64 -arch=%1 -startdir=none -no_logo || exit /b 1

REM clang, cmake and ninja must be installed and available in your environment path

cmake.exe --fresh -Wno-dev                                                      ^
  -G "Ninja"                                                                    ^
  -B "Build-%1"                                                                 ^
  -S "libPNG-%LIBPNG_VERSION%"                                                  ^
  -D CMAKE_BUILD_TYPE=%2                                                        ^
  -D CMAKE_C_COMPILER:FILEPATH="clang-cl.exe"                                   ^
  -D CMAKE_TOOLCHAIN_FILE="%~dp0libPNG-%LIBPNG_VERSION%\CMake-Windows-%1.cmake" ^
  -D %3                                                                         ^
  -D ZLIB_INCLUDE_DIR=..\zlib\%ZLIB_VERSION%\include                            ^
  -D ZLIB_LIBRARY_RELEASE="..\zlib\%ZLIB_VERSION%\lib\%~4"                      ^
  -D PNG_SHARED:BOOL=OFF                                                        ^
  -D PNG_TESTS:BOOL=OFF                                                         ^
  -D PNG_TOOLS:BOOL=OFF                                                         ^
  || exit /b 1

ninja.exe -C "Build-%1" png_static || exit /b 1

endlocal
goto :eof
