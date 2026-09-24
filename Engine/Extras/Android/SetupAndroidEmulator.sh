#!/bin/bash

if [[ "${OSTYPE}" == "darwin"* ]]; then
	echo "Please run SetupAndroidEmulator.command on MacOSX; attempting to run it for you."
	DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
	exec "${DIR}/SetupAndroidEmulator.command" "${@}"
	exit 1
fi

pause()
{
	read -rsp "Press any key to continue . . . " -n1
	echo
}

AVD_DEVICE=${1:-}
DEVICE_API_VERSION=${2:-}
DEVICE_DATA_SIZE=${3:-}
DEVICE_RAM_SIZE=${4:-}

if [ "${9:-}" == "-noninteractive" ]; then
    PAUSE=
else
    PAUSE="pause"
fi

# hardcoded versions for compatibility with non-Turnkey manual running
if [ -z "${AVD_DEVICE}" ]; then
    AVD_DEVICE="pixel_6"
fi
if [ -z "${DEVICE_API_VERSION}" ]; then
    DEVICE_API_VERSION="35"
fi
if [ -z "${DEVICE_DATA_SIZE}" ]; then
    DEVICE_DATA_SIZE="48G"
fi
if [ -z "${DEVICE_RAM_SIZE}" ]; then
    DEVICE_RAM_SIZE="6144"
fi

if [ ! -d "${ANDROID_HOME}" ]; then
	echo "Android SDK not found at: ${ANDROID_HOME}"
	echo "Unable to locate local Android SDK location. Did you run Android Studio after installing?"
	echo "If Android Studio is installed, please run again with SDK path as parameter, otherwise download Android Studio 2022.2.1 from https://developer.android.com/studio/archive"
	${PAUSE}
	exit 1
fi
echo "Android Studio SDK Path: ${ANDROID_HOME}"

CMDLINETOOLS_PATH="${ANDROID_HOME}/cmdline-tools/latest/bin"
if [ ! -d "${CMDLINETOOLS_PATH}" ]; then
	CMDLINETOOLS_PATH="${ANDROID_HOME}/tools/bin"
	if [ ! -d "${CMDLINETOOLS_PATH}" ]; then
		echo "Unable to locate sdkmanager. Did you run Android Studio and install cmdline-tools after installing?"
		${PAUSE}
		exit 1
	fi
fi

ABI=$(uname -m)
if [ "${ABI}" == "aarch64" ] || [ "${ABI}" == "arm64" ]; then
	ABI="arm64-v8a"
fi
if [ "${ABI}" != "x86_64" ] && [ "${ABI}" != "arm64-v8a" ]; then
	echo "Unsupported architecture ${ABI}."
	${PAUSE}
	exit 1
fi

AVD_TAG="google_apis_playstore"
AVD_PACKAGE="system-images;android-${DEVICE_API_VERSION};${AVD_TAG};${ABI}"

if ! "${CMDLINETOOLS_PATH}/sdkmanager" "emulator" "${AVD_PACKAGE}"; then
	echo "Update failed. Please check the Android Studio install."
	${PAUSE}
	exit 1
fi

EMULATOR_PATH="${ANDROID_HOME}/emulator"
if [ ! -d "${EMULATOR_PATH}" ]; then
	echo "Update failed. Did you accept the license agreement?"
	${PAUSE}
	exit 1
fi

DEVICE_NAME="UE_${AVD_DEVICE}_API_${DEVICE_API_VERSION}"
for LINE in $("${EMULATOR_PATH}/emulator" -list-avds); do
	if [ "${LINE}" == "${DEVICE_NAME}" ]; then
		echo "Android virtual device ${DEVICE_NAME} already exists...creation skipped."
		DEVICE_NAME=""
	fi
done

if [ -n "${DEVICE_NAME}" ]; then
	if ! "${CMDLINETOOLS_PATH}/avdmanager" create avd -n "${DEVICE_NAME}" -k "${AVD_PACKAGE}" -g "${AVD_TAG}" -b "${ABI}" -d "${AVD_DEVICE}" -f; then
		echo "Android virtual device ${DEVICE_NAME} creation failed."
		${PAUSE}
		exit 1
	fi
	
	echo "Android virtual device ${DEVICE_NAME} created."
	
	if [ -z "${ANDROID_USER_HOME}" ]; then
		ANDROID_USER_HOME="${HOME}/.android"
	fi
	if [ -z "${ANDROID_EMULATOR_HOME}" ]; then
		ANDROID_EMULATOR_HOME="${ANDROID_USER_HOME}"
	fi
	if [ -z "${ANDROID_AVD_HOME}" ]; then
		ANDROID_AVD_HOME="${ANDROID_EMULATOR_HOME}/avd"
	fi

	DEVICE_PATH="${ANDROID_AVD_HOME}/${DEVICE_NAME}.avd"
	DEVICE_CONFIG="config.ini"
	DEVICE_CONFIG_PATH="${DEVICE_PATH}/${DEVICE_CONFIG}"
	DEVICE_TEMP_CONFIG_PATH="${DEVICE_PATH}/${DEVICE_CONFIG}.tmp"

	{
		echo "AvdId=${DEVICE_NAME}"
		echo "avd.ini.displayname=${DEVICE_NAME//_/ }"
	} > "${DEVICE_TEMP_CONFIG_PATH}"
	
	while read -r LINE; do
		if [[ "${LINE}" =~ ^([^=]+)=([^=]+)$ ]]; then
			if [ "${BASH_REMATCH[1]}" == "PlayStore.enabled" ]; then
				echo "${BASH_REMATCH[1]}=yes"
			elif [ "${BASH_REMATCH[1]}" == "disk.dataPartition.size" ]; then
				echo "${BASH_REMATCH[1]}=${DEVICE_DATA_SIZE}"
			elif [ "${BASH_REMATCH[1]}" == "hw.gpu.enabled" ]; then
				echo "${BASH_REMATCH[1]}=yes"
			elif [ "${BASH_REMATCH[1]}" == "hw.initialOrientation" ]; then
				echo "${BASH_REMATCH[1]}=landscape"
			elif [ "${BASH_REMATCH[1]}" == "hw.keyboard" ]; then
				echo "${BASH_REMATCH[1]}=yes"
			elif [ "${BASH_REMATCH[1]}" == "hw.ramSize" ]; then
				echo "${BASH_REMATCH[1]}=${DEVICE_RAM_SIZE}"
			else
				echo "${BASH_REMATCH[1]}=${BASH_REMATCH[2]}"
			fi
		else
			echo "${LINE}="
		fi >> "${DEVICE_TEMP_CONFIG_PATH}"
	done < "${DEVICE_CONFIG_PATH}"

	if ! mv -f "${DEVICE_TEMP_CONFIG_PATH}" "${DEVICE_CONFIG_PATH}"; then
		echo "Android virtual device ${DEVICE_NAME} ${DEVICE_CONFIG} update failed."
		${PAUSE}
		exit 1
	fi
fi

echo Success!
${PAUSE}
exit 0
