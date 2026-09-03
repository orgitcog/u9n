// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinitionReference.h"
#include "SmartObjectDefinition.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDefinitionReference)

USmartObjectDefinition* FSmartObjectDefinitionReference::GetAssetVariation(UWorld* World) const
{
	USmartObjectDefinition* BaseDefinitionAsset = const_cast<USmartObjectDefinition*>(GetSmartObjectDefinition());
	return BaseDefinitionAsset != nullptr ? BaseDefinitionAsset->GetAssetVariation(GetParameters(), World) : nullptr;
}

void FSmartObjectDefinitionReference::SyncParameters()
{
	if (SmartObjectDefinition == nullptr)
	{
		Parameters.Reset();
	}
	else
	{
		UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Verbose, TEXT("%hs for '%s'")
			, __FUNCTION__, *GetNameSafe(SmartObjectDefinition));

		// In editor builds, sync with overrides.
		Parameters.MigrateToNewBagInstanceWithOverrides(SmartObjectDefinition->GetDefaultParameters(), PropertyOverrides);
		
		// Remove overrides that do not exist anymore
		if (!PropertyOverrides.IsEmpty())
		{
			if (const UPropertyBag* Bag = Parameters.GetPropertyBagStruct())
			{
				for (TArray<FGuid>::TIterator It = PropertyOverrides.CreateIterator(); It; ++It)
				{
					if (!Bag->FindPropertyDescByID(*It))
					{
						UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Verbose, TEXT("%hs removed override for Guid: '%s'")
							, __FUNCTION__, *LexToString(*It));
						It.RemoveCurrentSwap();
					}
				}
			}
		}
	}
}

bool FSmartObjectDefinitionReference::RequiresParametersSync() const
{
	bool bShouldSync = false;
	
	if (SmartObjectDefinition)
	{
		UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Log, TEXT("%hs for '%s'")
			, __FUNCTION__, *GetNameSafe(SmartObjectDefinition));

		const FInstancedPropertyBag& DefaultParameters = SmartObjectDefinition->GetDefaultParameters();
		const UPropertyBag* DefaultParametersBag = DefaultParameters.GetPropertyBagStruct();
		const UPropertyBag* ParametersBag = Parameters.GetPropertyBagStruct();
		
		// Mismatching property bags, needs sync.
		if (DefaultParametersBag != ParametersBag)
		{
			UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Log, TEXT("%hs - sync required: mismatching property bags DefaultParameters '%s' vs Parameters '%s'")
				, __FUNCTION__, *GetNameSafe(DefaultParametersBag), *GetNameSafe(ParametersBag));

			bShouldSync = true;
		}
		else if (ParametersBag && DefaultParametersBag)
		{
			// Check if non-overridden parameters are not identical, needs sync.
			const uint8* SourceAddress = DefaultParameters.GetValue().GetMemory();
			const uint8* TargetAddress = Parameters.GetValue().GetMemory();
			check(SourceAddress);
			check(TargetAddress);

			for (const FPropertyBagPropertyDesc& Desc : ParametersBag->GetPropertyDescs())
			{
				UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Verbose, TEXT("%hs - processing property '%s'"), __FUNCTION__, *Desc.Name.ToString());

				// Skip overridden
				if (PropertyOverrides.Contains(Desc.ID))
				{
					UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Verbose, TEXT("%hs - skipped since it is overridden"), __FUNCTION__, *Desc.Name.ToString());
					continue;
				}

				const uint8* SourceValueAddress = SourceAddress + Desc.CachedProperty->GetOffset_ForInternal();
				const uint8* TargetValueAddress = TargetAddress + Desc.CachedProperty->GetOffset_ForInternal();
				if (!Desc.CachedProperty->Identical(SourceValueAddress, TargetValueAddress))
				{
					FString Details;
					UE_SUPPRESS(LogSmartObject, Log,
					{
						FString SourceValueString;
						FString TargetValueString;
						Desc.CachedProperty->ExportTextItem_Direct(SourceValueString, SourceValueAddress, /*Default*/nullptr, /*Parent*/nullptr, PPF_None);
						Desc.CachedProperty->ExportTextItem_Direct(TargetValueString, TargetValueAddress, /*Default*/nullptr, /*Parent*/nullptr, PPF_None);
						Details = FString::Printf(TEXT(": '%s' vs '%s'"), *SourceValueString, *TargetValueString);
					})

					// Mismatching values, should sync.
					UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Log, TEXT("%hs - sync required: mismatching values for property '%s'%s")
						, __FUNCTION__, *Desc.Name.ToString(), *Details);
					bShouldSync = true;
					break;
				}
			}
		}
	}
	else
	{
		// Empty definition reference should not have parameters
		bShouldSync = Parameters.IsValid();
	}
	
	return bShouldSync;
}

void FSmartObjectDefinitionReference::ConditionallySyncParameters() const
{
	if (RequiresParametersSync())
	{
		FSmartObjectDefinitionReference* NonConstThis = const_cast<FSmartObjectDefinitionReference*>(this);
		NonConstThis->SyncParameters();
		UE_VLOG_UELOG(SmartObjectDefinition, LogSmartObject, Log, TEXT("%hs: Parameters for '%s' stored in SmartObjectDefinitionReference were auto-fixed to be usable at runtime.")
			, __FUNCTION__, *GetNameSafe(SmartObjectDefinition));
	}
}

void FSmartObjectDefinitionReference::SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden)
{
	if (bIsOverridden)
	{
		PropertyOverrides.AddUnique(PropertyID);
	}
	else
	{
		PropertyOverrides.Remove(PropertyID);
		ConditionallySyncParameters();
	}
}

bool FSmartObjectDefinitionReference::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Serialize from an object pointer.
	if (Tag.Type == NAME_ObjectProperty)
	{
		Slot << SmartObjectDefinition;
		return true;
	}
	
	return false;
}

uint32 GetTypeHash(const FSmartObjectDefinitionReference& DefinitionReference)
{
	const USmartObjectDefinition* Definition = DefinitionReference.GetSmartObjectDefinition();
	if (Definition == nullptr)
	{
		return 0;
	}

	const uint32 AssetPathHash = GetTypeHash(FSoftObjectPath(Definition).GetAssetPathString());
	const uint32 ParametersHash = GetTypeHash(USmartObjectDefinition::GetVariationParametersHash(DefinitionReference.GetParameters()));
	const uint32 OverridesHash = GetTypeHash(DefinitionReference.PropertyOverrides);

	return HashCombine(AssetPathHash, HashCombine(ParametersHash, OverridesHash));
}
