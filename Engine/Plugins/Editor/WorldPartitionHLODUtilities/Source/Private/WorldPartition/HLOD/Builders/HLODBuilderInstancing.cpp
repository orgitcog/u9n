// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/Builders/HLODBuilderInstancing.h"
#include "Serialization/ArchiveCrc32.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODBuilderInstancing)


UHLODBuilderInstancingSettings::UHLODBuilderInstancingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bDisallowNanite(false)
	, InstanceFilteringType(EInstanceFilteringType::FilterNone)
	, MinimumExtent(0)
	, MinimumArea(0)
	, MinimumVolume(0)
{
}

uint32 UHLODBuilderInstancingSettings::GetCRC() const
{
	UHLODBuilderInstancingSettings& This = *const_cast<UHLODBuilderInstancingSettings*>(this);

	FArchiveCrc32 Ar;

	// Base key, changing this will force a rebuild of all HLODs from this builder
	FString HLODBaseKey = "53809597CD9C4FB7AC75827A628513D6";
	Ar << HLODBaseKey;

	Ar << This.bDisallowNanite;
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - bDisallowNanite = %d"), Ar.GetCrc());

	if (InstanceFilteringType != EInstanceFilteringType::FilterNone)
	{
		Ar << This.InstanceFilteringType;
		switch (InstanceFilteringType)
		{
		case EInstanceFilteringType::FilterMinimumExtent:
			Ar << This.MinimumExtent;
			break;
		case EInstanceFilteringType::FilterMinimumArea:
			Ar << This.MinimumArea;
			break;
		case EInstanceFilteringType::FilterMinimumVolume:
			Ar << This.MinimumVolume;
			break;
		default:
			unimplemented();
		}
	}

	return Ar.GetCrc();
}


UHLODBuilderInstancing::UHLODBuilderInstancing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSubclassOf<UHLODBuilderSettings> UHLODBuilderInstancing::GetSettingsClass() const
{
	return UHLODBuilderInstancingSettings::StaticClass();
}

TArray<UActorComponent*> UHLODBuilderInstancing::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	const UHLODBuilderInstancingSettings& InstancingSettings = *CastChecked<UHLODBuilderInstancingSettings>(HLODBuilderSettings);

	int32 NumInstancesTotal = 0;
	int32 NumInstancesRejected = 0;

	auto FilterInstances = [&InstancingSettings, &NumInstancesTotal, &NumInstancesRejected](const FBox& InInstanceBounds)
	{
		bool bPassFilter = true;

		switch (InstancingSettings.InstanceFilteringType)
		{
			case EInstanceFilteringType::FilterMinimumExtent:
			{
				double MaxExtent = InInstanceBounds.GetExtent().GetMax();
				bPassFilter = MaxExtent >= InstancingSettings.MinimumExtent;
				break;
			}
			case EInstanceFilteringType::FilterMinimumArea:
			{
				FVector Extent = InInstanceBounds.GetExtent();
				double Area = 8 * (Extent.X * Extent.Y + Extent.X * Extent.Z + Extent.Y * Extent.Z);
				bPassFilter = Area >= InstancingSettings.MinimumArea;
				break;
			}
			case EInstanceFilteringType::FilterMinimumVolume:
			{
				double Volume = InInstanceBounds.GetVolume();
				bPassFilter = Volume >= InstancingSettings.MinimumVolume;
				break;
			}
		}

		NumInstancesTotal++;
		NumInstancesRejected += bPassFilter ? 0 : 1;

		return bPassFilter;
	};

	TArray<UActorComponent*> HLODComponents = UHLODBuilder::BatchInstances(InSourceComponents, FilterInstances);

	if (NumInstancesRejected > 0)
	{
		UE_LOG(LogHLODBuilder, Log, TEXT("UHLODBuilderInstancing: Filter rejected %d out of %d instances"), NumInstancesRejected, NumInstancesTotal);
	}

	// If requested, disallow Nanite on components whose mesh is Nanite enabled
	if (InstancingSettings.bDisallowNanite)
	{
		for (UActorComponent* HLODComponent : HLODComponents)
		{
			if (UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(HLODComponent))
			{
				if (UStaticMesh* StaticMesh = SMComponent->GetStaticMesh())
				{
					if (!SMComponent->bDisallowNanite && StaticMesh->HasValidNaniteData())
					{
						SMComponent->bDisallowNanite = true;
						SMComponent->MarkRenderStateDirty();
					}
				}
			}
		}
	}

	return HLODComponents;
}
