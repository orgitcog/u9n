// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"

#include "HAL/Platform.h"

/** Asset creation error types. */
enum class EAssetCreationError
{
	InternalError = 1,
	InvalidArgument,
	NotFound,
	Warning,
};

/** Information related to an error during ingest asset creation. */
class DATAINGESTCOREEDITOR_API FAssetCreationError
{
public:

	FAssetCreationError(FText InMessage, EAssetCreationError InError = EAssetCreationError::InternalError);

	/** Get the error message. */
	const FText& GetMessage() const;

	/** Get the error type. */
	EAssetCreationError GetError() const;

private:

	/** Error message. */
	FText Message;

	/** Error type. */
	EAssetCreationError Error;
};