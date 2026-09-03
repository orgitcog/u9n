#!/bin/bash

set -e

LIBRARY_NAME="Boost"
REPOSITORY_NAME="boost"

# Informational, for the usage message.
CURRENT_LIBRARY_VERSION=1.85.0

BUILD_SCRIPT_NAME="$(basename $BASH_SOURCE)"
BUILD_SCRIPT_DIR=`cd $(dirname "$BASH_SOURCE"); pwd`

UsageAndExit()
{
    echo "Build $LIBRARY_NAME for use with Unreal Engine on Linux"
    echo
    echo "Usage:"
    echo
    echo "    $BUILD_SCRIPT_NAME <$LIBRARY_NAME Version> <Architecture Name> [<library name> [<library name> ...]]"
    echo
    echo "Usage examples:"
    echo
    echo "    $BUILD_SCRIPT_NAME $CURRENT_LIBRARY_VERSION x86_64-unknown-linux-gnu"
    echo "      -- Installs $LIBRARY_NAME version $CURRENT_LIBRARY_VERSION as header-only."
    echo
    echo "    $BUILD_SCRIPT_NAME $CURRENT_LIBRARY_VERSION x86_64-unknown-linux-gnu iostreams system thread"
    echo "      -- Installs $LIBRARY_NAME version $CURRENT_LIBRARY_VERSION for x86_64 architecture with iostreams, system, and thread libraries."
    echo
    echo "    $BUILD_SCRIPT_NAME $CURRENT_LIBRARY_VERSION aarch64-unknown-linux-gnueabi all"
    echo "      -- Installs $LIBRARY_NAME version $CURRENT_LIBRARY_VERSION for arm64 architecture with all of its libraries."
    echo
    exit 1
}

# Get version and architecture from arguments.
LIBRARY_VERSION=$1
if [ -z "$LIBRARY_VERSION" ]
then
    UsageAndExit
fi

ARCH_NAME=$2
if [ -z "$ARCH_NAME" ]
then
    UsageAndExit
fi

shift
shift

# Get the requested libraries to build from arguments, if any.
ARG_LIBRARIES=()
BOOST_WITH_LIBRARIES=""

for arg in "$@"
do
    ARG_LIBRARIES+=("$arg")
    BOOST_WITH_LIBRARIES="$BOOST_WITH_LIBRARIES --with-$arg"
done

UE_MODULE_LOCATION=`cd $BUILD_SCRIPT_DIR/../..; pwd`
UE_SOURCE_THIRD_PARTY_LOCATION=`cd $UE_MODULE_LOCATION/..; pwd`
UE_ENGINE_LOCATION=`cd $UE_MODULE_LOCATION/../../..; pwd`

PYTHON_EXECUTABLE_LOCATION="$UE_ENGINE_LOCATION/Binaries/ThirdParty/Python3/Linux/bin/python3"
PYTHON_VERSION=3.11

