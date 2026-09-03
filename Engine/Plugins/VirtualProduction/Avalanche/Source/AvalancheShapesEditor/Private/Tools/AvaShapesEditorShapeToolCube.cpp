// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolCube.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeCubeDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolCube::UAvaShapesEditorShapeToolCube()
{
	ShapeClass = UAvaShapeCubeDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolCube::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Cube,
		TEXT("Parametric Cube Tool"),
		1000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolCube>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeCubeDynamicMesh>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName3D, MoveTemp(ToolParameters));
}
