#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved 

CLANG_PATH=$(xcrun --sdk macosx --find clang)
SYSROOT=(
	-isysroot
	$(xcrun --sdk macosx --show-sdk-path)
	)

mkdir tmp
mkdir tmp/5.0.1
mkdir tmp/4.2.0

$CLANG_PATH ${SYSROOT[@]} -DASTC_SUPPORTS_RDO -arch arm64 -shared -mmacosx-version-min=14.00 -fPIC -pthread -Wall -Wextra -gdwarf-4 -ffp-model=precise -ffp-contract=off -Wno-unused-parameter -std=c++17 -lc++ -I5.0.1/Source -o tmp/5.0.1/astc_thunk_5.0.1_arm64.dylib -L5.0.1/lib/Mac/Release -lastcenc-static astc_thunk.cpp


$CLANG_PATH ${SYSROOT[@]} -DASTC_SUPPORTS_RDO -arch x86_64 -shared -mmacosx-version-min=14.00 -fPIC -pthread -Wall -Wextra -gdwarf-4 -ffp-model=precise -ffp-contract=off -Wno-unused-parameter -std=c++17 -lc++ -I5.0.1/Source -o tmp/5.0.1/astc_thunk_5.0.1_x64.dylib -L5.0.1/lib/Mac/Release -lastcenc-static astc_thunk.cpp

lipo -create -output Thunks/libastcenc_thunk_osx64_5.0.1.dylib tmp/5.0.1/astc_thunk_5.0.1_x64.dylib tmp/5.0.1/astc_thunk_5.0.1_arm64.dylib


$CLANG_PATH ${SYSROOT[@]} -arch arm64 -shared -mmacosx-version-min=14.00 -fPIC -pthread -Wall -Wextra -gdwarf-4 -ffp-model=precise -ffp-contract=off -Wno-unused-parameter -std=c++17 -lc++ -I4.2.0/Source -o tmp/4.2.0/astc_thunk_4.2.0_arm64.dylib -L4.2.0/lib/Mac/Release -lastcenc-static astc_thunk.cpp


$CLANG_PATH ${SYSROOT[@]} -arch x86_64 -shared -mmacosx-version-min=14.00 -fPIC -pthread -Wall -Wextra -gdwarf-4 -ffp-model=precise -ffp-contract=off -Wno-unused-parameter -std=c++17 -lc++ -I4.2.0/Source -o tmp/4.2.0/astc_thunk_4.2.0_x64.dylib -L4.2.0/lib/Mac/Release -lastcenc-static astc_thunk.cpp

lipo -create -output Thunks/libastcenc_thunk_osx64_4.2.0.dylib tmp/4.2.0/astc_thunk_4.2.0_x64.dylib tmp/4.2.0/astc_thunk_4.2.0_arm64.dylib

