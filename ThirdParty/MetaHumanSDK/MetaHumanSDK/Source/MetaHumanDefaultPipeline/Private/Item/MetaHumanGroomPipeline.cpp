// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanGroomPipeline.h"

#include "MetaHumanDefaultPipelineLog.h"
#include "MetaHumanItemEditorPipeline.h"

#include "Algo/Find.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Logging/StructuredLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"

UMetaHumanGroomPipeline::UMetaHumanGroomPipeline()
{
	// Initialize the specification
	{
		Specification = CreateDefaultSubobject<UMetaHumanCharacterPipelineSpecification>("Specification");

		Specification->BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
		Specification->AssemblyInputStruct = FMetaHumanGroomPipelineAssemblyInput::StaticStruct();
		Specification->AssemblyOutputStruct = FMetaHumanGroomPipelineAssemblyOutput::StaticStruct();
	}
}

#if WITH_EDITOR
void UMetaHumanGroomPipeline::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSubclassOf<UMetaHumanItemEditorPipeline> EditorPipelineClass = GetEditorPipelineClass();
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanItemEditorPipeline>(this, EditorPipelineClass);
	}
}

const UMetaHumanItemEditorPipeline* UMetaHumanGroomPipeline::GetEditorPipeline() const
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

TSubclassOf<UMetaHumanItemEditorPipeline> UMetaHumanGroomPipeline::GetEditorPipelineClass() const
{
	const TSoftClassPtr<UMetaHumanItemEditorPipeline> SoftEditorPipelineClass(FSoftObjectPath(TEXT("/Script/MetaHumanDefaultEditorPipeline.MetaHumanGroomEditorPipeline")));
	
	return SoftEditorPipelineClass.Get();
}
#endif

void UMetaHumanGroomPipeline::AssembleItem(
	const FMetaHumanPaletteItemPath& BaseItemPath,
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
	const FMetaHumanPaletteBuiltData& ItemBuiltData,
	const FInstancedStruct& AssemblyInput,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnAssemblyComplete& OnComplete) const
{
	const FInstancedStruct& BuildOutput = ItemBuiltData.ItemBuiltData[BaseItemPath].BuildOutput;

	if (!BuildOutput.GetPtr<FMetaHumanGroomPipelineBuildOutput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "Build output not provided to Groom pipeline during assembly");
		
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	if (!AssemblyInput.GetPtr<FMetaHumanGroomPipelineAssemblyInput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "Assembly input not provided to Groom pipeline during assembly");
		
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	const FMetaHumanGroomPipelineBuildOutput& GroomBuildOutput = BuildOutput.Get<FMetaHumanGroomPipelineBuildOutput>();
	const FMetaHumanGroomPipelineAssemblyInput& GroomAssemblyInput = AssemblyInput.Get<FMetaHumanGroomPipelineAssemblyInput>();

	const TObjectPtr<UGroomBindingAsset>* GroomBindingPtr = GroomBuildOutput.Bindings.FindByPredicate(
		[TargetMesh = GroomAssemblyInput.TargetMesh](const TObjectPtr<UGroomBindingAsset>& Binding)
		{
			return Binding
				&& TargetMesh == Binding->GetTargetSkeletalMesh();
		});

	if (!GroomBindingPtr)
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "The requested skeletal mesh ({Mesh}) was not found in the Groom pipeline's build output", GetPathNameSafe(GroomAssemblyInput.TargetMesh));
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	FMetaHumanAssemblyOutput AssemblyOutput;
	FMetaHumanInstanceParameterOutput InstanceParameterOutput;

	FMetaHumanGroomPipelineAssemblyOutput& GroomAssemblyOutput = AssemblyOutput.PipelineAssemblyOutput.InitializeAs<FMetaHumanGroomPipelineAssemblyOutput>();
	GroomAssemblyOutput.Binding = *GroomBindingPtr;

	// TODO: Could initialise this only if there are parameters
	FMetaHumanGroomPipelineParameterContext& ParameterContext = InstanceParameterOutput.ParameterContext.InitializeAs<FMetaHumanGroomPipelineParameterContext>();

	if (GroomAssemblyOutput.Binding)
	{
		if (const UGroomAsset* Groom = GroomAssemblyOutput.Binding->GetGroom())
		{
			const TArray<FHairGroupsMaterial>& HairGroupMaterials = Groom->GetHairGroupsMaterials();

			for (int32 SlotIndex = 0; SlotIndex < HairGroupMaterials.Num(); ++SlotIndex)
			{
				const FHairGroupsMaterial& GroupMaterial = HairGroupMaterials[SlotIndex];

				ParameterContext.AvailableSlots.Add(GroupMaterial.SlotName);

				if (GroomAssemblyOutput.OverrideMaterials.Contains(GroupMaterial.SlotName))
				{
					// A slot with the same name has already been processed.
					//
					// We can only support one slot for each slot name.
					continue;
				}

				TObjectPtr<UMaterialInterface> AssemblyMaterial = GroupMaterial.Material;

				const TObjectPtr<UMaterialInterface>* PipelineMaterialOverride = OverrideMaterials.Find(GroupMaterial.SlotName);
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
				OverrideInitialMaterialValues(AssemblyMaterialDynamic, GroupMaterial.SlotName, SlotIndex);

				const TArray<FMetaHumanMaterialParameter> MaterialParamsForThisSlot = RuntimeMaterialParameters.FilterByPredicate(
					[SlotName = GroupMaterial.SlotName, SlotIndex](const FMetaHumanMaterialParameter& Parameter)
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
					AssemblyOutput.Metadata.Emplace(AssemblyMaterial, TEXT("Grooms"), AssemblyMaterial->GetName());
					AssemblyMaterial->Rename(nullptr, OuterForGeneratedObjects);
				}
				ParameterContext.MaterialSlotToMaterialInstance.Add(GroupMaterial.SlotName, AssemblyMaterialDynamic);

				if (AssemblyMaterial != GroupMaterial.Material)
				{
					GroomAssemblyOutput.OverrideMaterials.Add(GroupMaterial.SlotName, AssemblyMaterial);
				}
			}
		}
	}

	if (InstanceParameterOutput.Parameters.IsValid())
	{
		AssemblyOutput.InstanceParameters.Add(BaseItemPath, MoveTemp(InstanceParameterOutput));
	}

	OnComplete.ExecuteIfBound(MoveTemp(AssemblyOutput));
}

