// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

namespace UE::AnimNext
{

// The commands for general AnimNext RVM graph editing
class FRigVMCommands : public TCommands<FRigVMCommands>
{
public:
	FRigVMCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Compile;
	TSharedPtr<FUICommandInfo> AutoCompile;
	TSharedPtr<FUICommandInfo> CompileWholeWorkspace;
	TSharedPtr<FUICommandInfo> CompileDirtyFiles;
};

}
