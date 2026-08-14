@echo off

set ENGINE_ROOT=%~dp0..\..\..
set CLANG_PATH=%LINUX_MULTIARCH_ROOT%/x86_64-unknown-linux-gnu
set LibCxxIncludes=-I%ENGINE_ROOT%/Source/ThirdParty/Unix/LibCxx/include -I%ENGINE_ROOT%/Source/ThirdParty/Unix/LibCxx/include/c++/v1
set SWITCHES=-O3 -DNDEBUG -pthread -Wall -Wextra -Wno-unused-parameter -gdwarf-4 -ffp-model=precise -ffp-contract=off -msse4.1 -mpopcnt -fPIC -gdwarf-4 -std=c++17 -stdlib=libc++

mkdir tmp
mkdir tmp\5.0.1
mkdir tmp\4.2.0

rem 5.0.1
%CLANG_PATH%/bin/clang++.exe --target=x86_64-unknown-linux-gnu -DASTC_SUPPORTS_RDO --sysroot=%CLANG_PATH% %SWITCHES% %LibCxxIncludes% -I5.0.1/Source -o tmp/5.0.1/astc_thunk.cpp.o -c astc_thunk.cpp
%CLANG_PATH%/bin/clang++.exe --target=x86_64-unknown-linux-gnu -fuse-ld=lld --sysroot=%CLANG_PATH% -shared tmp/5.0.1/astc_thunk.cpp.o -o Thunks/libastcenc_thunk_linux64_5.0.1.so -LD:/devel/35.00/Engine/Source/ThirdParty/astcenc/5.0.1/lib/Linux/Release/ -lastcenc-sse4.1-static

rem 4.2.0
%CLANG_PATH%/bin/clang++.exe --target=x86_64-unknown-linux-gnu --sysroot=%CLANG_PATH% %SWITCHES% %LibCxxIncludes% -I4.2.0/Source -o tmp/4.2.0/astc_thunk.cpp.o -c astc_thunk.cpp
%CLANG_PATH%/bin/clang++.exe --target=x86_64-unknown-linux-gnu -fuse-ld=lld --sysroot=%CLANG_PATH% -shared tmp/4.2.0/astc_thunk.cpp.o -o Thunks/libastcenc_thunk_linux64_4.2.0.so -LD:/devel/35.00/Engine/Source/ThirdParty/astcenc/4.2.0/lib/Linux/Release/ -lastcenc-sse4.1-static
