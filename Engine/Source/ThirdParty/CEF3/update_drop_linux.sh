#!/bin/bash
set -e

if [[ "$1" == "" ]]; then
    echo "Usage: $0 <path to CEF3 drop>"
    exit 1
fi

# Sniff for the libcef_dll_wrapper.a lib in the CEF drop to confirm a valid location
if [ ! -f "$1/Release/libcef_dll_wrapper.a" ]; then
    echo "\"$1\" is not a valid CEF drop directory;"
    exit 1
fi

# this ensures there are no trailing slashes in the paths
SCRIPT_DIR=`cd "$(dirname "${BASH_SOURCE[0]}")" > /dev/null && pwd`
CEF_DROP_SRC_DIR=`cd "$1" > /dev/null && pwd`
CEF_DROP_NAME=`basename "$CEF_DROP_SRC_DIR"`

# CEF_DROP_NAME format is cef_binary_<version>_<platform>
extractCEFVersion() {
local IFS='_'
local DIR_COMPONENTS=($CEF_DROP_NAME)
CEF_VERSION=${DIR_COMPONENTS[2]}
}
extractCEFVersion

if [ -d "$SCRIPT_DIR/$CEF_DROP_NAME" ]; then
    echo "Target dir $CEF_DROP_NAME already exists! Please check it out in P4 before continuing"
    read -p "Press any key to continue..." -n1
fi

echo "Updating CEF3 sources into Engine/Sources/ThirdParty/CEF3/$CEF_DROP_NAME"
rsync -a -v --delete --exclude="libcef_dll" --exclude="tests" --exclude="Resources" --exclude="*.so" --exclude="*.so.1" --exclude="*.bin" --exclude="chrome-sandbox" --exclude="vk_swiftshader_icd.json" "$CEF_DROP_SRC_DIR" "$SCRIPT_DIR"

RUNTIME_DIR=$SCRIPT_DIR/../../../Binaries/ThirdParty/CEF3/Linux/$CEF_VERSION
mkdir -p $RUNTIME_DIR
pushd $RUNTIME_DIR > /dev/null

echo "Updating CEF3 binaries into Engine/Binaries/ThirdParty/CEF3/Linux/$CEF_VERSION"
rsync -a -v --delete --exclude="*.a" "$CEF_DROP_SRC_DIR/Release/" .
rsync -a -v --exclude="locales" "$CEF_DROP_SRC_DIR/Resources/" .
rsync -a -v "$CEF_DROP_SRC_DIR/Resources/locales" Resources

popd > /dev/null

echo "CE3 drop completed! You can update CEF3.Build.cs with new version $CEF_VERSION"
