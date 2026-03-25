// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

class FBaseCommandArgs;
class FCommandHandler
{
public:
	// Delegates can't accept move-only object therefore, instead of TUniquePtr we are using TSharedPtr
	DECLARE_DELEGATE_RetVal_OneParam(bool, FExecutor, TSharedPtr<FBaseCommandArgs>);

	FCommandHandler() = default;
	virtual ~FCommandHandler() = default;

	bool Execute(TSharedPtr<FBaseCommandArgs> InCommandArgs);

	TArray<FString> GetCommands() const;
protected:

	void RegisterCommand(const FString& InCommandName,
						 FExecutor InExecutor);

private:

	TMap<FString, FExecutor> Executors;
};