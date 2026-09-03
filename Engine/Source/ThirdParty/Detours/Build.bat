@echo off

if "%VCToolsInstallDir%"=="" (
	echo Run batch file from a x64 visual studio command prompt
	goto ExitAll
)


if "%~1"=="" (
	echo MUST set first parameter to root of Detours code sync
	goto ExitAll
)

if not exist %1\src (
	echo No src folder found in %1
	goto ExitAll
)

pushd %cd%
cd %1

SET VS_SDK=%VCToolsInstallDir%
SET WIN_SDK=%WindowsSdkDir%\%WindowsSDKVersion%

SET INCLUDE_PATHS=/I "%WIN_SDK%\shared" /I "%WIN_SDK%\um" /I "%WIN_SDK%\ucrt" /I "%VS_SDK%\include"
SET SOURCE_FILES=detours.cpp modules.cpp disasm.cpp image.cpp creatwth.cpp disolx86.cpp disolx64.cpp disolia64.cpp disolarm.cpp disolarm64.cpp
SET OBJ_FILES=detours.obj modules.obj disasm.obj image.obj creatwth.obj disolx86.obj disolx64.obj disolia64.obj disolarm.obj disolarm64.obj

SET CL_OPTIONS=/nologo /W4 /WX /we4777 /we4800 /Zi /Gy /Gm- /Zl /DDETOUR_DEBUG=0 /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x501 %INCLUDE_PATHS% %SOURCE_FILES%
SET LINK_OPTIONS=/nologo %OBJ_FILES%

SET ARCH=x64
call :ExecuteArch
SET ARCH=arm64
call :ExecuteArch
goto ExitFile


:ExecuteArch
SET VS_BIN=%VS_SDK%\bin\HostX64\%ARCH%

SET OUTPUTDIR=ReleaseMT_%ARCH%
SET CL_EXTRA_OPTIONS=/MT /O2
call :ExecuteConfig

SET OUTPUTDIR=DebugMT_%ARCH%
SET CL_EXTRA_OPTIONS=/MTd /Od 
call :ExecuteConfig

SET OUTPUTDIR=ReleaseMD_%ARCH%
SET CL_EXTRA_OPTIONS=/MD /O2
call :ExecuteConfig

SET OUTPUTDIR=DebugMD_%ARCH%
SET CL_EXTRA_OPTIONS=/MDd /Od
call :ExecuteConfig

goto:eof


:ExecuteConfig
echo BUILDING %OUTPUTDIR%

mkdir %OUTPUTDIR% 2>nul
cd src
mkdir %OUTPUTDIR% 2>nul
"%VS_BIN%\cl.exe" %CL_EXTRA_OPTIONS% /Fd..\%OUTPUTDIR%\detours.pdb /Fo%OUTPUTDIR%\ /c %CL_OPTIONS%
cd %OUTPUTDIR%
"%VS_BIN%\link" /lib /out:..\..\%OUTPUTDIR%\detours.lib %LINK_OPTIONS%
cd ..
cd ..
goto:eof

:ExitFile
popd
:ExitAll
