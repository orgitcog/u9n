// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCollectionPipeline.h"

#include "MetaHumanCharacterTestPipeline.generated.h"

class UMetaHumanCharacterPipelineSpecification;
class UMetaHumanCollectionEditorPipeline;

USTRUCT()
struct FMetaHumanCharacterTestAssemblyOutput
{
	GENERATED_BODY()
};

/**
 * Runtime counterpart of UMetaHumanCharacterTestPipeline
 */
UCLASS()
class UMetaHumanCharacterTestPipeline : public UMetaHumanCollectionPipeline
{
	GENERATED_BODY()

public:
	void SetSpecification(UMetaHumanCharacterPipelineSpecification* InSpecification);

	// Begin UMetaHumanCollectionPipeline interface
#if WITH_EDITOR
	virtual void SetDefaultEditorPipeline() override;
	virtual const UMetaHumanCollectionEditorPipeline* GetEditorPipeline() const override;
	virtual UMetaHumanCollectionEditorPipeline* GetMutableEditorPipeline() override;
#endif

	virtual TNotNull<const UMetaHumanCharacterPipelineSpecification*> GetSpecification() const override;

	virtual void AssembleCollection(
		TNotNull<const UMetaHumanCollection*> Collection,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
		const FInstancedStruct& AssemblyInput,
		TNotNull<UObject*> OuterForGeneratedObjects,
		const FOnAssemblyComplete& OnComplete) const override;

	virtual TSubclassOf<AActor> GetActorClass() const override;
	// End UMetaHumanCollectionPipeline interface

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, NoClear, Instanced, Category = "Character", meta = (FullyExpand, AllowedClasses = "/Script/MetaHumanCharacterPaletteEditor.MetaHumanCharacterTestEditorPipeline"))
	TObjectPtr<UMetaHumanCollectionEditorPipeline> EditorPipeline;
#endif

private:
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterPipelineSpecification> Specification;
};
