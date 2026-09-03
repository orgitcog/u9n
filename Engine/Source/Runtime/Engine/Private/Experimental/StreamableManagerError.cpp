// Copyright Epic Games, Inc. All Rights Reserved.
#include "Engine/Experimental/StreamableManagerError.h"
#include "UObject/UObjectGlobals.h"



namespace UE::UnifiedError::StreamableManager
{

UE::UnifiedError::FError GetStreamableError(EAsyncLoadingResult::Type Result)
{
	switch (Result)
	{
	case EAsyncLoadingResult::Failed:
	case EAsyncLoadingResult::FailedMissing:
	case EAsyncLoadingResult::FailedLinker:
		// We could supply an error string if async loading bubbled one up...
		// Possibly GetExplanationForUnavailablePackage?
		return AsyncLoadFailed::MakeError();
	case EAsyncLoadingResult::FailedNotInstalled:
		return AsyncLoadNotInstalled::MakeError();
	case EAsyncLoadingResult::Canceled:
		return AsyncLoadCancelled::MakeError();
	}

	return AsyncLoadUnknownError::MakeError((int32)Result);
}

} // namespace UE::UnifiedError::StreamableManager