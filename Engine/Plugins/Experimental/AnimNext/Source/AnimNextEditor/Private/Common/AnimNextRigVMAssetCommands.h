// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

namespace UE::AnimNext
{

// The Commands for the variable overrides menu
class FAnimNextRigVMAssetCommands : public TCommands<FAnimNextRigVMAssetCommands>
{
public:
	FAnimNextRigVMAssetCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> FindInAnimNextRigVMAsset;
};

}