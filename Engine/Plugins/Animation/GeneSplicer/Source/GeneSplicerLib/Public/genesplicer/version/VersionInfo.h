// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "genesplicer/Defs.h"
#include "genesplicer/types/Aliases.h"

namespace gs4 {

struct GSAPI VersionInfo {
    static int getMajorVersion();
    static int getMinorVersion();
    static int getPatchVersion();
    static StringView getVersionString();
};

}  // namespace gs4
