#!/bin/zsh -eux
# Copyright Epic Games, Inc. All Rights Reserved.

ENGINE_ROOT=${0:a:h:h:h:h}

LIBPNG_VERSION=1.6.44

# Mac
"${ENGINE_ROOT}/Source/ThirdParty/CMake/PlatformScripts/Mac/BuildLibForMac.command" libPNG libPNG-${LIBPNG_VERSION} --cmake-args="-DZLIB_INCLUDE_DIR=${ENGINE_ROOT}/Source/ThirdParty/zlib/1.3 -DZLIB_LIBRARY=${ENGINE_ROOT}/Source/ThirdParty/zlib/1.3/lib/Mac/Release/libz.a -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF" --make-target=png_static
rm "${ENGINE_ROOT}/Source/ThirdParty/libPNG/libPNG-${LIBPNG_VERSION}/lib/Mac"/{Debug/libpng16d.a,Release/libpng16.a}


# iOS device/arm64, simulator/x86_64, simulator/arm64
for ARCH in arm64 x86_64 iossimulator;
do
  "${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command" BuildCMakeLib -TargetPlatform=IOS -TargetArchitecture=${ARCH} -TargetLib=libPNG -TargetLibVersion=libPNG-${LIBPNG_VERSION} -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_TOOLCHAIN_FILE=${ENGINE_ROOT}/Source/ThirdParty/CMake/PlatformScripts/IOS/IOS.cmake" -MakeTarget=png_static -SkipCreateChangelist
done

pushd "${ENGINE_ROOT}/Source/ThirdParty/libPNG/libPNG-${LIBPNG_VERSION}/lib/IOS"
mkdir -p Device Simulator
mv arm64/Release/libpng16.a Device/libpng.a
lipo -create iossimulator/Release/libpng16.a x86_64/Release/libpng16.a -output Simulator/libpng.a 
rm -rf arm64 x86_64 iossimulator
popd


# tvOS device/arm64, simulator/x86_64, simulator/arm64
for ARCH in arm64 x86_64 tvossimulator;
do
  "${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command" BuildCMakeLib -TargetPlatform=TVOS -TargetArchitecture=${ARCH} -TargetLib=libPNG -TargetLibVersion=libPNG-${LIBPNG_VERSION} -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_TOOLCHAIN_FILE=${ENGINE_ROOT}/Source/ThirdParty/CMake/PlatformScripts/TVOS/TVOS.cmake" -MakeTarget=png_static -SkipCreateChangelist
done

pushd "${ENGINE_ROOT}/Source/ThirdParty/libPNG/libPNG-${LIBPNG_VERSION}/lib/TVOS"
mkdir -p Device Simulator
mv arm64/Release/libpng16.a Device/libpng.a
lipo -create tvossimulator/Release/libpng16.a x86_64/Release/libpng16.a -output Simulator/libpng.a 
rm -rf arm64 x86_64 tvossimulator
popd


# visionOS device/arm64, simulator/x86_64, simulator/arm64
for ARCH in arm64 x86_64 xrsimulator;
do
  "${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command" BuildCMakeLib -TargetPlatform=VisionOS -TargetArchitecture=${ARCH} -TargetLib=libPNG -TargetLibVersion=libPNG-${LIBPNG_VERSION} -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=1.0 -DCMAKE_TOOLCHAIN_FILE=${ENGINE_ROOT}/Platforms/VisionOS/Source/ThirdParty/CMake/PlatformScripts/VisionOS.cmake" -MakeTarget=png_static -SkipCreateChangelist
done

pushd "${ENGINE_ROOT}/Platforms/VisionOS/Source/ThirdParty/libPNG/libPNG-${LIBPNG_VERSION}/lib"
mkdir -p Device Simulator
mv arm64/Release/libpng16.a Device/libpng.a
lipo -create xrsimulator/Release/libpng16.a x86_64/Release/libpng16.a -output Simulator/libpng.a 
rm -rf arm64 x86_64 xrsimulator
popd
