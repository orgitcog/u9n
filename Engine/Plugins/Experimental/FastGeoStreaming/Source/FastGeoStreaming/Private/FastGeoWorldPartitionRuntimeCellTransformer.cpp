// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoWorldPartitionRuntimeCellTransformer.h"

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Level.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Streaming/ActorTextureStreamingBuildDataComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/Info.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartitionRuntimeCellTransformerISM.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelInstance/LevelInstanceEditorPivotInterface.h"
#include "ActorEditorUtils.h"
#include "Animation/AnimInstance.h"
#include "Selection.h"
#include "FastGeoContainer.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoComponent.h"
#include "FastGeoHLOD.h"
#include "FastGeoStaticMeshComponent.h"
#include "FastGeoInstancedStaticMeshComponent.h"
#include "FastGeoSkinnedMeshComponent.h"
#include "FastGeoInstancedSkinnedMeshComponent.h"
#include "FastGeoLog.h"
#endif

#if WITH_EDITORONLY_DATA
#include "WorldPartition/HLOD/HLODActor.h"
#endif

#define LOCTEXT_NAMESPACE "WorldPartition"

#if WITH_EDITOR
namespace FastGeo
{
	// Used to cancel package being dirtied when bDebugMode is modified (see PostEditChangeProperty)
	static bool GPackageWasDirty = false;

	// Tag use to force include actors into FastGeoStreaming
	static const FName NAME_FastGeo(TEXT("FastGeo"));

	// Tag use to force exclude actors from FastGeoStreaming
	static const FName NAME_NoFastGeo(TEXT("NoFastGeo"));

	static bool IsCollisionEnabled(UPrimitiveComponent* Component)
	{
		return Component->IsCollisionEnabled() && !Component->GetOwner()->IsA<AWorldPartitionHLOD>();
	}

	static FString GetComponentShortName(UActorComponent* InComponent)
	{
		TStringBuilder<256> Builder;
		Builder += InComponent->GetOwner()->GetName();
		Builder += TEXT(".");
		Builder += InComponent->GetName();
		return *Builder;
	}

	struct FTransformationStats
	{
		int32 TotalActorCount = 0;
		int32 TotalComponentCount = 0;
		int32 FullyTransformableActorCount = 0;
		int32 PartiallyTransformableActorCount = 0;
		int32 TransformedComponentCount = 0;

		void DumpStats(const TCHAR* InPrefixString)
		{
			if (TotalActorCount)
			{
				const float FullyTransformedActorPercentage = TotalActorCount > 0 ? (100.f * FullyTransformableActorCount) / TotalActorCount : 0.f;
				const float PartiallyTransformedActorPercentage = TotalActorCount > 0 ? (100.f * PartiallyTransformableActorCount) / TotalActorCount : 0.f;
				const float TransformedComponentPercentage = TotalComponentCount > 0 ? (100.f * TransformedComponentCount) / TotalComponentCount : 0.f;
				const int32 NonTransformableActorCount = FMath::Max(0, TotalActorCount - FullyTransformableActorCount - PartiallyTransformableActorCount);
				const float NonTransformableActorPercentage = NonTransformableActorCount > 0 ? (100.f * NonTransformableActorCount) / TotalActorCount : 0.f;

				UE_CLOG(FullyTransformableActorCount, LogFastGeoStreaming, Log, TEXT("%s Transformable Actors (Full)    = %d (%3.1f%%)"), InPrefixString, FullyTransformableActorCount, FullyTransformedActorPercentage);
				UE_CLOG(PartiallyTransformableActorCount,LogFastGeoStreaming, Log, TEXT("%s Transformable Actors (Partial) = %d (%3.1f%%)"), InPrefixString, PartiallyTransformableActorCount, PartiallyTransformedActorPercentage);
				UE_CLOG(TransformedComponentCount, LogFastGeoStreaming, Log, TEXT("%s Transformable Components       = %d (%3.1f%%)"), InPrefixString, TransformedComponentCount, TransformedComponentPercentage);
				UE_CLOG(NonTransformableActorCount, LogFastGeoStreaming, Log, TEXT("%s Non-Transformable Actors       = %d (%3.1f%%)"), InPrefixString, NonTransformableActorCount, NonTransformableActorPercentage);
			}
		}
	};

	FFastGeoElementType GetFastGeoComponentType(TSubclassOf<UPrimitiveComponent> InClass)
	{
		static const TMap<TSubclassOf<UPrimitiveComponent>, FFastGeoElementType> FastGeoComponentTypeMapping =
		{
			{ UStaticMeshComponent::StaticClass(), FFastGeoStaticMeshComponent::Type },
			{ UInstancedStaticMeshComponent::StaticClass(), FFastGeoInstancedStaticMeshComponent::Type },
			{ USkinnedMeshComponent::StaticClass(), FFastGeoSkinnedMeshComponent::Type },
			{ UInstancedSkinnedMeshComponent::StaticClass(), FFastGeoInstancedSkinnedMeshComponent::Type }
		};

		// Walk the component class hierarchy and look for a fast geo mapping
		for (UClass* Class = InClass; Class; Class = Class->GetSuperClass())
		{
			if (const FFastGeoElementType* Found = FastGeoComponentTypeMapping.Find(Class))
			{
				return *Found;
			}
		}

		return FFastGeoElementType::Invalid;
	}
}
#endif

UFastGeoWorldPartitionRuntimeCellTransformer::UFastGeoWorldPartitionRuntimeCellTransformer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	if (GIsEditor && !IsTemplate())
	{
		USelection::SelectionChangedEvent.AddUObject(this, &UFastGeoWorldPartitionRuntimeCellTransformer::OnSelectionChanged);
	}
#endif
}

void UFastGeoWorldPartitionRuntimeCellTransformer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << bDebugMode;
	}
