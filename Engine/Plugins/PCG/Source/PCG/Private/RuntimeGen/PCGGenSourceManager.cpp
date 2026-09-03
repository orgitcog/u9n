// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/PCGGenSourceManager.h"

#include "PCGModule.h"
#include "PCGWorldActor.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "RuntimeGen/GenSources/PCGGenSourceComponent.h"
#include "RuntimeGen/GenSources/PCGGenSourceEditorCamera.h"
#include "RuntimeGen/GenSources/PCGGenSourcePlayer.h"
#include "RuntimeGen/GenSources/PCGGenSourceWPStreamingSource.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

FPCGGenSourceManager::FPCGGenSourceManager(const UWorld* InWorld)
{
	// We capture the World so we can differentiate between editor-world and PIE-world Generation Sources.
	World = InWorld;

	// Collect PlayerControllers.
	FGameModeEvents::GameModePostLoginEvent.AddRaw(this, &FPCGGenSourceManager::OnGameModePostLogin);
	FGameModeEvents::GameModeLogoutEvent.AddRaw(this, &FPCGGenSourceManager::OnGameModePostLogout);

#if WITH_EDITOR
	EditorCameraGenSource = NewObject<UPCGGenSourceEditorCamera>();
#endif
}

FPCGGenSourceManager::~FPCGGenSourceManager()
{
	FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);
	FGameModeEvents::GameModeLogoutEvent.RemoveAll(this);
}

TSet<IPCGGenSourceBase*> FPCGGenSourceManager::GetAllGenSources(const APCGWorldActor* InPCGWorldActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGenSourceManager::GetAllGenSources);

	if (bDirty)
	{
		UpdatePerTickGenSources(InPCGWorldActor);
		bDirty = false;
	}

	TSet<IPCGGenSourceBase*> AllGenSources;
	AllGenSources.Reserve(RegisteredGenSources.Num() + RegisteredNamedGenSources.Num());

	for (TScriptInterface<IPCGGenSourceBase> GenSource : RegisteredGenSources)
	{
		AllGenSources.Add(GenSource.GetInterface());
	}

	for (const TPair<FName, TScriptInterface<IPCGGenSourceBase>>& NamedGenSource : RegisteredNamedGenSources)
	{
		AllGenSources.Add(NamedGenSource.Value.GetInterface());
	}

	// Acquire a generation source for each active streaming source that doesn't have a generation source already (based on name collision test).
	for (TObjectPtr<UPCGGenSourceWPStreamingSource> WPGenSource : WorldPartitionGenSources)
	{
		bool bAlreadyTracked = false;

		if (TScriptInterface<IPCGGenSourceBase>* FoundGenSource = (WPGenSource && WPGenSource->StreamingSource) ? RegisteredNamedGenSources.Find(WPGenSource->StreamingSource->Name) : nullptr)
		{
			// We've seen during cinematics there are two generation sources with the same name but with different locations in the world, so match based on position too.
			TOptional<FVector> WPGenSourcePosition = WPGenSource->GetPosition();
			TOptional<FVector> FoundGenSourcePosition = (*FoundGenSource)->GetPosition();
			if (WPGenSourcePosition && FoundGenSourcePosition && (*FoundGenSourcePosition - *WPGenSourcePosition).SquaredLength() < UE_KINDA_SMALL_NUMBER)
			{
				bAlreadyTracked = true;
			}
		}

		if (!bAlreadyTracked)
		{
			AllGenSources.Add(WPGenSource);
		}
	}

#if WITH_EDITOR
	// Acquire a generation source for the active editor viewport if one exists.
	if (EditorCameraGenSource && EditorCameraGenSource->EditorViewportClient)
	{
		AllGenSources.Add(EditorCameraGenSource);
	}
#endif

	return AllGenSources;
}

bool FPCGGenSourceManager::RegisterGenSource(IPCGGenSourceBase* InGenSource, FName InGenSourceName)
{
	if (UPCGGenSourceComponent* GenSourceComponent = Cast<UPCGGenSourceComponent>(InGenSource))
	{
		if (GenSourceComponent->GetWorld() != World)
		{
			return false;
		}
	}

	TScriptInterface<IPCGGenSourceBase> InterfacePtr(Cast<UObject>(InGenSource));
	if (InGenSourceName == NAME_None)
	{
		return RegisteredGenSources.Add(InterfacePtr).IsValidId();
	}
	else
	{
		if (RegisteredNamedGenSources.Find(InGenSourceName))
		{
			return false;
		}
		else
		{
			RegisteredNamedGenSources.Add(InGenSourceName, InterfacePtr);
			return true;
		}
	}
}

bool FPCGGenSourceManager::UnregisterGenSource(const IPCGGenSourceBase* InGenSource)
{
	if (const UPCGGenSourceComponent* GenSourceComponent = Cast<UPCGGenSourceComponent>(InGenSource))
	{
		if (GenSourceComponent->GetWorld() != World)
		{
			return false;
		}
	}

	// Start by removing from the unnamed gen sources.
	// @todo_pcg We could remove from named sources here too, but that might be limiting for more complex cases, BUT it might be less error-prone too.
	TScriptInterface<IPCGGenSourceBase> InterfacePtr(Cast<UObject>(const_cast<IPCGGenSourceBase*>(InGenSource)));
	if (RegisteredGenSources.Remove(InterfacePtr) > 0)
	{
		return true;
	}
	else // Then try to remove from the named sources
	{
		if (const FName* GenSourceNamePtr = RegisteredNamedGenSources.FindKey(InterfacePtr))
		{
			FName GenSourceName = *GenSourceNamePtr;
			return UnregisterGenSource(GenSourceName);
		}
	}

	return false;
}

