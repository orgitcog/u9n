// Copyright Epic Games, Inc. All Rights Reserved.

#include "FootageIngest/Utils/MetaHumanMediaSourceReader.h"

#if UE_SERVER || (!PLATFORM_WINDOWS && !PLATFORM_MAC)

static IMetaHumanMediaAudioSourceReader* IMetaHumanMediaAudioSourceReader::Create()
{
	checkf(false, TEXT("MetaHumanMediaAudioSourceReader is not supported on this platform"));
	return nullptr;
}

static IMetaHumanMediaVideoSourceReader* IMetaHumanMediaVideoSourceReader::Create()
{
	checkf(false, TEXT("MetaHumanMediaVideoSourceReader is not supported on this platform"));
	return nullptr;
}

#endif	// UE_SERVER || (!PLATFORM_WINDOWS && !PLATFORM_MAC)