#endif
}

#if WITH_EDITOR

bool UFastGeoWorldPartitionRuntimeCellTransformer::IsDebugModeEnabled = false;
FAutoConsoleVariableRef UFastGeoWorldPartitionRuntimeCellTransformer::CVarIsDebugModeEnabled(
	TEXT("FastGeo.EnableTransformerDebugMode"),
	UFastGeoWorldPartitionRuntimeCellTransformer::IsDebugModeEnabled,
	TEXT("Set to true to enable FastGeoStreaming transformer debug mode (used in PIE and at cook time)."),
	ECVF_Default);

bool UFastGeoWorldPartitionRuntimeCellTransformer::IsFastGeoEnabled = true;
FAutoConsoleVariableRef UFastGeoWorldPartitionRuntimeCellTransformer::CVarIsFastGeoEnabled(
	TEXT("FastGeo.Enable"),
	UFastGeoWorldPartitionRuntimeCellTransformer::IsFastGeoEnabled,
	TEXT("Set to false to disable FastGeoStreaming (used in PIE and at cook time)."),
	ECVF_Default);

void UFastGeoWorldPartitionRuntimeCellTransformer::BeginDestroy()
{
	Super::BeginDestroy();

	if (GIsEditor && !IsTemplate())
	{
		USelection::SelectionChangedEvent.RemoveAll(this);
	}
}

bool UFastGeoWorldPartitionRuntimeCellTransformer::IsDebugMode() const
{
	return bDebugMode || UFastGeoWorldPartitionRuntimeCellTransformer::IsDebugModeEnabled;
}

void UFastGeoWorldPartitionRuntimeCellTransformer::OnSelectionChanged(UObject* Object)
{
	if (bDebugModeOnSelection && IsEnabled())
	{
		TArray<AActor*> SelectedActors;

		TFunction<void(AActor* InActor)> AddActorToSelection = [this, &SelectedActors, &AddActorToSelection](AActor* InActor)
		{
			if (!CanAlwaysIgnoreActor(InActor))
			{
				const ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(InActor);
				if (LevelInstance && LevelInstance->GetDesiredRuntimeBehavior() == ELevelInstanceRuntimeBehavior::Partitioned && InActor->GetWorld())
				{
					if (const ULevelInstanceSubsystem* LevelInstanceSubsystem = InActor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
					{
						LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstance, [&AddActorToSelection](AActor* Actor)
						{
							AddActorToSelection(Actor);
							return true;
						});
					}
				}
				else
				{
					SelectedActors.Add(InActor);
				}
			}
		};

		if (USelection* Selection = Cast<USelection>(Object))
		{
			for (int32 Index = 0; Index < Selection->Num(); Index++)
			{
				if (AActor* SelectedActor = Cast<AActor>(Selection->GetSelectedObject(Index)))
				{
					AddActorToSelection(SelectedActor);
				}
			}
		}

		if (!SelectedActors.IsEmpty())
		{
			TGuardValue<bool> Guard(FFastGeoTransformResult::ShouldReport, true);

			UE_LOG(LogFastGeoStreaming, Log, TEXT("------------------------------------------------------------------------"));
			UE_LOG(LogFastGeoStreaming, Log, TEXT("- FastGeoStreaming Debug Mode: Transformation on %d selected actors "), SelectedActors.Num());

			FTransformationStats Stats;
			TMap<AActor*, FTransformableActor> TransformableActors;

			GatherTransformableActors(SelectedActors, SelectedActors[0]->GetLevel(), TransformableActors, Stats);

			Stats.DumpStats(TEXT("  - "));

			if (!FPhysScene::SupportsAsyncPhysicsStateCreation() ||
				!FPhysScene::SupportsAsyncPhysicsStateDestruction())
			{
				UE_LOG(LogFastGeoStreaming, Warning, TEXT(" - NOTE: FastGeoStreaming requires 'p.Chaos.EnableAsyncInitBody' to be enabled."));
			}

			UE_LOG(LogFastGeoStreaming, Log, TEXT("------------------------------------------------------------------------"));
		}
	}
}

bool UFastGeoWorldPartitionRuntimeCellTransformer::CanAlwaysIgnoreActor(AActor* InActor) const
{
	return InActor->IsA<AWorldSettings>() ||
		InActor->IsA<AWorldDataLayers>() ||
		InActor->IsA<ALevelInstanceEditorInstanceActor>() ||
		InActor->Implements<ULevelInstanceEditorPivotInterface>() ||
		FActorEditorUtils::IsABuilderBrush(InActor);
}

