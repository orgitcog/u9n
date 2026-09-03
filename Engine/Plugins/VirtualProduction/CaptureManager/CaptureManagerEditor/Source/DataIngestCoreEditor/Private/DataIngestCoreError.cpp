// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataIngestCoreError.h"

FAssetCreationError::FAssetCreationError(FText InMessage, EAssetCreationError InError)
	: Message(MoveTemp(InMessage))
	, Error(InError)
{
}

const FText& FAssetCreationError::GetMessage() const
{
	return Message;
}

EAssetCreationError FAssetCreationError::GetError() const
{
	return Error;
}