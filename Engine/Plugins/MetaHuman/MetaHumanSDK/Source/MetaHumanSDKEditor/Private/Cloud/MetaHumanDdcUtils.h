// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Memory/SharedBuffer.h"

namespace UE::MetaHuman
{
	// returns true if at least a DDC is available
	bool CacheAvailable();
	// tries to find the given key in the cache and returns its contents if found
	[[nodiscard]] FSharedBuffer TryCacheFetch(const FString& KeyString);
	// tries to update the cache entry for the given key with the given content (can silently fail)
	void UpdateCacheAsync(const FString& KeyString, FSharedString InRequestName, FSharedBuffer InOutSharedBuffer);
}