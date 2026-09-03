// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGSkinnedMeshSelector.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGSkinnedMeshSpawner.h"
#include "Elements/PCGSkinnedMeshSpawnerContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "MeshSelectors/PCGSkinnedMeshSelector.h"
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Metadata/PCGMetadataPartitionCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Algo/AnyOf.h"
#include "Animation/AnimBank.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSkinnedMeshSelector)

#define LOCTEXT_NAMESPACE "PCGSkinnedMeshSelector"

namespace PCGSkinnedMeshSelector
{
	// Returns variation based on mesh, material overrides and reverse culling
	FPCGSkinnedMeshInstanceList& GetInstanceList(
		TArray<FPCGSkinnedMeshInstanceList>& InstanceLists,
		const FPCGSoftSkinnedMeshComponentDescriptor& TemplateDescriptor,
		TSoftObjectPtr<USkinnedAsset> Asset,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& MaterialOverrides,
		bool bReverseCulling,
		const UPCGPointData* InPointData,
		const int AttributePartitionIndex = INDEX_NONE)
	{
		for (FPCGSkinnedMeshInstanceList& InstanceList : InstanceLists)
		{
			if (InstanceList.Descriptor.SkinnedAsset == Asset &&
				// GW-TODO: InstanceList.Descriptor.bReverseCulling == bReverseCulling &&
				// GW-TODO: InstanceList.Descriptor.OverrideMaterials == MaterialOverrides &&
				InstanceList.AttributePartitionIndex == AttributePartitionIndex)
			{
				return InstanceList;
			}
		}

		FPCGSkinnedMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(TemplateDescriptor);
		NewInstanceList.Descriptor.SkinnedAsset = Asset;
		// GW-TODO: NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		// GW-TODO: NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;
		NewInstanceList.AttributePartitionIndex = AttributePartitionIndex;
		NewInstanceList.PointData = InPointData;

		return NewInstanceList;
	}
}

void UPCGSkinnedMeshSelector::PostLoad()
{
	Super::PostLoad();

	// TODO: Remove if/when FBodyInstance is updated or replaced
	// Necessary to update the collision Response Container from the Response Array
	TemplateDescriptor.PostLoadFixup(this);
}

