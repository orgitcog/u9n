// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorToolNull.h"
#include "AvaInteractiveToolsCommands.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Framework/AvaNullActor.h"

UAvaInteractiveToolsActorToolNull::UAvaInteractiveToolsActorToolNull()
{
	ActorClass = AAvaNullActor::StaticClass();
}

void UAvaInteractiveToolsActorToolNull::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters =
	{
		FAvaInteractiveToolsCommands::Get().Tool_Actor_Null,
		TEXT("Null Actor Tool"),
		2000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaInteractiveToolsActorToolNull>(InEdMode);
			}),
		ActorClass
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryNameActor, MoveTemp(ToolParameters));
}
