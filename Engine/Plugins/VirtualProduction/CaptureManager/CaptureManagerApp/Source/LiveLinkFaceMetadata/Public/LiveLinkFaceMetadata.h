// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dom/JsonObject.h"
#include "IMediaTextureSample.h"

#include "Containers/UnrealString.h"

#include "CaptureManagerTakeMetadata.h"

namespace UE::CaptureManager::LiveLinkMetadata
{

LIVELINKFACEMETADATA_API TOptional<FTakeMetadata> ParseOldLiveLinkTakeMetadata(const FString& InJsonFile, TArray<FText>& OutValidationError);
LIVELINKFACEMETADATA_API TArray<FTakeMetadata::FVideo> ParseOldLiveLinkVideoMetadataFromString(const FString& InJsonString, TArray<FText>& OutValidationError);

}