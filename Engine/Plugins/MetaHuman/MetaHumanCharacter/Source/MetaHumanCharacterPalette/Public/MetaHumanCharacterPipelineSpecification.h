// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Misc/NotNull.h"

#include "MetaHumanCharacterPipelineSpecification.generated.h"

class UMetaHumanCharacterPipelineSpecification;

namespace UE::MetaHuman::CharacterPipelineSlots
{
/**
 * A pipeline that accepts MetaHuman Characters should have a slot for them with this name, to
 * ensure compatibility with the MetaHuman Character editor.
 */
extern METAHUMANCHARACTERPALETTE_API const FName Character;
}

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanCharacterPipelineSlot
{
	GENERATED_BODY()

public:
	bool IsVirtual() const;

	/** Returns true if the given asset is accepted by this slot */
	bool SupportsAsset(const FAssetData& Asset) const;

	/** Returns true if the given asset class is supported by the slot */
	bool SupportsAssetType(TNotNull<const UClass*> AssetType) const;

	/*
	 * The asset types accepted by this slot.
	 * 
	 * If this is a virtual slot, SupportedPrincipalAssetTypes on this slot must be a subset of 
	 * the target slot's SupportedPrincipalAssetTypes.
	 */
	UPROPERTY()
	TArray<TSoftClassPtr<UObject>> SupportedPrincipalAssetTypes;

	UPROPERTY()
	TObjectPtr<UScriptStruct> BuildOutputStruct;

	UPROPERTY()
	TObjectPtr<UScriptStruct> AssemblyInputStruct;

	UPROPERTY()
	TObjectPtr<UScriptStruct> AssemblyOutputStruct;

	/** 
	 * If TargetSlot is a valid name, this slot is a virtual slot that forwards its selections to 
	 * a target slot, which may be virtual or real.
	 */
	UPROPERTY()
	FName TargetSlot;

	/** If true, multiple items can be selected for this slot simultaneously */
	UPROPERTY()
	bool bAllowsMultipleSelection = false;
	
	/** 
	 * If true, this slot will be shown in UI such as the Character Instance editor.
	 * 
	 * When you have multiple wearables that are processed in the same way by the pipeline, e.g.
	 * earrings and glasses, it's useful to have a single, hidden slot that can process an
	 * arbitrary number of them. You can then easily add virtual slots that target this hidden slot
	 * to allow more types of wearable, without modifying the pipeline.
	 */
	UPROPERTY()
	bool bVisibleToUser = true;
};

/**
 * This class represents the data interface of a UMetaHumanCharacterPipeline.
 *
 * It allows code to determine if two pipelines are compatible.
 */
UCLASS()
class METAHUMANCHARACTERPALETTE_API UMetaHumanCharacterPipelineSpecification : public UObject
{
	GENERATED_BODY()

public:
	bool IsValid() const;

	/** 
	 * Given a virtual or real slot name, returns the real slot that it resolves to.
	 * 
	 * If the slot name is not found, the return value will be unset.
	 */
	TOptional<FName> ResolveRealSlotName(FName SlotName) const;

	UPROPERTY()
	TObjectPtr<UScriptStruct> BuildOutputStruct;

	UPROPERTY()
	TObjectPtr<UScriptStruct> AssemblyInputStruct;

	UPROPERTY()
	TObjectPtr<UScriptStruct> AssemblyOutputStruct;

	/**
	 * The specification for each slot.
	 * 
	 * The key is the slot name.
	 */
	UPROPERTY()
	TMap<FName, FMetaHumanCharacterPipelineSlot> Slots;
};
