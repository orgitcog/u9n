@echo off
setlocal
rem Copyright Epic Games, Inc. All Rights Reserved.

rem Setup part
setlocal

if [%1]==[] goto usage

set OCIO_VERSION=2.4.1
set OCIO_LIB_NAME=OpenColorIO-%OCIO_VERSION%
set ARCH_NAME=%1
set UE_MODULE_LOCATION=%cd%
set UE_THIRD_PARTY_LOCATION=%UE_MODULE_LOCATION%\..
set IMATH_CMAKE_LOCATION=%UE_THIRD_PARTY_LOCATION%\Imath\Deploy\Imath-3.1.12\VS2015\%ARCH_NAME%\lib\cmake\Imath
set ZLIB_CMAKE_LOCATION=%UE_THIRD_PARTY_LOCATION%\zlib\1.3
if %ARCH_NAME% == x64 (
    set ZLIB_LIBRARY_PATH=%UE_THIRD_PARTY_LOCATION%\zlib\1.3\lib\Win64\Release\zlibstatic.lib
) else (
    set ZLIB_LIBRARY_PATH=%UE_THIRD_PARTY_LOCATION%\zlib\1.3\lib\Win64\%ARCH_NAME%\zlibstatic.lib
)

rem Specify all of the include/bin/lib directory variables so that CMake can
rem compute relative paths correctly for the imported targets.
set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\OpenColorIO
set INSTALL_INCLUDEDIR=include
set INSTALL_BIN_DIR=bin\Win64\%ARCH_NAME%
set INSTALL_LIB_DIR=lib\Win64\%ARCH_NAME%

rem Remove previously extracted build library folder
if exist .\%OCIO_LIB_NAME% (
    rd /S /Q .\%OCIO_LIB_NAME%
)
    
git clone --depth 1 --branch v%OCIO_VERSION% https://github.com/AcademySoftwareFoundation/OpenColorIO.git %OCIO_LIB_NAME%

cd /d .\%OCIO_LIB_NAME%
set DEPLOY_FOLDER=..\Deploy\OpenColorIO

if %ARCH_NAME% == ARM64 (
    rem TODO: This will be fixed in the next version (see PR 2089), remove then.
    git apply ../ue_ocio_v24.patch
)

rem Configure OCIO cmake and launch a release build
echo Configuring %ARCH_NAME% build...
cmake -S . -B build -G "Visual Studio 17 2022"^
    -A %ARCH_NAME%^
    -DCMAKE_PREFIX_PATH="%IMATH_CMAKE_LOCATION%;%ZLIB_CMAKE_LOCATION%"^
    -DZLIB_LIBRARY="%ZLIB_LIBRARY_PATH%"^
    -DCMAKE_INSTALL_PREFIX:PATH="%INSTALL_LOCATION%"^
    -DCMAKE_INSTALL_INCLUDEDIR="%INSTALL_INCLUDEDIR%"^
    -DCMAKE_INSTALL_BINDIR="%INSTALL_BIN_DIR%"^
    -DCMAKE_INSTALL_LIBDIR="%INSTALL_LIB_DIR%"^
    -DBUILD_SHARED_LIBS=OFF^
    -DOCIO_BUILD_APPS=OFF^
    -DOCIO_BUILD_DOCS=OFF^
    -DOCIO_BUILD_TESTS=OFF^
    -DOCIO_BUILD_GPU_TESTS=OFF^
    -DOCIO_BUILD_PYTHON=OFF

echo Building %ARCH_NAME% Release build...
cmake --build build --config Release
cmake --install build --config Release

echo Copying external static library dependencies...
xcopy .\build\ext\dist\lib\Win64\%ARCH_NAME%\*.lib %DEPLOY_FOLDER%\lib\Win64\%ARCH_NAME%\* /Y

endlocal
pause

echo Done.
exit /B 0

:usage
echo Arch: x64 or ARM64
exit /B 1

endlocal