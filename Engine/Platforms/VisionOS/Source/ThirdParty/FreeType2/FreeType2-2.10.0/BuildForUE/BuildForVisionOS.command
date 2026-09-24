#!/bin/zsh -eux
# Copyright Epic Games, Inc. All Rights Reserved.

ENGINE_ROOT=${0:a:h:h:h:h:h:h:h:h}
THIRD_PARTY_DIRECTORY=${ENGINE_ROOT}/Source/ThirdParty
VISIONOS_THIRD_PARTY=${ENGINE_ROOT}/Platforms/VisionOS/Source/ThirdParty
# We use the version of zlib found in the VisionOS SDK 

LIBPNG_VERSION=libPNG-1.6.44
PATH_TO_LIBPNG=${THIRD_PARTY_DIRECTORY}/libPNG/${LIBPNG_VERSION}
PATH_TO_PNG_SRC=${PATH_TO_LIBPNG}
# This is just a stub in path so that cmake is able to find a libpng library file to satisfy find_package etc 
# This is not used for actual linking etc 
PATH_TO_PNG_LIB=${PATH_TO_LIBPNG}/lib/IOS/Device/libpng.a

TOOLCHAIN_FILE=${VISIONOS_THIRD_PARTY}/CMake/PlatformScripts/VisionOS.cmake
FREETYPE_VISIONOS_VERSION=FreeType2-2.10.0
PATH_TO_FREETYPE=${THIRD_PARTY_DIRECTORY}/FreeType2/${FREETYPE_VISIONOS_VERSION}

CMAKE_ADDITIONAL_ARGUMENTS=-DFT_WITH_ZLIB=ON
CMAKE_ADDITIONAL_ARGUMENTS+=" -DFT_WITH_PNG=ON"
# We do not want to use HB 
CMAKE_ADDITIONAL_ARGUMENTS+=" -DFT_WITH_HARFBUZZ=OFF"
CMAKE_ADDITIONAL_ARGUMENTS+=" -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE"
CMAKE_ADDITIONAL_ARGUMENTS+=" -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE"
CMAKE_ADDITIONAL_ARGUMENTS+=" -DCMAKE_OSX_DEPLOYMENT_TARGET=1.0"
# Uncomment this to output debug messages while building the library 
#CMAKE_ADDITIONAL_ARGUMENTS+=" -DCMAKE_FIND_DEBUG_MODE=ON"
CMAKE_ADDITIONAL_ARGUMENTS+=" -DPNG_PNG_INCLUDE_DIR=${PATH_TO_PNG_SRC}"
CMAKE_ADDITIONAL_ARGUMENTS+=" -DPNG_LIBRARY=${PATH_TO_PNG_LIB}"
CMAKE_ADDITIONAL_ARGUMENTS+=" -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}"

MAKE_TARGET=freetype

for ARCH in arm64 x86_64 xrsimulator;
do
  "${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command" BuildCMakeLib -TargetPlatform=VisionOS -TargetArchitecture=${ARCH} -TargetLib=FreeType2 -TargetLibVersion=${FREETYPE_VISIONOS_VERSION} -TargetConfigs=Debug+Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="${CMAKE_ADDITIONAL_ARGUMENTS}" -MakeTarget=${MAKE_TARGET} -SkipCreateChangelist
done

pushd "${VISIONOS_THIRD_PARTY}/FreeType2/${FREETYPE_VISIONOS_VERSION}/lib"
mkdir -p Debug Release Simulator Simulator/Debug Simulator/Release
mv arm64/Debug/libfreetyped.a Debug/libfreetyped.a
mv arm64/Release/libfreetype.a Release/libfreetype.a
lipo -create xrsimulator/Debug/libfreetyped.a x86_64/Debug/libfreetyped.a -output Simulator/Debug/libfreetyped.a 
lipo -create xrsimulator/Release/libfreetype.a x86_64/Release/libfreetype.a -output Simulator/Release/libfreetype.a 
rm -rf arm64 x86_64 xrsimulator
popd
