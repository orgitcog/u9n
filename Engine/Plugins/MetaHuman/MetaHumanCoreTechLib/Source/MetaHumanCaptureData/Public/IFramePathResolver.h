// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

namespace UE::MetaHuman
{

struct METAHUMANCAPTUREDATA_API IFramePathResolver
{
	virtual ~IFramePathResolver() = default;
	virtual FString ResolvePath(int32 InFrameNumber) const = 0;
};

}

