#!/bin/bash

set -e

LIBRARY_NAME="Embree"
REPOSITORY_NAME="embree"

# Informational, for the usage message.
CURRENT_LIBRARY_VERSION=4.3.3

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
UE_SOURCE_THIRD_PARTY_LOCATION=`cd $UE_MODULE_LOCATION/../..; pwd`
TBB_CMAKE_LOCATION="$UE_SOURCE_THIRD_PARTY_LOCATION/Intel/TBB/Deploy/oneTBB-2021.13.0/Mac/lib/cmake/TBB"

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
    -DCMAKE_PREFIX_PATH="$TBB_CMAKE_LOCATION"
    -DCMAKE_INSTALL_INCLUDEDIR="$INSTALL_INCLUDEDIR"
    -DCMAKE_INSTALL_BINDIR="$INSTALL_BIN_DIR"
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="13.0"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DCMAKE_DEBUG_POSTFIX=_d
    -DBUILD_TESTING=OFF
    -DEMBREE_ZIP_MODE=OFF
    -DEMBREE_TUTORIALS=OFF
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

echo Checking for invalid code signatures in generated libraries...
for DYLIB in `find $INSTALL_LOCATION/$INSTALL_LIB_DIR -type f -name '*.dylib'`
do
    if ! codesign -v $DYLIB
    then
        echo Invalid code signature detected in library: $DYLIB
        echo Replacing with ad-hoc signature...
        codesign -f -s - $DYLIB
    fi
done

echo Converting installed $LIBRARY_NAME library symlinks to files...
pushd $INSTALL_LOCATION/$INSTALL_LIB_DIR > /dev/null
for SYMLINKED_LIB in `find . -type l`
do
    _LINK_DEST=`readlink $SYMLINKED_LIB` && \
    unlink $SYMLINKED_LIB && \
    cp -pL $_LINK_DEST $SYMLINKED_LIB \
    && unset _LINK_DEST
done
popd > /dev/null

echo "Removing share directory (documentation)..."
rm -rf "$INSTALL_LOCATION/share"

echo Done.
