// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorTool.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Builders/AvaInteractiveToolsActorToolBuilder.h"

UAvaInteractiveToolsActorTool::UAvaInteractiveToolsActorTool()
{
}

void UAvaInteractiveToolsActorTool::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = UAvaInteractiveToolsActorToolBuilder::CreateToolParameters(
	    Category,
	    Command,
	    Identifier,
	    Priority,
	    ActorClass,
	    GetClass()
    );
	
	InAITModule->RegisterTool(Category, MoveTemp(ToolParameters));
}