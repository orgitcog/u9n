// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "FrameNumberTransformer.h"
#include "IFramePathResolver.h"

namespace UE::MetaHuman
{

class METAHUMANCAPTUREDATA_API FFramePathResolver : public IFramePathResolver
{
public:
	FFramePathResolver(FString InFilePathTemplate);
	FFramePathResolver(FString InFilePathTemplate, FFrameNumberTransformer InFrameNumberTransformer);
	virtual ~FFramePathResolver() override;

	virtual FString ResolvePath(int32 InFrameNumber) const override;

private:
	FString FilePathTemplate;
	FFrameNumberTransformer FrameNumberTransformer;
};

}
