@echo off
setlocal

rem Copyright Epic Games, Inc. All Rights Reserved.

set LIBRARY_NAME=oneTBB
set REPOSITORY_NAME=oneTBB

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
    -DCMAKE_INSTALL_INCLUDEDIR="%INSTALL_INCLUDEDIR%"^
    -DCMAKE_INSTALL_BINDIR="%INSTALL_BIN_DIR%"^
    -DCMAKE_INSTALL_LIBDIR="%INSTALL_LIB_DIR%"^
    -DTBB_TEST=OFF^
    -DTBB_STRICT=OFF
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

echo Removing legacy-named copies of libraries...
rmdir /S /Q "%INSTALL_LOCATION%\lib"

echo Removing share directory (documentation)...
rmdir /S /Q "%INSTALL_LOCATION%\share"

echo Removing pkgconfig files...
rmdir /S /Q "%INSTALL_LOCATION%\%INSTALL_LIB_DIR%\pkgconfig"

echo Done.

goto :eof

:usage
echo Usage: %BUILD_SCRIPT_NAME% ^<version^> ^<architecture: x64 or ARM64^>
exit /B 1

endlocal
