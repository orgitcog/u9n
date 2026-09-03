@echo off
SETLOCAL ENABLEEXTENSIONS
IF ERRORLEVEL 1 ECHO Unable to enable extensions

IF "%~1"=="" goto :USAGE

rem extract the CEF version string from the binary_distrib folder name
FOR /F "tokens=3 delims=_" %%g IN ('dir /b %~1\*windows*') DO (SET CEF_VERSION=%%g)

goto :MAIN

:COPY_DISTRIB_SOURCES
FOR /F "tokens=*" %%g IN ('dir /b %~1\*windows%~2') DO (SET DISTRIB_DIR=%%g)
SET DISTRIB_SOURCE_PATH=%~1\%DISTRIB_DIR%
SET TARGET_SRC_DIR=%DISTRIB_DIR%

IF EXIST %TARGET_SRC_DIR% (
    echo Folder %TARGET_SRC_DIR% already exists!
    IF "%~3"=="/f" (
        rmdir /q /s %TARGET_SRC_DIR%
    ) else (
        exit /b 1
    )
)

echo Copying CEF3 headers and libs to %TARGET_SRC_DIR%...

mkdir %TARGET_SRC_DIR%\include
xcopy /s /y %DISTRIB_SOURCE_PATH%\include %TARGET_SRC_DIR%\include >NUL

mkdir %TARGET_SRC_DIR%\Debug
xcopy /y %DISTRIB_SOURCE_PATH%\Debug\*.lib %TARGET_SRC_DIR%\Debug >NUL

mkdir %TARGET_SRC_DIR%\Release
xcopy /y %DISTRIB_SOURCE_PATH%\Release\*.lib %TARGET_SRC_DIR%\Release >NUL

mkdir %TARGET_SRC_DIR%\VS2015
xcopy /s /y %DISTRIB_SOURCE_PATH%\VS2015 %TARGET_SRC_DIR%\VS2015 >NUL

exit /b 0

:COPY_DISTRIB_BINARIES
FOR /F "tokens=*" %%g IN ('dir /b %~1\*windows%~2') DO (SET DISTRIB_DIR=%%g)
SET DISTRIB_SOURCE_PATH=%~1\%DISTRIB_DIR%
SET TARGET_BIN_DIR=..\..\..\Binaries\ThirdParty\CEF3\Win%~2\%CEF_VERSION%

IF EXIST %TARGET_BIN_DIR% (
    echo Folder %TARGET_BIN_DIR% already exists!
    IF "%~3"=="/f" (
        rmdir /q /s %TARGET_BIN_DIR%
    ) else (
        exit /b 1
    )
)

echo Copying CEF3 binaries and resources to %TARGET_BIN_DIR%...

mkdir %TARGET_BIN_DIR%\Resources\locales
xcopy /y %DISTRIB_SOURCE_PATH%\Release\*.dll %TARGET_BIN_DIR% >NUL
xcopy /y %DISTRIB_SOURCE_PATH%\Release\*.bin %TARGET_BIN_DIR% >NUL
xcopy /y %DISTRIB_SOURCE_PATH%\Release\vk_swiftshader_icd.json %TARGET_BIN_DIR% >NUL
xcopy /y %DISTRIB_SOURCE_PATH%\Resources\*.pak %TARGET_BIN_DIR% >NUL
xcopy /y %DISTRIB_SOURCE_PATH%\Resources\icudtl.dat %TARGET_BIN_DIR% >NUL
xcopy /y %DISTRIB_SOURCE_PATH%\Resources\locales %TARGET_BIN_DIR%\Resources\locales >NUL

exit /b 0

:MAIN
FOR %%a IN (64, Arm64) DO (
    call :COPY_DISTRIB_SOURCES %~1 %%a %~2 || goto :FAIL
    call :COPY_DISTRIB_BINARIES %~1 %%a %~2 || goto :FAIL
)

echo CEF drop completed, you can update CEF3.build.cs with new version %CEF_VERSION%

exit /b 0

:USAGE
echo "Usage: %~0 <path to CEF drop update> [/f]"
exit /b 1

:FAIL
echo Failed.
exit /b 1
