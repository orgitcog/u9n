// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBinding.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinitionPropertyBinding.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

class USmartObjectDefinition;

#if WITH_EDITORONLY_DATA
UENUM()
enum class ESmartObjectPropertyPathRetargetingStatus : uint8
{
	NoRetargeting,
	PickedPath,
	RetargetedPath
};
#endif

/**
 * Representation of a property binding used inside a FSmartObjectBindingCollection.
 * This is an extension of FPropertyBindingBinding with source and target FSmartObjectDefinitionDataHandle
 * allowing to refer to bindable structs in the SmartObjectDefinition (e.g., Parameters, Slots, etc.)
 */
USTRUCT()
struct FSmartObjectDefinitionPropertyBinding : public FPropertyBindingBinding
{
	GENERATED_BODY()

	using FPropertyBindingBinding::FPropertyBindingBinding;

protected:
	friend USmartObjectDefinition;

	virtual FConstStructView GetSourceDataHandleStruct() const override
	{
		return FConstStructView::Make(SourceDataHandle);
	}

	UPROPERTY(VisibleAnywhere, Category = "SmartObject")
	FSmartObjectDefinitionDataHandle SourceDataHandle;

	UPROPERTY(VisibleAnywhere, Category = "SmartObject")
	FSmartObjectDefinitionDataHandle TargetDataHandle;

#if WITH_EDITORONLY_DATA
public:
	/**
	 * Bindings targeting properties inside FWorldConditionQueryDefinition (e.g., SelectionPreconditions) require
	 * an additional binding with a retarget path for runtime. For that reason we categorized them
	 * so we can get rid of the original picked path when cooking the content.
	 */
	UPROPERTY(VisibleAnywhere, Category = "SmartObject")
	ESmartObjectPropertyPathRetargetingStatus TargetPathRetargetingStatus = ESmartObjectPropertyPathRetargetingStatus::NoRetargeting;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectDefinitionPropertyBinding(const FSmartObjectDefinitionPropertyBinding& Other) = default;
	FSmartObjectDefinitionPropertyBinding(FSmartObjectDefinitionPropertyBinding&& Other) = default;
	FSmartObjectDefinitionPropertyBinding& operator=(const FSmartObjectDefinitionPropertyBinding& Other) = default;

	UE_API void PostSerialize(const FArchive& Ar);

private:
	UE_DEPRECATED(5.6, "Use FPropertyBindingBinding::SourcePropertyPath instead.")
	UPROPERTY()
	FPropertyBindingPath SourcePath_DEPRECATED;

	UE_DEPRECATED(5.6, "Use FPropertyBindingBinding::TargetPropertyPath instead.")
	UPROPERTY()
	FPropertyBindingPath TargetPath_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FSmartObjectDefinitionPropertyBinding> : public TStructOpsTypeTraitsBase2<FSmartObjectDefinitionPropertyBinding>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
#endif // WITH_EDITORONLY_DATA

#undef UE_API
