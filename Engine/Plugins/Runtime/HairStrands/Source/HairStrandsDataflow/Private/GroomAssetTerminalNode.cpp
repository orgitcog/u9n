// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomAssetTerminalNode.h"

#include "AssetCompilingManager.h"
#include "GroomCollectionFacades.h"
#include "GroomEdit.h"
#include "Dataflow/DataflowConnectionTypes.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomAssetTerminalNode)

namespace UE::Groom::Private
{
	FORCEINLINE void BuildEditableGuides(const UE::Groom::FGroomGuidesFacade& GuidesFacade, FEditableGroom& EditGroom)
	{
		int32 ObjectIndex = 0, CurveIndex = 0;
		int32 PrevCurve = 0, PrevPoint = 0;;
		for (FEditableGroomGroup& Group : EditGroom.Groups)
		{
			const int32 NextCurve = GuidesFacade.GetObjectCurveOffsets()[ObjectIndex];
			Group.Guides.SetNum(NextCurve-PrevCurve);
			
			// Only rebuild the guides since the strands are the same
			for (FEditableHairGuide& Guide : Group.Guides)
			{
				const int32 NextPoint = GuidesFacade.GetCurvePointOffsets()[CurveIndex];
				const int32 StrandIndex = GuidesFacade.GetCurveStrandIndices()[CurveIndex];
						
				Guide.ControlPoints.Reset();
				for(int32 PointIndex = PrevPoint; PointIndex < NextPoint; ++PointIndex)
				{
					Guide.ControlPoints.Add({GuidesFacade.GetPointRestPositions()[PointIndex],
						FMath::Clamp(static_cast<float>(PointIndex-PrevPoint) / (NextPoint-PrevPoint-1), 0.f, 1.f)});
				}
						
				if((StrandIndex != INDEX_NONE) && (StrandIndex < Group.Strands.Num()))
				{
					Guide.GuideID = CurveIndex;
					Guide.RootUV = Group.Strands[StrandIndex].RootUV;
				}
				
				PrevPoint = NextPoint;
				++CurveIndex;
			}
			PrevCurve = NextCurve;
			++ObjectIndex;
		}
	}

	FORCEINLINE void CopyCollectionAttributes(const FManagedArrayCollection* InputCollection, FManagedArrayCollection* OutputCollection,
		const TArray<FCollectionAttributeKey>& AttributesToCopy = TArray<FCollectionAttributeKey>())
	{
		for(const FCollectionAttributeKey& AttributeToCopy : AttributesToCopy)
		{
			const FName AttributeName(AttributeToCopy.Attribute);
			const FName GroupName(AttributeToCopy.Group);

			if (InputCollection->HasGroup(GroupName))
			{
				if(!OutputCollection->HasGroup(GroupName))
				{
					OutputCollection->AddGroup(GroupName);
				}
				if (InputCollection->NumElements(GroupName) != OutputCollection->NumElements(GroupName))
				{
					OutputCollection->EmptyGroup(GroupName);
					OutputCollection->AddElements(InputCollection->NumElements(GroupName), GroupName);
				}
				OutputCollection->CopyAttribute(*InputCollection, AttributeName, GroupName);
			}
		}
	}

	template<typename FacadeType, typename AttributeType>
	static void BuildVerticesAttribute(const FManagedArrayCollection& InCollection, FManagedArrayCollection* OutCollection, const int32 NumPoints, const FName& AttributeName)
	{
		const TManagedArray<AttributeType>& VerticesAttribute = InCollection.GetAttribute<AttributeType>(AttributeName, FacadeType::VerticesGroup);
		if(OutCollection->NumElements(FacadeType::PointsGroup) != NumPoints)
		{
			if(OutCollection->NumElements(FacadeType::PointsGroup) > 0)
			{
				OutCollection->EmptyGroup(FacadeType::PointsGroup);
			}
			OutCollection->AddElements(NumPoints, FacadeType::PointsGroup);
		}
		TManagedArray<AttributeType>& PointsAttribute = OutCollection->AddAttribute<AttributeType>(AttributeName, FacadeType::PointsGroup);
		
		for(int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			PointsAttribute[PointIndex] = VerticesAttribute[2*PointIndex];
		}
	}