void UFastGeoWorldPartitionRuntimeCellTransformer::Transform(ULevel* InLevel)
{
	TGuardValue<bool> Guard(FFastGeoTransformResult::ShouldReport, IsDebugMode());

	if (!UFastGeoWorldPartitionRuntimeCellTransformer::IsFastGeoEnabled)
	{
		return;
	}

	if (!FPhysScene::SupportsAsyncPhysicsStateCreation() || 
		!FPhysScene::SupportsAsyncPhysicsStateDestruction())
	{
		UE_LOG(LogFastGeoStreaming, Error, TEXT("FastGeoStreaming Cell Transformer requires 'p.Chaos.EnableAsyncInitBody' to be enabled."));
		return;
	}

	check(InLevel);
	
	if (IsDebugMode())
	{
		UE_LOG(LogFastGeoStreaming, Log, TEXT("------------------------------------------------------------------------"));
		UE_LOG(LogFastGeoStreaming, Log, TEXT("- FastGeoStreaming Debug Mode: Transforming Level '%s'"), *InLevel->GetPathName());
	}
	
	FTransformationStats Stats;
	TMap<AActor*, FTransformableActor> TransformableActors;
	GatherTransformableActors(InLevel->Actors, InLevel, TransformableActors, Stats);

	if (!TransformableActors.IsEmpty())
	{
		const FString CellName = InLevel->GetWorldPartitionRuntimeCell() ? Cast<UObject>(InLevel->GetWorldPartitionRuntimeCell())->GetName() : TEXT("Cell");
		UFastGeoContainer* FastGeo = NewObject<UFastGeoContainer>(InLevel, *FString::Printf(TEXT("FastGeoContainer_%s"), *CellName));
		InLevel->AddAssetUserData(FastGeo);

		TUniquePtr<FFastGeoComponentCluster> LevelComponentCluster(new FFastGeoComponentCluster(FastGeo, *FString::Printf(TEXT("FastGeoComponentCluster_%s"), *CellName)));

		for (TMap<AActor*, FTransformableActor>::TConstIterator It = TransformableActors.CreateConstIterator(); It; ++It)
		{
			const TPair<AActor*, FTransformableActor>& Entry = *It;
			AActor* Actor = Entry.Key;

			FFastGeoComponentCluster* CurrentComponentCluster = LevelComponentCluster.Get();
			TUniquePtr<FFastGeoHLOD> FastGeoHLOD;
			if (AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(Actor))
			{
				FastGeoHLOD.Reset(new FFastGeoHLOD(FastGeo, *FString::Printf(TEXT("FastGeoHLOD_%s"), *Actor->GetName())));
				FastGeoHLOD->SetSourceCellGuid(HLODActor->GetSourceCellGuid());
				FastGeoHLOD->SetRequireWarmup(HLODActor->DoesRequireWarmup());
				FastGeoHLOD->SetStandaloneHLODGuid(HLODActor->GetStandaloneHLODGuid());
				CurrentComponentCluster = FastGeoHLOD.Get();
			}

			const TArray<UActorComponent*>& Components = Entry.Value.TransformableComponents;
			check(!Components.IsEmpty());
			for (UActorComponent* Component : Components)
			{
				FFastGeoElementType FastGeoComponentType = FastGeo::GetFastGeoComponentType(Component->GetClass());
				check(FastGeoComponentType.IsValid());

				FFastGeoComponent& FastGeoComponent = CurrentComponentCluster->AddComponent(FastGeoComponentType);
				FastGeoComponent.InitializeFromComponent(Component);

				// Remove the component from the actor
				Actor->RemoveOwnedComponent(Component);
				Component->MarkAsGarbage();
			}

			const bool bIsActorFullyTransformed = Entry.Value.bIsActorFullyTransformable;
			if (bIsActorFullyTransformed)
			{
				InLevel->Actors[Entry.Value.ActorIndex] = nullptr;
			}
			else if (USceneComponent* OldRootComponent = Actor->GetRootComponent(); OldRootComponent && !IsValid(OldRootComponent))
			{
				// Replace removed root component with a SceneComponent and remap attachment of other components
				USceneComponent* NewRootComponent = NewObject<USceneComponent>(Actor);
				NewRootComponent->SetRelativeTransform(OldRootComponent->GetRelativeTransform());
				Actor->SetRootComponent(NewRootComponent);

				Actor->ForEachComponent<USceneComponent>(false, [OldRootComponent, NewRootComponent](USceneComponent* Component)
				{
					if (Component->GetAttachParent() == OldRootComponent)
					{
						Component->SetupAttachment(NewRootComponent);
					}
				});
			}

			if (FastGeoHLOD.IsValid())
			{
				check(FastGeoHLOD->HasComponents());
				FastGeo->AddComponentCluster(FastGeoHLOD.Get());
			}
		}

		// Add level component cluster (if not empty)
		if (LevelComponentCluster->HasComponents())
		{
			FastGeo->AddComponentCluster(LevelComponentCluster.Get());
		}

		// Finalize post-creation intialization
		FastGeo->OnCreated();
	}

	InLevel->Actors.Remove(nullptr);

	if (IsDebugMode())
	{
		UE_LOG(LogFastGeoStreaming, Log, TEXT("- Transformation result of Level '%s'"), *InLevel->GetPathName());
		Stats.DumpStats(TEXT("  - "));
		UE_LOG(LogFastGeoStreaming, Log, TEXT("------------------------------------------------------------------------"));
	}
}

TMap<AActor*, TArray<AActor*>> UFastGeoWorldPartitionRuntimeCellTransformer::BuildActorsReferencesMap(const TArray<AActor*>& InActors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldPartitionRuntimeCellTransformer::BuildActorsReferencesMap);

	TMap<AActor*, TArray<AActor*>> ReferencedActors;
	TSet<UObject*> VisitedObjects;
	
	// Visit all actors properties and look for references to other actors
	for (AActor* ReferencingActor : InActors)
	{
		if (!IsValid(ReferencingActor))
		{
			continue;
		}

		if (CanAlwaysIgnoreActor(ReferencingActor))
		{
			continue;
		}

		VisitedObjects.Reset();
		VisitedObjects.Add(ReferencingActor);

		ReferencingActor->GetClass()->Visit(ReferencingActor, [&ReferencingActor, &ReferencedActors, &VisitedObjects](const FPropertyVisitorContext& Context) -> EPropertyVisitorControlFlow
		{
			const FPropertyVisitorPath& Path = Context.Path;
			const FPropertyVisitorData& Data = Context.Data;
			const FProperty* Property = Path.Top().Property;

			// Step over editor only properties
			if (Property->IsEditorOnlyProperty())
			{
				return EPropertyVisitorControlFlow::StepOver;
			}

			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				if (UObject* PropertyObject = ObjectProperty->GetObjectPropertyValue(Data.PropertyData))
				{
					bool bWasAlreadyInSet;
					VisitedObjects.Add(PropertyObject, &bWasAlreadyInSet);

					if (bWasAlreadyInSet)
					{
						return EPropertyVisitorControlFlow::StepOver;
					}

					AActor* ReferencedActor = Cast<AActor>(PropertyObject);
					if (!ReferencedActor)
					{
						ReferencedActor = PropertyObject->GetTypedOuter<AActor>();
					}

					if (ReferencedActor && !ReferencedActor->HasAnyFlags(RF_ClassDefaultObject) && ReferencedActor != ReferencingActor)
					{
						ReferencedActors.FindOrAdd(ReferencedActor).Add(ReferencingActor);
					}

					// Constrain visitor to properties of objects that have ReferencingActor in their outer chain
					if (!PropertyObject->IsIn(ReferencingActor))
					{
						return EPropertyVisitorControlFlow::StepOver;
					}
				}
			}

			return EPropertyVisitorControlFlow::StepInto;
		}, FPropertyVisitorContext::EScope::ObjectRefs);
	}

	return ReferencedActors;
}

