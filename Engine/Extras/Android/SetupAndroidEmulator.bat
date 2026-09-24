@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set AVD_DEVICE=%1
set DEVICE_API_VERSION=%2
set DEVICE_DATA_SIZE=%3
set DEVICE_RAM_SIZE=%4

if "%5" equ "-noninteractive" (
	set PAUSE=
) else (
	set PAUSE=pause
)

rem hardcoded versions for compatibility with non-Turnkey manual running
if "%AVD_DEVICE%" equ "" set AVD_DEVICE=pixel_6
if "%DEVICE_API_VERSION%" equ "" set DEVICE_API_VERSION=35
if "%DEVICE_DATA_SIZE%" equ "" set DEVICE_DATA_SIZE=48G
if "%DEVICE_RAM_SIZE%" equ "" set DEVICE_RAM_SIZE=6144

if not exist "%ANDROID_HOME%" (
	echo Android SDK not found at: %ANDROID_HOME%
	echo Unable to locate local Android SDK location. Did you run Android Studio after installing?
	echo If Android Studio is installed, please run again with SDK path as parameter, otherwise download Android Studio 2022.2.1 from https://developer.android.com/studio/archive
	%PAUSE%
	exit /b 1
)

echo Android Studio SDK Path: %ANDROID_HOME%

set CMDLINETOOLS_PATH=%ANDROID_HOME%\cmdline-tools\latest\bin
if not exist "%CMDLINETOOLS_PATH%" (
	set CMDLINETOOLS_PATH=%ANDROID_HOME%\tools\bin
	if not exist "!CMDLINETOOLS_PATH!" (
		echo Unable to locate sdkmanager.bat. Did you run Android Studio and install cmdline-tools after installing?
		%PAUSE%
		exit /b 2
	)
)

if /i "%PROCESSOR_ARCHITECTURE%" equ "AMD64" (
	set ABI=x86_64
) else if /i "%PROCESSOR_ARCHITECTURE%" equ "ARM64" (
	set ABI=arm64-v8a
) else (
	echo Unsupported architecture %ABI%.
	%PAUSE%
	exit /b 3
)

set AVD_TAG=google_apis_playstore
set AVD_PACKAGE=system-images^;android-%DEVICE_API_VERSION%^;%AVD_TAG%^;%ABI%

call "%CMDLINETOOLS_PATH%\sdkmanager.bat" "emulator" "%AVD_PACKAGE%" || (
	echo Update failed. Please check the Android Studio install.
	%PAUSE%
	exit /b 4
)

set EMULATOR_PATH=%ANDROID_HOME%\emulator
if not exist "%EMULATOR_PATH%" (
	echo Update failed. Did you accept the license agreement?
	%PAUSE%
	exit /b 5
)

set DEVICE_NAME=UE_%AVD_DEVICE%_API_%DEVICE_API_VERSION%

for /f "tokens=*" %%G in ('%EMULATOR_PATH%\emulator.exe -list-avds') do (
	if "%%G" equ "%DEVICE_NAME%" (
		echo Android virtual device %DEVICE_NAME% already exists...creation skipped.
		goto :DONE
	)
)

call "%CMDLINETOOLS_PATH%\avdmanager.bat" create avd -n "%DEVICE_NAME%" -k "%AVD_PACKAGE%" -g "%AVD_TAG%" -b "%ABI%" -d "%AVD_DEVICE%" -f || (
	echo Android virtual device %DEVICE_NAME% creation failed.
	%PAUSE%
	exit /b 6
)

echo Android virtual device %DEVICE_NAME% created.

if "%ANDROID_USER_HOME%" equ "" set ANDROID_USER_HOME=%USERPROFILE%\.android
if "%ANDROID_EMULATOR_HOME%" equ "" set ANDROID_EMULATOR_HOME=%ANDROID_USER_HOME%
if "%ANDROID_AVD_HOME%" equ "" set ANDROID_AVD_HOME=%ANDROID_EMULATOR_HOME%\avd

set DEVICE_PATH=%ANDROID_AVD_HOME%\%DEVICE_NAME%.avd
set DEVICE_CONFIG=config.ini
set DEVICE_CONFIG_PATH=%DEVICE_PATH%\%DEVICE_CONFIG%
set DEVICE_TEMP_CONFIG_PATH=%DEVICE_CONFIG_PATH%.tmp

(echo AvdId=%DEVICE_NAME% & echo avd.ini.displayname=%DEVICE_NAME:_= %) > "%DEVICE_TEMP_CONFIG_PATH%"

for /f "tokens=1* delims=\=" %%G in (%DEVICE_CONFIG_PATH%) do (
	(if "%%G" equ "PlayStore.enabled" (
		echo %%G=yes
	) else if "%%G" equ "disk.dataPartition.size" (
		echo %%G=%DEVICE_DATA_SIZE%
	) else if "%%G" equ "hw.gpu.enabled" (
		echo %%G=yes
	) else if "%%G" equ "hw.initialOrientation" (
		echo %%G=landscape
	) else if "%%G" equ "hw.keyboard" (
		echo %%G=yes
	) else if "%%G" equ "hw.ramSize" (
		echo %%G=%DEVICE_RAM_SIZE%
	) else (
		echo %%G=%%H
	)) >> "%DEVICE_TEMP_CONFIG_PATH%"
)

move /y "%DEVICE_TEMP_CONFIG_PATH%" "%DEVICE_CONFIG_PATH%" || (
	echo Android virtual device %DEVICE_NAME% %DEVICE_CONFIG% update failed.
	%PAUSE%
	exit /b 7
)

:DONE
echo Success^^!
%PAUSE%
exit /b 0
