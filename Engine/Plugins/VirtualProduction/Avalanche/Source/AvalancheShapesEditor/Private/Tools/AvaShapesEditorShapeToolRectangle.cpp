// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolRectangle.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeRectangleDynMesh.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "AvaShapesEditorShapeToolRectangle"

UAvaShapesEditorShapeToolRectangle::UAvaShapesEditorShapeToolRectangle()
{
	ShapeClass = UAvaShapeRectangleDynamicMesh::StaticClass();
}

void UAvaShapesEditorShapeToolRectangle::OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule)
{
	Super::OnRegisterTool(InAITModule);

	FShapeFactoryParameters RectangleFactoryParameters =
	{
		.Size = FVector(0, 160, 90),
		.Functor = [](UAvaShapeDynamicMeshBase* InMesh)
		{
			InMesh->SetMaterialUVMode(UAvaShapeDynamicMeshBase::MESH_INDEX_PRIMARY, EAvaShapeUVMode::Stretch);
		}
	};

	FAvaInteractiveToolsToolParameters RectangleToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Rectangle,
		TEXT("Parametric Rectangle Tool"),
		1000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolRectangle>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeRectangleDynamicMesh>(RectangleFactoryParameters)
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(RectangleToolParameters));

	FShapeFactoryParameters SquareFactoryParameters =
	{
		.NameOverride = FString("Square")
	};

	FAvaInteractiveToolsToolParameters SquareToolParameters = 
	{
		FAvaShapesEditorCommands::Get().Tool_Shape_Square,
		TEXT("Parametric Square Tool"),
		1001,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolRectangle>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeRectangleDynamicMesh>(SquareFactoryParameters)
	};

	InAITModule->RegisterTool(IAvalancheInteractiveToolsModule::CategoryName2D, MoveTemp(SquareToolParameters));
}

#undef LOCTEXT_NAMESPACE