	template<typename FacadeType>
	static void TransferVerticesAttributes(const FManagedArrayCollection& InCollection, FManagedArrayCollection* OutCollection,
		const int32 NumPoints, const TArray<FName>& AttributesToSkip)
	{
		// Transfer vertices weight maps onto the points to be stored onto the rest collection
		const TArray<FName> AttributeNames = InCollection.AttributeNames(FacadeType::VerticesGroup);
		for(const FName& AttributeName : AttributeNames)
		{
			if(!AttributesToSkip.Contains(AttributeName))
			{
				if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FFloatType)
				{
					BuildVerticesAttribute<FacadeType, float>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FVector4fType)
				{
					BuildVerticesAttribute<FacadeType, FVector4f>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FVectorType)
				{
					BuildVerticesAttribute<FacadeType, FVector3f>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FVector2DType)
				{
					BuildVerticesAttribute<FacadeType, FVector2f>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FInt32Type)
				{
					BuildVerticesAttribute<FacadeType, int32>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FIntVector4Type)
				{
					BuildVerticesAttribute<FacadeType, FIntVector4>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FIntVectorType)
				{
					BuildVerticesAttribute<FacadeType, FIntVector3>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FIntVector2Type)
				{
					BuildVerticesAttribute<FacadeType, FIntVector2>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FBoolType)
				{
					BuildVerticesAttribute<FacadeType, bool>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FLinearColorType)
				{
					BuildVerticesAttribute<FacadeType, FLinearColor>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FQuatType)
				{
					BuildVerticesAttribute<FacadeType, FQuat4f>(InCollection, OutCollection, NumPoints, AttributeName);
				}
				else if(InCollection.GetAttributeType(AttributeName, FacadeType::VerticesGroup) == EManagedArrayType::FTransform3fType)
				{
					BuildVerticesAttribute<FacadeType, FTransform3f>(InCollection, OutCollection, NumPoints, AttributeName); 
				}
			}
		}
	}

	template<typename FacadeType>
	FORCEINLINE void RegisterSkeletalMeshes(const FManagedArrayCollection& InCollection, const FacadeType& GroomFacade, UGroomAsset* GroomAsset)
	{
		const TManagedArray<TObjectPtr<UObject>>& ObjectSkeletalMeshes =
						InCollection.GetAttribute<TObjectPtr<UObject>>(UE::Groom::FGroomGuidesFacade::ObjectSkeletalMeshesAttribute, FacadeType::ObjectsGroup);

		const TManagedArray<int32>& ObjectMeshLODs =
			InCollection.GetAttribute<int32>(UE::Groom::FGroomGuidesFacade::ObjectMeshLODsAttribute, FacadeType::ObjectsGroup);
				
		for(int32 GroupIndex = 0; GroupIndex < GroomFacade.GetNumObjects(); ++GroupIndex)
		{
			GroomAsset->GetDataflowSettings().SetSkeletalMesh(GroupIndex,
				Cast<USkeletalMesh>(ObjectSkeletalMeshes[GroupIndex]), ObjectMeshLODs[GroupIndex]);
		}
	}
	template<typename FacadeType>
	FORCEINLINE void TransferCollectionAttributes(const FManagedArrayCollection& InCollection, FManagedArrayCollection* OutCollection, const FacadeType& GroomFacade,
		const TArray<FCollectionAttributeKey>& ExternalAttributes, const TArray<FCollectionAttributeKey>& InternalAttributes)
	{
		// Register all the internal attributes to be copied
		TArray<FCollectionAttributeKey> AttributesToCopy;

		auto AddAttributeKeys = [&AttributesToCopy](const TArray<FCollectionAttributeKey>& AttributeKeys)
		{
			for (const FCollectionAttributeKey& AttributeKey : AttributeKeys)
			{
				if (AttributeKey.Group == FacadeType::CurvesGroup || AttributeKey.Group == FacadeType::ObjectsGroup ||
					AttributeKey.Group == FacadeType::PointsGroup || AttributeKey.Group == FacadeType::EdgesGroup)
				{
					AttributesToCopy.Add(AttributeKey);
				}
			}
		};

		AddAttributeKeys(ExternalAttributes);
		AddAttributeKeys(InternalAttributes);

		// Copy attributes from input collection
		UE::Groom::Private::CopyCollectionAttributes(&InCollection, OutCollection, AttributesToCopy);
				
		// Skip default vertex attributes as not defined by the user
		const TArray<FName> AttributesToSkip = {FacadeType::VertexLinearColorsAttribute};
				
		// Transfer vertices weight maps onto the points to be stored onto the rest collection
		UE::Groom::Private::TransferVerticesAttributes<FacadeType>(InCollection, OutCollection, GroomFacade.GetNumPoints(), AttributesToSkip);
	}
}