bool FPCGGenSourceManager::UnregisterGenSource(FName InGenSourceName)
{
	// Note: unnamed generation sources can't be unregistered by name, by definition.
	return RegisteredNamedGenSources.Remove(InGenSourceName) > 0;
}

void FPCGGenSourceManager::OnGameModePostLogin(AGameModeBase* InGameMode, APlayerController* InPlayerController)
{
	if (InPlayerController == nullptr || InPlayerController->GetWorld() != World)
	{
		return;
	}

	ensure(IsInGameThread());

	UPCGGenSourcePlayer* GenSource = NewObject<UPCGGenSourcePlayer>();
	GenSource->SetPlayerController(InPlayerController);

	RegisterGenSource(GenSource, InPlayerController->GetFName());
}

void FPCGGenSourceManager::OnGameModePostLogout(AGameModeBase* InGameMode, AController* InController)
{
	if (!InController || InController->GetWorld() != World)
	{
		return;
	}

	ensure(IsInGameThread());

	TScriptInterface<IPCGGenSourceBase> GenSource = nullptr;
	if (RegisteredNamedGenSources.RemoveAndCopyValue(InController->GetFName(), GenSource))
	{
		check(GenSource);
		UPCGGenSourcePlayer* GenSourcePlayer = Cast<UPCGGenSourcePlayer>(GenSource.GetInterface());
		if (!GenSourcePlayer)
		{
			// The gen source we removed isn't a player controller gen source, so let's put it back.
			RegisteredNamedGenSources.Add(InController->GetFName(), GenSource);
		}
	}
}

void FPCGGenSourceManager::UpdatePerTickGenSources(const APCGWorldActor* InPCGWorldActor)
{
	// Start by updating, adding and removing WP generation sources if it's needed.
	// TODO: Grab StreamingSourceProviders instead of StreamingSources?
	// TODO: Is it possible to avoid adding a StreamingSource for the Player, which we already capture in OnGameModePostLogin?
	// Note: GetStreamingSources only works in GameWorld, so StreamingSources do not act as generation sources in editor.
	const TArray<FWorldPartitionStreamingSource>* WPStreamingSources = nullptr;

	if (InPCGWorldActor->bEnableWorldPartitionGenerationSources &&
		InPCGWorldActor->GetWorld() &&
		InPCGWorldActor->GetWorld()->GetWorldPartition())
	{
		WPStreamingSources = &InPCGWorldActor->GetWorld()->GetWorldPartition()->GetStreamingSources();
	}

	const int NumWPStreamingSources = WPStreamingSources ? WPStreamingSources->Num() : 0;
	const int NumWPGenerationSources = WorldPartitionGenSources.Num();
	const int NumGenerationSourcesToUpdate = FMath::Min(NumWPStreamingSources, NumWPGenerationSources);
	const int NumGenerationSourcesToRemove = FMath::Max(0, NumWPGenerationSources - NumWPStreamingSources);
	const int NumGenerationSourcesToAdd = FMath::Max(0, NumWPStreamingSources - NumWPGenerationSources);
	check((NumGenerationSourcesToRemove == 0 && NumGenerationSourcesToAdd == 0) || ((NumGenerationSourcesToRemove > 0) != (NumGenerationSourcesToAdd > 0)));
	
	for (int I = 0; I < NumGenerationSourcesToAdd; ++I)
	{
		UPCGGenSourceWPStreamingSource* GenSource = WorldPartitionGenSources.Add_GetRef(NewObject<UPCGGenSourceWPStreamingSource>());
	}
	
	check((NumGenerationSourcesToUpdate + NumGenerationSourcesToAdd == 0) || WPStreamingSources);
	// Implementation note: not really needed but we need to appease the static analysis gods
	if (WPStreamingSources)
	{
		for (int I = 0; I < NumGenerationSourcesToUpdate + NumGenerationSourcesToAdd; ++I)
		{
			WorldPartitionGenSources[I]->StreamingSource = &(*WPStreamingSources)[I];
		}
	}

	WorldPartitionGenSources.SetNum(NumGenerationSourcesToUpdate + NumGenerationSourcesToAdd);

#if WITH_EDITOR
	EditorCameraGenSource->EditorViewportClient = nullptr;

	// Update the active editor viewport client for the EditorCameraGenSource, only if requested by the world actor, in-editor, and the viewport is visible.
	if (InPCGWorldActor->bTreatEditorViewportAsGenerationSource && !World->IsGameWorld() && GEditor)
	{
		if (FViewport* Viewport = GEditor->GetActiveViewport())
		{
			if (FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient()))
			{
				if (ViewportClient->IsVisible())
				{
					EditorCameraGenSource->EditorViewportClient = ViewportClient;
				}
			}
		}
	}
#endif
}

void FPCGGenSourceManager::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_EDITOR
	Collector.AddReferencedObject(EditorCameraGenSource);
#endif
	Collector.AddReferencedObjects(WorldPartitionGenSources);

	for (TScriptInterface<IPCGGenSourceBase>& GenSource : RegisteredGenSources)
	{
		GenSource.AddReferencedObjects(Collector);
	}

	for (auto& KeyValuePair : RegisteredNamedGenSources)
	{
		KeyValuePair.Value.AddReferencedObjects(Collector);
	}
}
