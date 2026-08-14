@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..

set LIBPNG_VERSION=1.6.44

if not exist "%ANDROID_NDK_ROOT%\source.properties" (
  echo Please set ANDROID_NDK_ROOT to Android NDK location!
  exit /b 1
)

call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Android -TargetArchitecture=x64   -TargetLib=libPNG -TargetLibVersion=libPNG-%LIBPNG_VERSION% -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF" -MakeTarget=png_static -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Android -TargetArchitecture=ARM64 -TargetLib=libPNG -TargetLibVersion=libPNG-%LIBPNG_VERSION% -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF" -MakeTarget=png_static -SkipCreateChangelist || exit /b
