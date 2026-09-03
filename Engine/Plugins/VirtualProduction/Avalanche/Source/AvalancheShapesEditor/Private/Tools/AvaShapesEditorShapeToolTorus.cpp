// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolTorus.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeTorusDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolTorus::UAvaShapesEditorShapeToolTorus()
{
	ShapeClass = UAvaShapeTorusDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolTorus::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Torus,
		TEXT("Parametric Torus Tool"),
		4000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolTorus>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeTorusDynamicMesh>()
	};
	
	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName3D, MoveTemp(ToolParameters));
}
