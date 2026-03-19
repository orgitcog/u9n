// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayAnim/PlayAnimCallbackProxy.h"
#include "Animation/AnimSequence.h"
#include "Component/AnimNextComponent.h"
#include "GraphInterfaces/AnimNextNativeDataInterface_AnimSequencePlayer.h"
#include "Injection/InjectionUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayAnimCallbackProxy)

//////////////////////////////////////////////////////////////////////////
// UPlayAnimCallbackProxy

UPlayAnimCallbackProxy::UPlayAnimCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPlayAnimCallbackProxy* UPlayAnimCallbackProxy::CreateProxyObjectForPlayAnim(
	UAnimNextComponent* AnimNextComponent,
	FName SiteName,
	UAnimSequence* AnimSequence,
	float PlayRate,
	float StartPosition,
	UE::AnimNext::FInjectionBlendSettings BlendInSettings,
	UE::AnimNext::FInjectionBlendSettings BlendOutSettings)
{
	UPlayAnimCallbackProxy* Proxy = NewObject<UPlayAnimCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->Play(AnimNextComponent, SiteName, AnimSequence, PlayRate, StartPosition, BlendInSettings, BlendOutSettings);
	return Proxy;
}

bool UPlayAnimCallbackProxy::Play(
	UAnimNextComponent* AnimNextComponent,
	FName SiteName,
	UAnimSequence* AnimSequence,
	float PlayRate,
	float StartPosition,
	const UE::AnimNext::FInjectionBlendSettings& BlendInSettings,
	const UE::AnimNext::FInjectionBlendSettings& BlendOutSettings)
{
	bool bPlayedSuccessfully = false;
	if (AnimNextComponent != nullptr)
	{
		UE::AnimNext::FInjectionLifetimeEvents LifetimeEvents;
		LifetimeEvents.OnCompleted.BindUObject(this, &UPlayAnimCallbackProxy::OnPlayAnimCompleted);
		LifetimeEvents.OnInterrupted.BindUObject(this, &UPlayAnimCallbackProxy::OnPlayAnimInterrupted);
		LifetimeEvents.OnBlendingOut.BindUObject(this, &UPlayAnimCallbackProxy::OnPlayAnimBlendingOut);

		PlayingRequest = UE::AnimNext::FInjectionUtils::PlayAnim(
			AnimNextComponent,
			UE::AnimNext::FInjectionSite(SiteName),
			AnimSequence,
			{ 
				.PlayRate = PlayRate,
				.StartPosition = StartPosition
			},
			BlendInSettings,
			BlendOutSettings,
			MoveTemp(LifetimeEvents));
		bWasInterrupted = false;
	}

	if (!PlayingRequest.IsValid())
	{
		OnInterrupted.Broadcast();
		Reset();
	}

	return bPlayedSuccessfully;
}

void UPlayAnimCallbackProxy::OnPlayAnimCompleted(const UE::AnimNext::FInjectionRequest& Request)
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

void UPlayAnimCallbackProxy::OnPlayAnimInterrupted(const UE::AnimNext::FInjectionRequest& Request)
{
	bWasInterrupted = true;

	OnInterrupted.Broadcast();
}

void UPlayAnimCallbackProxy::OnPlayAnimBlendingOut(const UE::AnimNext::FInjectionRequest& Request)
{
	if (!bWasInterrupted)
	{
		OnBlendOut.Broadcast();
	}
}

void UPlayAnimCallbackProxy::Reset()
{
	PlayingRequest = nullptr;
	bWasInterrupted = false;
}

void UPlayAnimCallbackProxy::BeginDestroy()
{
	Reset();

	Super::BeginDestroy();
}
