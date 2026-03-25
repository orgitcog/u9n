// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/BaseCommand.h"

FBaseCommandArgs::FBaseCommandArgs(const FString& InCommandName)
	: CommandName(InCommandName)
{
}

const FString& FBaseCommandArgs::GetCommandName() const
{
	return CommandName;
}