// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataflowEditorTools/DataflowEditorSkinWeightsPaintTool.h"

#include "ContextObjectStore.h"
#include "Dataflow/DataflowCollectionEditSkinWeightsNode.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowSNode.h"
#include "Engine/World.h"
#include "InteractiveToolManager.h"
#include "ModelingToolTargetUtil.h"
#include "Components/SkeletalMeshComponent.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditorSkinWeightsPaintTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UDataflowEditorSkinWeightsPaintTool"

void UDataflowEditorSkinWeightsPaintToolBuilder::GetSupportedConstructionViewModes(const UDataflowContextObject& ContextObject, TArray<const UE::Dataflow::IDataflowConstructionViewMode*>& Modes) const
{}

bool UDataflowEditorSkinWeightsPaintToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	auto HasManagedArrayCollection = [](const FDataflowNode* InDataflowNode, const TSharedPtr<UE::Dataflow::FEngineContext> Context)
	{
		if (InDataflowNode && Context)
		{
			for (const FDataflowOutput* const Output : InDataflowNode->GetOutputs())
			{
				if (Output->GetType() == FName("FManagedArrayCollection"))
				{
					return true;
				}
			}
		}

		return false;
	};

	if (USkinWeightsPaintToolBuilder::CanBuildTool(SceneState))
	{
		if (SceneState.SelectedComponents.Num() == 1 && SceneState.SelectedComponents[0]->IsA<USkeletalMeshComponent>())
		{
			if (UDataflowBaseContent* ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UDataflowBaseContent>())
			{
				if (const TSharedPtr<UE::Dataflow::FEngineContext> EvaluationContext = ContextObject->GetDataflowContext())
				{
					if (const FDataflowNode* PrimarySelection = ContextObject->GetSelectedNodeOfType<FDataflowCollectionEditSkinWeightsNode>())
					{
						return HasManagedArrayCollection(PrimarySelection, EvaluationContext);
					}
				}
			}
		}
	}
	return false;
}

const FToolTargetTypeRequirements& UDataflowEditorSkinWeightsPaintToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMeshDescriptionProvider::StaticClass(),
		UMeshDescriptionCommitter::StaticClass(),
		});
	return TypeRequirements;
}

UMeshSurfacePointTool* UDataflowEditorSkinWeightsPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UDataflowEditorSkinWeightsPaintTool* PaintTool = NewObject<UDataflowEditorSkinWeightsPaintTool>(SceneState.ToolManager);

	if (UDataflowBaseContent* ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UDataflowBaseContent>())
	{
		if (FDataflowNode* PrimarySelection = ContextObject->GetSelectedNodeOfType<FDataflowCollectionEditSkinWeightsNode>())
		{
			FDataflowCollectionEditSkinWeightsNode* SkinWeightNode =
				StaticCast<FDataflowCollectionEditSkinWeightsNode*>(PrimarySelection);
			PaintTool->SkinWeightNode = SkinWeightNode;

			SkinWeightNode->OnBoneSelectionChanged.AddLambda(
				[PaintTool](const TArray<FName>& BoneNames)
			{
				PaintTool->GetNotifier().HandleNotification(BoneNames, ESkeletalMeshNotifyType::BonesSelected);
			});

			PaintTool->SetDataflowEditorContextObject(ContextObject);
		}
	}
	return PaintTool;
}

FMeshDescription* UDataflowEditorSkinWeightsPaintTool::GetCurrentDescription(const int32 LODIndex) const 
{
	if(const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target))
	{
		if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent()))
		{
			if(USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset())
			{
				return SkeletalMesh->GetMeshDescription(LODIndex);
			}
		}
	}
	return nullptr;
}


int32 UDataflowEditorSkinWeightsPaintTool::GetVertexOffset() const
{
	if(const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target))
	{
		if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent()))
		{
			if(USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset())
			{
				return SkinWeightNode->GetSkeletalMeshOffset(SkeletalMesh);
			}
		}
	}
	return INDEX_NONE;
}

bool UDataflowEditorSkinWeightsPaintTool::ExtractSkinWeights(TArray<TArray<int32>>& CurrentIndices, TArray<TArray<float>>& CurrentWeights)
{
	// Setup DynamicMeshToWeight conversion and get Input weight map (if it exists)
	if(DataflowEditorContextObject && SkinWeightNode)
	{
		// Find the map if it exists.
		if (TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = DataflowEditorContextObject->GetDataflowContext())
		{
			if(DataflowEditorContextObject->GetSelectedCollection().IsValid())
			{
				// Fill the attribute values
				SkinWeightNode->FillAttributeWeights(*DataflowEditorContextObject->GetSelectedCollection().Get(),
					SkinWeightNode->GetBoneIndicesKey(*DataflowContext), SkinWeightNode->GetBoneWeightsKey(*DataflowContext), 
					SetupIndices, SetupWeights);
						
				CurrentWeights.SetNumZeroed(SetupWeights.Num()); 
				CurrentIndices.SetNumZeroed(SetupIndices.Num());
						
				SkinWeightNode->ExtractVertexWeights(SetupIndices, SetupWeights,
					TArrayView<TArray<int32>>(CurrentIndices), TArrayView<TArray<float>>(CurrentWeights));

				return true;
			}
		}
	}
	return false;
}

