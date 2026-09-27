// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"

namespace UE::MetaHuman
{

class METAHUMANCAPTUREDATA_API FFrameNumberTransformer
{
public:
	/** Pass-through transform (no change) */
	FFrameNumberTransformer();

	/** Apply a simple offset */
	explicit FFrameNumberTransformer(int32 InFrameNumberOffset);

	/** Adjust for differences in frame rate */
	FFrameNumberTransformer(FFrameRate InSourceFrameRate, FFrameRate InTargetFrameRate);

	/** Adjust for differences in frame rate and apply an offset */
	FFrameNumberTransformer(FFrameRate InSourceFrameRate, FFrameRate InTargetFrameRate, int32 InFrameNumberOffset);

	/* Transforms the input frame number */
	int32 Transform(int32 InFrameNumber) const;

private:
	FFrameRate SourceFrameRate;
	FFrameRate TargetFrameRate;
	int32 SkipFactor = 0;
	int32 DuplicationFactor = 0;
	int32 FrameNumberOffset = 0;
};

}
