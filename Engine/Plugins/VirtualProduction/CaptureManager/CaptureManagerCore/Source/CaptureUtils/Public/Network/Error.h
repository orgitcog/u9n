// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

#include "Templates/ValueOrError.h"

namespace UE::CaptureManager
{

class CAPTUREUTILS_API FCaptureProtocolError
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
	int32 Code = 0;
};

template <typename ValueType>
class TProtocolResult : public TValueOrError<ValueType, FCaptureProtocolError>
{
private:

	using Base = TValueOrError<ValueType, FCaptureProtocolError>;

public:

	TProtocolResult(ValueType&& InValueType)
		: Base(MakeValue(MoveTemp(InValueType)))
	{
	}

	TProtocolResult(const ValueType& InValueType)
		: Base(MakeValue(InValueType))
	{
	}

	TProtocolResult(FCaptureProtocolError&& InErrorType)
		: Base(MakeError(MoveTemp(InErrorType)))
	{
	}

	TProtocolResult(const FCaptureProtocolError& InErrorType)
		: Base(MakeError(InErrorType))
	{
	}

	template <typename... ArgTypes>
	TProtocolResult(TInPlaceType<ValueType>&&, ArgTypes&&... Args)
		: Base(MakeValue(Forward<ArgTypes>(Args)...))
	{
	}

	template <typename... ArgTypes>
	TProtocolResult(TInPlaceType<FCaptureProtocolError>&&, ArgTypes&&... Args)
		: Base(MakeError(Forward<ArgTypes>(Args)...))
	{
	}
};

template <>
class TProtocolResult<void> : public TValueOrError<void, FCaptureProtocolError>
{
private:

	using Base = TValueOrError<void, FCaptureProtocolError>;

public:

	TProtocolResult(TInPlaceType<void>)
		: Base(MakeValue())
	{
	}

	TProtocolResult(FCaptureProtocolError&& InErrorType)
		: Base(MakeError(MoveTemp(InErrorType)))
	{
	}

	TProtocolResult(const FCaptureProtocolError& InErrorType)
		: Base(MakeError(InErrorType))
	{
	}

	template <typename... ArgTypes>
	TProtocolResult(ArgTypes&&... Args)
		: Base(MakeError(Forward<ArgTypes>(Args)...))
	{
	}
};

const TProtocolResult<void> ResultOk = TInPlaceType<void>{};

}