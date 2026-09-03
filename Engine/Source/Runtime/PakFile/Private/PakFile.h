// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPlatformFilePak.h"

#if ENABLE_PAKFILE_USE_DIRECTORY_TREE
#if !UE_BUILD_SHIPPING
extern bool GPak_ValidateDirectoryTreeSearchConsistency;
#endif // UE_BUILD_SHIPPING
extern bool GPak_UseDirectoryTreeForPakSearch;
#endif // ENABLE_PAKFILE_USE_DIRECTORY_TREE
