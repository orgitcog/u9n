// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"

#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowTools.h"
#include "Misc/LazySingleton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowCollectionAddScalarVertexPropertyNode)

#define LOCTEXT_NAMESPACE "DataflowCollectionAddScalarVertexProperty"

//
// FDataflowAddScalarVertexPropertyCallbackRegistry
//

FDataflowAddScalarVertexPropertyCallbackRegistry& FDataflowAddScalarVertexPropertyCallbackRegistry::Get()
{
	return TLazySingleton<FDataflowAddScalarVertexPropertyCallbackRegistry>::Get();
}

void FDataflowAddScalarVertexPropertyCallbackRegistry::RegisterCallbacks(TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>&& Callbacks)
{
	AllCallbacks.Add(Callbacks->GetName(), MoveTemp(Callbacks));
}

void FDataflowAddScalarVertexPropertyCallbackRegistry::DeregisterCallbacks(const FName& CallbacksName)
{
	AllCallbacks.Remove(CallbacksName);
}

TArray<FName> FDataflowAddScalarVertexPropertyCallbackRegistry::GetTargetGroupNames() const
{
	TArray<FName> UniqueNames;

	for (const TPair<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>>& CallbacksEntry : AllCallbacks)
	{
		for (const FName& GroupName : CallbacksEntry.Value->GetTargetGroupNames())
		{
			UniqueNames.AddUnique(GroupName);
		}
	}
	return UniqueNames;
}

TArray<UE::Dataflow::FRenderingParameter> FDataflowAddScalarVertexPropertyCallbackRegistry::GetRenderingParameters() const
{
	TArray<UE::Dataflow::FRenderingParameter> UniqueParameters;

	for (const TPair<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>>& CallbacksEntry : AllCallbacks)
	{
		for (const UE::Dataflow::FRenderingParameter& RenderingParameter : CallbacksEntry.Value->GetRenderingParameters())
		{
			UniqueParameters.AddUnique(RenderingParameter);
		}
	}
	return UniqueParameters;
}

TArray<UE::Dataflow::FRenderingParameter> FDataflowAddScalarVertexPropertyCallbackRegistry::GetRenderingParameters(const FName& TargetGroup) const
{
	TArray<UE::Dataflow::FRenderingParameter> UniqueParameters;

	for (const TPair<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>>& CallbacksEntry : AllCallbacks)
	{
		const TArray<UE::Dataflow::FRenderingParameter> RenderingParameters = CallbacksEntry.Value->GetRenderingParameters();
		const TArray<FName> TargetGroups = CallbacksEntry.Value->GetTargetGroupNames();
		if(RenderingParameters.Num() == TargetGroups.Num())
		{
			for(int32 TargetIndex = 0, NumTargets = TargetGroups.Num(); TargetIndex < NumTargets; ++TargetIndex)
			{
				if(TargetGroups[TargetIndex] == TargetGroup)
				{
					UniqueParameters.AddUnique(RenderingParameters[TargetIndex]);
				}
			}
		}
		else if(TargetGroups.Find(TargetGroup) != INDEX_NONE)
		{
			for (const UE::Dataflow::FRenderingParameter& RenderingParameter : CallbacksEntry.Value->GetRenderingParameters())
			{
				UniqueParameters.AddUnique(RenderingParameter);
			}
		}
	}
	return UniqueParameters;
}


//
// Deprecated class
//

