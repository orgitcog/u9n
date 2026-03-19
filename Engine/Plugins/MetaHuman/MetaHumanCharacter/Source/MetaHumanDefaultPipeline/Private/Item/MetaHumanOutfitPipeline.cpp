// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanOutfitPipeline.h"

#include "MetaHumanDefaultPipelineLog.h"
#include "MetaHumanItemEditorPipeline.h"
#include "MetaHumanDefaultPipelineBase.h"

#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"

#include "Logging/StructuredLog.h"

UMetaHumanOutfitPipeline::UMetaHumanOutfitPipeline()
{
	// Initialize the specification
	{
		Specification = CreateDefaultSubobject<UMetaHumanCharacterPipelineSpecification>("Specification");

		Specification->BuildOutputStruct = FMetaHumanOutfitPipelineBuildOutput::StaticStruct();
		Specification->AssemblyInputStruct = FMetaHumanOutfitPipelineAssemblyInput::StaticStruct();
		Specification->AssemblyOutputStruct = FMetaHumanOutfitPipelineAssemblyOutput::StaticStruct();
	}
}

#if WITH_EDITOR
void UMetaHumanOutfitPipeline::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSubclassOf<UMetaHumanItemEditorPipeline> EditorPipelineClass = GetEditorPipelineClass();
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanItemEditorPipeline>(this, EditorPipelineClass);
	}
}

const UMetaHumanItemEditorPipeline* UMetaHumanOutfitPipeline::GetEditorPipeline() const
{
	// If there's no editor pipeline instance, we can use the Class Default Object, because 
	// pipelines are stateless and won't be modified when used.
	//
	// This is unfortunately a slow path, as it involves looking the class up by name. We could
	// cache this if it becomes a performance issue.
	if (!EditorPipeline)
	{
		const TSubclassOf<UMetaHumanItemEditorPipeline> EditorPipelineClass = GetEditorPipelineClass();
		if (EditorPipelineClass)
		{
			return EditorPipelineClass.GetDefaultObject();
		}
	}

	return EditorPipeline;
}

TSubclassOf<UMetaHumanItemEditorPipeline> UMetaHumanOutfitPipeline::GetEditorPipelineClass() const
{
	const TSoftClassPtr<UMetaHumanItemEditorPipeline> SoftEditorPipelineClass(FSoftObjectPath(TEXT("/Script/MetaHumanDefaultEditorPipeline.MetaHumanOutfitEditorPipeline")));
	
	return SoftEditorPipelineClass.Get();
}
#endif

