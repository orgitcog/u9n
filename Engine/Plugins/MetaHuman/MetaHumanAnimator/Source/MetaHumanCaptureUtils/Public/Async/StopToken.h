// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class METAHUMANCAPTUREUTILS_API FStopToken
{
public:
	FStopToken();

	FStopToken(const FStopToken& InOther) = default;
	FStopToken(FStopToken&& InOther) = default;
	FStopToken& operator=(const FStopToken& InOther) = default;
	FStopToken& operator=(FStopToken&& InOther) = default;

	void RequestStop();
	bool IsStopRequested() const;

private:

	struct FSharedState
	{
		std::atomic_bool State = false;
	};

	TSharedPtr<FSharedState> SharedState;
};