PRAGMA_DISABLE_DEPRECATION_WARNINGS
DataflowAddScalarVertexPropertyCallbackRegistry& DataflowAddScalarVertexPropertyCallbackRegistry::Get()
{
	return TLazySingleton<DataflowAddScalarVertexPropertyCallbackRegistry>::Get();
}
void DataflowAddScalarVertexPropertyCallbackRegistry::TearDown()
{
	TLazySingleton<DataflowAddScalarVertexPropertyCallbackRegistry>::TearDown();
}
void DataflowAddScalarVertexPropertyCallbackRegistry::RegisterCallbacks(TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>&& Callbacks)
{
	AllCallbacks.Add(Callbacks->GetName(), MoveTemp(Callbacks));
}
void DataflowAddScalarVertexPropertyCallbackRegistry::DeregisterCallbacks(const FName& CallbacksName)
{
	AllCallbacks.Remove(CallbacksName);
}
TArray<FName> DataflowAddScalarVertexPropertyCallbackRegistry::GetTargetGroupNames() const
{
	TArray<FName> UniqueNames;

	for (const TPair<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>>& CallbacksEntry : AllCallbacks)
	{
		for (const FName& GroupName : CallbacksEntry.Value->GetTargetGroupNames())
		{
			UniqueNames.AddUnique(GroupName);
		}
	}
	return UniqueNames;
}
TArray<UE::Dataflow::FRenderingParameter> DataflowAddScalarVertexPropertyCallbackRegistry::GetRenderingParameters() const
{
	TArray<UE::Dataflow::FRenderingParameter> UniqueParameters;

	for (const TPair<FName, TUniquePtr<IDataflowAddScalarVertexPropertyCallbacks>>& CallbacksEntry : AllCallbacks)
	{
		for (const UE::Dataflow::FRenderingParameter& RenderingParameter : CallbacksEntry.Value->GetRenderingParameters())
		{
			UniqueParameters.AddUnique(RenderingParameter);
		}
	}
	return UniqueParameters;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS


//
// FDataflowCollectionAddScalarVertexPropertyNode
//

FDataflowCollectionAddScalarVertexPropertyNode::FDataflowCollectionAddScalarVertexPropertyNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterInputConnection(&AttributeKey);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&AttributeKey, &AttributeKey);
}

TArray<UE::Dataflow::FRenderingParameter> FDataflowCollectionAddScalarVertexPropertyNode::GetRenderParametersImpl() const
{
	return FDataflowAddScalarVertexPropertyCallbackRegistry::Get().GetRenderingParameters(TargetGroup.Name);
}

void FDataflowCollectionAddScalarVertexPropertyNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Dataflow;

	// Get the pin value if plugged
	FCollectionAttributeKey Key = GetWeightAttributeKey(Context);

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if (!Key.Attribute.IsEmpty())
		{
			const FName InName(Key.Attribute);
			const FName InGroup(Key.Group);
			TManagedArray<float>& ScalarWeights = InCollection.AddAttribute<float>(InName, InGroup);

			if (VertexWeights.Num() > 0 && VertexWeights.Num() != ScalarWeights.Num())
			{
				FDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("VertexCountMismatchHeadline", "Vertex count mismatch."),
					FText::Format(LOCTEXT("VertexCountMismatchDetails", "Vertex weights in the node: {0}\n Vertices in group \"{1}\" in the Collection: {2}"),
						VertexWeights.Num(),
						FText::FromName(InGroup),
						ScalarWeights.Num()));
			}

			const TArray<float> SetupWeights = ScalarWeights.GetConstArray();
			ExtractVertexWeights(SetupWeights, TArrayView<float>(ScalarWeights.GetData(), ScalarWeights.Num()));
			
		}

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&AttributeKey))
	{
		SetValue(Context, MoveTemp(Key), &AttributeKey);
	}
}

bool FDataflowCollectionAddScalarVertexPropertyNode::FillAttributeWeights( 
	const TSharedPtr<const FManagedArrayCollection> SelectedCollection, const FCollectionAttributeKey& InAttributeKey, TArray<float>& OutAttributeValues) const
{
	if (!InAttributeKey.Attribute.IsEmpty())
	{
		const FName InName(InAttributeKey.Attribute);
		const FName InGroup(InAttributeKey.Group);
		
		if(const TManagedArray<float>* AttributeArray = SelectedCollection->FindAttributeTyped<float>(InName, InGroup))
		{
			OutAttributeValues = AttributeArray->GetConstArray();
			return true;
		}
	}
	OutAttributeValues.Init(0.0f, OutAttributeValues.Num());
	return false;
}

