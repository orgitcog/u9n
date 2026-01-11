// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tool/AvaTextActorTool.h"
#include "AvaInteractiveToolsSettings.h"
#include "AvaTextActorFactory.h"
#include "AvaTextEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "Text3DActor.h"

UAvaTextActorTool::UAvaTextActorTool()
{
	ActorClass = AText3DActor::StaticClass();
}

void UAvaTextActorTool::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters =
	{
		FAvaTextEditorCommands::Get().Tool_Actor_Text3D,
		TEXT("Text Actor Tool"),
		1000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaTextActorTool>(InEdMode);
			}),
		ActorClass,
		CreateActorFactory<UAvaTextActorFactory>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryNameActor, MoveTemp(ToolParameters));
}