void UMetaHumanGroomPipeline::SetInstanceParameters(const FInstancedStruct& ParameterContext, const FInstancedPropertyBag& Parameters) const
{
	if (!ParameterContext.GetPtr<FMetaHumanGroomPipelineParameterContext>())
	{
		// Can't do anything without context
		return;
	}

	const FMetaHumanGroomPipelineParameterContext& GroomParameterContext = ParameterContext.Get<FMetaHumanGroomPipelineParameterContext>();

	UE::MetaHuman::MaterialUtils::SetInstanceParameters(
		RuntimeMaterialParameters,
		GroomParameterContext.MaterialSlotToMaterialInstance,
		GroomParameterContext.AvailableSlots,
		Parameters);
}

TNotNull<const UMetaHumanCharacterPipelineSpecification*> UMetaHumanGroomPipeline::GetSpecification() const
{
	return Specification;
}

void UMetaHumanGroomPipeline::ApplyGroomAssemblyOutputToGroomComponent(const FMetaHumanGroomPipelineAssemblyOutput& GroomAssemblyOutput, UGroomComponent* GroomComponent)
{
	const bool bTemporaryEnableStrands = GroomComponent->bUseCards;
	if (bTemporaryEnableStrands)
	{
		GroomComponent->SetUseCards(false);		
	}
	GroomComponent->SetGroomAsset(GroomAssemblyOutput.Binding ? GroomAssemblyOutput.Binding->GetGroom() : nullptr, GroomAssemblyOutput.Binding);

	GroomComponent->EmptyOverrideMaterials();
	for (const TPair<FName, TObjectPtr<UMaterialInterface>>& OverrideMaterial : GroomAssemblyOutput.OverrideMaterials)
	{
		const int32 MaterialIndex = GroomComponent->GetMaterialIndex(OverrideMaterial.Key);
		if (MaterialIndex != INDEX_NONE)
		{
			GroomComponent->SetMaterial(MaterialIndex, OverrideMaterial.Value);
		}
	}
	if (bTemporaryEnableStrands)
	{
		GroomComponent->GetWorld()->SendAllEndOfFrameUpdates();
		GroomComponent->SetUseCards(true);
	}
}
