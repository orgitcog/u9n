// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolChevron.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeChevronDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolChevron::UAvaShapesEditorShapeToolChevron()
{
	ShapeClass = UAvaShapeChevronDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolChevron::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Chevron,
		TEXT("Parametric Chevron Tool"),
		8000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolChevron>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeChevronDynamicMesh>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(ToolParameters));
}