void UDataflowEditorSkinWeightsPaintTool::Setup()
{
	const int32 VertexOffset = GetVertexOffset();
	if(VertexOffset != INDEX_NONE)
	{
		if(FMeshDescription* MeshDescription = GetCurrentDescription(0))
		{
			const int32 NumVertices = MeshDescription->Vertices().Num();
			
			TArray<TArray<float>> CurrentWeights;
			TArray<TArray<int32>> CurrentIndices;
	
			if(ExtractSkinWeights(CurrentIndices, CurrentWeights))
			{
				FSkeletalMeshAttributes MeshAttribs(*MeshDescription);
				FSkinWeightsVertexAttributesRef SkinWeights =
					MeshAttribs.GetVertexSkinWeights(FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName);
			
				UE::AnimationCore::FBoneWeightsSettings WeightsSettings;
				WeightsSettings.SetNormalizeType(UE::AnimationCore::EBoneWeightNormalizeType::Always);

				for(int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
				{
					TArray<UE::AnimationCore::FBoneWeight> BoneWeights;
					const int32 CurrentIndex = VertexOffset+VertexIndex;
				
					if(CurrentIndex < CurrentWeights.Num() && CurrentIndex < CurrentIndices.Num() &&
						CurrentWeights[CurrentIndex].Num() == CurrentIndices[CurrentIndex].Num())
					{
						BoneWeights.Reserve(CurrentWeights[CurrentIndex].Num());
						for(int32 WeightIndex = 0, NumWeights = CurrentWeights[CurrentIndex].Num(); WeightIndex < NumWeights; ++WeightIndex)
						{
							BoneWeights.Add(UE::AnimationCore::FBoneWeight(CurrentIndices[CurrentIndex][WeightIndex],
								CurrentWeights[CurrentIndex][WeightIndex]));
						}
					}
					SkinWeights.Set(FVertexID(VertexIndex), UE::AnimationCore::FBoneWeights::Create(BoneWeights, WeightsSettings));
				}
			}
		}
	}
	USkinWeightsPaintTool::Setup();
}

void UDataflowEditorSkinWeightsPaintTool::OnShutdown(EToolShutdownType ShutdownType)
{
	USkinWeightsPaintTool::OnShutdown(ShutdownType);

	if(Target && ShutdownType == EToolShutdownType::Accept)
	{
		const int32 VertexOffset = GetVertexOffset();
		if(VertexOffset != INDEX_NONE)
		{
			TArray<TArray<int32>> CurrentIndices;
			TArray<TArray<float>> CurrentWeights;

			CurrentWeights.SetNumZeroed(SetupWeights.Num()); 
			CurrentIndices.SetNumZeroed(SetupIndices.Num());
						
			SkinWeightNode->ExtractVertexWeights(SetupIndices, SetupWeights,
				TArrayView<TArray<int32>>(CurrentIndices), TArrayView<TArray<float>>(CurrentWeights));
			
			if(FCleanedEditMesh* EditedMesh = EditedMeshes.Find(EMeshLODIdentifier::LOD0))
			{
				const FMeshDescription& MeshDescription = EditedMesh->GetEditableMeshDescription();
			
				// profile to edit
				const FName ActiveProfile = WeightToolProperties->GetActiveSkinWeightProfile();
			
				FSkeletalMeshConstAttributes MeshAttribs(MeshDescription);
				FSkinWeightsVertexAttributesConstRef SkinWeights = MeshAttribs.GetVertexSkinWeights(ActiveProfile);
			
				const int32 NumVertices = MeshDescription.Vertices().Num();
				for(int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
				{
					FVertexBoneWeightsConst BoneWeights = SkinWeights.Get(FVertexID(VertexIndex));
					const int32 CurrentIndex = VertexOffset+VertexIndex;
			
					if(CurrentIndex < CurrentWeights.Num() && CurrentIndex < CurrentIndices.Num() &&
						CurrentWeights[CurrentIndex].Num() == CurrentIndices[CurrentIndex].Num())
					{
						CurrentIndices[CurrentIndex].Reset(BoneWeights.Num());
						CurrentWeights[CurrentIndex].Reset(BoneWeights.Num());

						TArray<TPair<int32,float>> SortedWeights;
						SortedWeights.Reserve(BoneWeights.Num());
						
						for(int32 WeightIndex = 0, NumWeights = BoneWeights.Num(); WeightIndex < NumWeights; ++WeightIndex)
						{
							SortedWeights.Add({BoneWeights[WeightIndex].GetBoneIndex(), BoneWeights[WeightIndex].GetWeight()});
						}
						Algo::Sort(SortedWeights, [](const TPair<int32,float>& A, const TPair<int32,float>& B)
						{
							return A.Value > B.Value; 
						});

						for(int32 WeightIndex = 0, NumWeights = BoneWeights.Num(); WeightIndex < NumWeights; ++WeightIndex)
						{
							CurrentIndices[CurrentIndex].Add(SortedWeights[WeightIndex].Key);
							CurrentWeights[CurrentIndex].Add(SortedWeights[WeightIndex].Value);
						}
					}
				}
			}
			SkinWeightNode->ReportVertexWeights(SetupIndices, SetupWeights, CurrentIndices, CurrentWeights);
			SkinWeightNode->Invalidate();

			// Avoid rebuilding the skeletal mesh after updating the skin weights
			SkinWeightNode->ValidateSkeletalMeshes();
		}
	}
}

#undef LOCTEXT_NAMESPACE
