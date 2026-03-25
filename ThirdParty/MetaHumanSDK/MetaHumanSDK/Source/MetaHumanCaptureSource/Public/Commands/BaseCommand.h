// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

class METAHUMANCAPTURESOURCE_API FBaseCommandArgs
{
public:
	FBaseCommandArgs(const FString& InName);
	virtual ~FBaseCommandArgs() = default;

	const FString& GetCommandName() const;

private:

	FString CommandName;
};
