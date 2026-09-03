// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerTakeMetadata.h"

namespace UE::CaptureManager::StereoCameraMetadata
{

STEREOCAMERAMETADATA_API TOptional<FTakeMetadata> ParseOldStereoCameraMetadata(const FString& InTakeFolder, TArray<FText>& OutValidationError);

}