// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanItemPipeline.h"
#include "MetaHumanPaletteItemKey.h"

#include "Item/MetaHumanMaterialPipelineCommon.h"

#include "MetaHumanGroomPipeline.generated.h"

class UGroomBindingAsset;
class UGroomComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMesh;

USTRUCT()
struct FMetaHumanGroomPipelineBuildOutput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TObjectPtr<UGroomBindingAsset>> Bindings;
};

USTRUCT()
struct FMetaHumanGroomPipelineAssemblyInput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<const USkeletalMesh> TargetMesh;
};

USTRUCT(BlueprintType)
struct METAHUMANDEFAULTPIPELINE_API FMetaHumanGroomPipelineAssemblyOutput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UGroomBindingAsset> Binding;

	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

USTRUCT()
struct FMetaHumanGroomPipelineParameterContext
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> MaterialSlotToMaterialInstance;

	UPROPERTY()
	TArray<FName> AvailableSlots;
};

UCLASS(Blueprintable, EditInlineNew)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanGroomPipeline : public UMetaHumanItemPipeline
{
	GENERATED_BODY()

public:
	UMetaHumanGroomPipeline();

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
	static void ApplyGroomAssemblyOutputToGroomComponent(const FMetaHumanGroomPipelineAssemblyOutput& GroomAssemblyOutput, UGroomComponent* GroomComponent);

	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TMap<FName, TObjectPtr<UMaterialInterface>> OverrideMaterials;

	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TArray<FMetaHumanMaterialParameter> RuntimeMaterialParameters;

protected:
	// Allows pipeline to override default material values before they're initialized
	virtual void OverrideInitialMaterialValues(TNotNull<UMaterialInstanceDynamic*> InMID, FName InSlotName, int32 SlotIndex) const {}

private:
#if WITH_EDITOR
	TSubclassOf<UMetaHumanItemEditorPipeline> GetEditorPipelineClass() const;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, NoClear, Instanced, Category = "Pipeline", meta = (FullyExpand, AllowedClasses = "/Script/MetaHumanDefaultEditorPipeline.MetaHumanGroomEditorPipeline"))
	TObjectPtr<UMetaHumanItemEditorPipeline> EditorPipeline;
#endif

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterPipelineSpecification> Specification;
};
