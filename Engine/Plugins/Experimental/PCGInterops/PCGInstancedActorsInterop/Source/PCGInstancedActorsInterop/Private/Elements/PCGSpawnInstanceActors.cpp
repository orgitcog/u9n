// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSpawnInstancedActors.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGInstancedActorsResource.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "InstancedActorsSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "PCGSpawnInstanceActorsElement"

FPCGElementPtr UPCGSpawnInstancedActorsSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnInstancedActorsElement>();
}

TArray<FPCGPinProperties> UPCGSpawnInstancedActorsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& DependencyPin = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultExecutionDependencyLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true);
	DependencyPin.Usage = EPCGPinUsage::DependencyOnly;

	return PinProperties;
}

#if WITH_EDITOR
FText UPCGSpawnInstancedActorsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("SpawnInstancedActorsNodeTooltip", "Spawns instanced actors from the input data. Note that the actor classes should be previously registered and that this node does not work at runtime.");
}
#endif

bool FPCGSpawnInstancedActorsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnInstancedActorsElement::Execute);
#if WITH_EDITOR
	check(Context);

	const UPCGSpawnInstancedActorsSettings* Settings = Context->GetInputSettings<UPCGSpawnInstancedActorsSettings>();
	check(Settings);

	if (PCGHelpers::IsRuntimeOrPIE())
	{
		PCGLog::LogErrorOnGraph(LOCTEXT("CannotSpawnInstancedActorsAtRuntime", "It is not currently supported to spawn instanced actors at runtime."), Context);
		return true;
	}

	TSubclassOf<AActor> ActorSubclass = Settings->ActorClass;

	UWorld* World = Context->ExecutionSource.Get() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	UInstancedActorsSubsystem* IASubsystem = World ? UInstancedActorsSubsystem::Get(World) : nullptr;

	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());

	if (!IASubsystem || !SourceComponent)
	{
		return true;
	}

	// Early out if this will fail completely.
	if (!Settings->bSpawnByAttribute && !ActorSubclass)
	{
		if (!Settings->bMuteOnEmptyClass)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("InvalidActorSubclass", "Invalid actor subclass, nothing will be spawned."), Context);
		}
		
		return true;
	}

	TArray<FSoftClassPath> ActorClassPaths;
	TMap<FSoftClassPath, TSubclassOf<AActor>> ActorClassesMap;
	const bool bUseActorClassesMap = Settings->bSpawnByAttribute;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FInstancedActorsInstanceHandle> Handles;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGBasePointData* Data = Cast<UPCGBasePointData>(Input.Data);

		if (!Data)
		{
			continue;
		}

		const FPCGAttributePropertyInputSelector Selector = Settings->SpawnAttributeSelector.CopyAndFixLast(Data);
		ActorClassPaths.Reset();

		if (Settings->bSpawnByAttribute)
		{
			if (!PCGAttributeAccessorHelpers::ExtractAllValues(Data, Selector, ActorClassPaths, Context))
			{
				continue;
			}

			// Resolve all paths to their classes.
			for (const FSoftClassPath& ClassPath : ActorClassPaths)
			{
				if (!ActorClassesMap.Contains(ClassPath))
				{
					TSubclassOf<AActor> LoadedActorClass = ClassPath.TryLoadClass<AActor>();
					ActorClassesMap.Add(ClassPath, LoadedActorClass);

					if (!LoadedActorClass && !Settings->bMuteOnEmptyClass)
					{
						PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("InvalidLoadedActorClass", "Invalid loaded actor class from path '{0}'."), FText::FromString(ClassPath.ToString())), Context);
					}
				}
			}
		}

		// For every point, if the subclass isn't invalid, create the instanced actor.
		TConstPCGValueRange<FTransform> PointTransforms = Data->GetConstTransformValueRange();

		for (int32 Index = 0; Index < PointTransforms.Num(); ++Index)
		{
			const FTransform& CurrentTransform = PointTransforms[Index];
			TSubclassOf<AActor> CurrentActorClass = bUseActorClassesMap ? ActorClassesMap[ActorClassPaths[Index]] : ActorSubclass;
			if (CurrentActorClass)
			{
				FInstancedActorsInstanceHandle Handle = IASubsystem->InstanceActor(CurrentActorClass, CurrentTransform, SourceComponent->GetOwner()->GetLevel());

				if (Handle.IsValid())
				{
					Handles.Add(Handle);
				}
			}
		}
	}

	// Finally, if we had valid handles, create a resource and set the handles to it.
	if (!Handles.IsEmpty())
	{
		UPCGInstancedActorsManagedResource* ManagedInstances = FPCGContext::NewObject_AnyThread<UPCGInstancedActorsManagedResource>(Context, SourceComponent);

		ManagedInstances->Handles = std::move(Handles);
		SourceComponent->AddToManagedResources(ManagedInstances);
	}
#else
	PCGLog::LogErrorOnGraph(LOCTEXT("InstancedActorsCannotSpawnInNonEditorBuilds", "Instanced actors cannot be spawned in non-editor builds."), Context);
#endif
	return true;
}

#undef LOCTEXT_NAMESPACE