void UFastGeoWorldPartitionRuntimeCellTransformer::GatherTransformableActors(const TArray<AActor*>& InActors, const ULevel* InLevel, TMap<AActor*, FTransformableActor>& OutTransformableActors, FTransformationStats& OutStats)
{
	// Get transformation result for each actor
	// This will retrieve the transformable components, and whether the actor is fully transformable (ie. if the actor can be deleted)
	for (int32 ActorIndex = 0; ActorIndex < InActors.Num(); ++ActorIndex)
	{
		AActor* Actor = InActors[ActorIndex];
		if (IsValid(Actor) && !CanAlwaysIgnoreActor(Actor))
		{
			OutStats.TotalActorCount++;
			OutStats.TotalComponentCount += Actor->GetComponents().Num();

			bool bIsActorFullyTransformable;
			TArray<UActorComponent*> TransformableComponents;
			FFastGeoTransformResult ActorTransformResult = CanTransformActor(Actor, bIsActorFullyTransformable, TransformableComponents);
			if (ActorTransformResult.GetResult() == EFastGeoTransform::Allow)
			{
				OutTransformableActors.FindOrAdd(Actor, { ActorIndex, bIsActorFullyTransformable, MoveTemp(TransformableComponents) });
			}
		}
	}

	TMap<AActor*, TArray<AActor*>> ReferencedActors = BuildActorsReferencesMap(InLevel->Actors);
	for (TMap<AActor*, FTransformableActor>::TIterator It = OutTransformableActors.CreateIterator(); It; ++It)
	{
		const TPair<AActor*, FTransformableActor>& Entry = *It;
		const AActor* Actor = Entry.Key;

		// Exclude actors that have:
		//  * Non FastGeo referencers
		//  * FastGeo referencers that are going to be only partially transformed
		if (const TArray<AActor*>* Referencers = ReferencedActors.Find(Actor))
		{
			const AActor* Referencer = nullptr;
			const FTransformableActor* TransformableReferencer = nullptr;
			bool bHasNonFastGeoReferencer = Algo::AnyOf(*Referencers, [&Referencer, &TransformableReferencer, OutTransformableActors](const AActor* InReferencer)
			{
				Referencer = InReferencer;
				TransformableReferencer = OutTransformableActors.Find(InReferencer);
				return TransformableReferencer == nullptr || !TransformableReferencer->bIsActorFullyTransformable;
			});

			// If one of the referencer is not fully transformed to FastGeo
			if (bHasNonFastGeoReferencer)
			{
				if (TransformableReferencer)
				{
					FFastGeoTransformResult(EFastGeoTransform::Reject, [Actor, TransformableReferencer, &InActors] { return FString::Printf(TEXT("Actor '%s' is referenced by a non-fully transformed actor ('%s')"), *Actor->GetName(), *InActors[TransformableReferencer->ActorIndex]->GetName()); });
				}
				else
				{
					FFastGeoTransformResult(EFastGeoTransform::Reject, [Actor, &Referencer] { return FString::Printf(TEXT("Actor '%s' is referenced by a non transformed actor ('%s')"), *Actor->GetName(), *Referencer->GetName()); });
				}

				It.RemoveCurrent();
				continue;
			}
		}

		OutStats.FullyTransformableActorCount += Entry.Value.bIsActorFullyTransformable ? 1 : 0;
		OutStats.PartiallyTransformableActorCount += !Entry.Value.bIsActorFullyTransformable ? 1 : 0;
		OutStats.TransformedComponentCount += Entry.Value.TransformableComponents.Num();
	}
}

void UFastGeoWorldPartitionRuntimeCellTransformer::FTransformationStats::DumpStats(const TCHAR* InPrefixString)
{
	if (TotalActorCount)
	{
		const float FullyTransformedActorPercentage = TotalActorCount > 0 ? (100.f * FullyTransformableActorCount) / TotalActorCount : 0.f;
		const float PartiallyTransformedActorPercentage = TotalActorCount > 0 ? (100.f * PartiallyTransformableActorCount) / TotalActorCount : 0.f;
		const float TransformedComponentPercentage = TotalComponentCount > 0 ? (100.f * TransformedComponentCount) / TotalComponentCount : 0.f;
		const int32 NonTransformableActorCount = FMath::Max(0, TotalActorCount - FullyTransformableActorCount - PartiallyTransformableActorCount);
		const float NonTransformableActorPercentage = NonTransformableActorCount > 0 ? (100.f * NonTransformableActorCount) / TotalActorCount : 0.f;

		UE_CLOG(FullyTransformableActorCount, LogFastGeoStreaming, Log, TEXT("%s Transformable Actors (Full)    = %d (%3.1f%%)"), InPrefixString, FullyTransformableActorCount, FullyTransformedActorPercentage);
		UE_CLOG(PartiallyTransformableActorCount, LogFastGeoStreaming, Log, TEXT("%s Transformable Actors (Partial) = %d (%3.1f%%)"), InPrefixString, PartiallyTransformableActorCount, PartiallyTransformedActorPercentage);
		UE_CLOG(TransformedComponentCount, LogFastGeoStreaming, Log, TEXT("%s Transformable Components       = %d (%3.1f%%)"), InPrefixString, TransformedComponentCount, TransformedComponentPercentage);
		UE_CLOG(NonTransformableActorCount, LogFastGeoStreaming, Log, TEXT("%s Non-Transformable Actors       = %d (%3.1f%%)"), InPrefixString, NonTransformableActorCount, NonTransformableActorPercentage);
	}
}

