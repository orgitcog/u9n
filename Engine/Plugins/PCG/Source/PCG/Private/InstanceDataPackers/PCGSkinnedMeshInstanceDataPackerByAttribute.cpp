// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataPackers/PCGSkinnedMeshInstanceDataPackerByAttribute.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGSpatialData.h"
#include "InstanceDataPackers/PCGSkinnedMeshInstanceDataPackerBase.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "MeshSelectors/PCGSkinnedMeshSelector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSkinnedMeshInstanceDataPackerByAttribute)

#define LOCTEXT_NAMESPACE "PCGSkinnedMeshInstanceDataPackerByAttribute"

#if WITH_EDITOR
void UPCGSkinnedMeshInstanceDataPackerByAttribute::PostLoad()
{
	Super::PostLoad();
}
#endif // WITH_EDITOR

void UPCGSkinnedMeshInstanceDataPackerByAttribute::PackInstances_Implementation(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGSkinnedMeshInstanceList& InstanceList, FPCGSkinnedMeshPackedCustomData& OutPackedCustomData) const
{
	if (!InSpatialData || !InSpatialData->Metadata)
	{
		PCGLog::InputOutput::LogInvalidInputDataError(&Context);
		return;
	}

	TArray<TUniquePtr<const IPCGAttributeAccessor>> SelectedAccessors;
	TArray<TUniquePtr<const IPCGAttributeAccessorKeys>> SelectedKeys;

	SelectedAccessors.Reserve(AttributeSelectors.Num());
	SelectedKeys.Reserve(AttributeSelectors.Num());

	// Find attributes and calculate NumCustomDataFloats
	for (const FPCGAttributePropertyInputSelector& Selector : AttributeSelectors)
	{
		TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InSpatialData, Selector);
		TUniquePtr<const IPCGAttributeAccessorKeys> Keys;
		const UPCGPointData* PointData = InstanceList.PointData.Get();
		TArray<const PCGMetadataEntryKey> ExtractedKeys;
		
		if (InSpatialData == PointData)
		{
			Keys = MakeUnique<const FPCGAttributeAccessorKeysPointsSubset>(PointData->GetPoints(), InstanceList.InstancePointIndices);
		}
		else
		{
			// Convert indices to entry keys
			ExtractedKeys.Reserve(InstanceList.InstancePointIndices.Num());
			Algo::Transform(InstanceList.InstancePointIndices, ExtractedKeys, [](const int32 Index) -> PCGMetadataEntryKey{ return Index; });
			Keys = MakeUnique<const FPCGAttributeAccessorKeysEntries>(ExtractedKeys);
		}

		if (!Accessor.IsValid() || !Keys.IsValid())
		{
			PCGLog::Metadata::LogFailToCreateAccessorError(Selector, &Context);
			continue;
		}

		if (!AddTypeToPacking(Accessor->GetUnderlyingType(), OutPackedCustomData))
		{
			PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("AttributeInvalidType", "Attribute/property '{0}' is not a valid type - skipped."), Selector.GetDisplayText()), &Context);
			continue;
		}

		SelectedAccessors.Add(std::move(Accessor));
		SelectedKeys.Add(std::move(Keys));
	}

	PackCustomDataFromAccessors(InstanceList, std::move(SelectedAccessors), std::move(SelectedKeys), OutPackedCustomData);
}

bool UPCGSkinnedMeshInstanceDataPackerByAttribute::GetAttributeNames(TArray<FName>* OutNames)
{
	if (OutNames)
	{
		for (const FPCGAttributePropertyInputSelector& AttributeSelector : AttributeSelectors)
		{
			OutNames->Add(AttributeSelector.GetAttributeName());
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
