// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolEllipse.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeEllipseDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolEllipse::UAvaShapesEditorShapeToolEllipse()
{
	ShapeClass = UAvaShapeEllipseDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolEllipse::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Ellipse,
		TEXT("Parametric Ellipse Tool"),
		2000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolEllipse>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeEllipseDynamicMesh>()
	};
	
	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(ToolParameters));
}