bool UFastGeoWorldPartitionRuntimeCellTransformer::IsBlueprintActorWithLogic(AActor* InActor) const
{
	const FName FN_UserConstructionScript(TEXT("UserConstructionScript"));

	UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(InActor->GetClass());
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return false;
	}

	check(Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(AActor::StaticClass()));

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	if (!BPClass)
	{
		return false;
	}

	if (Blueprint->DelegateSignatureGraphs.Num() > 0)
	{
		return true;
	}

	if (Blueprint->ImplementedInterfaces.Num() > 0)
	{
		return true;
	}

	// Check if no extra functions, other than the user construction script (only AActor and subclasses of AActor have)
	if (Blueprint->FunctionGraphs.Num() > 1)
	{
		return true;
	}

	check(Blueprint->FunctionGraphs.Num() == 0 || Blueprint->FunctionGraphs[0]->GetFName() == FN_UserConstructionScript);

	// Check if the generated class has overridden any functions dynamically
	for (TFieldIterator<UFunction> It(BPClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		UFunction* Function = *It;

		// Ignore functions from native C++ classes (i.e., inherited but not overridden in BP)
		if (Function->GetOwnerClass() == BPClass && Function->GetFName() != FN_UserConstructionScript)
		{
			return true; // Found an overridden function
		}
	}

	// If there is an enabled node in the event graph, the Blueprint is not data only
	for (UEdGraph* EventGraph : Blueprint->UbergraphPages)
	{
		for (UEdGraphNode* GraphNode : EventGraph->Nodes)
		{
			if (GraphNode && (GraphNode->GetDesiredEnabledState() != ENodeEnabledState::Disabled))
			{
				return true;
			}
		}
	}

	return false;
}

FFastGeoTransformResult UFastGeoWorldPartitionRuntimeCellTransformer::IsAllowedActorClass(AActor* InActor) const
{
	UClass* ActorClass = InActor->GetClass();
	for (TSubclassOf<AActor> DisallowedActorClass : DisallowedActorClasses)
	{
		if (ActorClass->IsChildOf(DisallowedActorClass))
		{
			return { EFastGeoTransform::Reject, [InActor, DisallowedActorClass] { return FString::Printf(TEXT("Actor %s class is child of a disallowed class (%s)"), *InActor->GetName(), *DisallowedActorClass->GetName()); } };
		}
	}

	for (TSubclassOf<AActor> DisallowedActorClass : BuiltinDisallowedActorClasses)
	{
		if (ActorClass->IsChildOf(DisallowedActorClass))
		{
			return { EFastGeoTransform::Reject, [InActor, DisallowedActorClass] { return FString::Printf(TEXT("Actor %s class is child of a built-in disallowed class (%s)"), *InActor->GetName(), *DisallowedActorClass->GetName()); } };
		}
	}

	for (TSubclassOf<AActor> DisallowedActorClass : DisallowedExactActorClasses)
	{
		if (ActorClass == DisallowedActorClass)
		{
			return { EFastGeoTransform::Reject, [InActor, DisallowedActorClass] { return FString::Printf(TEXT("Actor %s class is a disallowed exact class (%s)"), *InActor->GetName(), *DisallowedActorClass->GetName()); } };
		}
	}

	for (TSubclassOf<AActor> AllowedActorClass : AllowedActorClasses)
	{
		if (ActorClass->IsChildOf(AllowedActorClass))
		{
			return EFastGeoTransform::Allow;
		}
	}

	for (TSubclassOf<AActor> AllowedActorClass : BuiltinAllowedActorClasses)
	{
		if (ActorClass->IsChildOf(AllowedActorClass))
		{
			return EFastGeoTransform::Allow;
		}
	}

	for (TSubclassOf<AActor> AllowedActorClass : AllowedExactActorClasses)
	{
		if (ActorClass->IsChildOf(AllowedActorClass))
		{
			return EFastGeoTransform::Allow;
		}
	}

	// Special case where we allow an actor class if actor is tagged 'FastGeo'
	if (InActor->Tags.Contains(FastGeo::NAME_FastGeo))
	{
		return EFastGeoTransform::Allow;
	}

	return { EFastGeoTransform::Reject, [InActor, ActorClass] { return FString::Printf(TEXT("Actor %s class is an unsupported class (%s)"), *InActor->GetName(), *ActorClass->GetName()); } };
}

