#!/bin/bash

set -e

LIBRARY_NAME="OpenEXR"
REPOSITORY_NAME="openexr"

# Informational, for the usage message.
CURRENT_LIBRARY_VERSION=3.3.2

# If building OpenEXR version 3.3.2, be sure to apply this patch to address
# an issue with reading very small images using the stream API.
# From the "openexr-3.3.2" source directory, run:
#     git apply ../openexr_v3.3.2_small_images_PR_1985.patch

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
    -DBUILD_TESTING=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DOPENEXR_BUILD_TOOLS=OFF
    -DOPENEXR_BUILD_EXAMPLES=OFF
    -DOPENEXR_INSTALL_PKG_CONFIG=OFF
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
