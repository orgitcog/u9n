// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolSphere.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeSphereDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolSphere::UAvaShapesEditorShapeToolSphere()
{
	ShapeClass = UAvaShapeSphereDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolSphere::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Sphere,
		TEXT("Parametric Sphere Tool"),
		2000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolSphere>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeSphereDynamicMesh>()
	};
	
	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName3D, MoveTemp(ToolParameters));
}
