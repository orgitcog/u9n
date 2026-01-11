// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGPin.h"
#include "Accessors/PCGAttributeAccessorHelpers.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "PCGObjectPropertyOverride.generated.h"

class IPCGAttributeAccessor;
struct FPCGContext;

/**
* Represents the override source (to be read) and the object property (to be written).
*/
USTRUCT(BlueprintType)
struct FPCGObjectPropertyOverrideDescription
{
	GENERATED_BODY()

	FPCGObjectPropertyOverrideDescription() = default;

	FPCGObjectPropertyOverrideDescription(const FPCGAttributePropertyInputSelector& InInputSource, const FString& InPropertyTarget)
		: InputSource(InInputSource)
		, PropertyTarget(InPropertyTarget)
	{}

	/** Provide an attribute or property to read the override value from. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	/**
	* Provide an object property name to be overridden. If you have a property "A" on your object, use "A" as the property target.
	*
	* For example, if you want to override the "Is Editor Only" flag, find it in the details panel, right-click, select 'Copy Internal Name', and paste that as the property target.
	*
	* If you have a component property, such as the static mesh of a static mesh component, use "StaticMeshComponent.StaticMesh".
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};

namespace PCGObjectPropertyOverrideHelpers
{
	/** Create an advanced ParamData pin for capturing property overrides. */
	PCG_API FPCGPinProperties CreateObjectPropertiesOverridePin(FName Label, const FText& Tooltip);

	/** Apply property overrides to the TargetObject directly from the ObjectPropertiesOverride pin. Use CreateObjectPropertiesOverridePin(). */
	PCG_API void ApplyOverridesFromParams(const TArray<FPCGObjectPropertyOverrideDescription>& InObjectPropertyOverrideDescriptions, UObject* TargetObject, FName OverridesPinLabel, FPCGContext* Context);

	PCG_API void ApplyOverrides(const TArray<FPCGObjectPropertyOverrideDescription>& InObjectPropertyOverrideDescriptions, const TArray<TPair<UObject*, int32>>& TargetObjectAndIndex, FName OverridesPinLabel, int32 InputDataIndex, FPCGContext* Context);
}

/**
* Represents a single property override on the provided object. Applies an override function to read the InputAccessor
* and write its value to the OutputAccessor.
* 
* The InputAccessor's InputKeys are created from the given SourceData and InputSelector.
*/
struct FPCGObjectSingleOverride
{
	/** Initialize the single object override. Call before using Apply(InputKeyIndex, OutputKey). */
	PCG_API void Initialize(const FPCGAttributePropertySelector& InputSelector, const FString& OutputProperty, const UStruct* TemplateClass, const UPCGData* SourceData, FPCGContext* Context);

	/** Returns true if initialization succeeded in creating the accessors and accessor keys. */
	PCG_API bool IsValid() const;

	/** Applies a single property override to the object by reading from the InputAccessor at the given KeyIndex, and writing to the OutputKey which represents the object property. */
	PCG_API bool Apply(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey);

	/** Gathers overrides into an array, if they need loading. */
	PCG_API void GatherAllOverridesToLoad(TArray<FSoftObjectPath>& OutObjectsToLoad) const;

private:
	TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys;
	TUniquePtr<const IPCGAttributeAccessor> ObjectOverrideInputAccessor;
	TUniquePtr<IPCGAttributeAccessor> ObjectOverrideOutputAccessor;

	bool bWillNeedLoading = false;

	// InputKeyIndex, OutputKeys
	using ApplyOverrideFunction = bool(FPCGObjectSingleOverride::*)(int32, IPCGAttributeAccessorKeys&);
	ApplyOverrideFunction ObjectOverrideFunction = nullptr;

	template <typename Type>
	bool ApplyImpl(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey)
	{
		if (!IsValid())
		{
			return false;
		}

		Type Value{};
		check(ObjectOverrideInputAccessor.IsValid());
		if (ObjectOverrideInputAccessor->Get<Type>(Value, InputKeyIndex, *InputKeys.Get(), EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible))
		{
			check(ObjectOverrideOutputAccessor.IsValid());
			if (ObjectOverrideOutputAccessor->Set<Type>(Value, OutputKey))
			{
				return true;
			}
		}

		return false;
	}
};