FFastGeoTransformResult UFastGeoWorldPartitionRuntimeCellTransformer::CanTransformActor(AActor* InActor, bool& bOutIsActorFullyTransformable, TArray<UActorComponent*>& OutTransformableComponents) const
{
	bOutIsActorFullyTransformable = false;

	FFastGeoTransformResult AllowedActorClassResult = IsAllowedActorClass(InActor);
	if (AllowedActorClassResult.GetResult() != EFastGeoTransform::Allow)
	{
		return AllowedActorClassResult;
	}

	FString Reason;
	
	if (InActor->ActorHasTag(NAME_CellTransformerIgnoreActor))
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is tagged '%s'"), *InActor->GetName(), *NAME_CellTransformerIgnoreActor.ToString()); } };
	}
	
	if (InActor->ActorHasTag(FastGeo::NAME_NoFastGeo))
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is tagged '%s'"), *InActor->GetName(), *FastGeo::NAME_NoFastGeo.ToString()); } };
	}
	
	if (!IsActorTransformable(InActor, Reason))
	{
		return { EFastGeoTransform::Reject, [InActor, Reason] { return FString::Printf(TEXT("Actor %s [%s]"), *InActor->GetName(), *Reason); } };
	}

	// Reject actors that are never going to be streamed
	// This test is only useful for the FastGeo debug mode, as normally during PIE & cook, only streamed actors will be processed by the cell transformer.
	if (!InActor->GetLevel()->GetWorldPartitionRuntimeCell())
	{
		if (!InActor->GetIsSpatiallyLoaded())
		{
			bool bHasRuntimeDataLayers = Algo::AnyOf(InActor->GetDataLayerInstances(), [](const UDataLayerInstance* DataLayer) { return DataLayer->IsRuntime(); });
			if (!bHasRuntimeDataLayers)
			{
				return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is non-spatially loaded"), *InActor->GetName()); } };
			}
		}
	}

	if (InActor->GetIsReplicated())
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is replicated"), *InActor->GetName()); } };
	}

	if (!InActor->IsRootComponentStatic())
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s RootComponent Mobility is not Static"), *InActor->GetName()); } };
	}

	if (InActor->IsEditorOnly())
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is editor-only"), *InActor->GetName()); } };
	}

	if (InActor->Children.Num())
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s has children"), *InActor->GetName()); } };
	}

	if (InActor->IsChildActor())
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is a child actor"), *InActor->GetName()); } };
	}

	if (IsBlueprintActorWithLogic(InActor))
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is a Blueprint Actor with logic"), *InActor->GetName()); } };
	}

	auto CanIgnoreRemainingComponent = [this](UActorComponent* InComponent)
	{
		UClass* ComponentClass = InComponent->GetClass();
		for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : IgnoredRemainingComponentClasses)
		{
			if (ComponentClass->IsChildOf(IgnoredComponentClass))
			{
				return true;
			}
		}

		for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : BuiltinIgnoredRemainingComponentClasses)
		{
			if (ComponentClass->IsChildOf(IgnoredComponentClass))
			{
				return true;
			}
		}

		for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : IgnoredRemainingExactComponentClasses)
		{
			if (ComponentClass == IgnoredComponentClass)
			{
				return true;
			}
		}

		for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : BuiltinIgnoredRemainingExactComponentClasses)
		{
			if (ComponentClass == IgnoredComponentClass)
			{
				return true;
			}
		}
		return false;
	};

	// Gather transformable components
	TArray<UActorComponent*> TransformResults[EnumToIndex(EFastGeoTransform::MAX)];
	InActor->ForEachComponent<UActorComponent>(false, [this, &TransformResults, CanIgnoreRemainingComponent](UActorComponent* Component)
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			FFastGeoTransformResult TransformComponentResult = CanTransformComponent(PrimitiveComponent);
			TransformResults[TransformComponentResult.GetResultIndex()].Add(Component);
		}
		else if (CanIgnoreRemainingComponent(Component))
		{
			FFastGeoTransformResult TransformComponentResult = { EFastGeoTransform::Discard };
			TransformResults[TransformComponentResult.GetResultIndex()].Add(Component);
		}
		else
		{
			FFastGeoTransformResult TransformComponentResult = { EFastGeoTransform::Reject, [Component] { return FString::Printf(TEXT("Component %s class %s is not allowed"), *FastGeo::GetComponentShortName(Component), *Component->GetClass()->GetName()); } };
			TransformResults[TransformComponentResult.GetResultIndex()].Add(Component);
		}
	});

	if (TransformResults[EnumToIndex(EFastGeoTransform::Allow)].IsEmpty())
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s has no transformable components"), *InActor->GetName()); } };
	}

	// If actor contains only discardable or transformable components, we can actually get rid of it
	bOutIsActorFullyTransformable = TransformResults[EnumToIndex(EFastGeoTransform::Reject)].IsEmpty();

	Reason.Reset();
	if (!IsFullyTransformedActorDeletable(InActor, Reason))
	{
		bOutIsActorFullyTransformable = false;
		UE_CLOG(FFastGeoTransformResult::ShouldReport, LogFastGeoStreaming, Log, TEXT("  * Can't delete fully transformed actor %s [%s]"), *InActor->GetName(), *Reason);
	}

	// Can't convert partially a BP actors
	// Rerun CS will be called in PIE when registering the component and
	// also called when registering components during cook/save of the level.
	if (UBlueprint::GetBlueprintFromClass(InActor->GetClass()) && !bOutIsActorFullyTransformable)
	{
		return { EFastGeoTransform::Reject, [InActor] { return FString::Printf(TEXT("Actor %s is a Blueprint and can't be fully transformed."), *InActor->GetName()); } };
	}

	OutTransformableComponents = MoveTemp(TransformResults[EnumToIndex(EFastGeoTransform::Allow)]);
	return EFastGeoTransform::Allow;
}