bool UPCGSkinnedMeshSelector::SelectInstances(
	FPCGSkinnedMeshSpawnerContext& Context,
	const UPCGSkinnedMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGSkinnedMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMeshSelector::SelectInstances);

	if (!InPointData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InputMissingData", "Missing input data"));
		return true;
	}

	if (!InPointData->Metadata)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InputMissingMetadata", "Unable to get metadata from input"));
		return true;
	}

	if (!InPointData->Metadata->HasAttribute(BankAttribute.GetAttributeName()))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Attribute '{0}' is not in the metadata"), BankAttribute.GetDisplayText()));
		return true;
	}

	if (!InPointData->Metadata->HasAttribute(SequenceIndexAttribute.GetAttributeName()))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Attribute '{0}' is not in the metadata"), SequenceIndexAttribute.GetDisplayText()));
		return true;
	}

	if (!InPointData->Metadata->HasAttribute(MeshAttribute.GetAttributeName()))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Attribute '{0}' is not in the metadata"), MeshAttribute.GetDisplayText()));
		return true;
	}

	const FPCGMetadataAttributeBase* BankAttributeBase = InPointData->Metadata->GetConstAttribute(BankAttribute.GetAttributeName());
	check(BankAttributeBase);

	const FPCGMetadataAttributeBase* SequenceIndexAttributeBase = InPointData->Metadata->GetConstAttribute(SequenceIndexAttribute.GetAttributeName());
	check(SequenceIndexAttributeBase);

	const FPCGMetadataAttributeBase* MeshAttributeBase = InPointData->Metadata->GetConstAttribute(MeshAttribute.GetAttributeName());
	check(MeshAttributeBase);

	// Validate that the "bank" attribute is of the right type
	if (!PCG::Private::IsOfTypes<FSoftObjectPath, FString>(BankAttributeBase->GetTypeId()))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("BankAttributeInvalidType", "Attribute '{0}' is not of valid type (must be FString or FSoftObjectPath)"), BankAttribute.GetDisplayText()));
		return true;
	}

	// Validate that the "sequence index" attribute is of the right type
	if (!PCG::Private::IsOfTypes<int32>(SequenceIndexAttributeBase->GetTypeId()))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("SeqIndexAttributeInvalidType", "Attribute '{0}' is not of valid type (must be int32)"), SequenceIndexAttribute.GetDisplayText()));
		return true;
	}

	// Validate that the "mesh" attribute is of the right type
	if (!PCG::Private::IsOfTypes<FSoftObjectPath, FString>(MeshAttributeBase->GetTypeId()))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("MeshAttributeInvalidType", "Attribute '{0}' is not of valid type (must be FString or FSoftObjectPath)"), MeshAttribute.GetDisplayText()));
		return true;
	}

	FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
	if (!MaterialOverrideHelper.IsInitialized())
	{
		MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, {} /*TemplateDescriptor.OverrideMaterials*/, MaterialOverrideAttributes, InPointData->Metadata);
	}

	if (!MaterialOverrideHelper.IsValid())
	{
		return true;
	}

	// ByAttribute takes in SoftObjectPaths per point in the metadata, so we can pass those directly into the outgoing pin if it exists
	if (Context.CurrentPointIndex == 0 && OutPointData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMeshSelector::SetupOutPointData);
		OutPointData->SetPoints(InPointData->GetPoints());
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSkinnedMeshSelector::SelectEntries);

	if (!Context.bPartitionDone)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSkinnedMeshSelector::SelectEntries::BuildingPartition);

		TArray<FPCGObjectPropertyOverrideDescription> PropertyOverrides = Settings->SkinnedMeshComponentPropertyOverrides;
		const FString SkinnedAssetPropertyString = GET_MEMBER_NAME_CHECKED(FPCGSoftSkinnedMeshComponentDescriptor, SkinnedAsset).ToString();

		// Add the skinned asset override to the list only if not already provided
		if (!Algo::AnyOf(PropertyOverrides, [&SkinnedAssetPropertyString](const FPCGObjectPropertyOverrideDescription& PropertyOverride) { return PropertyOverride.PropertyTarget == SkinnedAssetPropertyString; }))
		{
			PropertyOverrides.Emplace(MeshAttribute, SkinnedAssetPropertyString);
		}

		// Validate all the selectors are actual FPCGSoftSkinnedMeshComponentDescriptor properties
		TArray<FPCGAttributePropertySelector> ValidSelectorOverrides;

		for (const FPCGObjectPropertyOverrideDescription& PropertyOverride : PropertyOverrides)
		{
			if (FPCGSoftSkinnedMeshComponentDescriptor::StaticStruct()->FindPropertyByName(FName(PropertyOverride.PropertyTarget)))
			{
				ValidSelectorOverrides.Emplace_GetRef() = FPCGAttributePropertySelector::CreateFromOtherSelector<FPCGAttributePropertySelector>(PropertyOverride.InputSource);
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("OverriddenPropertyNotFound", "Property '{0}' not a valid property with an ABM Descriptor. It will be ignored."), FText::FromString(PropertyOverride.PropertyTarget)));
			}
		}

		// If there are valid overrides, partition the points on those attributes so that an instance can be created for each
		if (!ValidSelectorOverrides.IsEmpty())
		{
			Context.AttributeOverridePartition = PCGMetadataPartitionCommon::AttributeGenericPartition(InPointData, ValidSelectorOverrides, &Context, Settings->bSilenceOverrideAttributeNotFoundErrors);
		}

		// Set the descriptors to match the partition count. Uninitialized, because it will be copied from the template below
		Context.OverriddenDescriptors.Reserve(Context.AttributeOverridePartition.Num());
		for (int I = 0; I < Context.AttributeOverridePartition.Num(); ++I)
		{
			FPCGSoftSkinnedMeshComponentDescriptor& Descriptor = Context.OverriddenDescriptors.Add_GetRef(TemplateDescriptor);

			// If partition is empty (which can happen, esp. for the default partition on the default value, we'll just skip it here.
			if (Context.AttributeOverridePartition[I].IsEmpty())
			{
				continue;
			}

			// Use the Object Override to map the user's input selector and property to the descriptor
			FPCGObjectOverrides Overrides(&Descriptor);
			Overrides.Initialize(PropertyOverrides, &Descriptor, InPointData, &Context);

			// Since they are already partitioned and identical, we can just use the value on the first point
			check(!Context.AttributeOverridePartition[I].IsEmpty());
			int32 AnyPointIndexOnThisPartition = Context.AttributeOverridePartition[I][0];
			Overrides.Apply(AnyPointIndexOnThisPartition);
		}

		Context.PointBankValues.Reset();
		Context.PointSequenceIndexValues.Reset();

		// Given partitioning is expensive, check if we're out of time for this frame
		Context.bPartitionDone = true;
		if (Context.ShouldStop())
		{
			return false;
		}
	}

	// Assign points to entries
	int32 CurrentPartitionIndex = Context.CurrentPointIndex; // misnomer but we're reusing another concept from the context
	const TArray<FPCGPoint>& Points = InPointData->GetPoints();

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSkinnedMeshSelector::SelectEntries::PushingPointsToInstanceLists);

		if (Context.PointBankValues.IsEmpty() || Context.PointSequenceIndexValues.IsEmpty())
		{
			PCGAttributeAccessorHelpers::ExtractAllValues(InPointData, BankAttribute, Context.PointBankValues, &Context);
			PCGAttributeAccessorHelpers::ExtractAllValues(InPointData, SequenceIndexAttribute, Context.PointSequenceIndexValues, &Context);
		}

		// TODO: Revisit this when attribute partitioning is returned in a more optimized form
		// The partition index is used to assign the point to the correct partition's instance
		while (CurrentPartitionIndex < Context.AttributeOverridePartition.Num())
		{
			const int32 PartitionIndex = CurrentPartitionIndex++;
			TArray<int32>& Partition = Context.AttributeOverridePartition[PartitionIndex];
			const FPCGSoftSkinnedMeshComponentDescriptor& CurrentPartitionDescriptor = Context.OverriddenDescriptors[PartitionIndex];
			
			if (Partition.IsEmpty() || CurrentPartitionDescriptor.SkinnedAsset.IsNull())
			{
				continue;
			}

			bool bSkip = false;

			for (const FSoftAnimBankItem& BankItem : CurrentPartitionDescriptor.BankItems)
			{
				if (BankItem.BankAsset.IsNull())
				{
					bSkip = true;
					break;
				}
			}

			if (bSkip)
			{
				continue;
			}

			// Setup data for mesh bounds computation
			if (OutPointData && Settings->bApplyMeshBoundsToPoints)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSkinnedMeshSelector::SelectEntries::PushPointsToApplyMeshBounds);
				TArray<int32>& PointIndices = Context.MeshToOutPoints.FindOrAdd(CurrentPartitionDescriptor.SkinnedAsset).FindOrAdd(OutPointData);
				PointIndices.Append(Partition);
			}

			// Separate the inverse determinant instances so we can push them to a different ABM
			TArray<int32> ReverseInstances;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSkinnedMeshSelector::SelectEntries::BuildReverseInstances);
				int32 WriteIndex = 0;
				for (int32 ReadIndex = 0; ReadIndex < Partition.Num(); ++ReadIndex)
				{
					const FPCGPoint& Point = Points[Partition[ReadIndex]];
					if (Point.Transform.GetDeterminant() < 0)
					{
						ReverseInstances.Add(Partition[ReadIndex]);
					}
					else
					{
						if (WriteIndex != ReadIndex)
						{
							Partition[WriteIndex] = Partition[ReadIndex];
						}
						++WriteIndex;
					}
				}

				Partition.SetNum(WriteIndex);
			}

			auto AddPointsToInstanceList = [&OutMeshInstances, &CurrentPartitionDescriptor, &Context, &MaterialOverrideHelper, &Points, PartitionIndex, InPointData](const TArray<int32>& PointIndices, bool bReverseTransform)
			{
				if (MaterialOverrideHelper.OverridesMaterials())
				{
					for (int32 PointIndex = 0; PointIndex < PointIndices.Num(); ++PointIndex)
					{
						const int32 InstanceToPoint = PointIndices[PointIndex];

						const FPCGPoint& Point = Points[InstanceToPoint];
						FPCGSkinnedMeshInstanceList& InstanceList = PCGSkinnedMeshSelector::GetInstanceList(
							OutMeshInstances,
							CurrentPartitionDescriptor,
							CurrentPartitionDescriptor.SkinnedAsset,
							MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry),
							bReverseTransform,
							InPointData,
							PartitionIndex
						);

						FSoftAnimBankItem BankItem;
						{
							BankItem.BankAsset = Context.PointBankValues[InstanceToPoint];
							BankItem.SequenceIndex = Context.PointSequenceIndexValues[InstanceToPoint];
						}

						FPCGSkinnedMeshInstance Instance;
						Instance.BankIndex = InstanceList.Descriptor.GetOrAddBankIndex(BankItem);
						Instance.Transform = Point.Transform;

						InstanceList.Instances.Emplace(Instance);
						InstanceList.InstancePointIndices.Emplace(PointIndex);
					}
				}
				else
				{
					TArray<TSoftObjectPtr<UMaterialInterface>> DummyMaterialList;
					FPCGSkinnedMeshInstanceList& InstanceList = PCGSkinnedMeshSelector::GetInstanceList(
						OutMeshInstances,
						CurrentPartitionDescriptor,
						CurrentPartitionDescriptor.SkinnedAsset,
						DummyMaterialList,
						bReverseTransform,
						InPointData,
						PartitionIndex
					);

					check(InstanceList.Instances.Num() == InstanceList.InstancePointIndices.Num());
					const int32 InstanceOffset = InstanceList.Instances.Num();
					InstanceList.Instances.SetNum(InstanceOffset + PointIndices.Num());
					InstanceList.InstancePointIndices.Append(PointIndices);

					for (int32 PointIndex = 0; PointIndex < PointIndices.Num(); ++PointIndex)
					{
						FPCGSkinnedMeshInstance& Instance = InstanceList.Instances[InstanceOffset + PointIndex];

						const int32 InstanceToPoint = PointIndices[PointIndex];

						FSoftAnimBankItem BankItem;
						{
							BankItem.BankAsset = Context.PointBankValues[InstanceToPoint];
							BankItem.SequenceIndex = Context.PointSequenceIndexValues[InstanceToPoint];
						}

						Instance.BankIndex = InstanceList.Descriptor.GetOrAddBankIndex(BankItem);
						Instance.Transform = Points[InstanceToPoint].Transform;
					}
				}
			};

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSkinnedMeshSelector::SelectEntries::AddPointsToInstanceList);
				AddPointsToInstanceList(Partition, false);
				AddPointsToInstanceList(ReverseInstances, true);
			}

			if (Context.ShouldStop())
			{
				break;
			}
		}
	}

	Context.CurrentPointIndex = CurrentPartitionIndex; // misnomer, but we're using the same context
	return CurrentPartitionIndex == Context.AttributeOverridePartition.Num();
}

#undef LOCTEXT_NAMESPACE
