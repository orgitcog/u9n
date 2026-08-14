// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolStar.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeStarDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolStar::UAvaShapesEditorShapeToolStar()
{
	ShapeClass = UAvaShapeStarDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolStar::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Star,
		TEXT("Parametric Star Tool"),
		6000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolStar>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeStarDynamicMesh>()
	};
	
	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(ToolParameters));
}