FFastGeoTransformResult UFastGeoWorldPartitionRuntimeCellTransformer::IsAllowedComponentClass(UActorComponent* InComponent) const
{
	FFastGeoElementType FastGeoComponentType = FastGeo::GetFastGeoComponentType(InComponent->GetClass());
	if (!FastGeoComponentType.IsValid())
	{
		return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Component %s class is unsupported (%s)"), *FastGeo::GetComponentShortName(InComponent), *InComponent->GetClass()->GetName()); } };
	}

	UClass* ComponentClass = InComponent->GetClass();
	for (TSubclassOf<UActorComponent> DisallowedComponentClass : DisallowedComponentClasses)
	{
		if (ComponentClass->IsChildOf(DisallowedComponentClass))
		{
			return { EFastGeoTransform::Reject, [InComponent, DisallowedComponentClass] { return FString::Printf(TEXT("Component %s class is child of a disallowed class (%s)"), *FastGeo::GetComponentShortName(InComponent), *DisallowedComponentClass->GetName()); } };
		}
	}

	for (TSubclassOf<UActorComponent> DisallowedComponentClass : BuiltinDisallowedComponentClasses)
	{
		if (ComponentClass->IsChildOf(DisallowedComponentClass))
		{
			return { EFastGeoTransform::Reject, [InComponent, DisallowedComponentClass] { return FString::Printf(TEXT("Component %s class is child of a built-in disallowed class (%s)"), *FastGeo::GetComponentShortName(InComponent), *DisallowedComponentClass->GetName()); } };
		}
	}

	for (TSubclassOf<UActorComponent> DisallowedComponentClass : DisallowedExactComponentClasses)
	{
		if (ComponentClass == DisallowedComponentClass)
		{
			return { EFastGeoTransform::Reject, [InComponent, DisallowedComponentClass] { return FString::Printf(TEXT("Component %s class is a disallowed exact class (%s)"), *FastGeo::GetComponentShortName(InComponent), *DisallowedComponentClass->GetName()); } };
		}
	}

	for (TSubclassOf<UActorComponent> AllowedComponentClass : AllowedComponentClasses)
	{
		if (ComponentClass->IsChildOf(AllowedComponentClass))
		{
			return EFastGeoTransform::Allow;
		}
	}

	for (TSubclassOf<UActorComponent> AllowedComponentClass : BuiltinAllowedComponentClasses)
	{
		if (ComponentClass->IsChildOf(AllowedComponentClass))
		{
			return EFastGeoTransform::Allow;
		}
	}

	for (TSubclassOf<UActorComponent> AllowedComponentClass : AllowedExactComponentClasses)
	{
		if (ComponentClass == AllowedComponentClass)
		{
			return EFastGeoTransform::Allow;
		}
	}

	return { EFastGeoTransform::Reject, [InComponent, ComponentClass] { return FString::Printf(TEXT("Component %s class is an unsupported class (%s)"), *FastGeo::GetComponentShortName(InComponent), *ComponentClass->GetName()); } };
};

