#!/bin/bash

SYMLINK_PATH=IOS/Tentacle.xcframework

rm -f $SYMLINK_PATH
ln -s ../../../../Restricted/NotForLicensees/Source/ThirdParty/TentacleSDK/apple/build/Tentacle.xcframework $SYMLINK_PATH
