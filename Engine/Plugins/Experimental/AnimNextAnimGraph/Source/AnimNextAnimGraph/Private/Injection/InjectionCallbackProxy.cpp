// Copyright Epic Games, Inc. All Rights Reserved.

#include "Injection/InjectionCallbackProxy.h"
#include "Animation/AnimSequence.h"
#include "Component/AnimNextComponent.h"
#include "GraphInterfaces/AnimNextNativeDataInterface_AnimSequencePlayer.h"
#include "Injection/InjectionUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InjectionCallbackProxy)

UInjectionCallbackProxy* UInjectionCallbackProxy::CreateProxyObjectForInjection(
	UAnimNextComponent* AnimNextComponent,
	FName SiteName,
	UObject* Object,
	UAnimNextComponent* BindingComponent,
	FInstancedStruct Payload,
	UE::AnimNext::FInjectionBlendSettings BlendInSettings,
	UE::AnimNext::FInjectionBlendSettings BlendOutSettings)
{
	UInjectionCallbackProxy* Proxy = NewObject<UInjectionCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->Inject(AnimNextComponent, SiteName, Object, BindingComponent, MoveTemp(Payload), BlendInSettings, BlendOutSettings);
	return Proxy;
}

bool UInjectionCallbackProxy::Inject(
	UAnimNextComponent* AnimNextComponent,
	FName SiteName,
	UObject* Object,
	UAnimNextComponent* BindingComponent,
	FInstancedStruct&& Payload,
	const UE::AnimNext::FInjectionBlendSettings& BlendInSettings,
	const UE::AnimNext::FInjectionBlendSettings& BlendOutSettings)
{
	if (AnimNextComponent == nullptr)
	{
		return false;
	}

	UE::AnimNext::FInjectionRequestArgs RequestArgs;
	RequestArgs.Site = FAnimNextInjectionSite(SiteName);
	RequestArgs.Object = Object;
	RequestArgs.BlendInSettings = BlendInSettings;
	RequestArgs.BlendOutSettings = BlendOutSettings;
	RequestArgs.BindingModuleHandle = BindingComponent ? BindingComponent->GetModuleHandle() : UE::AnimNext::FModuleHandle();
	RequestArgs.Payload.AddNative(MoveTemp(Payload));

	UE::AnimNext::FInjectionLifetimeEvents LifetimeEvents;
	LifetimeEvents.OnCompleted.BindUObject(this, &UInjectionCallbackProxy::OnInjectionCompleted);
	LifetimeEvents.OnInterrupted.BindUObject(this, &UInjectionCallbackProxy::OnInjectionInterrupted);
	LifetimeEvents.OnBlendingOut.BindUObject(this, &UInjectionCallbackProxy::OnInjectionBlendingOut);

	PlayingRequest = UE::AnimNext::FInjectionUtils::Inject(AnimNextComponent, AnimNextComponent->GetModuleHandle(), MoveTemp(RequestArgs), MoveTemp(LifetimeEvents));

	bWasInterrupted = false;

	if (!PlayingRequest.IsValid())
	{
		OnInterrupted.Broadcast();
		Reset();
	}

	return PlayingRequest.IsValid();
}

EUninjectionResult UInjectionCallbackProxy::Uninject()
{
	if(!PlayingRequest.IsValid())
	{
		return EUninjectionResult::Failed;
	}

	UE::AnimNext::FInjectionUtils::Uninject(PlayingRequest);

	return EUninjectionResult::Succeeded;
}

void UInjectionCallbackProxy::Cancel()
{
	Super::Cancel();
	Uninject();
}

void UInjectionCallbackProxy::OnInjectionCompleted(const UE::AnimNext::FInjectionRequest& Request)
{
	if (!bWasInterrupted)
	{
		const UE::AnimNext::EInjectionStatus Status = Request.GetStatus();
		check(!EnumHasAnyFlags(Status, UE::AnimNext::EInjectionStatus::Interrupted));

		if (EnumHasAnyFlags(Status, UE::AnimNext::EInjectionStatus::Expired))
		{
			OnInterrupted.Broadcast();
		}
		else
		{
			OnCompleted.Broadcast();
		}
	}

	Reset();
}

void UInjectionCallbackProxy::OnInjectionInterrupted(const UE::AnimNext::FInjectionRequest& Request)
{
	bWasInterrupted = true;

	OnInterrupted.Broadcast();
}

void UInjectionCallbackProxy::OnInjectionBlendingOut(const UE::AnimNext::FInjectionRequest& Request)
{
	if (!bWasInterrupted)
	{
		OnBlendOut.Broadcast();
	}
}

void UInjectionCallbackProxy::Reset()
{
	PlayingRequest = nullptr;
	bWasInterrupted = false;
}

void UInjectionCallbackProxy::BeginDestroy()
{
	Reset();

	Super::BeginDestroy();
}
