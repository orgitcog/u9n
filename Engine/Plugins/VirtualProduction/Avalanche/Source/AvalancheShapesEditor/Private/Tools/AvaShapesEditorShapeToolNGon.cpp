// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolNGon.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeNGonDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolNGon::UAvaShapesEditorShapeToolNGon()
{
	ShapeClass = UAvaShapeNGonDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolNGon::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters =
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_NGon,
		TEXT("Parametric Regular Polygon Tool"),
		3000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolNGon>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeNGonDynamicMesh>()
	};
	
	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(ToolParameters));
}
