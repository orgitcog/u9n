// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tool/AvaSVGActorTool.h"
#include "AvaInteractiveToolsSettings.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Factories/SVGActorFactory.h"
#include "SVGActor.h"
#include "SVGImporter.h"
#include "SVGImporterEditorCommands.h"

UAvaSVGActorTool::UAvaSVGActorTool()
{
	ActorClass = ASVGActor::StaticClass();
}

void UAvaSVGActorTool::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	// Load svg module
	FSVGImporterModule::Get();

	FAvaInteractiveToolsToolParameters ToolParameters =
	{
		FSVGImporterEditorCommands::GetExternal().SpawnSVGActor,
		TEXT("SVG Actor Tool"),
		6000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaSVGActorTool>(InEdMode);
			}),
		ActorClass,
		CreateActorFactory<USVGActorFactory>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryNameActor, MoveTemp(ToolParameters));
}