echo [`date +"%r"`] Building $LIBRARY_NAME for Unreal Engine
echo "    Version  : $LIBRARY_VERSION"
if [ ${#ARG_LIBRARIES[@]} -eq 0 ]
then
    echo "    Libraries: <headers-only>"
else
    echo "    Libraries: ${ARG_LIBRARIES[*]}"
fi

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

INSTALL_INCLUDEDIR=include

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/$REPOSITORY_NAME-$LIBRARY_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_UNIX_ARCH_LOCATION="$INSTALL_LOCATION/Unix/$ARCH_NAME"
INSTALL_LIB_LOCATION="$INSTALL_UNIX_ARCH_LOCATION/lib"

rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_UNIX_ARCH_LOCATION

# Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
# that the toolchain is setup and verify that this name matches.
TOOLCHAIN_NAME=v25_clang-18.1.0-rockylinux8

UE_TOOLCHAIN_LOCATION="$UE_ENGINE_LOCATION/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME"
UE_TOOLCHAIN_ARCH_INCLUDE_LOCATION="$UE_TOOLCHAIN_LOCATION/$ARCH_NAME/include/c++/v1"
UE_TOOLCHAIN_ARCH_LIB_LOCATION="$UE_TOOLCHAIN_LOCATION/$ARCH_NAME/lib64"

# Set the following variable to 1 if you have already downloaded and extracted
# the Boost sources and you need to play around with the build configuration.
ALREADY_HAVE_SOURCES=0

LIBRARY_UNDERSCORE_VERSION="`echo $LIBRARY_VERSION | sed s/\\\./_/g`"
LIBRARY_VERSION_FILENAME="${REPOSITORY_NAME}_$LIBRARY_UNDERSCORE_VERSION"

if [ $ALREADY_HAVE_SOURCES -eq 0 ]
then
    rm -rf $BUILD_LOCATION
    mkdir $BUILD_LOCATION
else
    echo Expecting sources to already be available at $BUILD_LOCATION/$LIBRARY_VERSION_FILENAME
fi

pushd $BUILD_LOCATION > /dev/null

if [ $ALREADY_HAVE_SOURCES -eq 0 ]
then
    # Download Boost source file.
    BOOST_SOURCE_FILE="$LIBRARY_VERSION_FILENAME.tar.gz"
    BOOST_URL="https://boostorg.jfrog.io/artifactory/main/release/$LIBRARY_VERSION/source/$BOOST_SOURCE_FILE"

    echo [`date +"%r"`] Downloading $BOOST_URL...
    curl -# -L -o $BOOST_SOURCE_FILE $BOOST_URL

    # Extract Boost source file.
    echo
    echo [`date +"%r"`] Extracting $BOOST_SOURCE_FILE...
    tar -xf $BOOST_SOURCE_FILE
fi

pushd $LIBRARY_VERSION_FILENAME > /dev/null

if [ ${#ARG_LIBRARIES[@]} -eq 0 ]
then
    # No libraries requested. Just copy header files.
    echo [`date +"%r"`] Copying header files...
    
    BOOST_HEADERS_DIRECTORY_NAME=boost

    mkdir -p $INSTALL_INCLUDE_LOCATION

    cp -rp $BOOST_HEADERS_DIRECTORY_NAME $INSTALL_INCLUDE_LOCATION
else
    # Build and install with libraries.
    echo [`date +"%r"`] Building $LIBRARY_NAME libraries...

    # Set tool set to current UE tool set.
    BOOST_TOOLSET=clang

    # Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
    # that the toolchain is setup and verify that the path where it installed
    # the compiler matches the path specified in the
    # user-config.<architecture>.jam file.

    # Provide user config to specify compiler/architecture and Python
    # configuration.
    BOOST_USER_CONFIG="$BUILD_SCRIPT_DIR/user-config.$ARCH_NAME.jam"

    CXX_FLAGS="-target $ARCH_NAME -fPIC -nostdinc++ -I$UE_TOOLCHAIN_ARCH_INCLUDE_LOCATION"
    LINKER_FLAGS="-target $ARCH_NAME -fuse-ld=lld -nodefaultlibs -stdlib=libc++ $UE_TOOLCHAIN_ARCH_LIB_LOCATION/libc++.a $UE_TOOLCHAIN_ARCH_LIB_LOCATION/libc++abi.a -lm -lc -lgcc_s -lgcc"

    # Bootstrap before build.
    echo [`date +"%r"`] Bootstrapping $LIBRARY_NAME $LIBRARY_VERSION build...
    ./bootstrap.sh \
        --prefix=$INSTALL_LOCATION \
        --includedir=$INSTALL_INCLUDE_LOCATION \
        --libdir=$INSTALL_LIB_LOCATION \
        --with-toolset=$BOOST_TOOLSET \
        --with-python=$PYTHON_EXECUTABLE_LOCATION \
        --with-python-version=$PYTHON_VERSION \
        cflags="$CXX_FLAGS" \
        cxxflags="$CXX_FLAGS" \
        linkflags="$LINKER_FLAGS"

    echo [`date +"%r"`] Building $LIBRARY_NAME $LIBRARY_VERSION...

    NUM_CPU=`grep -c ^processor /proc/cpuinfo`

    ./b2 \
        --prefix=$INSTALL_LOCATION \
        --includedir=$INSTALL_INCLUDE_LOCATION \
        --libdir=$INSTALL_LIB_LOCATION \
        -j$NUM_CPU \
        address-model=64 \
        threading=multi \
        variant=release \
        $BOOST_WITH_LIBRARIES \
        --user-config=$BOOST_USER_CONFIG \
        --hash \
        --build-type=complete \
        --layout=tagged \
        --debug-configuration \
        toolset=$BOOST_TOOLSET \
        cflags="$CXX_FLAGS" \
        cxxflags="$CXX_FLAGS" \
        linkflags="$LINKER_FLAGS" \
        install

    echo [`date +"%r"`] Converting installed $LIBRARY_NAME library symlinks to files...
    pushd $INSTALL_LIB_LOCATION > /dev/null
    for SYMLINKED_LIB in `find . -type l`
    do
        cp --remove-destination `readlink $SYMLINKED_LIB` $SYMLINKED_LIB
    done
    popd > /dev/null
fi

popd > /dev/null

popd > /dev/null

echo [`date +"%r"`] $LIBRARY_NAME $LIBRARY_VERSION installed to $INSTALL_LOCATION
echo [`date +"%r"`] Done.
