// Copyright Epic Games, Inc. All Rights Reserved.

#include "FramePathResolverSingleFile.h"

namespace UE::MetaHuman
{

FFramePathResolverSingleFile::FFramePathResolverSingleFile(FString InFilePath) :
	FilePath(MoveTemp(InFilePath))
{
	// We don't expect a template path
	check(!FilePath.Contains(TEXT("%")));
}

FFramePathResolverSingleFile::~FFramePathResolverSingleFile() = default;

FString FFramePathResolverSingleFile::ResolvePath([[maybe_unused]] const int32 InFrameNumber) const
{
	return FilePath;
}

}
