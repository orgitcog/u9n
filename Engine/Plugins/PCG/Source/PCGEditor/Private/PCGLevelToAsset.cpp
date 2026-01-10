// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGLevelToAsset.h"

#include "PCGEditorModule.h"
#include "PCGEditorUtils.h"
#include "PCGAssetExporterUtils.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPointArrayData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGTagHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/LevelStreaming.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Materials/MaterialInstance.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"

extern PCG_API TAutoConsoleVariable<bool> CVarPCGEnablePointArrayData;

void UPCGLevelToAsset::CreateOrUpdatePCGAssets(const TArray<FAssetData>& WorldAssets, const FPCGAssetExporterParameters& InParameters, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	TArray<UPackage*> PackagesToSave;
	FPCGAssetExporterParameters Parameters = InParameters;

	if (WorldAssets.Num() > 1)
	{
		Parameters.bOpenSaveDialog = false;
	}

	for (const FAssetData& WorldAsset : WorldAssets)
	{
		if (UPackage* Package = CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld>(WorldAsset.GetSoftObjectPath()), Parameters, ExporterSubclass))
		{
			PackagesToSave.Add(Package);
		}
	}

	// Save the file(s)
	if (!PackagesToSave.IsEmpty() && Parameters.bSaveOnExportEnded)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
	}
}

UPackage* UPCGLevelToAsset::CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld> WorldPath, const FPCGAssetExporterParameters& Parameters, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	return CreateOrUpdatePCGAsset(WorldPath.LoadSynchronous(), Parameters, ExporterSubclass);
}

UPackage* UPCGLevelToAsset::CreateOrUpdatePCGAsset(UWorld* World, const FPCGAssetExporterParameters& InParameters, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	if (!World)
	{
		return nullptr;
	}

	UPCGLevelToAsset* Exporter = nullptr;
	if (ExporterSubclass)
	{
		Exporter = NewObject<UPCGLevelToAsset>(GetTransientPackage(), ExporterSubclass);
	}
	else
	{
		Exporter = NewObject<UPCGLevelToAsset>(GetTransientPackage());
	}

	if (!Exporter)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Unable to create Level to Settings exporter."));
		return nullptr;
	}

	Exporter->WorldToExport = World;

	FPCGAssetExporterParameters Parameters = InParameters;
	Parameters.AssetName = World->GetName() + TEXT("_PCG");

	if (InParameters.AssetPath.IsEmpty() && World->GetPackage())
	{
		Parameters.AssetPath = FPackageName::GetLongPackagePath(World->GetPackage()->GetName());
	}

	return UPCGAssetExporterUtils::CreateAsset(Exporter, Parameters);
}

UPackage* UPCGLevelToAsset::UpdateAsset(const FAssetData& PCGAsset)
{
	UPCGDataAsset* Asset = Cast<UPCGDataAsset>(PCGAsset.GetAsset());
	if (!Asset)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Asset '%s' isn't a PCG data asset or could not be properly loaded."), *PCGAsset.GetObjectPathString());
		return nullptr;
	}

	UPackage* Package = Asset->GetPackage();
	if (!Package)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Unable to retrieve package from Asset '%s'."), *PCGAsset.GetObjectPathString());
		return nullptr;
	}

	TSoftObjectPtr<UWorld> WorldPtr(Asset->ObjectPath);
	UWorld* World = WorldPtr.LoadSynchronous();

	if (!World)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("PCG asset was unable to load world '%s'."), *Asset->ObjectPath.ToString());
		return nullptr;
	}

	WorldToExport = World;

	if (ExportAsset(Package->GetPathName(), Asset))
	{
		FCoreUObjectDelegates::BroadcastOnObjectModified(Asset);
		return Package;
	}
	else
	{
		return nullptr;
	}
}

bool UPCGLevelToAsset::ExportAsset(const FString& PackageName, UPCGDataAsset* Asset)
{
	return BP_ExportWorld(WorldToExport, PackageName, Asset);
}

