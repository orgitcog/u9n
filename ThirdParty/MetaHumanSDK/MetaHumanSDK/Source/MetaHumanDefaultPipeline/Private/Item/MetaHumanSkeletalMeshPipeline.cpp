// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanSkeletalMeshPipeline.h"

#include "MetaHumanDefaultPipelineLog.h"
#include "MetaHumanItemEditorPipeline.h"

#include "Algo/Find.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"

UMetaHumanSkeletalMeshPipeline::UMetaHumanSkeletalMeshPipeline()
{
	// Initialize the specification
	{
		Specification = CreateDefaultSubobject<UMetaHumanCharacterPipelineSpecification>("Specification");

		Specification->BuildOutputStruct = FMetaHumanSkeletalMeshPipelineBuildOutput::StaticStruct();
		Specification->AssemblyInputStruct = FMetaHumanSkeletalMeshPipelineAssemblyInput::StaticStruct();
		Specification->AssemblyOutputStruct = FMetaHumanSkeletalMeshPipelineAssemblyOutput::StaticStruct();
	}
}

#if WITH_EDITOR
void UMetaHumanSkeletalMeshPipeline::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSubclassOf<UMetaHumanItemEditorPipeline> EditorPipelineClass = GetEditorPipelineClass();
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanItemEditorPipeline>(this, EditorPipelineClass);
	}
}

const UMetaHumanItemEditorPipeline* UMetaHumanSkeletalMeshPipeline::GetEditorPipeline() const
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

TSubclassOf<UMetaHumanItemEditorPipeline> UMetaHumanSkeletalMeshPipeline::GetEditorPipelineClass() const
{
	const TSoftClassPtr<UMetaHumanItemEditorPipeline> SoftEditorPipelineClass(FSoftObjectPath(TEXT("/Script/MetaHumanDefaultEditorPipeline.MetaHumanSkeletalMeshEditorPipeline")));
	
	return SoftEditorPipelineClass.Get();
}
#endif

void UMetaHumanSkeletalMeshPipeline::AssembleItem(
	const FMetaHumanPaletteItemPath& BaseItemPath,
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
	const FMetaHumanPaletteBuiltData& ItemBuiltData,
	const FInstancedStruct& AssemblyInput,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnAssemblyComplete& OnComplete) const
{
	const FInstancedStruct& BuildOutput = ItemBuiltData.ItemBuiltData[BaseItemPath].BuildOutput;

	if (!AssemblyInput.GetPtr<FMetaHumanSkeletalMeshPipelineAssemblyInput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "Assembly input not provided to SkeletalMesh pipeline during assembly");
		
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	const FMetaHumanSkeletalMeshPipelineAssemblyInput& SkeletalMeshAssemblyInput = AssemblyInput.Get<FMetaHumanSkeletalMeshPipelineAssemblyInput>();

	if (!SkeletalMeshAssemblyInput.TargetMesh)
	{
		UE_LOGFMT(LogMetaHumanDefaultPipeline, Error, "The requested skeletal mesh is missing");
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	FMetaHumanAssemblyOutput AssemblyOutput;
	FMetaHumanSkeletalMeshPipelineAssemblyOutput& SkeletalMeshAssemblyOutput = AssemblyOutput.PipelineAssemblyOutput.InitializeAs<FMetaHumanSkeletalMeshPipelineAssemblyOutput>();
	SkeletalMeshAssemblyOutput.SkelMesh = SkeletalMeshAssemblyInput.TargetMesh;
	SkeletalMeshAssemblyOutput.AnimBlueprintToUse = AnimBlueprintToUse;

	FMetaHumanInstanceParameterOutput InstanceParameterOutput;
	FMetaHumanSkeletalMeshPipelineParameterContext& ParameterContext = InstanceParameterOutput.ParameterContext.InitializeAs<FMetaHumanSkeletalMeshPipelineParameterContext>();

	const TArray<FSkeletalMaterial>& MaterialSections = SkeletalMeshAssemblyInput.TargetMesh->GetMaterials();

	for (int32 SlotIndex = 0; SlotIndex < MaterialSections.Num(); SlotIndex++)
	{
		const FSkeletalMaterial& Section = MaterialSections[SlotIndex];
		ParameterContext.AvailableSlots.Add(Section.MaterialSlotName);

		if (SkeletalMeshAssemblyOutput.OverrideMaterials.Contains(Section.MaterialSlotName))
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
			AssemblyOutput.Metadata.Emplace(AssemblyMaterial, TEXT("SkelMesh"), AssemblyMaterial->GetName());
			AssemblyMaterial->Rename(nullptr, OuterForGeneratedObjects);
		}
		ParameterContext.MaterialSlotToMaterialInstance.Add(Section.MaterialSlotName, AssemblyMaterialDynamic);
		
		if (AssemblyMaterial != Section.MaterialInterface)
		{
			SkeletalMeshAssemblyOutput.OverrideMaterials.Add(Section.MaterialSlotName, AssemblyMaterial);
		}
	}

	if (InstanceParameterOutput.Parameters.IsValid())
	{
		AssemblyOutput.InstanceParameters.Add(BaseItemPath, MoveTemp(InstanceParameterOutput));
	}

	OnComplete.ExecuteIfBound(MoveTemp(AssemblyOutput));
}