FCollectionAttributeKey FDataflowCollectionAddScalarVertexPropertyNode::GetWeightAttributeKey(UE::Dataflow::FContext& Context) const
{
	// Get the pin value if plugged
	FCollectionAttributeKey Key = GetValue(Context, &AttributeKey, AttributeKey);

	// If nothing set used the local value
	if(Key.Attribute.IsEmpty() && Key.Group.IsEmpty())
	{
		Key.Group = TargetGroup.Name.ToString();
		Key.Attribute = Name;
	}
	return Key;
}

void FDataflowCollectionAddScalarVertexPropertyNode::ReportVertexWeights(const TArray<float>& SetupWeights, const TArray<float>& FinalWeights,
		const TArray<int32>& WeightIndices)
{
	check((WeightIndices.Num() == FinalWeights.Num()) || (WeightIndices.IsEmpty() && (SetupWeights.Num() == FinalWeights.Num())));
	VertexWeights.SetNumZeroed(SetupWeights.Num());
	for (int32 WeightIndex = 0, NumWeights = FinalWeights.Num(); WeightIndex <NumWeights; ++WeightIndex)
	{
		const int32 VertexIndex = WeightIndices.IsEmpty() ? WeightIndex : WeightIndices[WeightIndex];
		switch (OverrideType)
		{
			case EDataflowWeightMapOverrideType::ReplaceAll:
				VertexWeights[VertexIndex] = FinalWeights[WeightIndex];
				break;
			case EDataflowWeightMapOverrideType::ReplaceChanged:
				VertexWeights[VertexIndex] = (SetupWeights[VertexIndex] == FinalWeights[WeightIndex]) ? ReplaceChangedPassthroughValue : FinalWeights[WeightIndex];
				break;
			case EDataflowWeightMapOverrideType::AddDifference:
				VertexWeights[VertexIndex] = FinalWeights[WeightIndex] - SetupWeights[VertexIndex];
				break;
			default: unimplemented();
		}
	}
}

void FDataflowCollectionAddScalarVertexPropertyNode::ExtractVertexWeights(const TArray<float>& SetupWeights, TArrayView<float> FinalWeights) const
{
	check(SetupWeights.Num() == FinalWeights.Num());

	int32 NumWeights = FMath::Min(VertexWeights.Num(), SetupWeights.Num());
	if(NumWeights != 0)
	{ 
		for (int32 WeightIndex = 0; WeightIndex < NumWeights; ++WeightIndex)
		{
			switch (OverrideType)
			{
				case EDataflowWeightMapOverrideType::ReplaceAll:
					FinalWeights[WeightIndex] = FMath::Clamp(VertexWeights[WeightIndex], 0.f, 1.f);
					break;
				case EDataflowWeightMapOverrideType::ReplaceChanged:
					FinalWeights[WeightIndex] = FMath::Clamp( (VertexWeights[WeightIndex] == ReplaceChangedPassthroughValue) ? SetupWeights[WeightIndex] : VertexWeights[WeightIndex], 0.0f, 1.0f);
					break;
				case EDataflowWeightMapOverrideType::AddDifference:
					FinalWeights[WeightIndex] = FMath::Clamp(SetupWeights[WeightIndex] + VertexWeights[WeightIndex], 0.f, 1.f);
					break;
				default: unimplemented();
			}
		}
	}
	else
	{
		NumWeights = SetupWeights.Num();
		for (int32 WeightIndex = 0; WeightIndex < NumWeights; ++WeightIndex)
		{
			FinalWeights[WeightIndex] = SetupWeights[WeightIndex];
		}
	}
}

#undef LOCTEXT_NAMESPACE
