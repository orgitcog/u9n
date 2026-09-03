#!/bin/bash

set -e

LIBRARY_NAME="Alembic"
REPOSITORY_NAME="alembic"

# Informational, for the usage message.
CURRENT_LIBRARY_VERSION=1.8.7

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
IMATH_CMAKE_LOCATION="$UE_SOURCE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.12/Mac/lib/cmake/Imath"

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

CXX_FLAGS="-fvisibility-ms-compat -fvisibility-inlines-hidden"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION"
    -DCMAKE_INSTALL_INCLUDEDIR="$INSTALL_INCLUDEDIR"
    -DCMAKE_INSTALL_BINDIR="$INSTALL_BIN_DIR"
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="13.0"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DUSE_TESTS=OFF
    -DALEMBIC_SHARED_LIBS=OFF
    -DUSE_BINARIES=OFF
    -DALEMBIC_ILMBASE_LINK_STATIC=ON
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
