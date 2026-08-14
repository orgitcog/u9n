#!/bin/zsh
# Copyright Epic Games, Inc. All Rights Reserved 

LIB_ROOT=${0:a:h:h}
# ${LIB_ROOT:h:h}/CMake/PlatformScripts/Mac/BuildLibForMac.command ${LIB_ROOT:h:t} ${LIB_ROOT:t} --config=Release
ENGINE_ROOT=${0:a:h:h:h:h:h:h}

${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command BuildCMakeLib -TargetPlatform=Mac -TargetArchitecture=arm64 -TargetLib=astcenc -TargetLibVersion=5.0.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -MakeTarget=astcenc-neon-static -CMakeAdditionalArguments="-DASTCENC_CLI=OFF" -SkipCreateChangelist

${ENGINE_ROOT}/Build/BatchFiles/RunUAT.command BuildCMakeLib -TargetPlatform=Mac -TargetArchitecture=x86_64 -TargetLib=astcenc -TargetLibVersion=5.0.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -MakeTarget=astcenc-sse4.1-static -CMakeAdditionalArguments="-DASTCENC_CLI=OFF" -SkipCreateChangelist

mkdir ${0:a:h:h}/lib/Mac/Release
lipo -create -output ${0:a:h:h}/lib/Mac/Release/libastcenc-static.a ${0:a:h:h}/lib/Mac/x86_64/Release/libastcenc-sse4.1-static.a ${0:a:h:h}/lib/Mac/arm64/Release/libastcenc-neon-static.a 
