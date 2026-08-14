// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConvertNodes.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowNodeColorsRegistry.h"
#include "GeometryCollection/Facades/CollectionTransformSelectionFacade.h"

#include "MathUtil.h"

namespace UE::Dataflow
{
	void RegisterDataflowConvertNodes()
	{
		// Convert
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertNumericTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertVectorTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertStringTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertBoolTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertTransformTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertStringConvertibleTypesDataflowNode);
		// Needs to be fixed
//		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertUObjectConvertibleTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertSelectionTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertVectorArrayTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertNumericArrayTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertStringArrayTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertBoolArrayTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertTransformArrayTypesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvertRotationDataflowNode);

		// Deprecated
	}
}

//-----------------------------------------------------------------------------------------------

FConvertNumericTypesDataflowNode::FConvertNumericTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertNumericTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const double InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertVectorTypesDataflowNode::FConvertVectorTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertVectorTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const FVector4 InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertStringTypesDataflowNode::FConvertStringTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertStringTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const FString InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertBoolTypesDataflowNode::FConvertBoolTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertBoolTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const bool InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertTransformTypesDataflowNode::FConvertTransformTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertTransformTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const FTransform InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertStringConvertibleTypesDataflowNode::FConvertStringConvertibleTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertStringConvertibleTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const FString InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertUObjectConvertibleTypesDataflowNode::FConvertUObjectConvertibleTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertUObjectConvertibleTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const TObjectPtr<UObject> InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertSelectionTypesDataflowNode::FConvertSelectionTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Out);
}

void FConvertSelectionTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
		const FDataflowSelection InValue = GetValue(Context, &In);

		GeometryCollection::Facades::FCollectionTransformSelectionFacade TransformSelectionFacade(InCollection);

		const FDataflowInput* SelectionInput = FindInput(&In);
		if (SelectionInput)
		{
			if (SelectionInput->IsType<FDataflowTransformSelection>())
			{
				if (Output && Output->IsType<FDataflowFaceSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertTransformSelectionToFaceSelection(InValue.AsArray());

					FDataflowFaceSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowVertexSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertTransformSelectionToVertexSelection(InValue.AsArray());

					FDataflowVertexSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowGeometrySelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertTransformSelectionToGeometrySelection(InValue.AsArray());

					FDataflowGeometrySelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
			}
			else if (SelectionInput->IsType<FDataflowFaceSelection>())
			{
				if (Output && Output->IsType<FDataflowTransformSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertFaceSelectionToTransformSelection(InValue.AsArray(), bAllElementsMustBeSelected);

					FDataflowTransformSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowVertexSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertFaceSelectionToVertexSelection(InValue.AsArray());

					FDataflowVertexSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowGeometrySelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertFaceSelectionToGeometrySelection(InValue.AsArray(), bAllElementsMustBeSelected);

					FDataflowGeometrySelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
			}
			else if (SelectionInput->IsType<FDataflowVertexSelection>())
			{
				if (Output && Output->IsType<FDataflowTransformSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertVertexSelectionToTransformSelection(InValue.AsArray(), bAllElementsMustBeSelected);

					FDataflowTransformSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowFaceSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertVertexSelectionToFaceSelection(InValue.AsArray(), bAllElementsMustBeSelected);

					FDataflowFaceSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowGeometrySelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertVertexSelectionToGeometrySelection(InValue.AsArray(), bAllElementsMustBeSelected);

					FDataflowGeometrySelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
			}
			else if (SelectionInput->IsType<FDataflowGeometrySelection>())
			{
				if (Output && Output->IsType<FDataflowTransformSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertGeometrySelectionToTransformSelection(InValue.AsArray());

					FDataflowTransformSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowFaceSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertGeometrySelectionToFaceSelection(InValue.AsArray());

					FDataflowFaceSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
				else if (Output && Output->IsType<FDataflowVertexSelection>())
				{
					const TArray<int32>& SelectionArr = TransformSelectionFacade.ConvertGeometrySelectionToVertexSelection(InValue.AsArray());

					FDataflowVertexSelection NewSelection;
					NewSelection.InitFromArray(InCollection, SelectionArr);

					SetValue(Context, MoveTemp(NewSelection), &Out);
				}
			}
		}
	}
	else if (Output->IsA(&Collection))
	{
		SafeForwardInput(Context, &Collection, &Collection);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertVectorArrayTypesDataflowNode::FConvertVectorArrayTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertVectorArrayTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const TArray<FVector4> InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertNumericArrayTypesDataflowNode::FConvertNumericArrayTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertNumericArrayTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const TArray<double> InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertStringArrayTypesDataflowNode::FConvertStringArrayTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertStringArrayTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const TArray<FString> InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertBoolArrayTypesDataflowNode::FConvertBoolArrayTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertBoolArrayTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const TArray<bool> InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertTransformArrayTypesDataflowNode::FConvertTransformArrayTypesDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertTransformArrayTypesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const TArray<FTransform> InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------

FConvertRotationDataflowNode::FConvertRotationDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&In);
	RegisterOutputConnection(&Out);
}

void FConvertRotationDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Output) const
{
	if (Output->IsA(&Out))
	{
		const FRotator InValue = GetValue(Context, &In);
		SetValue(Context, InValue, &Out);
	}
}

//-----------------------------------------------------------------------------------------------




