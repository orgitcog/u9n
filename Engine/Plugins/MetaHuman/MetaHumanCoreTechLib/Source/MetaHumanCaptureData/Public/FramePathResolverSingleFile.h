// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFramePathResolver.h"

namespace UE::MetaHuman
{

/** Always resolves to the same file path */
class METAHUMANCAPTUREDATA_API FFramePathResolverSingleFile : public IFramePathResolver
{
public:
	explicit FFramePathResolverSingleFile(FString InFilePath);
	virtual ~FFramePathResolverSingleFile() override;

	virtual FString ResolvePath(int32 InFrameNumber) const override;

private:
	FString FilePath;
};

}

