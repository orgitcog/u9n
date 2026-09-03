// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AnimNextRigVMAssetCommands.h"

#define LOCTEXT_NAMESPACE "AnimNextRigVMAssetCommands"

namespace UE::AnimNext
{

FAnimNextRigVMAssetCommands::FAnimNextRigVMAssetCommands()
	: TCommands<FAnimNextRigVMAssetCommands>("UAFRigVMAssetCommand", LOCTEXT("UAFRigVMAssetCommands", "UAF Asset Commands"), NAME_None, "AnimNextStyle")
{
}

void FAnimNextRigVMAssetCommands::RegisterCommands()
{
	UI_COMMAND(FindInAnimNextRigVMAsset, "FindInUAFRigVMAsset", "Search the current UAF Asset.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F));
}

}

#undef LOCTEXT_NAMESPACE
