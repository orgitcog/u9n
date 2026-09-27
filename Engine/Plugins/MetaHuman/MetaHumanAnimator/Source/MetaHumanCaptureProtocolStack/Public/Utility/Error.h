// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

#include "Error/Result.h"

class METAHUMANCAPTUREPROTOCOLSTACK_API FCaptureProtocolError
{
public:

	FCaptureProtocolError();
	FCaptureProtocolError(FString InMessage, int32 InCode = 0);

	FCaptureProtocolError(const FCaptureProtocolError& InOther) = default;
	FCaptureProtocolError(FCaptureProtocolError&& InOther) = default;

	FCaptureProtocolError& operator=(const FCaptureProtocolError& InOther) = default;
	FCaptureProtocolError& operator=(FCaptureProtocolError&& InOther) = default;

	const FString& GetMessage() const;
	int32 GetCode() const;

private:

	FString Message;
	int32 Code;
};

template <typename ValueType>
using TProtocolResult = TResult<ValueType, FCaptureProtocolError>;

#define CPS_CHECK_VOID_RESULT(Function) if (TProtocolResult<void> Result = Function; Result.IsError()) { return Result.ClaimError(); }