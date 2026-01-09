// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGInstancedActorsResource.h"

#include "PCGInstancedActorsInteropModule.h"
#include "InstancedActorsSubsystem.h"

#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInstancedActorsResource)

void UPCGInstancedActorsManagedResource::PostEditImport()
{
	// In this case, the managed actors won't be copied along the actor/component,
	// So we just have to "forget" the actors, leaving the ownership to the original actor only.
	Super::PostEditImport();
	Handles.Reset();
}

bool UPCGInstancedActorsManagedResource::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGInstancedActorsManagedResource::Release);
	// Note: this type of resource does not support soft reset because we don't have a good way to target specific instances wrt to raycasts (for rejection).

#if WITH_EDITOR
	// Implementation note: this will prevent any change on the resource when something changes at runtime - cleanup or refresh.
	// It's the lesser of multiple evils since this will not trigger ensures and preserves data as the user would expect it.
	if (PCGHelpers::IsRuntimeOrPIE())
	{
		return false;
	}

	for (const FInstancedActorsInstanceHandle& Handle : Handles)
	{
		UInstancedActorsSubsystem* IASubsystem = UInstancedActorsSubsystem::Get(Handle.GetManager());
		if (IASubsystem)
		{
			IASubsystem->RemoveActorInstance(Handle, /*bDestroyManagerIfEmpty=*/true);
		}
	}

	Handles.Reset();
#endif

	return true;
}

bool UPCGInstancedActorsManagedResource::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	return Handles.IsEmpty() && Super::ReleaseIfUnused(OutActorsToDelete);
}

bool UPCGInstancedActorsManagedResource::MoveResourceToNewActor(AActor* NewActor)
{
	Super::MoveResourceToNewActor(NewActor);

	for (FInstancedActorsInstanceHandle& Handle : Handles)
	{
		if (AActor* Manager = Handle.GetManager())
		{
			if (!Manager->IsAttachedTo(NewActor))
			{
				Manager->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				Manager->SetOwner(nullptr);
				Manager->AttachToActor(NewActor, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	return true;
}

void UPCGInstancedActorsManagedResource::MarkAsUsed()
{
	Super::MarkAsUsed();
	ensure(0);
}

#if WITH_EDITOR
void UPCGInstancedActorsManagedResource::MarkTransientOnLoad()
{
	UE_LOG(LogPCGInstancedActorsInterop, Warning, TEXT("Instanced actors cannot currently be marked as transient on load."));
	Super::MarkTransientOnLoad();
}

void UPCGInstancedActorsManagedResource::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	if (NewEditingMode != EPCGEditorDirtyMode::Normal)
	{
		UE_LOG(LogPCGInstancedActorsInterop, Warning, TEXT("Instanced actors cannot currently be marked as transient or load as preview. Will flush instances to prevent data corruption."));
		TSet<TSoftObjectPtr<AActor>> Dummy;
		Release(/*bHardRelease=*/true, Dummy);
	}

	Super::ChangeTransientState(NewEditingMode);
}
#endif