void FGroomAssetTerminalDataflowNode::SetAssetValue(TObjectPtr<UObject> Asset, UE::Dataflow::FContext& Context) const
{
	if (UGroomAsset* GroomAsset = Cast<UGroomAsset>(Asset.Get()))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		UE::Groom::FGroomGuidesFacade GuidesFacade(InCollection);
		UE::Groom::FGroomStrandsFacade StrandsFacade(InCollection);

		FManagedArrayCollection* OutCollection = (GuidesFacade.IsValid() || StrandsFacade.IsValid()) ? new FManagedArrayCollection() : nullptr;

		if(StrandsFacade.IsValid())
		{
			UE::Groom::Private::TransferCollectionAttributes<UE::Groom::FGroomStrandsFacade>(InCollection, OutCollection, StrandsFacade, AttributeKeys, {});
		}
		if(GuidesFacade.IsValid())
		{
			UE::Groom::Private::TransferCollectionAttributes<UE::Groom::FGroomGuidesFacade>(InCollection, OutCollection, GuidesFacade, AttributeKeys,
				{{UE::Groom::FGroomGuidesFacade::CurveParentIndicesAttribute.ToString(), UE::Groom::FGroomGuidesFacade::CurvesGroup.ToString()},
				{UE::Groom::FGroomGuidesFacade::CurveLodIndicesAttribute.ToString(), UE::Groom::FGroomGuidesFacade::CurvesGroup.ToString()}});

			// Build an editable groom asset for the strands
			FEditableGroom EditGroom;
			ConvertFromGroomAsset(const_cast<UGroomAsset*>(GroomAsset), &EditGroom, false, false, false);

			if(GuidesFacade.GetNumObjects() == EditGroom.Groups.Num())
			{
				// Build the editable guides
				UE::Groom::Private::BuildEditableGuides(GuidesFacade,EditGroom);

				// Ensure compilation dependent assets is done
				FAssetCompilingManager::Get().FinishCompilationForObjects({ GroomAsset });

				// Convert to groom asset
				ConvertToGroomAsset(const_cast<UGroomAsset*>(GroomAsset), &EditGroom, EEditableGroomOperations::ControlPoints_Modified);
			}
			// To prevent future reconstruction in the BuildData we set the type to be imported
			for(FHairGroupsInterpolation& GroupInterpolation : GroomAsset->GetHairGroupsInterpolation())
			{
				GroupInterpolation.InterpolationSettings.GuideType = EGroomGuideType::Imported;
			}
		}  

		if(OutCollection)
		{
			GroomAsset->GetDataflowSettings().SetRestCollection(OutCollection);
			GroomAsset->GetDataflowSettings().InitSkeletalMeshes(GuidesFacade.GetNumObjects());

			if(InCollection.HasAttribute(UE::Groom::FGroomGuidesFacade::ObjectSkeletalMeshesAttribute, UE::Groom::FGroomGuidesFacade::ObjectsGroup))
			{
				UE::Groom::Private::RegisterSkeletalMeshes(InCollection, GuidesFacade, GroomAsset);
			}
			else if(InCollection.HasAttribute(UE::Groom::FGroomGuidesFacade::ObjectSkeletalMeshesAttribute, UE::Groom::FGroomStrandsFacade::ObjectsGroup))
			{
				UE::Groom::Private::RegisterSkeletalMeshes(InCollection, StrandsFacade, GroomAsset);
			}
		}
	}
}

void FGroomAssetTerminalDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	SetValue(Context, InCollection, &Collection);
}

UE::Dataflow::TConnectionReference<FCollectionAttributeKey> FGroomAssetTerminalDataflowNode::GetConnectionReference(int32 Index) const
{
	return { &AttributeKeys[Index], Index, &AttributeKeys };
}

TArray<UE::Dataflow::FPin> FGroomAssetTerminalDataflowNode::AddPins()
{
	const int32 Index = AttributeKeys.AddDefaulted();
	const FDataflowInput& Input = RegisterInputArrayConnection(GetConnectionReference(Index));
	return { { UE::Dataflow::FPin::EDirection::INPUT, Input.GetType(), Input.GetName() } };
}

TArray<UE::Dataflow::FPin> FGroomAssetTerminalDataflowNode::GetPinsToRemove() const
{
	const int32 Index = AttributeKeys.Num() - 1;
	check(AttributeKeys.IsValidIndex(Index));
	if (const FDataflowInput* const Input = FindInput(GetConnectionReference(Index)))
	{
		return { { UE::Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() } };
	}
	return Super::GetPinsToRemove();
}

void FGroomAssetTerminalDataflowNode::OnPinRemoved(const UE::Dataflow::FPin& Pin)
{
	const int32 Index = AttributeKeys.Num() - 1;
	check(AttributeKeys.IsValidIndex(Index));
#if DO_CHECK
	const FDataflowInput* const Input = FindInput(GetConnectionReference(Index));
	check(Input);
	check(Input->GetName() == Pin.Name);
	check(Input->GetType() == Pin.Type);
#endif
	AttributeKeys.SetNum(Index);

	return Super::OnPinRemoved(Pin);
}

void FGroomAssetTerminalDataflowNode::PostSerialize(const FArchive& Ar)
{
	// because we add pins we need to make sure we restore them when loading
	// to make sure they can get properly reconnected

	if (Ar.IsLoading())
	{
		check(AttributeKeys.Num() >= 0);
		// register new elements from the array as inputs
		for (int32 Index = 0; Index < AttributeKeys.Num(); ++Index)
		{
			FindOrRegisterInputArrayConnection(GetConnectionReference(Index));
		}
		if (Ar.IsTransacting())
		{
			// if we have more inputs than materials then we need to unregister the inputs 
			const int32 NumAttributeInputs = (GetNumInputs() - NumOtherInputs);
			const int32 NumInputs = AttributeKeys.Num();
			if (NumAttributeInputs > NumInputs)
			{
				// Inputs have been removed.
				// Temporarily expand Collections so we can get connection references.
				AttributeKeys.SetNum(NumAttributeInputs);
				for (int32 Index = NumInputs; Index < AttributeKeys.Num(); ++Index)
				{
					UnregisterInputConnection(GetConnectionReference(Index));
				}
				AttributeKeys.SetNum(NumInputs);
			}
		}
		else
		{
			ensureAlways(AttributeKeys.Num() + NumOtherInputs == GetNumInputs());
		}
	}
}




