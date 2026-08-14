#!/bin/sh

set -e

echo "Setting up build environment..."

ConfigPath=`dirname "$0"`
pushd $ConfigPath/..
projectPath=`pwd`
cd ../../../../../..
EnginePath=`pwd`
cd ..
RootDir=`pwd`
popd

echo "UE_SDKS_ROOT = ${UE_SDKS_ROOT}"

if [ -z ${UE_SDKS_ROOT+x} ]; then
    echo "UE_SDKS_ROOT is unset";
    exit -1;
fi

if [ ! -f "$ConfigPath/SDKsRoot.xcconfig" ]; then
    echo "Create $ConfigPath/SDKsRoot.xcconfig"
    echo UESDKRoot = $UE_SDKS_ROOT > "$ConfigPath/SDKsRoot.xcconfig"
    echo ArchiCADLocalMacDir = "$RootDir/ArchiCADLocalMacDir" >> "$ConfigPath/SDKsRoot.xcconfig"
    echo UE_Engine = $EnginePath >> "$ConfigPath/SDKsRoot.xcconfig"
fi

# ls -l $UE_SDKS_ROOT/HostMac/Mac/Archicad/Archicad.zip
echo "Unzipping $UE_SDKS_ROOT/HostMac/Mac/Archicad/Archicad.zip"
unzip -qq $UE_SDKS_ROOT/HostMac/Mac/Archicad/Archicad.zip -d $RootDir/ArchiCADLocalMacDir
# ls -l $RootDir/ArchiCADLocalMacDir/Archicad/23/Support/Frameworks/ACOperations.framework/Versions/Current

# Remove ArchiCAD resource tool from quarantine
pushd "$RootDir/ArchiCADLocalMacDir/Archicad"
	chmod 777 23.1/Support/Tools/OSX/ResConv
	/usr/bin/xattr -r -d com.apple.quarantine 23.1/Support/Tools/OSX/ResConv

	chmod 777 24/Support/Tools/OSX/ResConv
	/usr/bin/xattr -r -d com.apple.quarantine 24/Support/Tools/OSX/ResConv

	chmod 777 25/Support/Tools/OSX/ResConv
	/usr/bin/xattr -r -d com.apple.quarantine 25/Support/Tools/OSX/ResConv

	chmod 777 26/Support/Tools/OSX/ResConv
	/usr/bin/xattr -r -d com.apple.quarantine 26/Support/Tools/OSX/ResConv

	chmod 777 27/Support/Tools/OSX/ResConv
	/usr/bin/xattr -r -d com.apple.quarantine 27/Support/Tools/OSX/ResConv

	chmod 777 28/Support/Tools/OSX/ResConv
	/usr/bin/xattr -r -d com.apple.quarantine 28/Support/Tools/OSX/ResConv
popd

OurDylibFolder=$projectPath/Dylibs

mkdir -p "$OurDylibFolder"

dylibLibFreeImage=libfreeimage-3.18.0.dylib
dylibtbb=libtbb.dylib
dylibtbbmalloc=libtbbmalloc.dylib

SetUpThirdPartyDll() {
	DylibName=$1
	DylibPath=$2
	if [[ "$DylibPath" -nt "$OurDylibFolder/$DylibName" ]]; then
		if [ -f "$OurDylibFolder/$DylibName" ]; then
			unlink "$OurDylibFolder/$DylibName"
		fi
	fi
	if [ ! -f "$OurDylibFolder/$DylibName" ]; then
		echo "Copy $DylibName"
		cp "$DylibPath" "$OurDylibFolder"
		chmod +w "$OurDylibFolder/$DylibName"
		install_name_tool -id @loader_path/$DylibName "$OurDylibFolder/$DylibName" > /dev/null 2>&1
	fi
}

SetUpDll() {
	DylibName=$1
	OriginalDylibPath="$EnginePath/Binaries/Mac/DatasmithUE4ArchiCAD/$DylibName"

	if [[ "$OriginalDylibPath" -nt "$OurDylibFolder/$DylibName" ]]; then
		if [ -f "$OurDylibFolder/$DylibName" ]; then
			unlink "$OurDylibFolder/$DylibName"
		fi
	fi
	if [ ! -f "$OurDylibFolder/$DylibName" ]; then
		if [ -f "$OriginalDylibPath" ]; then
			echo "Copy $DylibName"
			cp "$OriginalDylibPath" "$OurDylibFolder"
			install_name_tool -id @loader_path/$DylibName "$OurDylibFolder/$DylibName" > /dev/null 2>&1
			install_name_tool -change @rpath/$dylibLibFreeImage @loader_path/$dylibLibFreeImage "$OurDylibFolder/$DylibName" > /dev/null 2>&1
		else
			echo "Missing $DylibName"
		fi
	fi
}

SetUpThirdPartyDll $dylibLibFreeImage "$EnginePath/Binaries/ThirdParty/FreeImage/Mac/$dylibLibFreeImage"

tbbLibDir="$EnginePath/Source/ThirdParty/Intel/TBB/Deploy/oneTBB-2021.13.0/Mac/lib"
dylibLibTbbFilenames=$(find "${tbbLibDir}" -name "libtbb*.dylib" -maxdepth 1 -exec basename {} \;)
for dylibName in $dylibLibTbbFilenames; do 
    SetUpThirdPartyDll $dylibName "${tbbLibDir}/$dylibName"
done

SetUpDll DatasmithUE4ArchiCAD.dylib
SetUpDll DatasmithUE4ArchiCAD-Mac-Debug.dylib

echo "Build environment set."
