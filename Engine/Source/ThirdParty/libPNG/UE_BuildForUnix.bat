@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..

set LIBPNG_VERSION=1.6.44

call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu      -TargetLib=libPNG -TargetLibVersion=libPNG-%LIBPNG_VERSION% -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DZLIB_INCLUDE_DIR=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3 -DZLIB_LIBRARY=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3\lib\Unix\x86_64-unknown-linux-gnu\Release\libz.a -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF" -MakeTarget=png_static -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu      -TargetLib=libPNG -TargetLibVersion=libPNG-%LIBPNG_VERSION% -TargetConfigs=Debug   -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DZLIB_INCLUDE_DIR=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3 -DZLIB_LIBRARY=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3\lib\Unix\x86_64-unknown-linux-gnu\Release\libz.a -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF" -MakeTarget=png_static -SkipCreateChangelist || exit /b

call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=libPNG -TargetLibVersion=libPNG-%LIBPNG_VERSION% -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DZLIB_INCLUDE_DIR=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3 -DZLIB_LIBRARY=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3\lib\Unix\aarch64-unknown-linux-gnueabi\Release\libz.a -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF" -MakeTarget=png_static -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=libPNG -TargetLibVersion=libPNG-%LIBPNG_VERSION% -TargetConfigs=Debug   -LibOutputPath=lib -CMakeGenerator=Makefile -CMakeAdditionalArguments="-DZLIB_INCLUDE_DIR=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3 -DZLIB_LIBRARY=%ENGINE_ROOT%\Source\ThirdParty\zlib\1.3\lib\Unix\aarch64-unknown-linux-gnueabi\Release\libz.a -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF" -MakeTarget=png_static -SkipCreateChangelist || exit /b