void UMetaHumanOutfitPipeline::AssembleItem(
	const FMetaHumanPaletteItemPath& BaseItemPath,
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
	const FMetaHumanPaletteBuiltData& ItemBuiltData,
	const FInstancedStruct& AssemblyInput,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnAssemblyComplete& OnComplete) const
{
	const FInstancedStruct& BuildOutput = ItemBuiltData.ItemBuiltData[BaseItemPath].BuildOutput;

	if (!BuildOutput.GetPtr<FMetaHumanOutfitPipelineBuildOutput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "Build output not provided to Outfit pipeline during assembly");
		
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	if (!AssemblyInput.GetPtr<FMetaHumanOutfitPipelineAssemblyInput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "Assembly input not provided to Outfit pipeline during assembly");
		
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	const FMetaHumanOutfitPipelineAssemblyInput& OutfitAssemblyInput = AssemblyInput.Get<FMetaHumanOutfitPipelineAssemblyInput>();
	const FMetaHumanOutfitGeneratedAssets* SelectedCharacterOutfit = BuildOutput.Get<FMetaHumanOutfitPipelineBuildOutput>().CharacterAssets.Find(OutfitAssemblyInput.SelectedCharacter);

	if (!SelectedCharacterOutfit)
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "Selected character {Character} not found in Outfit pipeline build output", OutfitAssemblyInput.SelectedCharacter.ToDebugString());
		
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	FMetaHumanAssemblyOutput AssemblyOutput;
	FMetaHumanOutfitPipelineAssemblyOutput& OutfitAssemblyOutput = AssemblyOutput.PipelineAssemblyOutput.InitializeAs<FMetaHumanOutfitPipelineAssemblyOutput>();
	OutfitAssemblyOutput.Outfit = SelectedCharacterOutfit->Outfit;
	OutfitAssemblyOutput.OutfitMesh = SelectedCharacterOutfit->OutfitMesh;
	OutfitAssemblyOutput.BodyHiddenFaceMap = SelectedCharacterOutfit->BodyHiddenFaceMap;

	FMetaHumanInstanceParameterOutput InstanceParameterOutput;
	// TODO: Could initialize this only if there are parameters
	FMetaHumanOutfitPipelineParameterContext& ParameterContext = InstanceParameterOutput.ParameterContext.InitializeAs<FMetaHumanOutfitPipelineParameterContext>();

	const USkinnedAsset* MaterialSource = OutfitAssemblyOutput.Outfit ? static_cast<USkinnedAsset*>(OutfitAssemblyOutput.Outfit) : static_cast<USkinnedAsset*>(OutfitAssemblyOutput.OutfitMesh);
	if (MaterialSource)
	{
		const TArray<FSkeletalMaterial>& MaterialSections = MaterialSource->GetMaterials();

		for (int32 SlotIndex = 0; SlotIndex < MaterialSections.Num(); ++SlotIndex)
		{
			const FSkeletalMaterial& Section = MaterialSections[SlotIndex];
			ParameterContext.AvailableSlots.Add(Section.MaterialSlotName);

			if (OutfitAssemblyOutput.OverrideMaterials.Contains(Section.MaterialSlotName))
			{
				// A slot with the same name has already been processed.
				//
				// We can only support one slot for each slot name.
				continue;
			}

			TObjectPtr<UMaterialInterface> AssemblyMaterial = Section.MaterialInterface;

			const TObjectPtr<UMaterialInterface>* PipelineMaterialOverride = OverrideMaterials.Find(Section.MaterialSlotName);
			if (PipelineMaterialOverride != nullptr)
			{
				AssemblyMaterial = *PipelineMaterialOverride;
			}

			if (!AssemblyMaterial)
			{
				// No material is assigned to this slot
				continue;
			}

			bool bNewMaterial = false;

			if (!AssemblyMaterial->IsA<UMaterialInstanceDynamic>())
			{
				bNewMaterial = true;
				AssemblyMaterial = UMaterialInstanceDynamic::Create(AssemblyMaterial, nullptr);
			}

			UMaterialInstanceDynamic* AssemblyMaterialDynamic = CastChecked<UMaterialInstanceDynamic>(AssemblyMaterial);

			const TArray<FMetaHumanMaterialParameter> MaterialParamsForThisSlot = RuntimeMaterialParameters.FilterByPredicate(
				[SlotName = Section.MaterialSlotName, SlotIndex](const FMetaHumanMaterialParameter& Parameter)
				{
					switch (Parameter.SlotTarget)
					{
					case EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames:
						return Parameter.SlotNames.Contains(SlotName);
					case EMetaHumanRuntimeMaterialParameterSlotTarget::SlotIndices:
						return Parameter.SlotIndices.Contains(SlotIndex);
					default:
						return false;
					}
				});

			const bool bSuccessful = UE::MetaHuman::MaterialUtils::ParametersToPropertyBag(
				AssemblyMaterialDynamic,
				MaterialParamsForThisSlot,
				InstanceParameterOutput.Parameters);

			if (!bSuccessful)
			{
				continue;
			}

			if (bNewMaterial)
			{
				AssemblyOutput.Metadata.Emplace(AssemblyMaterial, TEXT("Clothing"), AssemblyMaterial->GetName());
				AssemblyMaterial->Rename(nullptr, OuterForGeneratedObjects);
			}
			ParameterContext.MaterialSlotToMaterialInstance.Add(Section.MaterialSlotName, AssemblyMaterialDynamic);

			if (AssemblyMaterial != Section.MaterialInterface)
			{
				OutfitAssemblyOutput.OverrideMaterials.Add(Section.MaterialSlotName, AssemblyMaterial);
			}
		}
	}

	if (InstanceParameterOutput.Parameters.IsValid())
	{
		auto SetBoolParameters = [](FInstancedPropertyBag& InOutPropertyBag, const FName& InPropertyName, bool bInValue)
		{
			InOutPropertyBag.AddProperty(InPropertyName, EPropertyBagPropertyType::Bool);
			InOutPropertyBag.SetValueBool(InPropertyName, bInValue);
		};

		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("PruneSkinWeights"), true);
		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("RelaxSkinWeights"), true);
		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("HammerSkinWeights"), true);
		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("ClampSkinWeights"), true);
		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("NormalizeSkinWeights"), true);
		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("ResizeUVs"), true);
		SetBoolParameters(InstanceParameterOutput.Parameters, TEXT("CustomRegionResizing"), true);
		
		AssemblyOutput.InstanceParameters.Add(BaseItemPath, MoveTemp(InstanceParameterOutput));
	}

	OnComplete.ExecuteIfBound(MoveTemp(AssemblyOutput));
}

