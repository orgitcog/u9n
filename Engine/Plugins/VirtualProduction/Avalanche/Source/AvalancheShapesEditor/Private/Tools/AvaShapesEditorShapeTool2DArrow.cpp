// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeTool2DArrow.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShape2DArrowDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeTool2DArrow::UAvaShapesEditorShapeTool2DArrow()
{
	ShapeClass = UAvaShape2DArrowDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeTool2DArrow::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_2DArrow,
		TEXT("Parametric 2D Arrow Tool"),
		7000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeTool2DArrow>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShape2DArrowDynamicMesh>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(ToolParameters));
}