FFastGeoTransformResult UFastGeoWorldPartitionRuntimeCellTransformer::CanTransformComponent(UPrimitiveComponent* InComponent) const
{
	FFastGeoTransformResult AllowedComponentClassResult = IsAllowedComponentClass(InComponent);
	if (AllowedComponentClassResult.GetResult() != EFastGeoTransform::Allow)
	{
		return AllowedComponentClassResult;
	}

	FString Reason;
	if (!IsComponentTransformable(InComponent, Reason))
	{
		return { EFastGeoTransform::Reject, [InComponent, Reason] { return FString::Printf(TEXT("Component %s [%s]"), *FastGeo::GetComponentShortName(InComponent), *Reason); } };
	}

	if (InComponent->IsEditorOnly())
	{
		return { EFastGeoTransform::Discard, [InComponent] { return FString::Printf(TEXT("Component %s is editor-only"), *FastGeo::GetComponentShortName(InComponent)); } };
	}

	if (InComponent->GetLODParentPrimitive())
	{
		return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Component %s has a valid LOD Parent Primitive"), *FastGeo::GetComponentShortName(InComponent)); } };
	}

	if (InComponent->Mobility != EComponentMobility::Static)
	{
		return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Component %s Mobility is not Static"), *FastGeo::GetComponentShortName(InComponent)); } };
	}

	bool bIsComponentVisible = InComponent->IsVisible() && !InComponent->GetOwner()->IsHidden();
	bool bShouldAddToRenderScene = bIsComponentVisible || InComponent->bCastHiddenShadow || InComponent->bAffectIndirectLightingWhileHidden || InComponent->bRayTracingFarField;

	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(InComponent))
	{
		if (!StaticMeshComponent->GetStaticMesh())
		{
			return { EFastGeoTransform::Discard, [InComponent] { return FString::Printf(TEXT("Component %s has an invalid static mesh"), *FastGeo::GetComponentShortName(InComponent)); } };
		}

		// Make sure BodyInstance CollisionEnabled is updated first before testing below
		StaticMeshComponent->UpdateCollisionFromStaticMesh();
	}

	if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(InComponent))
	{
		if (InstancedStaticMeshComponent->GetNumInstances() == 0)
		{
			return { EFastGeoTransform::Discard, [InComponent] { return FString::Printf(TEXT("Component %s has no instances"), *FastGeo::GetComponentShortName(InComponent)); } };
		}
	}

	if (UHierarchicalInstancedStaticMeshComponent* HierarchicalInstancedStaticMeshComponent = Cast<UHierarchicalInstancedStaticMeshComponent>(InComponent))
	{
		// FastGeo doesn't really support HISMC. These components get converted to ISMC.
		// However, we can afford to convert nanite HISMC as all the LODing logic is performed by Nanite.
		// We also allow the transformation of HISMC which are using a mesh with a single LOD - so in effect it's handled as an ISMC.
		if (!HierarchicalInstancedStaticMeshComponent->GetStaticMesh()->IsNaniteEnabled() || HierarchicalInstancedStaticMeshComponent->IsForceDisableNanite())
		{
			if (HierarchicalInstancedStaticMeshComponent->GetStaticMesh()->GetNumLODs() > 1)
			{
				return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Hierarchical instanced static mesh component %s has multiple LODs"), *FastGeo::GetComponentShortName(InComponent)); } };
			}
		}
	}

	if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(InComponent))
	{
		if (!SkinnedMeshComponent->GetSkinnedAsset())
		{
			return { EFastGeoTransform::Discard, [InComponent] { return FString::Printf(TEXT("Skinned mesh component %s has an invalid skinned asset"), *FastGeo::GetComponentShortName(InComponent)); } };
		}

		if (SkinnedMeshComponent->LeaderPoseComponent.IsValid())
		{
			return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Skinned mesh component %s has a leader pose component"), *FastGeo::GetComponentShortName(InComponent)); } };
		}

		if (SkinnedMeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Skinned mesh component %s has collisions enabled"), *FastGeo::GetComponentShortName(InComponent)); } };
		}

		if (SkinnedMeshComponent->IsNavigationRelevant())
		{
			return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Skinned mesh component %s is navigation relevant"), *FastGeo::GetComponentShortName(InComponent)); } };
		}

		bShouldAddToRenderScene &= !SkinnedMeshComponent->bHideSkin;
	}

	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InComponent))
	{
		if ((SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::AnimationSingleNode && SkeletalMeshComponent->AnimationData.AnimToPlay != nullptr) ||
			(SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::AnimationBlueprint && SkeletalMeshComponent->AnimClass != nullptr) ||
			(SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::AnimationCustomMode))
		{
			return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Skeletal mesh component %s is animated"), *FastGeo::GetComponentShortName(InComponent)); } };
		}
	}

	if (UInstancedSkinnedMeshComponent* InstancedSkinnedMeshComponent = Cast<UInstancedSkinnedMeshComponent>(InComponent))
	{
		for (const FAnimBankItem& AnimBankItem : InstancedSkinnedMeshComponent->GetAnimBankItems())
		{
			if (AnimBankItem.BankAsset != nullptr)
			{
				return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Instanced skinned mesh component %s is animated"), *FastGeo::GetComponentShortName(InComponent)); } };
			}
		}

		if (InstancedSkinnedMeshComponent->GetInstanceCount() == 0)
		{
			return { EFastGeoTransform::Discard, [InComponent] { return FString::Printf(TEXT("Instanced skinned mesh component %s has no instances"), *FastGeo::GetComponentShortName(InComponent)); } };
		}
	}

	const bool bIsCollisionEnabled = FastGeo::IsCollisionEnabled(InComponent);

	// If collision is enabled, only allow if async physics state creation and destruction are supported 
	if (bIsCollisionEnabled && (!InComponent->AllowsAsyncPhysicsStateCreation() || !InComponent->AllowsAsyncPhysicsStateDestruction()))
	{
		return { EFastGeoTransform::Reject, [InComponent] { return FString::Printf(TEXT("Component %s has collision enabled but doesn't allow asynchronous physics state creation/destruction"), *FastGeo::GetComponentShortName(InComponent)); } };
	}

	// Disallow transform if collision is disabled and component doesn't need to be added to the render scene
	if (!bIsCollisionEnabled && !bShouldAddToRenderScene)
	{
		return { EFastGeoTransform::Discard, [InComponent] { return FString::Printf(TEXT("Component %s has no collision and is not visible"), *FastGeo::GetComponentShortName(InComponent)); } };
	}

	UClass* ComponentClass = InComponent->GetClass();
	for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : IgnoredRemainingComponentClasses)
	{
		if (ComponentClass->IsChildOf(IgnoredComponentClass))
		{
			return { EFastGeoTransform::Discard };
		}
	}

	for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : BuiltinIgnoredRemainingComponentClasses)
	{
		if (ComponentClass->IsChildOf(IgnoredComponentClass))
		{
			return { EFastGeoTransform::Discard };
		}
	}

	for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : IgnoredRemainingExactComponentClasses)
	{
		if (ComponentClass == IgnoredComponentClass)
		{
			return { EFastGeoTransform::Discard };
		}
	}

	for (const TSubclassOf<UActorComponent>& IgnoredComponentClass : BuiltinIgnoredRemainingExactComponentClasses)
	{
		if (ComponentClass == IgnoredComponentClass)
		{
			return { EFastGeoTransform::Discard };
		}
	}

	return EFastGeoTransform::Allow;
}

void UFastGeoWorldPartitionRuntimeCellTransformer::PreEditChange(FProperty* InPropertyAboutToChange)
{
	FastGeo::GPackageWasDirty = GetPackage()->IsDirty();
	Super::PreEditChange(InPropertyAboutToChange);
}

void UFastGeoWorldPartitionRuntimeCellTransformer::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UFastGeoWorldPartitionRuntimeCellTransformer, bDebugMode) || 
		 PropertyName == GET_MEMBER_NAME_CHECKED(UFastGeoWorldPartitionRuntimeCellTransformer, bDebugModeOnSelection)) && !FastGeo::GPackageWasDirty)
	{
		GetPackage()->ClearDirtyFlag();
	}
}

bool FFastGeoTransformResult::ShouldReport = false;

FFastGeoTransformResult::FFastGeoTransformResult(EFastGeoTransform InTransformResult, const TCHAR* InFailureReason)
	: TransformResult(InTransformResult)
{
	if (TransformResult != EFastGeoTransform::Allow && InFailureReason && ShouldReport)
	{
		UE_LOG(LogFastGeoStreaming, Log, TEXT("  * Can't transform: %s"), InFailureReason);
	}
}

FFastGeoTransformResult::FFastGeoTransformResult(EFastGeoTransform InTransformResult, TFunctionRef<FString()> InFailureReasonFunc)
	: TransformResult(InTransformResult)
{
	if (TransformResult != EFastGeoTransform::Allow && ShouldReport)
	{
		UE_LOG(LogFastGeoStreaming, Log, TEXT("  * Can't transform: %s"), *InFailureReasonFunc());
	}
}

#endif

#undef LOCTEXT_NAMESPACE
