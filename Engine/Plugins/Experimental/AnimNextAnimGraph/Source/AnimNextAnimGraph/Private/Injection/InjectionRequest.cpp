// Copyright Epic Games, Inc. All Rights Reserved.

#include "Injection/InjectionRequest.h"

#include "Injection/InjectionEvents.h"
#include "Component/AnimNextComponent.h"
#include "Component/AnimNextWorldSubsystem.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Logging/StructuredLog.h"
#include "Engine/World.h"

namespace UE::AnimNext
{
	bool FInjectionRequest::Inject(FInjectionRequestArgs&& InRequestArgs, FInjectionLifetimeEvents&& InLifetimeEvents, UObject* InHost, FModuleHandle InHandle)
	{
		check(IsInGameThread());
		if (InHost == nullptr || !InHandle.IsValid())
		{
			return false;	// Nothing to play
		}

		if (Status != EInjectionStatus::None)
		{
			return false;	// Already playing, cannot play again
		}

		if(!ValidateArgs(InRequestArgs))
		{
			return false;	// Invalid args
		}

		UWorld* World = InHost->GetWorld();
		if(World == nullptr)
		{
			return false;	// No world to play in
		}

		SetupBindingModuleDependencies(World, InHandle, InRequestArgs);

		RequestArgs = MoveTemp(InRequestArgs);
		LifetimeEvents = MoveTemp(InLifetimeEvents);
		WeakHost = InHost;
		WeakWorld = World;
		Handle = InHandle;
		Status = EInjectionStatus::Pending;

		auto InjectEvent = MakeTraitEvent<FInjection_InjectEvent>();
		InjectEvent->Request = AsShared();

		UAnimNextWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextWorldSubsystem>();
		check(Subsystem);
		Subsystem->QueueInputTraitEventHandle(InHandle, InjectEvent);

		InjectionEvent = InjectEvent;

		return true;
	}

	void FInjectionRequest::Uninject()
	{
		check(IsInGameThread());
		if (!EnumHasAnyFlags(Status, EInjectionStatus::Playing))
		{
			return;	// Not playing
		}
		else if (EnumHasAnyFlags(Status, EInjectionStatus::Interrupted))
		{
			return;	// We already got interrupted
		}

		// Cancel our (possibly persistent) event
		if (TSharedPtr<FAnimNextTraitEvent> PinnedInjectionEvent = InjectionEvent.Pin())
		{
			PinnedInjectionEvent->MarkConsumed();
			InjectionEvent = nullptr;
		}

		auto UninjectEvent = MakeTraitEvent<FInjection_UninjectEvent>();
		UninjectEvent->Request = AsShared();

		if (UWorld* World = WeakWorld.Get())
		{
			UAnimNextWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextWorldSubsystem>();
			check(Subsystem);
			Subsystem->QueueInputTraitEventHandle(Handle, UninjectEvent);
		}
	}

	bool FInjectionRequest::ValidateArgs(const FInjectionRequestArgs& InRequestArgs)
	{
		switch(InRequestArgs.Type)
		{
		case EAnimNextInjectionType::InjectObject:
			if(InRequestArgs.Site.DesiredSiteName == NAME_None)
			{
				UE_LOGFMT(LogAnimation, Warning, "FInjectionRequest::ValidateArgs: Missing injection site name");
				return false;
			}
			if(InRequestArgs.Object == nullptr)
			{
				UE_LOGFMT(LogAnimation, Warning, "FInjectionRequest::ValidateArgs: Missing object");
				return false;
			}
			break;
		case EAnimNextInjectionType::EvaluationModifier:
			if(InRequestArgs.Object != nullptr)
			{
				UE_LOGFMT(LogAnimation, Warning, "FInjectionRequest::ValidateArgs: Object provided when injecting evaluation modifier");
				return false;
			}
			break;
		}

		return true;
	}

	void FInjectionRequest::SetupBindingModuleDependencies(UWorld* InWorld, FModuleHandle InHandle, const FInjectionRequestArgs& InRequestArgs)
	{
		check(IsInGameThread());
		check(InWorld);

		if(InRequestArgs.BindingModuleHandle.IsValid())
		{
			UAnimNextWorldSubsystem* Subsystem = InWorld->GetSubsystem<UAnimNextWorldSubsystem>();
			check(Subsystem);
			Subsystem->AddModuleDependencyHandle(InRequestArgs.BindingModuleHandle, InHandle);
		}
	}

	void FInjectionRequest::RemoveBindingModuleDependencies(UWorld* InWorld, FModuleHandle InHandle, const FInjectionRequestArgs& InRequestArgs)
	{
		check(IsInGameThread());
		check(InWorld);

		if(InRequestArgs.BindingModuleHandle.IsValid())
		{
			UAnimNextWorldSubsystem* Subsystem = InWorld->GetSubsystem<UAnimNextWorldSubsystem>();
			check(Subsystem);
			Subsystem->RemoveModuleDependencyHandle(InRequestArgs.BindingModuleHandle, InHandle);
		}
	}

