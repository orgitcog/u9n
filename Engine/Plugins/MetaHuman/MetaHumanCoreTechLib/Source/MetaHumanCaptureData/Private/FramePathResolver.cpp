// Copyright Epic Games, Inc. All Rights Reserved.

#include "FramePathResolver.h"

#include "TrackingPathUtils.h"

namespace UE::MetaHuman
{

FFramePathResolver::FFramePathResolver(FString InFilePathTemplate) :
	FilePathTemplate(MoveTemp(InFilePathTemplate))
{
	// We expect a template path
	check(FilePathTemplate.Contains(TEXT("%")));
}

FFramePathResolver::FFramePathResolver(FString InFilePathTemplate, FFrameNumberTransformer InFrameNumberTransformer) :
	FilePathTemplate(MoveTemp(InFilePathTemplate)),
	FrameNumberTransformer(MoveTemp(InFrameNumberTransformer))
{
	// We expect a template path
	check(FilePathTemplate.Contains(TEXT("%")));
}

FFramePathResolver::~FFramePathResolver() = default;

FString FFramePathResolver::ResolvePath(const int32 InFrameNumber) const
{
	const int32 NewFrameNumber = FrameNumberTransformer.Transform(InFrameNumber);
	FString ResolvedPath = FTrackingPathUtils::ExpandFilePathFormat(FilePathTemplate, NewFrameNumber);

	return ResolvedPath;
}

}