void UMetaHumanOutfitPipeline::SetInstanceParameters(const FInstancedStruct& ParameterContext, const FInstancedPropertyBag& Parameters) const
{
	if (!ParameterContext.GetPtr<FMetaHumanOutfitPipelineParameterContext>())
	{
		// Can't do anything without context
		return;
	}

	const FMetaHumanOutfitPipelineParameterContext& OutfitParameterContext = ParameterContext.Get<FMetaHumanOutfitPipelineParameterContext>();

	UE::MetaHuman::MaterialUtils::SetInstanceParameters(
		RuntimeMaterialParameters,
		OutfitParameterContext.MaterialSlotToMaterialInstance,
		OutfitParameterContext.AvailableSlots,
		Parameters);
}

TNotNull<const UMetaHumanCharacterPipelineSpecification*> UMetaHumanOutfitPipeline::GetSpecification() const
{
	return Specification;
}

void UMetaHumanOutfitPipeline::ApplyOutfitAssemblyOutputToClothComponent(const FMetaHumanOutfitPipelineAssemblyOutput& InOutfitAssemblyOutput, UChaosClothComponent* InClothComponent)
{
	InClothComponent->SetAsset(InOutfitAssemblyOutput.Outfit);
	InClothComponent->EmptyOverrideMaterials();

	const TArray<FName> SlotNames = InClothComponent->GetMaterialSlotNames();

	for (const TPair<FName, TObjectPtr<UMaterialInterface>>& OverrideMaterial : InOutfitAssemblyOutput.OverrideMaterials)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < SlotNames.Num(); ++MaterialIndex)
		{
			if (OverrideMaterial.Key == SlotNames[MaterialIndex])
			{
				InClothComponent->SetMaterial(MaterialIndex, OverrideMaterial.Value);
			}
		}
	}
}

void UMetaHumanOutfitPipeline::ApplyOutfitAssemblyOutputToMeshComponent(const FMetaHumanOutfitPipelineAssemblyOutput& InOutfitAssemblyOutput, USkeletalMeshComponent* InMeshComponent, bool bUpdateSkelMesh)
{
	InMeshComponent->SetSkeletalMesh(InOutfitAssemblyOutput.OutfitMesh);
	InMeshComponent->EmptyOverrideMaterials();

	for (const TPair<FName, TObjectPtr<UMaterialInterface>>& OverrideMaterial : InOutfitAssemblyOutput.OverrideMaterials)
	{
		const int32 MaterialIndex = InMeshComponent->GetMaterialIndex(OverrideMaterial.Key);
		if (MaterialIndex != INDEX_NONE)
		{
			InMeshComponent->SetMaterial(MaterialIndex, OverrideMaterial.Value);
		}
	}

	if (bUpdateSkelMesh && InOutfitAssemblyOutput.OutfitMesh)
	{
		TArray<FSkeletalMaterial> Materials = InOutfitAssemblyOutput.OutfitMesh->GetMaterials();

		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			if (const TObjectPtr<UMaterialInterface>* FoundMaterial = InOutfitAssemblyOutput.OverrideMaterials.Find(Materials[i].MaterialSlotName))
			{
				Materials[i].MaterialInterface = *FoundMaterial;
			}
		}

		InOutfitAssemblyOutput.OutfitMesh->SetMaterials(Materials);
	}
}
