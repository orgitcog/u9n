// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateObject.h"
#include "PropertyBindingDataView.h"
#include "SceneStateEvent.h"
#include "SceneStateEventStream.h"
#include "SceneStateExecutionContextRegistry.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStatePlayer.h"

USceneStateObject::USceneStateObject()
{
	ContextRegistry = MakeShared<UE::SceneState::FExecutionContextRegistry>();

	EventStream = CreateDefaultSubobject<USceneStateEventStream>(TEXT("EventStream"));
}

FString USceneStateObject::GetContextName() const
{
	if (USceneStatePlayer* Player = Cast<USceneStatePlayer>(GetOuter()))
	{
		return Player->GetContextName();
	}
	return FString();
}

UObject* USceneStateObject::GetContextObject() const
{
	if (USceneStatePlayer* Player = Cast<USceneStatePlayer>(GetOuter()))
	{
		return Player->GetContextObject();
	}
	return nullptr;
}

bool USceneStateObject::IsActive() const
{
	if (GeneratedClass)
	{
		if (const FSceneState* RootState = GeneratedClass->GetRootState())
		{
			const FSceneStateInstance* RootStateInstance = RootExecutionContext.FindStateInstance(*RootState);
			return RootStateInstance && RootStateInstance->Status == UE::SceneState::EExecutionStatus::Running;
		}
	}
	return false;
}

void USceneStateObject::Setup()
{
	GeneratedClass = Cast<USceneStateGeneratedClass>(GetClass());
	RootExecutionContext.Setup(this);
}

void USceneStateObject::Enter()
{
	if (EventStream)
	{
		EventStream->Register();
	}

	if (GeneratedClass)
	{
		if (const FSceneState* RootState = GeneratedClass->GetRootState())
		{
			ReceiveEnter();
			RootState->Enter(RootExecutionContext);
		}
	}
}

void USceneStateObject::Tick(float InDeltaSeconds)
{
	if (!GeneratedClass)
	{
		return;
	}

	if (const FSceneState* RootState = GeneratedClass->GetRootState())
	{
		ReceiveTick(InDeltaSeconds);
		RootState->Tick(RootExecutionContext, InDeltaSeconds);
	}
}

void USceneStateObject::Exit()
{
	if (GeneratedClass)
	{
		if (const FSceneState* RootState = GeneratedClass->GetRootState())
		{
			ReceiveExit();
			RootState->Exit(RootExecutionContext);
		}
	}

	if (EventStream)
	{
		EventStream->Unregister();
	}

	RootExecutionContext.Reset();
}

TSharedRef<UE::SceneState::FExecutionContextRegistry> USceneStateObject::GetContextRegistry() const
{
	return ContextRegistry.ToSharedRef();
}

UWorld* USceneStateObject::GetWorld() const
{
	if (UObject* Context = GetContextObject())
	{
		return Context->GetWorld();
	}
	return nullptr;
}

void USceneStateObject::BeginDestroy()
{
	Super::BeginDestroy();
    RootExecutionContext.Reset();
}
