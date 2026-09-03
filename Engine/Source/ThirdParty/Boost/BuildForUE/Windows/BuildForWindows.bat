@echo off
setlocal EnableDelayedExpansion

rem Copyright Epic Games, Inc. All Rights Reserved.

set LIBRARY_NAME=Boost
set REPOSITORY_NAME=boost

rem Informational, for the usage message.
set CURRENT_LIBRARY_VERSION=1.85.0

set BUILD_SCRIPT_NAME=%~n0%~x0
set BUILD_SCRIPT_DIR=%~dp0

rem Get version and architecture from arguments.
set LIBRARY_VERSION=%1
if [%LIBRARY_VERSION%]==[] goto usage

set ARCH_NAME=%2
if [%ARCH_NAME%]==[] goto usage

rem Boost uses the name "x86" for x86_64 architectures
rem and "arm" for arm64 architectures (because we use
rem address-model=64).
set BOOST_ARCH_NAME=""

if "%ARCH_NAME%"=="x64" (
    set BOOST_ARCH_NAME=x86
)
if "%ARCH_NAME%"=="ARM64" (
    set BOOST_ARCH_NAME=arm
)

if [%BOOST_ARCH_NAME%]==[""] goto usage

shift
shift

rem Get the requested libraries to build from arguments, if any.
set ARG_LIBRARIES=%1
set BOOST_WITH_LIBRARIES=--with-%1
shift

:extract_arg_libraries
if not "%1"=="" (
    set ARG_LIBRARIES=%ARG_LIBRARIES%, %1
    set BOOST_WITH_LIBRARIES=%BOOST_WITH_LIBRARIES% --with-%1
    shift
    goto extract_arg_libraries
)

set BOOST_BUILD_LIBRARIES=0
if not [!ARG_LIBRARIES!]==[] set BOOST_BUILD_LIBRARIES=1

rem Print arguments to make spotting errors in the arguments easier.
echo Provided arguments:
echo     Boost version: %LIBRARY_VERSION%
echo     Architecture : %ARCH_NAME%
if !BOOST_BUILD_LIBRARIES!==1 (
    echo     Build libraries: !ARG_LIBRARIES!
) else (
    echo     Build libraries: ^<headers-only^>
)
echo.

rem Set as VS2015 for backwards compatibility even though VS2022 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015

set UE_MODULE_LOCATION=%BUILD_SCRIPT_DIR%..\..
set UE_ENGINE_LOCATION=%UE_MODULE_LOCATION%\..\..\..

set PYTHON_EXECUTABLE_LOCATION=%UE_ENGINE_LOCATION%\Binaries\ThirdParty\Python3\Win64\python.exe
set PYTHON_VERSION=3.11

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

set INSTALL_INCLUDEDIR=include

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\%REPOSITORY_NAME%-%LIBRARY_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\%INSTALL_INCLUDEDIR%
set INSTALL_WIN_ARCH_LOCATION=%INSTALL_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%
set INSTALL_LIB_LOCATION=%INSTALL_WIN_ARCH_LOCATION%\lib

if exist %INSTALL_INCLUDE_LOCATION% (
    rmdir %INSTALL_INCLUDE_LOCATION% /S /Q)
if exist %INSTALL_WIN_ARCH_LOCATION% (
    rmdir %INSTALL_WIN_ARCH_LOCATION% /S /Q)

rem Set the following variable to 1 if you already downloaded and extracted the boost sources, and you need to play around with the build configuration.
set ALREADY_HAVE_SOURCES=0

rem Set up paths and filenames.
set LIBRARY_VERSION_FILENAME=boost_%LIBRARY_VERSION:.=_%

if %ALREADY_HAVE_SOURCES%==0 (
    rem Remove previous intermediate files to allow for a clean build.
    if exist %BUILD_LOCATION% (
        rem Filenames in the intermediate directory are likely too long for tools like 'rmdir' to handle. Instead, we use robocopy to mirror an empty temporary folder, and then delete it.
        echo [%time%] Deleting previous intermediate files in '%BUILD_LOCATION%'...
        mkdir "%BUILD_LOCATION%_DELETE"
        robocopy "%BUILD_LOCATION%_DELETE" "%BUILD_LOCATION%" /purge /W:0 /R:0 > NUL
        rmdir "%BUILD_LOCATION%_DELETE"
        rmdir "%BUILD_LOCATION%"
    )

    mkdir %BUILD_LOCATION%
)

pushd %BUILD_LOCATION%

if %ALREADY_HAVE_SOURCES%==0 (
    rem Download ZIP files.
    set BOOST_ZIP_FILE=!LIBRARY_VERSION_FILENAME!.zip
    set BOOST_URL=https://boostorg.jfrog.io/artifactory/main/release/%LIBRARY_VERSION%/source/!BOOST_ZIP_FILE!
    echo [!time!] Downloading !BOOST_URL!...
    powershell -Command "(New-Object Net.WebClient).DownloadFile('!BOOST_URL!', '!BOOST_ZIP_FILE!')"
    if not errorlevel 0 goto error

    rem Extract ZIP file.
    echo [!time!] Extracting !BOOST_ZIP_FILE!...
    tar -xf !BOOST_ZIP_FILE!
    if not errorlevel 0 goto error
) else (
    echo Expecting sources to already be available at '%BUILD_LOCATION%\%LIBRARY_VERSION_FILENAME%'.
)

