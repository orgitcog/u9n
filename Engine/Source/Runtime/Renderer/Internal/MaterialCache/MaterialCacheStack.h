// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class FMaterialRenderProxy;

struct FMaterialCacheStackEntry
{
	/** The material to be rendered on top of the proxy */
	const FMaterialRenderProxy* Material = nullptr;
};

struct FMaterialCacheStack
{
	/** All material stacks to be composited, respects the given order */
	TArray<FMaterialCacheStackEntry, TInlineAllocator<8u>> Stack;
};
