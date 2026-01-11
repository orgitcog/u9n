// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

enum EMetaHumanCaptureError
{
	InvalidArguments,
	AbortedByUser,
	InvalidErrorCode,
	CommunicationError,
	NotFound,
	InternalError,
	Warning,
};

// This is designed to be used with TResult, which is why there is no "NoError" code
class METAHUMANCAPTURESOURCE_API FMetaHumanCaptureError
{
public:

	FMetaHumanCaptureError();
	FMetaHumanCaptureError(EMetaHumanCaptureError InCode, FString InMessage = TEXT(""));

	const FString& GetMessage() const;
	EMetaHumanCaptureError GetCode() const;

private:

	EMetaHumanCaptureError Code;
	FString Message;
};