#!/bin/bash

# Unreal Engine 5 Build script for ISPC + LLVM on linux.
# Copyright Epic Games, Inc. All Rights Reserved.
#
# This script spawns the actual build in a docker container running our target distribution (Rocky Linux 8 at the moment).
# It was tested under WLS on Windows, which is probably the primary use case, but it should also work on a native linux.
#
# The script expects the ISPC and LLVM versions to be given on the command line, but if they are missing, it will
# use the defaults provided below (the most recent versions at the time of writing).
# The right ISPC source code has to be already present (e.g. ispc-1.24.0). LLVM is cloned from GitHub on demand.
#
# Sample usage:
# 	RunMe.sh
#	RunMe.sh 1.24.0 18.1.6
#
# If everything goes fine, the script will extract the 'ispc' executable into ./out.
# If the Perforce client is found, it will also attempt to check out ./bin/Linux/ispc and update it with the generated version.

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)

# We expect the ISPC and LLVM versions to be provided as arguments. If missing, we use these defaults (the current versions at the time this scrip is writen).
ISPC_VERSION=${1:-1.24.0}
LLVM_VERSION=${2:-18.1.6}

# We currently only handle Rocky8, so no need to provide anything else.
DISTRO=${3:-Rocky8}

BuildISPCWithDocker()
{
	local Platform=$1
	local Image=$2
	local ImageName=ISPCBuild
	local ISPCDirectory=ispc-${ISPC_VERSION}
	local MAIN_DIR=${SCRIPT_DIR}/../..
	
	echo ===== Starting the build for $Platform...
	sudo docker run -t --name ${ImageName} --platform ${Platform} -v ${SCRIPT_DIR}:/ISPC/build/linux -v ${MAIN_DIR}/${ISPCDirectory}:/ISPC/${ISPCDirectory} ${Image} /ISPC/build/linux/BuildFromDocker.command ${ISPC_VERSION} ${LLVM_VERSION} ${DISTRO}

	# Copy the generated binary into the out directory in case we don't have Perforce or something goes wrong during the checkout, so that we didn't lose the results.
	rm -rf ${MAIN_DIR}/out
	mkdir -p ${MAIN_DIR}/out
	
	echo Copying the result from /ISPC/ispc/bin/ispc in the container into ${MAIN_DIR}/out/ispc
	sudo docker cp ${ImageName}:/ISPC/ispc/bin/ispc ${MAIN_DIR}/out/ispc
	
	# Try to check out file from Perforce and update if possible
	local P4BIN=

	# We check both p4.exe and p4 to cover the script running under WSL on Windows and on native linux installations.
	if hash p4 2>/dev/null; then
		P4BIN=p4
	elif hash p4.exe 2>/dev/null; then
		P4BIN=p4.exe
	fi

	if [ ${P4BIN} != "" ]; then
		echo Checking out ${MAIN_DIR}/bin/Linux/ispc
		${P4BIN} edit ${MAIN_DIR}/bin/Linux/ispc
		
		echo Updating the binary at ${MAIN_DIR}/bin/Linux/ispc
		cp ${MAIN_DIR}/out/ispc ${MAIN_DIR}/bin/Linux/ispc
	fi

	sudo docker rm ${ImageName} > /dev/null
}

# Uncomment if we start building ISPC for arm64
#sudo docker run --rm --privileged docker/binfmt:820fdd95a9972a5308930a2bdfb8573dd4447ad3

if [ ${DISTRO} == "Rocky8" ]; then
	echo ===== Building ISPC and LLVM on Rocky 8.4
	BuildISPCWithDocker linux/amd64    rockylinux/rockylinux:8.4
	
	# Uncomment if we start building ISPC for arm64
	#BuildISPCWithDocker linux/arm64    rockylinux/rockylinux:8.4
else
	echo Unsupported distro: ${DISTRO}
	exit 1
fi
