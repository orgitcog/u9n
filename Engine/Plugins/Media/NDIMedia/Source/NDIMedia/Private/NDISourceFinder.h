// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NDIMediaAPI.h"

class FNDIMediaRuntimeLibrary;

/**
 *  Wrapper for the NDI source finder instance.
 */
class FNDISourceFinder
{
public:
	FNDISourceFinder(const TSharedPtr<FNDIMediaRuntimeLibrary>& InNdiLib);
	~FNDISourceFinder();

	/** Call prior to doing other calls to ensure the runtime lib is up to date. */
	void Validate(const TSharedPtr<FNDIMediaRuntimeLibrary>& InNdiLib);

	/** NDI Source information */
	struct FNDISourceInfo
	{
		FString Name;
		FString Url;
	};

	/** Returns current list of sources discovered on the network. */
	TArray<FNDISourceInfo> GetSources() const;

private:
	void Create();
	void Destroy();
	
private:
	TSharedPtr<FNDIMediaRuntimeLibrary> NdiLib;
	NDIlib_find_instance_t FindInstance = nullptr;
	mutable FCriticalSection FindSyncContext;
};
