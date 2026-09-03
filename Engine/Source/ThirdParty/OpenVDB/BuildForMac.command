#!/bin/bash

set -e

LIBRARY_NAME="OpenVDB"
REPOSITORY_NAME="openvdb"

# Informational, for the usage message.
CURRENT_LIBRARY_VERSION=12.0.0

# When building OpenVDB, be sure to apply the following patches:
#   - allow use of the library with RTTI disabled
#   - add missing math::half instantiations of cwiseAdd()
# From the "openvdb-12.0.0" source directory, run:
#     git apply ../openvdb_12.0.0_support_disabling_RTTI.patch
#     git apply ../openvdb_12.0.0_Vec_half_cwiseAdd.patch

BUILD_SCRIPT_NAME="$(basename $BASH_SOURCE)"
BUILD_SCRIPT_DIR=`cd $(dirname "$BASH_SOURCE"); pwd`

UsageAndExit()
{
    echo "Build $LIBRARY_NAME for use with Unreal Engine on Mac"
    echo
    echo "Usage:"
    echo
    echo "    $BUILD_SCRIPT_NAME <$LIBRARY_NAME Version>"
    echo
    echo "Usage examples:"
    echo
    echo "    $BUILD_SCRIPT_NAME $CURRENT_LIBRARY_VERSION"
    echo "      -- Installs $LIBRARY_NAME version $CURRENT_LIBRARY_VERSION."
    echo
    exit 1
}

# Get version from arguments.
LIBRARY_VERSION=$1
if [ -z "$LIBRARY_VERSION" ]
then
    UsageAndExit
fi

UE_MODULE_LOCATION=$BUILD_SCRIPT_DIR
UE_SOURCE_THIRD_PARTY_LOCATION=`cd $UE_MODULE_LOCATION/..; pwd`

ZLIB_LOCATION="$UE_SOURCE_THIRD_PARTY_LOCATION/zlib/1.3"
ZLIB_INCLUDE_LOCATION="$ZLIB_LOCATION/include"
ZLIB_LIB_LOCATION="$ZLIB_LOCATION/lib/Mac/Release/libz.a"

TBB_LOCATION="$UE_SOURCE_THIRD_PARTY_LOCATION/Intel/TBB/Deploy/oneTBB-2021.13.0"
TBB_CMAKE_LOCATION="$TBB_LOCATION/Mac/lib/cmake/TBB"
# The TBB CMake config would be sufficient, but OpenVDB has its own
# FindTBB.cmake that we have to appease.
TBB_INCLUDE_LOCATION="$TBB_LOCATION/include"
TBB_LIB_LOCATION="$TBB_LOCATION/Mac/lib"

BLOSC_LOCATION="$UE_SOURCE_THIRD_PARTY_LOCATION/Blosc/Deploy/c-blosc-1.21.0"
BLOSC_INCLUDE_LOCATION="$BLOSC_LOCATION/include"
BLOSC_LIB_LOCATION="$BLOSC_LOCATION/Mac/lib"
BLOSC_LIBRARY_LOCATION_RELEASE="$BLOSC_LIB_LOCATION/libblosc.a"
BLOSC_LIBRARY_LOCATION_DEBUG="$BLOSC_LIB_LOCATION/libblosc_d.a"

BOOST_CMAKE_LOCATION="$UE_SOURCE_THIRD_PARTY_LOCATION/Boost/Deploy/boost-1.85.0/Mac/lib/cmake/Boost-1.85.0"

SOURCE_LOCATION="$UE_MODULE_LOCATION/$REPOSITORY_NAME-$LIBRARY_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

# Specify all of the include/bin/lib directory variables so that CMake can
# compute relative paths correctly for the imported targets.
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR="Mac/bin"
INSTALL_LIB_DIR="Mac/lib"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/$REPOSITORY_NAME-$LIBRARY_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$TBB_CMAKE_LOCATION;$BOOST_CMAKE_LOCATION"
    -DCMAKE_INSTALL_INCLUDEDIR="$INSTALL_INCLUDEDIR"
    -DCMAKE_INSTALL_BINDIR="$INSTALL_BIN_DIR"
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_LOCATION"
    -DZLIB_LIBRARY="$ZLIB_LIB_LOCATION"
    -DTBB_INCLUDEDIR="$TBB_INCLUDE_LOCATION"
    -DTBB_LIBRARYDIR="$TBB_LIB_LOCATION"
    -DBLOSC_INCLUDEDIR="$BLOSC_INCLUDE_LOCATION"
    -DBLOSC_LIBRARYDIR="$BLOSC_LIB_LOCATION"
    -DBLOSC_USE_STATIC_LIBS=ON
    -DBlosc_LIBRARY_RELEASE="$BLOSC_LIBRARY_LOCATION_RELEASE"
    -DBlosc_LIBRARY_DEBUG="$BLOSC_LIBRARY_LOCATION_DEBUG"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="13.0"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DUSE_PKGCONFIG=OFF
    -DUSE_EXPLICIT_INSTANTIATION=OFF
    -DOPENVDB_BUILD_BINARIES=OFF
    -DOPENVDB_INSTALL_CMAKE_MODULES=OFF
    -DOPENVDB_CORE_SHARED=OFF
    -DOPENVDB_CORE_STATIC=ON
    -DCMAKE_DEBUG_POSTFIX=_d
)

NUM_CPU=8

echo Configuring build for $LIBRARY_NAME version $LIBRARY_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building $LIBRARY_NAME for Debug...
cmake --build . --config Debug -j$NUM_CPU

echo Installing $LIBRARY_NAME for Debug...
cmake --install . --config Debug

echo Building $LIBRARY_NAME for Release...
cmake --build . --config Release -j$NUM_CPU

echo Installing $LIBRARY_NAME for Release...
cmake --install . --config Release

popd > /dev/null

echo Done.
