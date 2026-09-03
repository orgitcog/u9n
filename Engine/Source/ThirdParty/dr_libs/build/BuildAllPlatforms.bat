@echo off

call ..\..\..\..\Build\BatchFiles\RunUAT.bat BuildCMakeLib -TargetLib=dr_libs -TargetLibVersion=build -TargetPlatform=Win64^
 -TargetConfigs=Release -LibOutputPath=../dr_libs/lib -CMakeGenerator=VS2022 -SkipSubmit
 
call ..\..\..\..\Build\BatchFiles\RunUAT.bat BuildCMakeLib -TargetLib=dr_libs -TargetLibVersion=build -TargetPlatform=Win64 -TargetArchitecture=Arm64^
 -TargetConfigs=Release -LibOutputPath=../dr_libs/lib -CMakeGenerator=VS2022 -SkipSubmit

rem Add more platforms, maybe even loop over platformextensions looking for SoundTouchZ dirs and building those platforms