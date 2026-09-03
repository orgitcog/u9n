set CMAKEPATH=%ANDROID_HOME%\cmake\3.22.1\bin

mkdir .build-arm64-v8a
pushd .build-arm64-v8a

%CMAKEPATH%\cmake.exe -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_SYSTEM_NAME=Android -DCMAKE_SYSTEM_VERSION=28 -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a -DCMAKE_ANDROID_STL_TYPE=c++_static -DCMAKE_MAKE_PROGRAM=%CMAKEPATH%\ninja.exe ..\Source
%CMAKEPATH%\ninja.exe

popd

mkdir lib
mkdir lib\arm64-v8a
move /y .build-arm64-v8a\libGPUCounters.so lib\arm64-v8a
rmdir .build-arm64-v8a /s /q
