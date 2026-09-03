@echo off
setlocal 

pushd "%~dp0"

set config=Development

if "%~1"=="" goto blank

set config=%1

:blank

echo.
echo === Building Targets ===
pushd "../../../.."
call Engine/Build/BatchFiles/RunUBT.bat -NoUba -NoUbaLocal -NoPCH -DisableUnity -AllModules -NoLink -NoVFS -WarningsAsErrors -IncludeHeaders -staticanalyzer=Default ^
	-Target="UbaAgent Win64 %config%" ^
	-Target="UbaCli Win64 %config%" ^
	-Target="UbaDetours Win64 %config%" ^
	-Target="UbaHost Win64 %config%" ^
	-Target="UbaStorageProxy Win64 %config%" ^
	-Target="UbaVisualizer Win64 %config%" ^
	-Target="UbaCacheService Win64 %config%" ^
	-Target="UbaObjTool Win64 %config%" ^
	-Target="UbaTest Win64 %config%"

popd

popd

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

endlocal

exit /b 0
