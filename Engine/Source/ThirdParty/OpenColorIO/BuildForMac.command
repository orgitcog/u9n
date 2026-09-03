#!/bin/sh

OCIO_VERSION="2.4.1"
OCIO_LIB_NAME="OpenColorIO-$OCIO_VERSION"

UE_MODULE_LOCATION=`cd $(dirname "$BASH_SOURCE"); pwd`
UE_ENGINE_DIR=`cd $UE_MODULE_LOCATION/../../..; pwd`
UE_THIRD_PARTY_DIR="$UE_ENGINE_DIR/Source/ThirdParty"

cd $UE_MODULE_LOCATION

# Remove previously extracted build library folder
if [ -d "$OCIO_LIB_NAME" ]; then
   echo "Deleting previously extracted $OCIO_LIB_NAME folder"
   rm -rf "$OCIO_LIB_NAME"
fi

git clone --depth 1 --branch v$OCIO_VERSION https://github.com/AcademySoftwareFoundation/OpenColorIO.git $OCIO_LIB_NAME

pushd $OCIO_LIB_NAME

UE_C_FLAGS="-mmacosx-version-min=10.9 -arch x86_64 -arch arm64"
UE_CXX_FLAGS="-mmacosx-version-min=10.9 -arch x86_64 -arch arm64"

IMATH_INCLUDE_DIR="$UE_THIRD_PARTY_DIR/Imath/Deploy/Imath-3.1.9/include"
IMATH_LIBRARY_PATH="$UE_THIRD_PARTY_DIR/Imath/Deploy/Imath-3.1.9/Mac/lib/libImath-3_1.a"
ZLIB_INCLUDE_DIR="$UE_THIRD_PARTY_DIR/zlib/1.3/include"
ZLIB_LIBRARY_PATH="$UE_THIRD_PARTY_DIR/zlib/1.3/lib/Mac/Release/libz.a"

INSTALL_LOCATION=$UE_MODULE_LOCATION/Deploy/OpenColorIO
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR=bin/Mac
INSTALL_LIB_DIR=lib/Mac

# Configure OCIO cmake and launch a release build
echo "Configuring build..."
cmake -S . -B build \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="13.0" \
    -DCMAKE_BUILD_TYPE="Release" \
    -DCMAKE_MACOSX_RPATH=TRUE \
    -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_CXX_STANDARD=11 \
    -DCMAKE_C_FLAGS="${UE_C_FLAGS}" \
    -DCMAKE_CXX_FLAGS="${UE_CXX_FLAGS}" \
    -DOCIO_BUILD_APPS=OFF \
    -DOCIO_BUILD_GPU_TESTS=OFF \
    -DOCIO_BUILD_DOCS=OFF \
    -DOCIO_BUILD_TESTS=OFF \
    -DOCIO_BUILD_PYTHON=OFF \
    -DImath_INCLUDE_DIR="${IMATH_INCLUDE_DIR}" \
    -DImath_LIBRARY="${IMATH_LIBRARY_PATH}" \
    -DZLIB_INCLUDE_DIR="${ZLIB_INCLUDE_DIR}" \
    -DZLIB_LIBRARY="${ZLIB_LIBRARY_PATH}" \
    -DEXPAT_CXX_FLAGS="${UE_CXX_FLAGS}" \
    -Dyaml-cpp_CXX_FLAGS="${UE_CXX_FLAGS}" \
    -Dpystring_CXX_FLAGS="${UE_CXX_FLAGS}" \
    -DCMAKE_INSTALL_PREFIX:PATH="${INSTALL_LOCATION}" \
    -DCMAKE_INSTALL_INCLUDEDIR="${INSTALL_INCLUDEDIR}" \
    -DCMAKE_INSTALL_BINDIR="${INSTALL_BIN_DIR}" \
    -DCMAKE_INSTALL_LIBDIR="${INSTALL_LIB_DIR}"

echo "Building Release build..."
cmake --build build --config Release

echo "Installing Release build..."
cmake --install build --config Release

echo "Copying external static library dependencies..."
cp build/ext/dist/lib/Mac/*.a $INSTALL_LOCATION/$INSTALL_LIB_DIR

popd