bool UPCGLevelToAsset::BP_ExportWorld_Implementation(UWorld* World, const FString& PackageName, UPCGDataAsset* Asset)
{
	check(World && Asset);
	Asset->ObjectPath = FSoftObjectPath(World);
	Asset->Description = FText::Format(NSLOCTEXT("PCGLevelToAsset", "DefaultDescriptionOnExportedLevel", "Generated from world: {0}"), FText::FromString(World->GetName()));
	Asset->ExporterClass = GetClass();

	FPCGDataCollection& DataCollection = Asset->Data;
	DataCollection.TaggedData.Reset();

	// Select proper point data class
	UClass* PointDataClass = CVarPCGEnablePointArrayData.GetValueOnAnyThread() ? UPCGPointArrayData::StaticClass() : UPCGPointData::StaticClass();

	// Create Root Data
	UPCGBasePointData* RootPointData = NewObject<UPCGBasePointData>(Asset, PointDataClass);
	UPCGMetadata* RootMetadata = RootPointData->MutableMetadata();

	RootMetadata->CreateAttribute<FString>(TEXT("Name"), World->GetName(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	RootMetadata->CreateAttribute<FSoftObjectPath>(TEXT("Source"), FSoftObjectPath(World), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

	// Add to data collection
	{
		FPCGTaggedData& RootsTaggedData = DataCollection.TaggedData.Emplace_GetRef();
		RootsTaggedData.Data = RootPointData;
		RootsTaggedData.Pin = TEXT("Root");
	}

	// Create PCGData
	UPCGBasePointData* PointData = NewObject<UPCGBasePointData>(Asset, PointDataClass);
	UPCGMetadata* PointMetadata = PointData->MutableMetadata();

	// Add to data collection
	{
		FPCGTaggedData& PointsTaggedData = DataCollection.TaggedData.Emplace_GetRef();
		PointsTaggedData.Data = PointData;
		PointsTaggedData.Pin = TEXT("Points");
	}

	// Common data shared across steps
	FBox AllActorBounds(EForceInit::ForceInit);

	// Hardcoded attributes
	const FName& MaterialAttributeName = PCGLevelToAssetConstants::MaterialAttributeName;
	const FName& MeshAttributeName = PCGLevelToAssetConstants::MeshAttributeName;
	const FName& HierarchyDepthAttributeName = PCGLevelToAssetConstants::HierarchyDepthAttributeName;
	const FName& ActorIndexAttributeName = PCGLevelToAssetConstants::ActorIndexAttributeName;
	const FName& ParentIndexAttributeName = PCGLevelToAssetConstants::ParentIndexAttributeName;
	const FName& RelativeTransformAttributeName = PCGLevelToAssetConstants::RelativeTransformAttributeName;

	// Attribute setup on the points
	FPCGMetadataAttribute<FSoftObjectPath>* MaterialAttribute = PointMetadata->CreateAttribute<FSoftObjectPath>(MaterialAttributeName, FSoftObjectPath(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = PointMetadata->CreateAttribute<FSoftObjectPath>(MeshAttributeName, FSoftObjectPath(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* HierarchyDepthAttribute = PointMetadata->CreateAttribute<int64>(HierarchyDepthAttributeName, 0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* ActorIndexAttribute = PointMetadata->CreateAttribute<int64>(ActorIndexAttributeName, -1, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* ParentIndexAttribute = PointMetadata->CreateAttribute<int64>(ParentIndexAttributeName, -1, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<FTransform>* RelativeTransformAttribute = PointMetadata->CreateAttribute<FTransform>(RelativeTransformAttributeName, FTransform::Identity, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

	// Map from raw/unsanitized tag name to corresponding attribute.
	TMap<FName, FPCGMetadataAttributeBase*> TagToAttributeMap;

	// Relationship Tag:SanitizedName is many:1, so keep track of which sanitized names are created so we don't attempt to create the same one multiple times.
	TSet<FName> SanitizedAttributeNames;

	TSet<FName> ReservedTags;
	ReservedTags.Add(MaterialAttributeName);
	ReservedTags.Add(MeshAttributeName);
	ReservedTags.Add(HierarchyDepthAttributeName);
	ReservedTags.Add(ActorIndexAttributeName);
	ReservedTags.Add(ParentIndexAttributeName);
	ReservedTags.Add(RelativeTransformAttributeName);

	// Hierarchy root point
	{
		PointData->SetNumPoints(1);
		PointData->SetTransform(FTransform::Identity);
		PointData->SetDensity(1.0f);
		PointData->SetBoundsMin(FVector::Zero());
		PointData->SetBoundsMax(FVector::Zero());
		PointData->SetSteepness(1.0f);
		PointData->AllocateProperties(EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::MetadataEntry | EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax | EPCGPointNativeProperties::Seed);

		TPCGValueRange<int64> MetadataEntryRange = PointData->GetMetadataEntryValueRange();
		PointMetadata->InitializeOnSet(MetadataEntryRange[0]);
		ActorIndexAttribute->SetValue(MetadataEntryRange[0], 0);
	}

	// Make sure all actors are loaded
	TMap<UWorldPartition*, TArray<FWorldPartitionReference>> ActorReferencesPerWorldPartition;

	auto LoadAllActors = [&ActorReferencesPerWorldPartition](UWorldPartition* InWorldPartition) -> bool
	{
		if (InWorldPartition)
		{
			TArray<FWorldPartitionReference> ActorReferences;
			InWorldPartition->LoadAllActors(ActorReferences);
			ActorReferencesPerWorldPartition.Emplace(InWorldPartition, MoveTemp(ActorReferences));

			return true;
		}

		return false;
	};

	// Load all actors
	bool bProcessedNewActors = LoadAllActors(World->GetWorldPartition());
		
	// Make sure to call this once as non World Partition levels do not load new actors
	World->BlockTillLevelStreamingCompleted();

	// Load all level instances and their actors recursively
	TSet<ULevelStreaming*> ProcessedLevelStreamings;
	
	while(bProcessedNewActors)
	{
		bProcessedNewActors = false;

		// Make sure to load all Level Instances
		World->BlockTillLevelStreamingCompleted();

		// For each Streaming Level, make sure to load its actors if it is a World Partition
		for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
		{
			bool bAlreadySet = false;
			if (!ProcessedLevelStreamings.Contains(LevelStreaming))
			{
				ProcessedLevelStreamings.Add(LevelStreaming);
				
				if (ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel())
				{
					bProcessedNewActors |= LoadAllActors(FWorldPartitionHelpers::GetWorldPartition(LoadedLevel));
				}
			}
		}
	}
	
	struct FActorEntry
	{
		FActorEntry(int InIndex) : Index(InIndex) {}
		int Index = 0;
		AActor* AttachmentParent = nullptr;
		TArray<FName> Tags;
	};

	ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(World);
	check(LevelInstanceSubsystem);

	TFunction<AActor*(AActor*)> GetAttachParentActor = [&GetAttachParentActor, LevelInstanceSubsystem](AActor* InActor)
	{
		if(InActor->IsInLevelInstance())
		{
			AActor* AttachParentActor = InActor->GetAttachParentActor();
			if (AttachParentActor && AttachParentActor->IsA<ALevelInstanceEditorInstanceActor>())
			{
				return GetAttachParentActor(Cast<AActor>(LevelInstanceSubsystem->GetParentLevelInstance(InActor)));
			}
			else
			{
				return AttachParentActor;
			}
		}
		else
		{
			return InActor->GetAttachParentActor();
		}
	};

	auto IsActorExcluded = [LevelInstanceSubsystem, &GetAttachParentActor](AActor* InActor)
	{
		if (InActor->ActorHasTag(PCGLevelToAssetConstants::ExcludedActorTag))
		{
			return true;
		}

		// Check Level Instance Hierarchy first
		if (InActor->IsInLevelInstance())
		{
			bool bExcluded = false;
			LevelInstanceSubsystem->ForEachLevelInstanceAncestors(InActor, [&bExcluded](ILevelInstanceInterface* Ancestor)
			{
				AActor* AncestorActor = CastChecked<AActor>(Ancestor);
				if (AncestorActor->ActorHasTag(PCGLevelToAssetConstants::ExcludedActorTag))
				{
					bExcluded = true;
					return false;
				}
				return true;
			});

			if (bExcluded)
			{
				return true;
			}
		}

		// Check Attachment next (which will go passed Level Instance Hierarchy)
		AActor* AttachParent = GetAttachParentActor(InActor);
		while (AttachParent)
		{
			if (AttachParent->ActorHasTag(PCGLevelToAssetConstants::ExcludedActorTag))
			{
				return true;
			}
			AttachParent = GetAttachParentActor(AttachParent);
		}

		return false;
	};

	// Parent tags are meant to be at the end of the array so they are applied last in the MakePoint lambda (we want the parent tag values to override the childrens)
	auto GetActorTags = [LevelInstanceSubsystem](AActor* InActor, TArray<FName>& OutTags)
	{
		OutTags.Append(InActor->Tags);
		
		if (InActor->IsInLevelInstance())
		{
			LevelInstanceSubsystem->ForEachLevelInstanceAncestors(InActor, [&OutTags](ILevelInstanceInterface* Ancestor)
			{
				AActor* AncestorActor = CastChecked<AActor>(Ancestor);
				OutTags.Append(AncestorActor->Tags);
				return true;
			});
		}
	};

	// Build actor-index map
	TMap<AActor*, FActorEntry> ActorIndexMap;
	TSet<AActor*> ExcludedActors;
	
	int LastActorIndex = 1; // Since the root is the "first" point we'll have, we'll have the map start from 1.
	UPCGActorHelpers::ForEachActorInWorld(World, AActor::StaticClass(), [&ActorIndexMap, &ExcludedActors, &LastActorIndex, &GetAttachParentActor, &GetActorTags, &IsActorExcluded](AActor* Actor)
	{
		if(IsActorExcluded(Actor))
		{
			ExcludedActors.Add(Actor);
			return true;
		}

		FActorEntry ActorEntry(LastActorIndex++);
		
		ActorEntry.AttachmentParent = GetAttachParentActor(Actor);

		GetActorTags(Actor, ActorEntry.Tags);

		ActorIndexMap.Add(Actor, MoveTemp(ActorEntry));
		return true;
	});

	// Create points
	UPCGActorHelpers::ForEachActorInWorld(World, AActor::StaticClass(), [&](AActor* Actor)
	{
		// TODO Actor-level decisions if any; if the actor is "consumed" at this step, make sure to update AllActorBounds as well.

		if (ExcludedActors.Contains(Actor))
		{
			return true;
		}

		// Otherwise, parse "known" actor components
		TArray<UStaticMeshComponent*> SMCs;
		Actor->GetComponents(SMCs);

		if (SMCs.IsEmpty()) // early out
		{
			return true;
		}

		const FBox ActorBounds = PCGHelpers::GetActorBounds(Actor, /*bIgnorePCGCreatedComponents=*/true);
		AllActorBounds += ActorBounds;

		const FActorEntry& ActorEntry = ActorIndexMap[Actor];

		for (FName ActorTag : ActorEntry.Tags)
		{
			PCG::Private::FParseTagResult TagData(ActorTag);

			if (!TagData.IsValid())
			{
				continue;
			}

			FName OriginalAttributeName(TagData.GetOriginalAttribute());
			FName SanitizedAttributeName(TagData.Attribute);

			// Check if we can safely skip that tag
			if (ReservedTags.Contains(SanitizedAttributeName) || TagToAttributeMap.Contains(OriginalAttributeName) || SanitizedAttributeNames.Contains(SanitizedAttributeName))
			{
				continue;
			}

			// Try to create the attribute
			if (!PCG::Private::CreateAttributeFromTag(TagData, PointMetadata))
			{
				continue;
			}

			// Log warning if we sanitized some values
			if (TagData.HasBeenSanitized())
			{
				UE_LOG(LogPCGEditor, Warning, TEXT("Sanitized tag string on actor '%s' to remove invalid characters: '%s' -> '%s'"), *Actor->GetName(), *TagData.GetOriginalAttribute(), *TagData.Attribute);
			}

			TagToAttributeMap.Add(OriginalAttributeName, PointMetadata->GetMutableAttribute(SanitizedAttributeName));
			SanitizedAttributeNames.Add(SanitizedAttributeName);
		}

		// Prepare actor-level data that's propagated to all points
		const FTransform& ActorTransform = Actor->GetTransform();
		const int64 ActorIndex = ActorEntry.Index;
		AActor* ParentActor = ActorEntry.AttachmentParent;
		const int64 ParentActorIndex = ParentActor ? ActorIndexMap[ParentActor].Index : 0;
		const FTransform RelativeTransform = ParentActor ? ActorTransform.GetRelativeTransform(ParentActor->GetTransform()) : ActorTransform;

		// Hierarchy depth, starts at 1 if the actor doesn't have a parent
		int HierarchyDepth = 1;
		while (ParentActor)
		{
			++HierarchyDepth;
			ParentActor = GetAttachParentActor(ParentActor);
		}

		auto MakePoint = [&](int32 PointIndex, const FTransform& Transform, const FSoftObjectPath& MeshPath, const FBox& MeshBounds, const TArray<UMaterialInterface*>& MeshMaterials)
		{
			TPCGValueRange<FTransform> TransformRange = PointData->GetTransformValueRange();
			TPCGValueRange<int32> SeedRange = PointData->GetSeedValueRange();
			TPCGValueRange<FVector> BoundsMinRange = PointData->GetBoundsMinValueRange();
			TPCGValueRange<FVector> BoundsMaxRange = PointData->GetBoundsMaxValueRange();
			TPCGValueRange<int64> MetadataEntryRange = PointData->GetMetadataEntryValueRange();

			TransformRange[PointIndex] = Transform;
			SeedRange[PointIndex] = PCGHelpers::ComputeSeedFromPosition(Transform.GetLocation());
			BoundsMinRange[PointIndex] = MeshBounds.Min;
			BoundsMaxRange[PointIndex] = MeshBounds.Max;
			
			int64& MetadataEntry = MetadataEntryRange[PointIndex];
			PointMetadata->InitializeOnSet(MetadataEntry);
			MeshAttribute->SetValue(MetadataEntry, MeshPath);

			if (!MeshMaterials.IsEmpty())
			{
				// Avoid references to transient materials, in this case we reference the first non transient parent
				UMaterialInterface* Material = MeshMaterials[0];
				UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
				while (MaterialInstance && MaterialInstance->GetPackage() == GetTransientPackage())
				{
					Material = MaterialInstance->Parent;
					MaterialInstance = Cast<UMaterialInstance>(Material);
				}

				MaterialAttribute->SetValue(MetadataEntry, FSoftObjectPath(Material));
			}

			ActorIndexAttribute->SetValue(MetadataEntry, ActorIndex);
			ParentIndexAttribute->SetValue(MetadataEntry, ParentActorIndex);
			RelativeTransformAttribute->SetValue(MetadataEntry, RelativeTransform);
			HierarchyDepthAttribute->SetValue(MetadataEntry, HierarchyDepth);

			// For all tags, if the tag is of format 'Name:Value' then create attribute Name and assign Value, otherwise
			// create a boolean attribute with the name given by the sanitized tag string.
			for (FName ActorTag : ActorEntry.Tags)
			{
				// Implementation note: we don't set values directly from the tag in the eventuality that there are some name collisions
				PCG::Private::FParseTagResult TagData(ActorTag);

				if (TagData.IsValid() && TagToAttributeMap.Contains(FName(TagData.GetOriginalAttribute())))
				{
					PCG::Private::SetAttributeFromTag(TagData, PointMetadata, MetadataEntry);
				}
			}
		};

		for (UStaticMeshComponent* SMC : SMCs)
		{
			TObjectPtr<UStaticMesh> StaticMesh = SMC->GetStaticMesh();

			if (!StaticMesh)
			{
				continue;
			}

			const FSoftObjectPath MeshPath(StaticMesh);
			const FBox MeshBounds = StaticMesh->GetBoundingBox();
			TArray<UMaterialInterface*> Materials = SMC->GetMaterials();

			const int FirstIndex = PointData->GetNumPoints();

			// For all instances (or a single instance if this is a static mesh and not an ISM)
			// if a static mesh -> use actor transform (which might be wrong?)
			// if ISM -> get instance transform in world space
			if (UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC))
			{
				const int InstanceCount = ISMC->GetNumInstances();
				
				PointData->SetNumPoints(PointData->GetNumPoints() + InstanceCount);
				for (int I = 0; I < InstanceCount; ++I)
				{
					FTransform InstanceTransform;
					ISMC->GetInstanceTransform(I, InstanceTransform, /*bWorldSpace=*/true);

					MakePoint(FirstIndex+I, InstanceTransform, MeshPath, MeshBounds, Materials);
				}
			}
			else
			{
				PointData->SetNumPoints(PointData->GetNumPoints() + 1);
				MakePoint(FirstIndex, SMC->GetComponentTransform(), MeshPath, MeshBounds, Materials);
			}
		}

		return true;
	});

	// Finally, create root point in the root data
	{
		RootPointData->SetNumPoints(1);
		RootPointData->SetTransform(FTransform::Identity);
		RootPointData->SetDensity(1.0f);
		RootPointData->SetSeed(0);
		RootPointData->SetBoundsMin(AllActorBounds.Min);
		RootPointData->SetBoundsMax(AllActorBounds.Max);
		RootPointData->SetSteepness(1.0f);
	}

	return true;
}

void UPCGLevelToAsset::SetWorld(UWorld* World)
{
	WorldToExport = World;
}

UWorld* UPCGLevelToAsset::GetWorld() const
{
	return WorldToExport;
}