@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\..\

REM make sure we are running from VS2019 command prompt
if "%VS160COMNTOOLS%"=="" (
	echo ERROR: Please run this from Developer Command Prompt For VS2019
	goto end
)

REM Temporary build directories (used as working directories when running CMake)
set VS2019_X64_PATH="%PATH_TO_CMAKE_FILE%\..\lib\Win64\VS2019\Build"
set VS2019_ARM64_PATH="%PATH_TO_CMAKE_FILE%\..\lib\WinArm64\VS2019\Build"

REM Build for VS2019 (64-bit)
echo Generating Flite solution for VS2019 (64-bit)...
if exist %VS2019_X64_PATH% (rmdir %VS2019_X64_PATH% /s/q)
mkdir %VS2019_X64_PATH%
cd %VS2019_X64_PATH%
cmake -G "Visual Studio 16 2019" -A x64 %PATH_TO_CMAKE_FILE%
echo Building Flite solution for VS2019 (64-bit, Debug)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build Debug
echo Building Flite solution for VS2019 (64-bit, Release)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build Release
echo Building Flite solution for VS2019 (64-bit, RelWithDebInfo)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
rmdir %VS2019_X64_PATH% /s/q

REM Build for VS2019 (Arm64)
echo Generating Flite solution for VS2019 (Arm64)...
if exist %VS2019_ARM64_PATH% (rmdir %VS2019_ARM64_PATH% /s/q)
mkdir %VS2019_ARM64_PATH%
cd %VS2019_ARM64_PATH%
cmake -G "Visual Studio 16 2019" -A arm64 %PATH_TO_CMAKE_FILE%
echo Building Flite solution for VS2019 (Arm64, Debug)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build Debug
echo Building Flite solution for VS2019 (Arm64, Release)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build Release
echo Building Flite solution for VS2019 (Arm64, RelWithDebInfo)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
rmdir %VS2019_ARM64_PATH% /s/q


:end
endlocal
