// Copyright Epic Games, Inc. All Rights Reserved.

#include "genesplicer/version/VersionInfo.h"

#include "genesplicer/version/Version.h"

#include <cstring>

namespace gs4 {

namespace {

constexpr int majorVersion = GS_MAJOR_VERSION;
constexpr int minorVersion = GS_MINOR_VERSION;
constexpr int patchVersion = GS_PATCH_VERSION;
constexpr const char* versionString = GS_VERSION_STRING;

}  // namespace

int VersionInfo::getMajorVersion() {
    return majorVersion;
}

int VersionInfo::getMinorVersion() {
    return minorVersion;
}

int VersionInfo::getPatchVersion() {
    return patchVersion;
}

StringView VersionInfo::getVersionString() {
    return {versionString, std::strlen(versionString)};
}

}  // namespace gs4
