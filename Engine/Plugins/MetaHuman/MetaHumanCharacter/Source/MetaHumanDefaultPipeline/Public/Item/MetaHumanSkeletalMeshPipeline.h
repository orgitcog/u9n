// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanItemPipeline.h"
#include "MetaHumanPaletteItemKey.h"

#include "Item/MetaHumanMaterialPipelineCommon.h"

#include "MetaHumanSkeletalMeshPipeline.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMesh;
class UAnimBlueprint;

USTRUCT()
struct FMetaHumanSkeletalMeshPipelineBuildOutput
{
	GENERATED_BODY()

};

USTRUCT()
struct FMetaHumanSkeletalMeshPipelineAssemblyInput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<USkeletalMesh> TargetMesh;
};

USTRUCT(BlueprintType)
struct METAHUMANDEFAULTPIPELINE_API FMetaHumanSkeletalMeshPipelineAssemblyOutput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkelMesh;

	UPROPERTY()
	TSoftObjectPtr<UAnimBlueprint> AnimBlueprintToUse;

	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

USTRUCT()
struct FMetaHumanSkeletalMeshPipelineParameterContext
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> MaterialSlotToMaterialInstance;

	UPROPERTY()
	TArray<FName> AvailableSlots;
};

UCLASS(Blueprintable, EditInlineNew)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanSkeletalMeshPipeline : public UMetaHumanItemPipeline
{
	GENERATED_BODY()

public:
	UMetaHumanSkeletalMeshPipeline();

#if WITH_EDITOR
	virtual void SetDefaultEditorPipeline() override;

	virtual const UMetaHumanItemEditorPipeline* GetEditorPipeline() const override;
#endif

	virtual void AssembleItem(
		const FMetaHumanPaletteItemPath& BaseItemPath,
		const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
		const FMetaHumanPaletteBuiltData& ItemBuiltData,
		const FInstancedStruct& AssemblyInput,
		TNotNull<UObject*> OuterForGeneratedObjects,
		const FOnAssemblyComplete& OnComplete) const override;

	virtual void SetInstanceParameters(const FInstancedStruct& ParameterContext, const FInstancedPropertyBag& Parameters) const override;

	virtual TNotNull<const UMetaHumanCharacterPipelineSpecification*> GetSpecification() const override;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman Character Pipeline")
	static void ApplySkeletalMeshAssemblyOutputToSkeletalMeshComponent(
		const FMetaHumanSkeletalMeshPipelineAssemblyOutput& InAssemblyOutput,
		USkeletalMeshComponent* InComponent,
		USkeletalMeshComponent* InLeaderComponent);

	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TMap<FName, TObjectPtr<UMaterialInterface>> OverrideMaterials;

	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TArray<FMetaHumanMaterialParameter> RuntimeMaterialParameters;

	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TSoftObjectPtr<UAnimBlueprint> AnimBlueprintToUse;

private:
#if WITH_EDITOR
	TSubclassOf<UMetaHumanItemEditorPipeline> GetEditorPipelineClass() const;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, NoClear, Instanced, Category = "Pipeline", meta = (FullyExpand, AllowedClasses = "/Script/MetaHumanDefaultEditorPipeline.MetaHumanSkeletalMeshEditorPipeline"))
	TObjectPtr<UMetaHumanItemEditorPipeline> EditorPipeline;
#endif

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterPipelineSpecification> Specification;
};
