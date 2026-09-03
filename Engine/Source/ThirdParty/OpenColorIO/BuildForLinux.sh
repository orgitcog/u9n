#!/bin/bash

OCIO_VERSION="2.4.1"
OCIO_LIB_NAME="OpenColorIO-$OCIO_VERSION"

UE_MODULE_LOCATION=`cd $(dirname "$BASH_SOURCE"); pwd`
UE_ENGINE_DIR=`cd $UE_MODULE_LOCATION/../../..; pwd`
UE_SOURCE_THIRD_PARTY_LOCATION="$UE_ENGINE_DIR/Source/ThirdParty"

IMATH_INCLUDE_DIR="$UE_SOURCE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.9/include"
IMATH_LIBRARY_PATH="$UE_SOURCE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.9/Unix/$ARCH_NAME/lib/libImath-3_1.a"
ZLIB_INCLUDE_DIR="$UE_SOURCE_THIRD_PARTY_LOCATION/zlib/1.3/include"
ZLIB_LIBRARY_PATH="$UE_SOURCE_THIRD_PARTY_LOCATION/zlib/1.3/lib/Unix/$ARCH_NAME/Release/libz.a"

ARCH_NAME=$1
if [ -z "$ARCH_NAME" ]
then
    echo "Arch: 'x86_64-unknown-linux-gnu' or 'aarch64-unknown-linux-gnueabi'"
    exit 1
fi

# Specify all of the include/bin/lib directory variables so that CMake can
# compute relative paths correctly for the imported targets.
INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/OpenColorIO"
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR="bin/Unix/$ARCH_NAME"
INSTALL_LIB_DIR="lib/Unix/$ARCH_NAME"

cd $UE_MODULE_LOCATION

# Remove previously extracted build library folder
if [ -d "$OCIO_LIB_NAME" ]; then
    echo "Deleting previously extracted $OCIO_LIB_NAME folder"
    rm -rf "$OCIO_LIB_NAME"
fi

git clone --depth 1 --branch v$OCIO_VERSION https://github.com/AcademySoftwareFoundation/OpenColorIO.git $OCIO_LIB_NAME

cd $OCIO_LIB_NAME

TOOLCHAIN_NAME=v25_clang-18.1.0-rockylinux8
UE_TOOLCHAIN_LOCATION="$UE_ENGINE_DIR/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME"
UE_TOOLCHAIN_ARCH_INCLUDE_LOCATION="$UE_TOOLCHAIN_LOCATION/$ARCH_NAME/include/c++/v1"
UE_TOOLCHAIN_ARCH_LIB_LOCATION="$UE_TOOLCHAIN_LOCATION/$ARCH_NAME/lib64"
CXX_FLAGS="-fvisibility=hidden -nostdinc++ -I$UE_TOOLCHAIN_ARCH_INCLUDE_LOCATION"
LINKER_FLAGS="-fuse-ld=lld -nodefaultlibs -stdlib=libc++ $UE_TOOLCHAIN_ARCH_LIB_LOCATION/libc++.a $UE_TOOLCHAIN_ARCH_LIB_LOCATION/libc++abi.a -lm -lc -lgcc_s -lgcc"

# Determine whether we're cross compiling for an architecture that doesn't
# match the host. This is the way that CMake determines the value for the
# CMAKE_HOST_SYSTEM_PROCESSOR variable.
HOST_SYSTEM_PROCESSOR=`uname -m`
TARGET_SYSTEM_PROCESSOR=$HOST_SYSTEM_PROCESSOR

if [[ $ARCH_NAME != $HOST_SYSTEM_PROCESSOR* ]]
then
    ARCH_NAME_PARTS=(${ARCH_NAME//-/ })
    TARGET_SYSTEM_PROCESSOR=${ARCH_NAME_PARTS[0]}
fi

( cat <<_EOF_
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR ${TARGET_SYSTEM_PROCESSOR})

    set(CMAKE_SYSROOT ${UE_TOOLCHAIN_LOCATION}/${ARCH_NAME})
    set(CMAKE_LIBRARY_ARCHITECTURE ${ARCH_NAME})

    set(CMAKE_C_COMPILER \${CMAKE_SYSROOT}/bin/clang)
    set(CMAKE_C_COMPILER_TARGET ${ARCH_NAME})
    set(CMAKE_C_FLAGS "-target ${ARCH_NAME} ${C_FLAGS}")

    set(CMAKE_CXX_COMPILER \${CMAKE_SYSROOT}/bin/clang++)
    set(CMAKE_CXX_COMPILER_TARGET ${ARCH_NAME})
    set(CMAKE_CXX_FLAGS "-target ${ARCH_NAME} ${CXX_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS "${LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${LINKER_FLAGS}")

    set(CMAKE_FIND_ROOT_PATH "${UE_TOOLCHAIN_LOCATION}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
_EOF_
) > /tmp/__cmake_toolchain.cmake

CMAKE_ARGS=(
    -DCMAKE_TOOLCHAIN_FILE="/tmp/__cmake_toolchain.cmake" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_CXX_STANDARD=11 \
    -DOCIO_BUILD_APPS=OFF \
    -DOCIO_BUILD_DOCS=OFF \
    -DOCIO_BUILD_GPU_TESTS=OFF \
    -DOCIO_BUILD_TESTS=OFF \
    -DOCIO_BUILD_PYTHON=OFF \
    -DImath_INCLUDE_DIR="$IMATH_INCLUDE_DIR" \
    -DImath_LIBRARY="$IMATH_LIBRARY_PATH" \
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_DIR" \
    -DZLIB_LIBRARY="$ZLIB_LIBRARY_PATH" \
    -DEXPAT_CXX_FLAGS="$CXX_FLAGS" \
    -Dyaml-cpp_CXX_FLAGS="$CXX_FLAGS" \
    -Dpystring_CXX_FLAGS="$CXX_FLAGS" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION" \
    -DCMAKE_INSTALL_INCLUDEDIR="$INSTALL_INCLUDEDIR" \
    -DCMAKE_INSTALL_BINDIR="$INSTALL_BIN_DIR" \
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
)

# Configure OCIO cmake and launch a release build
NUM_CPU=`grep -c ^processor /proc/cpuinfo`

echo "Configuring build..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "${CMAKE_ARGS[@]}"

echo "Building Release build..."
cmake --build build --config Release -j$NUM_CPU

echo "Installing Release build..."
cmake --install build --config Release

echo "Copying external static library dependencies..."
cp build/ext/dist/lib/Unix/$ARCH_NAME/*.a $INSTALL_LOCATION/$INSTALL_LIB_DIR
