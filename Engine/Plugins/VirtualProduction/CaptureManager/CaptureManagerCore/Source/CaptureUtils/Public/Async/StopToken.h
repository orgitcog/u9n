// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace UE::CaptureManager
{

struct FSharedState;

class CAPTUREUTILS_API FStopToken
{
public:

	FStopToken() = default;
	FStopToken(const FStopToken& InOther) = default;
	FStopToken(FStopToken&& InOther) = default;
	FStopToken& operator=(const FStopToken& InOther) = default;
	FStopToken& operator=(FStopToken&& InOther) = default;

	bool IsStopRequested() const;

private:

	friend class FStopRequester;

	FStopToken(TWeakPtr<const FSharedState> InSharedState);

	TWeakPtr<const FSharedState> SharedStateWeak;
};

class CAPTUREUTILS_API FStopRequester
{
public:
	FStopRequester();

	FStopRequester(const FStopRequester& InOther) = default;
	FStopRequester(FStopRequester&& InOther) = default;
	FStopRequester& operator=(const FStopRequester& InOther) = default;
	FStopRequester& operator=(FStopRequester&& InOther) = default;

	void RequestStop();
	bool IsStopRequested() const;

	FStopToken CreateToken() const;

private:

	TSharedPtr<FSharedState> SharedState;
};

}
