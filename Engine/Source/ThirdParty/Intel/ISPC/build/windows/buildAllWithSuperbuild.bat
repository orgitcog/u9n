rem This script builds both LLVM and ISPC using the ISPC's 'superbuild' solution.
rem It is an alternative to building LLVM and ISPC separately using our own build scripts (buildllvm.bat and buildispc.bat).
rem It should be considered the preferred way now, because it ensures that everything is built in the same way as in
rem the official ISPC binaries. For instance, it ensures all the right LLVM options are used.
rem
rem In addition to the usual required dependencies, superbuild requires a few other tools:
rem    * ninja
rem 		If you don't have it already, you may add the engine's version to Path (Engine\Extras\ThirdPartyNotUE\ninja-build)
rem    * mt
rem			It is part of the Windows SDK. You may add C:\Program Files (x86)\Windows Kits\10\bin\<version>\x64 to your path.
rem    * ml64
rem			It is part of Visual Studio installation. If you don't have it already available, you may add this to your Path
rem				C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\<version>\bin\HostX64\x64
rem
rem Superbuild assumes you have GnuWin32 installed in the default location. Since this project is not maintained, you may
rem want to use cygwin instead. To do so, you will need to modify ispc-<version>\superbuild\CMakeLists.txt and adit
rem 	set(GNUWIN32 ...
rem
rem to point to "c:/cygwin64".
rem 
rem Another caveat is that by default ISPC build system is often set up to compile against a different LLVM version than
rem the one we use. You may need to edit ispc-<version>/superbuild/osPresets.json to have the right references
rem for your LLVM or otherwise some LLVM additional tools like SPIRV may not build. The easiest way is to simply
rem check the official ISPC github repository for the head revision and check if they don't have the up to date info there.
rem If not, you may find the matching change hash yourself, or (possibly) disable building XE dependencies (which we don't use
rem anyway) by passing XE_DEPS=OFF to cmake.


@echo off
setlocal

Set LLVM_VERSION=18.1
Set ISPC_VERSION=1.24.0

pushd %~dp0\..\..

Set HOME_ROOT=%cd%
Set ISPC_HOME=%HOME_ROOT%\ispc-%ISPC_VERSION%

Set LLVM_INSTALL=%HOME_ROOT%\llvm
Set LLVM_BUILD=%HOME_ROOT%\build-llvm

Set ISPC_INSTALL=%HOME_ROOT%\ispc
Set ISPC_BUILD=%HOME_ROOT%\build-ispc

Set COMMON_OPTIONS=-DCMAKE_BUILD_TYPE=Release -DLTO=ON

echo ----------------------------------------------------
echo Configuring LLVM
echo ----------------------------------------------------

cmake -B %LLVM_BUILD% %ISPC_HOME%/superbuild --preset os -G "Visual Studio 17" %COMMON_OPTIONS% -DLLVM_VERSION=%LLVM_VERSION% -DCMAKE_INSTALL_PREFIX=%LLVM_INSTALL% -DBUILD_STAGE2_TOOLCHAIN_ONLY=ON

echo ----------------------------------------------------
echo Building LLVM
echo ----------------------------------------------------

cmake --build %LLVM_BUILD%

echo ----------------------------------------------------
echo Configuring ISPC
echo ----------------------------------------------------

Set PATH=%LLVM_INSTALL%\bin;%ISPC_INSTALL%\bin;%PATH%

cmake -B %ISPC_BUILD% %ISPC_HOME%/superbuild --preset os -G "Visual Studio 17" %COMMON_OPTIONS% -DINSTALL_ISPC=ON -DCMAKE_INSTALL_PREFIX=%ISPC_INSTALL% -DPREBUILT_STAGE2_PATH=%LLVM_INSTALL% -DISPC_CROSS=ON

echo ----------------------------------------------------
echo Building ISPC
echo ----------------------------------------------------

cmake --build %ISPC_BUILD%

echo ----------------------------------------------------
echo Copying the generated ISPC
echo ----------------------------------------------------

cd /d %HOME_ROOT%

p4 edit bin\Windows\ispc.exe
p4 edit bin\Windows\ispcrt.dll

copy %ISPC_INSTALL%\bin\ispc.exe bin\Windows\ispc.exe
copy %ISPC_INSTALL%\bin\ispcrt.dll bin\Windows\ispcrt.dll

popd
