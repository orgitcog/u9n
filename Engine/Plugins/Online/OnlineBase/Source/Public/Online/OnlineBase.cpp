// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineBase.h"

static TAutoConsoleVariable<int32> CVarBuildIdOverride(
TEXT("buildidoverride"),
0,
TEXT("Sets build id used for matchmaking "));

TAutoConsoleVariable<int32>& GetBuildIdOverrideCVar()
{
	return CVarBuildIdOverride;
}
