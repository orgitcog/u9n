#!/bin/zsh
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

SCRIPT_DIR=`dirname ${(%):-%x}`
CEF_DROP_DIR=`basename $1`

# CEF_DROP_DIR format is cef_binary_<version>_<platform>
extractCEFVersion() {
local IFS='_'
local DIR_COMPONENTS=(${=CEF_DROP_DIR})
CEF_VERSION=$DIR_COMPONENTS[3]
}
extractCEFVersion

if [ -d "$SCRIPT_DIR/$CEF_DROP_DIR" ]; then
    echo "Target dir $CEF_DROP_DIR already exists! Please check it out in P4 before continuing"
    read -k "?Press any key to continue..."
fi

echo "Updating CEF3 sources into Engine/Sources/ThirdParty/CEF3/$CEF_DROP_DIR"
rsync -a -v --delete --exclude="libcef_dll" --exclude="tests" --exclude="*.framework" --exclude=".DS_Store" "$1" "$SCRIPT_DIR"

RUNTIME_DIR=$SCRIPT_DIR/../../../Binaries/ThirdParty/CEF3/Mac/$CEF_VERSION
mkdir -p $RUNTIME_DIR
pushd $RUNTIME_DIR

echo "Updating CEF3 binaries into Engine/Binaries/ThirdParty/CEF3/Mac/$CEF_VERSION"
rsync -a -v --delete --exclude=".DS_Store" "$1/Release/Chromium Embedded Framework.framework" .
rm -f "Chromium Embedded Framework.framework.zip"
zip -qr "Chromium Embedded Framework.framework.zip" "Chromium Embedded Framework.framework"

popd

echo "CE3 drop completed! You can update CEF3.Build.cs with new version $CEF_VERSION"
