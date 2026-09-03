@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

set ENGINE_ROOT=%~dp0..\..\..\..\..

call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Linux -TargetArchitecture=x86_64-unknown-linux-gnu -MakeTarget=astcenc-sse4.1-static -CMakeAdditionalArguments="-DASTCENC_ISA_AVX2=OFF -DCMAKE_CXX_VISIBILITY_PRESET=hidden -DASTCENC_ISA_SSE41=ON -DASTCENC_CLI=OFF -DASTCENC_X86_GATHERS=OFF -DBUILD_WITH_LIBCXX=ON" -TargetLib=astcenc -TargetLibVersion=5.0.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -SkipCreateChangelist || exit /b

mkdir %~dp0..\lib\Linux\Release
move %~dp0..\lib\Linux\x86_64-unknown-linux-gnu\Release\libastcenc-sse4.1-static.a %~dp0..\lib\Linux\Release