	const FInjectionRequestArgs& FInjectionRequest::GetArgs() const
	{
		return RequestArgs;
	}

	FInjectionRequestArgs& FInjectionRequest::GetMutableArgs()
	{
		return RequestArgs;
	}

	const FInjectionLifetimeEvents& FInjectionRequest::GetLifetimeEvents() const
	{
		return LifetimeEvents;
	}

	FInjectionLifetimeEvents& FInjectionRequest::GetMutableLifetimeEvents()
	{
		return LifetimeEvents;
	}

	EInjectionStatus FInjectionRequest::GetStatus() const
	{
		check(IsInGameThread());
		return Status;
	}

	const FTimelineState& FInjectionRequest::GetTimelineState() const
	{
		check(IsInGameThread());
		ensureMsgf(RequestArgs.bTrackTimelineProgress, TEXT("Attempting to query the timeline state of an injection request that isn't tracking the timeline progress"));
		return TimelineState;
	}

	bool FInjectionRequest::HasExpired() const
	{
		check(IsInGameThread());
		return EnumHasAnyFlags(Status, EInjectionStatus::Expired);
	}

	bool FInjectionRequest::HasCompleted() const
	{
		check(IsInGameThread());
		return EnumHasAnyFlags(Status, EInjectionStatus::Completed);
	}

	bool FInjectionRequest::IsPlaying() const
	{
		check(IsInGameThread());
		return EnumHasAnyFlags(Status, EInjectionStatus::Playing);
	}

	bool FInjectionRequest::IsBlendingOut() const
	{
		check(IsInGameThread());
		return EnumHasAnyFlags(Status, EInjectionStatus::BlendingOut);
	}

	bool FInjectionRequest::WasInterrupted() const
	{
		check(IsInGameThread());
		return EnumHasAnyFlags(Status, EInjectionStatus::Interrupted);
	}

	void FInjectionRequest::OnStatusUpdate(EInjectionStatus NewStatus)
	{
		check(IsInGameThread());
		switch (NewStatus)
		{
		case EInjectionStatus::Playing:
			ensureMsgf(Status == EInjectionStatus::Pending, TEXT("Expected Injection status to be pending, found: %u"), (uint32)Status);
			Status = NewStatus;
			LifetimeEvents.OnStarted.ExecuteIfBound(*this);
			break;
		case EInjectionStatus::Playing | EInjectionStatus::Interrupted:
			ensureMsgf(EnumHasAnyFlags(Status, EInjectionStatus::Playing), TEXT("Expected Injection status to be playing, found: %u"), (uint32)Status);
			EnumAddFlags(Status, EInjectionStatus::Interrupted);
			LifetimeEvents.OnInterrupted.ExecuteIfBound(*this);
			break;
		case EInjectionStatus::BlendingOut:
			ensureMsgf(EnumHasAnyFlags(Status, EInjectionStatus::Playing), TEXT("Expected Injection status to be playing, found: %u"), (uint32)Status);
			EnumAddFlags(Status, EInjectionStatus::BlendingOut);
			LifetimeEvents.OnBlendingOut.ExecuteIfBound(*this);
			break;
		case EInjectionStatus::Completed:
			ensureMsgf(EnumHasAnyFlags(Status, EInjectionStatus::Playing), TEXT("Expected Injection status to be playing, found: %u"), (uint32)Status);
			// Maintain our interrupted status if it was present
			Status = EInjectionStatus::Completed | (Status & EInjectionStatus::Interrupted);
			LifetimeEvents.OnCompleted.ExecuteIfBound(*this);
			// Remove any dependencies we have set up
			if (UWorld* World = WeakWorld.Get())
			{
				RemoveBindingModuleDependencies(World, Handle, RequestArgs);
			}
			break;
		case EInjectionStatus::Expired:
			ensureMsgf(Status == EInjectionStatus::Pending, TEXT("Expected Injection status to be pending, found: %u"), (uint32)Status);
			Status = NewStatus;
			LifetimeEvents.OnCompleted.ExecuteIfBound(*this);
			break;
		default:
			ensureMsgf(false, TEXT("Unsupported Injection status update value: %u"), (uint32)NewStatus);
			break;
		}
	}

	void FInjectionRequest::OnTimelineUpdate(const FTimelineState& NewTimelineState)
	{
		check(IsInGameThread());
		TimelineState = NewTimelineState;
	}

	void FInjectionRequest::ExternalAddReferencedObjects(FReferenceCollector& Collector)
	{
		AddReferencedObjects(Collector);
	}

	void FInjectionRequest::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddPropertyReferencesWithStructARO(FInjectionRequestArgs::StaticStruct(), &RequestArgs);
	}
}