void UMetaHumanSkeletalMeshPipeline::SetInstanceParameters(const FInstancedStruct& ParameterContext, const FInstancedPropertyBag& Parameters) const
{
	if (!ParameterContext.GetPtr<FMetaHumanSkeletalMeshPipelineParameterContext>())
	{
		// Can't do anything without context
		return;
	}

	const FMetaHumanSkeletalMeshPipelineParameterContext& SkeletalMeshParameterContext = ParameterContext.Get<FMetaHumanSkeletalMeshPipelineParameterContext>();

	UE::MetaHuman::MaterialUtils::SetInstanceParameters(
		RuntimeMaterialParameters,
		SkeletalMeshParameterContext.MaterialSlotToMaterialInstance,
		SkeletalMeshParameterContext.AvailableSlots,
		Parameters);
}

TNotNull<const UMetaHumanCharacterPipelineSpecification*> UMetaHumanSkeletalMeshPipeline::GetSpecification() const
{
	return Specification;
}

void UMetaHumanSkeletalMeshPipeline::ApplySkeletalMeshAssemblyOutputToSkeletalMeshComponent(
	const FMetaHumanSkeletalMeshPipelineAssemblyOutput& InAssemblyOutput,
	USkeletalMeshComponent* InComponent,
	USkeletalMeshComponent* InLeaderComponent)
{
	InComponent->SetSkeletalMesh(InAssemblyOutput.SkelMesh);

	// If there is an AnimBP specified by the pipeline, use that
	if (UAnimBlueprint* AnimBlueprint = InAssemblyOutput.AnimBlueprintToUse.LoadSynchronous())
	{
		InComponent->SetLeaderPoseComponent(nullptr);
		InComponent->SetAnimInstanceClass(AnimBlueprint->GetClass());
	}
	// If there is post process AnimBP on the skeletal mesh, use that
	else if (InAssemblyOutput.SkelMesh && InAssemblyOutput.SkelMesh->GetPostProcessAnimBlueprint() != nullptr)
	{
		InComponent->SetLeaderPoseComponent(nullptr);
		InComponent->SetAnimInstanceClass(nullptr);
	}
	// If no AnimBP is defined, use the leader pose component
	else if (InLeaderComponent)
	{
		InComponent->SetLeaderPoseComponent(InLeaderComponent);
		InComponent->SetAnimInstanceClass(nullptr);
	}

	InComponent->EmptyOverrideMaterials();

	for (const TPair<FName, TObjectPtr<UMaterialInterface>>& OverrideMaterial : InAssemblyOutput.OverrideMaterials)
	{
		const int32 MaterialIndex = InComponent->GetMaterialIndex(OverrideMaterial.Key);
		if (MaterialIndex != INDEX_NONE)
		{
			InComponent->SetMaterial(MaterialIndex, OverrideMaterial.Value);
		}
	}
}