/**
* Represents a set of property overrides for the provided object. Provide a SourceData to read from, and a collection of ObjectPropertyOverrides matching the TemplateObject's class properties.
*/
template <typename T>
struct FPCGObjectOverrides
{
	FPCGObjectOverrides(T* TemplateObject) : OutputKey(TemplateObject)
	{}

	/** Initialize the object overrides. Call before using Apply(InputKeyIndex). */
	void Initialize(const TArray<FPCGObjectPropertyOverrideDescription>& OverrideDescriptions, T* TemplateObject, const UPCGData* SourceData, FPCGContext* Context)
	{
		bInitialized = false;

		if (!TemplateObject)
		{
			PCGLog::LogErrorOnGraph(NSLOCTEXT("PCGObjectPropertyOverride", "InitializeOverrideFailedNoObject", "Failed to initialize property overrides. No template object was provided."), Context);
			return;
		}

		OutputKey = FPCGAttributeAccessorKeysSingleObjectPtr<T>(TemplateObject);

		const UStruct* ClassObject = nullptr;
		if constexpr (TIsDerivedFrom<T, UObject>::Value)
		{
			ClassObject = TemplateObject->GetClass();
		}
		else
		{
			ClassObject = TemplateObject->StaticStruct();
		}

		ObjectSingleOverrides.Empty(OverrideDescriptions.Num());

		for (int32 i = 0; i < OverrideDescriptions.Num(); ++i)
		{
			const FPCGAttributePropertyInputSelector InputSelector = OverrideDescriptions[i].InputSource.CopyAndFixLast(SourceData);
			const FString& OutputProperty = OverrideDescriptions[i].PropertyTarget;

			FPCGObjectSingleOverride Override;
			Override.Initialize(InputSelector, OutputProperty, ClassObject, SourceData, Context);

			if (Override.IsValid())
			{
				ObjectSingleOverrides.Add(std::move(Override));
			}
			else
			{
				PCGLog::LogErrorOnGraph(FText::Format(NSLOCTEXT("PCGObjectPropertyOverride", "InitializeOverrideFailed", "Failed to initialize override '{0}' for property {1} on object '{2}'."), InputSelector.GetDisplayText(), FText::FromString(OutputProperty), FText::FromName(ClassObject->GetFName())), Context);
			}
		}

		bInitialized = true;
	}

	/** Applies each property override to the object by reading from the InputAccessor at the given KeyIndex, and writing to the OutputKey which represents the object property. */
	bool Apply(int32 InputKeyIndex)
	{
		bool bAllSucceeded = true;

		for (FPCGObjectSingleOverride& ObjectSingleOverride : ObjectSingleOverrides)
		{
			bAllSucceeded &= ObjectSingleOverride.Apply(InputKeyIndex, OutputKey);
		}

		return bAllSucceeded;
	}

	/** Update the template object, only if it was already initialized */
	void UpdateTemplateObject(T* InTemplateObject)
	{
		if (IsValid())
		{
			OutputKey = FPCGAttributeAccessorKeysSingleObjectPtr<T>(InTemplateObject);
		}
	}

	/** Returns true if we have any override to apply */
	bool IsValid() const { return bInitialized && !ObjectSingleOverrides.IsEmpty(); }

	/** Gathers overrides into an array, if they need loading. */
	void GatherAllOverridesToLoad(TArray<FSoftObjectPath>& OutObjectsToLoad) const
	{
		for (const FPCGObjectSingleOverride& SingleOverride : ObjectSingleOverrides)
		{
			SingleOverride.GatherAllOverridesToLoad(OutObjectsToLoad);
		}
	}

private:
	FPCGAttributeAccessorKeysSingleObjectPtr<T> OutputKey;
	TArray<FPCGObjectSingleOverride> ObjectSingleOverrides;
	bool bInitialized = false;
};

USTRUCT(BlueprintType, meta=(Deprecated = "5.4", DeprecationMessage="Use FPCGObjectPropertyOverrideDescription instead."))
struct FPCGActorPropertyOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	UPROPERTY(EditAnywhere, Category = Settings)
	FString PropertyTarget;
};
