// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorToolSpline.h"
#include "AvaInteractiveToolsCommands.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Framework/AvaSplineActor.h"

UAvaInteractiveToolsActorToolSpline::UAvaInteractiveToolsActorToolSpline()
{
	ActorClass = AAvaSplineActor::StaticClass();
}

void UAvaInteractiveToolsActorToolSpline::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters =
	{
		FAvaInteractiveToolsCommands::Get().Tool_Actor_Spline,
		TEXT("Spline Actor Tool"),
		5000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaInteractiveToolsActorToolSpline>(InEdMode);
			}),
		ActorClass
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryNameActor, MoveTemp(ToolParameters));
}
