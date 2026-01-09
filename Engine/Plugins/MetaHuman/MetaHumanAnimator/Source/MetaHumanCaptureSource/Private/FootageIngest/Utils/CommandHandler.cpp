// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommandHandler.h"
#include "Commands/BaseCommand.h"

bool FCommandHandler::Execute(TSharedPtr<FBaseCommandArgs> InCommandArgs)
{
	if (FExecutor* Executor = Executors.Find(InCommandArgs->GetCommandName()); Executor)
	{
		if (Executor->IsBound())
		{
			return Executor->Execute(MoveTemp(InCommandArgs));
		}
	}

	return false;
}

TArray<FString> FCommandHandler::GetCommands() const
{
	TArray<FString> Commands;
	Executors.GetKeys(Commands);

	return Commands;
}

void FCommandHandler::RegisterCommand(const FString& InCommandName, FExecutor InExecutor)
{
	if (!Executors.Contains(InCommandName))
	{
		Executors.Emplace(InCommandName, MoveTemp(InExecutor));
	}
}