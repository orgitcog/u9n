// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DaySequenceModifierVolume.h"

#include "DaySequenceActor.h"
#include "DaySequenceModifierComponent.h"
#include "DaySequenceSubsystem.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace UE::DaySequence
{
	static TAutoConsoleVariable<bool> CVarModifierVolumeEnableSplitscreenSupport(
	TEXT("DaySequence.ModifierVolume.EnableSplitscreenSupport"),
	true,
	TEXT("When true, Day Sequence Modifier Volumes attempt to initialize transient modifier components for all local players."),
	ECVF_Default
	);
}

ADaySequenceModifierVolume::ADaySequenceModifierVolume(const FObjectInitializer& Init)
: Super(Init)
, bEnableSplitscreenSupport(false)
{
	PrimaryActorTick.bCanEverTick = true;
	
	DaySequenceModifier = CreateDefaultSubobject<UDaySequenceModifierComponent>(TEXT("DaySequenceModifier"));
	DaySequenceModifier->SetupAttachment(RootComponent);

	DefaultBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	DefaultBox->SetupAttachment(DaySequenceModifier);
	DefaultBox->SetLineThickness(10.f);
	DefaultBox->SetBoxExtent(FVector(500.f));

	FComponentReference DefaultBoxReference;
	DefaultBoxReference.ComponentProperty = TEXT("DefaultBox");
	DaySequenceModifier->AddVolumeShapeComponent(DefaultBoxReference);
}

void ADaySequenceModifierVolume::BeginPlay()	
{
	Super::BeginPlay();

	Initialize();
}

void ADaySequenceModifierVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deinitialize();
	
	Super::EndPlay(EndPlayReason);
}

void ADaySequenceModifierVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	Initialize();
}

void ADaySequenceModifierVolume::Initialize()
{
	if (IsTemplate())
	{
		return;
	}
	
	// This actor should only initialize on the client.
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorEnableCollision(false);
		return;
	}

	if (const UWorld* World = GetWorld())
	{
#if WITH_EDITOR
		if (World->WorldType == EWorldType::Editor)
		{
			DaySequenceActor = nullptr;
			if (IsValid(DaySequenceModifier))
			{
				DaySequenceModifier->UnbindFromDaySequenceActor();
			}
		}
#endif

		DaySequenceActorSetup();
		
		if (World->IsGameWorld())
		{
			auto HandleNewPlayerController = [this](APlayerController* PlayerController)
			{
				if (PlayerController->IsLocalController())
				{
					CreatePlayer(PlayerController);
				}
			};

			// Create players that exist initially. We may not find anything here, but the actor spawned handler below should handle anything we miss.
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				if (APlayerController* PlayerController = Iterator->Get())
				{
					HandleNewPlayerController(PlayerController);
				}
			}

			ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateWeakLambda(this, [HandleNewPlayerController](AActor* SpawnedActor)
			{
				if (APlayerController* PlayerController = Cast<APlayerController>(SpawnedActor))
				{
					HandleNewPlayerController(PlayerController);
				}
			}));
		}

		if (World->IsPlayingReplay())
		{
			ReplayScrubbedHandle = FNetworkReplayDelegates::OnReplayScrubComplete.AddWeakLambda(this, [this](const UWorld* InWorld)
			{
				if (InWorld == GetWorld())
				{
					DaySequenceActorSetup();
				}
			});
		}
	}
}

void ADaySequenceModifierVolume::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
			ActorSpawnedHandle.Reset();
		}
		
		if (ReplayScrubbedHandle.IsValid())
		{
			FNetworkReplayDelegates::OnReplayScrubComplete.Remove(ReplayScrubbedHandle);
			ReplayScrubbedHandle.Reset();
		}
	}
}

void ADaySequenceModifierVolume::DaySequenceActorSetup()
{
	SetupDaySequenceSubsystemCallbacks();
	BindToDaySequenceActor();
}

void ADaySequenceModifierVolume::BindToDaySequenceActor()
{
	auto RebindAllModifiers = [this]()
	{
		DaySequenceModifier->BindToDaySequenceActor(DaySequenceActor);
		for (auto AdditionalPlayerIterator = AdditionalPlayers.CreateIterator(); AdditionalPlayerIterator; ++AdditionalPlayerIterator)
		{
			UDaySequenceModifierComponent* ModifierComponent = AdditionalPlayerIterator->Value;
			ModifierComponent->BindToDaySequenceActor(DaySequenceActor);
		}

		OnDaySequenceActorBound(DaySequenceActor);
	};
	
	if (const UWorld* World = GetWorld())
	{
		if (const UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			if (ADaySequenceActor* NewActor = DaySequenceSubsystem->GetDaySequenceActor())
			{
				if (NewActor != DaySequenceActor)
				{
					DaySequenceActor = NewActor;
					RebindAllModifiers();
				}
			}
		}
	}
}

void ADaySequenceModifierVolume::SetupDaySequenceSubsystemCallbacks()
{
	if (const UWorld* World = GetWorld())
	{
		if (UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			// Prevent consecutive calls to this function from adding redundant lambdas to invocation list.
			if (!DaySequenceSubsystem->OnDaySequenceActorSetEvent.IsBoundToObject(this))
			{
				DaySequenceSubsystem->OnDaySequenceActorSetEvent.AddWeakLambda(this, [this](ADaySequenceActor* InActor)
				{
					BindToDaySequenceActor();
				});
			}
		}
	}
}

void ADaySequenceModifierVolume::CreatePlayer(APlayerController* InPC)
{
	if (CachedPlayerController == InPC || AdditionalPlayers.Find(InPC))
	{
		return;
	}

	UDaySequenceModifierComponent* PlayerModifier;
	
	if (CachedPlayerController == nullptr)
	{
		CachedPlayerController = InPC;
		PlayerModifier = DaySequenceModifier;
	}
	else if (IsSplitscreenSupported())
	{
        PlayerModifier = DuplicateObject<UDaySequenceModifierComponent>(DaySequenceModifier, this, TEXT("AdditionalPlayerModifier"));
		AdditionalPlayers.FindOrAdd(InPC) = PlayerModifier;
	}
	else
	{
		// If CachedPlayerController is not null (points to the first local player) and we don't support splitscreen, just bail out.
		return;
	}

	if (ensureMsgf(PlayerModifier, TEXT("PlayerModifier is nullptr!")))
	{
		if (!PlayerModifier->IsRegistered())
		{
			// This happens for duplicated modifiers.
			PlayerModifier->RegisterComponent();
			PlayerModifier->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			PlayerModifier->SetBias(DaySequenceModifier->GetBias() + AdditionalPlayers.Num());
		}

		PlayerModifier->SetBlendTarget(InPC);
		PlayerModifier->BindToDaySequenceActor(DaySequenceActor);
	}
}

bool ADaySequenceModifierVolume::IsSplitscreenSupported() const
{
	return bEnableSplitscreenSupport && UE::DaySequence::CVarModifierVolumeEnableSplitscreenSupport.GetValueOnAnyThread();
}