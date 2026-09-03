@echo off
setlocal

rem Copyright Epic Games, Inc. All Rights Reserved.

set LIBRARY_NAME=OpenVDB
set REPOSITORY_NAME=openvdb

rem When building OpenVDB, be sure to apply the following patches:
rem   - allow use of the library with RTTI disabled
rem   - add missing math::half instantiations of cwiseAdd()
rem From the "openvdb-12.0.0" source directory, run:
rem     git apply ../openvdb_12.0.0_support_disabling_RTTI.patch
rem     git apply ../openvdb_12.0.0_Vec_half_cwiseAdd.patch

set BUILD_SCRIPT_NAME=%~n0%~x0
set BUILD_SCRIPT_LOCATION=%~dp0

rem Get version and architecture from arguments.
set LIBRARY_VERSION=%1
if [%LIBRARY_VERSION%]==[] goto usage

set ARCH_NAME=%2
if [%ARCH_NAME%]==[] goto usage

rem Set as VS2015 for backwards compatibility even though VS2022 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015

set UE_MODULE_LOCATION=%BUILD_SCRIPT_LOCATION%
set UE_SOURCE_THIRD_PARTY_LOCATION=%UE_MODULE_LOCATION%\..

set ZLIB_LOCATION=%UE_SOURCE_THIRD_PARTY_LOCATION%\zlib\1.3
set ZLIB_INCLUDE_LOCATION=%ZLIB_LOCATION%\include
if "%ARCH_NAME%"=="x64" (
    set ZLIB_LIB_LOCATION=%ZLIB_LOCATION%\lib\Win64\Release\zlibstatic.lib
) else (
    set ZLIB_LIB_LOCATION=%ZLIB_LOCATION%\lib\Win64\arm64\Release\zlibstatic.lib
)

set TBB_LOCATION=%UE_SOURCE_THIRD_PARTY_LOCATION%\Intel\TBB\Deploy\oneTBB-2021.13.0
set TBB_CMAKE_LOCATION=%TBB_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib\cmake\TBB
rem The TBB CMake config would be sufficient, but OpenVDB has its own
rem FindTBB.cmake that we have to appease.
set TBB_INCLUDE_LOCATION=%TBB_LOCATION%\include
set TBB_LIB_LOCATION=%TBB_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib

set BLOSC_LOCATION=%UE_SOURCE_THIRD_PARTY_LOCATION%\Blosc\Deploy\c-blosc-1.21.0
set BLOSC_INCLUDE_LOCATION=%BLOSC_LOCATION%\include
set BLOSC_LIB_LOCATION=%BLOSC_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib
set BLOSC_LIBRARY_LOCATION_RELEASE=%BLOSC_LIB_LOCATION%\libblosc.lib
set BLOSC_LIBRARY_LOCATION_DEBUG=%BLOSC_LIB_LOCATION%\libblosc_d.lib

set BOOST_CMAKE_LOCATION=%UE_SOURCE_THIRD_PARTY_LOCATION%\Boost\Deploy\boost-1.85.0\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib\cmake\Boost-1.85.0

set SOURCE_LOCATION=%UE_MODULE_LOCATION%\%REPOSITORY_NAME%-%LIBRARY_VERSION%

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

rem Specify all of the include/bin/lib directory variables so that CMake can
rem compute relative paths correctly for the imported targets.
set INSTALL_INCLUDEDIR=include
set INSTALL_BIN_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\bin
set INSTALL_LIB_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\%REPOSITORY_NAME%-%LIBRARY_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\%INSTALL_INCLUDEDIR%
set INSTALL_WIN_ARCH_LOCATION=%INSTALL_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)
if exist %INSTALL_INCLUDE_LOCATION% (
    rmdir %INSTALL_INCLUDE_LOCATION% /S /Q)
if exist %INSTALL_WIN_ARCH_LOCATION% (
    rmdir %INSTALL_WIN_ARCH_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

set NUM_CPU=8

echo Configuring build for %LIBRARY_NAME% version %LIBRARY_VERSION%...
cmake -G "Visual Studio 17 2022" %SOURCE_LOCATION%^
    -A %ARCH_NAME%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_PREFIX_PATH="%TBB_CMAKE_LOCATION%;%BOOST_CMAKE_LOCATION%"^
    -DCMAKE_INSTALL_INCLUDEDIR="%INSTALL_INCLUDEDIR%"^
    -DCMAKE_INSTALL_BINDIR="%INSTALL_BIN_DIR%"^
    -DCMAKE_INSTALL_LIBDIR="%INSTALL_LIB_DIR%"^
    -DZLIB_INCLUDE_DIR="%ZLIB_INCLUDE_LOCATION%"^
    -DZLIB_LIBRARY="%ZLIB_LIB_LOCATION%"^
    -DTBB_INCLUDEDIR="%TBB_INCLUDE_LOCATION%"^
    -DTBB_LIBRARYDIR="%TBB_LIB_LOCATION%"^
    -DBLOSC_INCLUDEDIR="%BLOSC_INCLUDE_LOCATION%"^
    -DBLOSC_LIBRARYDIR="%BLOSC_LIB_LOCATION%"^
    -DBLOSC_USE_STATIC_LIBS=ON^
    -DBlosc_LIBRARY_RELEASE="%BLOSC_LIBRARY_LOCATION_RELEASE%"^
    -DBlosc_LIBRARY_DEBUG="%BLOSC_LIBRARY_LOCATION_DEBUG%"^
    -DUSE_PKGCONFIG=OFF^
    -DUSE_EXPLICIT_INSTANTIATION=OFF^
    -DOPENVDB_BUILD_BINARIES=OFF^
    -DOPENVDB_INSTALL_CMAKE_MODULES=OFF^
    -DOPENVDB_CORE_SHARED=OFF^
    -DOPENVDB_CORE_STATIC=ON^
    -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"^
    -DCMAKE_DEBUG_POSTFIX=_d^
    -DMSVC_MP_THREAD_COUNT="%NUM_CPU%"
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building %LIBRARY_NAME% for Debug...
cmake --build . --config Debug -j%NUM_CPU%
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing %LIBRARY_NAME% for Debug...
cmake --install . --config Debug
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building %LIBRARY_NAME% for Release...
cmake --build . --config Release -j%NUM_CPU%
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing %LIBRARY_NAME% for Release...
cmake --install . --config Release
if %errorlevel% neq 0 exit /B %errorlevel%

popd

echo Done.

goto :eof

:usage
echo Usage: %BUILD_SCRIPT_NAME% ^<version^> ^<architecture: x64 or ARM64^>
exit /B 1

endlocal
