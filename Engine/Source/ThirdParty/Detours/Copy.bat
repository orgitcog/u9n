@echo off

if "%~1"=="" (
	echo MUST set first parameter to root of Detours code sync
	goto ExitAll
)

if not exist %1\src (
	echo No src folder found in %1
	goto ExitAll
)

copy %1\ReleaseMT_arm64\detours.lib .\Windows\arm64-windows-static\detours_arm64-windows-static\lib\detours.lib
copy %1\DebugMD_arm64\detours.lib .\Windows\arm64-windows-static\detours_arm64-windows-static\debug\lib\detours.lib

copy %1\ReleaseMD_arm64\detours.lib .\Windows\arm64-windows-static-md\detours_arm64-windows-static-md\lib\detours.lib
copy %1\DebugMD_arm64\detours.lib .\Windows\arm64-windows-static-md\detours_arm64-windows-static-md\debug\lib\detours.lib

copy %1\ReleaseMT_x64\detours.lib .\Windows\x64-windows-static\detours_x64-windows-static\lib\detours.lib
copy %1\DebugMD_x64\detours.lib .\Windows\x64-windows-static\detours_x64-windows-static\debug\lib\detours.lib

copy %1\ReleaseMD_x64\detours.lib .\Windows\x64-windows-static-md\detours_x64-windows-static-md\lib\detours.lib
copy %1\DebugMD_x64\detours.lib .\Windows\x64-windows-static-md\detours_x64-windows-static-md\debug\lib\detours.lib
