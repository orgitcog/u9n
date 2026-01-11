// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Item/MetaHumanSkeletalMeshPipeline.h"

#include "MetaHumanDefaultSkeletalMeshPipeline.generated.h"

class UTexture;

/**
 * Class defining the MetaHuman skeletal mesh standard - it lists all the available parameters
 * and maps them against the material parameter name.
 */
UCLASS()
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultSkeletalMeshPipelineMaterialParameters : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (MaterialParamName = "diffuse_color_1", SkelMeshCategory = "Default"))
	FLinearColor Color1;

	UPROPERTY(meta = (MaterialParamName = "diffuse_color_2", SkelMeshCategory = "Default"))
	FLinearColor Color2;

};

/**
 * Skeletal Mesh pipeline used for compatibility with the original MetaHumanCreator.
 */
UCLASS(Blueprintable, EditInlineNew)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultSkeletalMeshPipeline : public UMetaHumanSkeletalMeshPipeline
{
	GENERATED_BODY()

public:
	UMetaHumanDefaultSkeletalMeshPipeline();

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~End UObject interface

	UPROPERTY(EditAnywhere, Category = "Support")
	EMetaHumanRuntimeMaterialParameterSlotTarget SlotTarget = EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames;

	UPROPERTY(EditAnywhere, Category = "Support", meta = (EditCondition = "SlotTarget == EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames"))
	TArray<FName> SlotNames;

	UPROPERTY(EditAnywhere, Category = "Support", meta = (EditCondition = "SlotTarget == EMetaHumanRuntimeMaterialParameterSlotTarget::SlotIndices"))
	TArray<int32> SlotIndices;

private:
#if WITH_EDITOR
	void AddRuntimeParameter(TNotNull<FProperty*> InProperty, const FName& InMaterialParameterName);
#endif

	void UpdateParameters();
};
