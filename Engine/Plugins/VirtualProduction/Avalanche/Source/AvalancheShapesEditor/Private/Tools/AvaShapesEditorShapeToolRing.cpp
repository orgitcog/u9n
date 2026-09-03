// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolRing.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeRingDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolRing::UAvaShapesEditorShapeToolRing()
{
	ShapeClass = UAvaShapeRingDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolRing::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Ring,
		TEXT("Parametric Ring Tool"),
		5000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolRing>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeRingDynamicMesh>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(ToolParameters));
}
