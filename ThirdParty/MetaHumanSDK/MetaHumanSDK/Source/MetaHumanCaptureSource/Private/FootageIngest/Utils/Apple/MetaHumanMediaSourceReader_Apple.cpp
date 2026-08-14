// Copyright Epic Games, Inc. All Rights Reserved.

#include "FootageIngest/Utils/MetaHumanMediaSourceReader.h"

#if PLATFORM_MAC

TSharedPtr<IMetaHumanMediaAudioSourceReader> IMetaHumanMediaAudioSourceReader::Create()
{
	checkf(false, TEXT("MetaHumanMediaAudioSourceReader is not supported on this platform"));
	return nullptr;
}

TSharedPtr<IMetaHumanMediaVideoSourceReader> IMetaHumanMediaVideoSourceReader::Create()
{
	checkf(false, TEXT("MetaHumanMediaVideoSourceReader is not supported on this platform"));
	return nullptr;
}

#endif	// PLATFORM_MAC
