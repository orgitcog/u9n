// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS
#include "UbaWinBinDependencyParser.h"
#elif PLATFORM_LINUX
#include "UbaLinuxBinDependencyParser.h"
#elif PLATFORM_MAC
#include "UbaMacBinDependencyParser.h"
#endif
