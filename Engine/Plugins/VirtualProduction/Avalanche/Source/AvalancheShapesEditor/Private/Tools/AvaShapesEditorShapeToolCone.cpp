// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolCone.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeConeDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolCone::UAvaShapesEditorShapeToolCone()
{
	ShapeClass = UAvaShapeConeDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolCone::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FAvaInteractiveToolsToolParameters ConeToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Cone,
		TEXT("Parametric Cone Tool"),
		3000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolCone>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeConeDynamicMesh>()
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName3D, MoveTemp(ConeToolParameters));

	const FShapeFactoryParameters CylinderFactoryParameters =
	{
		.Functor = [](UAvaShapeDynamicMeshBase* InMesh)
		{
			UAvaShapeConeDynamicMesh* Cone = Cast<UAvaShapeConeDynamicMesh>(InMesh);
			Cone->SetTopRadius(1.f);
		},
		.NameOverride = FString("Cylinder"),
	};

	FAvaInteractiveToolsToolParameters CylinderToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Cylinder,
		TEXT("Parametric Cylinder Tool"),
		3000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolCone>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeConeDynamicMesh>(CylinderFactoryParameters)
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName3D, MoveTemp(CylinderToolParameters));
}