rem Build and install or just copy header files.
pushd %LIBRARY_VERSION_FILENAME%

if !BOOST_BUILD_LIBRARIES!==1 (
    rem Set tool set to current UE tool set.
    set BOOST_TOOLSET=msvc-14.3

    rem Bootstrap before build.
    set LOG_FILE=%BUILD_LOCATION%\%LIBRARY_VERSION_FILENAME%_bootstrap.log
    echo [!time!] Bootstrapping Boost %LIBRARY_VERSION%, see '!LOG_FILE!' for details...

    call .\bootstrap.bat ^
        --prefix=%INSTALL_LOCATION%^
        --includedir=%INSTALL_INCLUDE_LOCATION%^
        --libdir=%INSTALL_LIB_LOCATION%^
        --with-toolset=%BOOST_TOOLSET%^
        --with-python=%PYTHON_EXECUTABLE_LOCATION%^
        --with-python-version=%PYTHON_VERSION%^
        > !LOG_FILE! 2>&1
    if not errorlevel 0 goto error

    rem Provide user config to provide tool set version and Python configuration.
    set BOOST_USER_CONFIG=%BUILD_SCRIPT_DIR%\user-config.jam

    set NUM_CPU=8

    rem Build all libraries.
    set LOG_FILE=%BUILD_LOCATION%\%LIBRARY_VERSION_FILENAME%_build.log
    echo [!time!] Building Boost %LIBRARY_VERSION%, see '!LOG_FILE!' for details...
    .\b2.exe ^
        --prefix=%INSTALL_LOCATION%^
        --includedir=%INSTALL_INCLUDE_LOCATION%^
        --libdir=%INSTALL_LIB_LOCATION%^
        -j!NUM_CPU!^
        address-model=64^
        threading=multi^
        variant=release^
        %BOOST_WITH_LIBRARIES%^
        --user-config=!BOOST_USER_CONFIG!^
        --hash^
        --build-type=complete^
        --layout=tagged^
        --debug-configuration^
        toolset=!BOOST_TOOLSET!^
        architecture=!BOOST_ARCH_NAME!^
        install^
        > !LOG_FILE! 2>&1
    if not errorlevel 0 goto error
) else (
    rem Copy header files using robocopy to prevent issues with long file paths.
    if not exist %INSTALL_LOCATION% (
        mkdir %INSTALL_LOCATION%
    )
    set LOG_FILE=%BUILD_LOCATION%\%LIBRARY_VERSION_FILENAME%_robocopy.log
    echo [!time!] Copying header files, see '!LOG_FILE!' for details...
    set HEADERS_SOURCE=boost
    set HEADERS_DESTINATION=%INSTALL_LOCATION%\include\boost
    robocopy !HEADERS_SOURCE! !HEADERS_DESTINATION! /e > !LOG_FILE! 2>&1
    set ROBOCOPY_SUCCESS=false
    if errorlevel 0 set ROBOCOPY_SUCCESS=true
    if errorlevel 1 set ROBOCOPY_SUCCESS=true
    if !ROBOCOPY_SUCCESS!=="false" goto error
)

popd
popd

echo [!time!] Boost %LIBRARY_VERSION% installed to '%INSTALL_LOCATION%'.
echo [!time!] Done.
goto :eof


rem Helper functions

:error
echo [!time!] Last command returned an error!
echo [!time!] Abort.
exit /B 1

:usage
echo Build %LIBRARY_NAME% for use with Unreal Engine on Windows
echo.
echo Usage:
echo.
echo     %BUILD_SCRIPT_NAME% ^<%LIBRARY_NAME% Version^> ^<Architecture Name: x64 or ARM64^> [^<library name^> [^<library name^> ...]]
echo.
echo Usage examples:
echo.
echo     %BUILD_SCRIPT_NAME% %CURRENT_LIBRARY_VERSION% x64
echo       -- Installs %LIBRARY_NAME% version %CURRENT_LIBRARY_VERSION% as header-only.
echo.
echo     %BUILD_SCRIPT_NAME% %CURRENT_LIBRARY_VERSION% x64 iostreams system thread
echo       -- Installs %LIBRARY_NAME% version %CURRENT_LIBRARY_VERSION% for x64 architecture with iostreams, system, and thread libraries.
echo.
echo     %BUILD_SCRIPT_NAME% %CURRENT_LIBRARY_VERSION% ARM64 all
echo       -- Installs %LIBRARY_NAME% version %CURRENT_LIBRARY_VERSION% for ARM64 architecture with all of its libraries.
echo.
exit /B 1

endlocal
