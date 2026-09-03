#!/bin/bash

# Unreal Engine 5 Build script for ISPC + LLVM on linux.
# Copyright Epic Games, Inc. All Rights Reserved.
#
# This script is the main script wrapping the whole build process and is expected to be run in a docker container.
# It is not intended to be used directly. Use RunMe.sh to initiate the build.
#
# Expects these mapped directories:
#	/ISPC/ispc-${ISPC_VERSION}	- the ISPC sources
#	/ISPC/build/linux 			- directory with this script
#
# Built executable will be in:
#	/ISPC/ispc/bin

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)

ISPC_VERSION=${1:-1.24.0}
LLVM_VERSION=${2:-18.1.6}

DISTRO=${3:-Rocky8}

if [ $UID -eq 0 ]; then

	echo ===== Installing dependencies
	
	if [ ${DISTRO} == "Rocky8" ]; then
		yum install -y cmake python3 bison flex git tbb tbb-devel clang glibc-devel.i686 glibc-devel.x86_64 ncurses ncurses-devel
	else
		echo Unsupported distro ${DISTRO}
		exit 1
	fi

	# Create non-privileged user and workspace
	adduser buildmaster

	setfacl -m u:buildmaster:rwx /ISPC

	cd /ISPC

	exec su buildmaster "$0" $ISPC_VERSION $LLVM_VERSION $DISTRO
fi

# This will be run from user buildmaster

set -e

echo ===== Building LLVM ${LLVM_VERSION}
./build/linux/BuildLLVM.command ${LLVM_VERSION}

echo ===== Building ISPC ${ISPC_VERSION}
./build/linux/BuildISPC.command ${ISPC_VERSION}

set +e
