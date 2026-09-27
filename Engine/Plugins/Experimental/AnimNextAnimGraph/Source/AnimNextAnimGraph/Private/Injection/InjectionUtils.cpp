// Copyright Epic Games, Inc. All Rights Reserved.

#include "Injection/InjectionUtils.h"
#include "GraphInterfaces/AnimNextNativeDataInterface_AnimSequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Component/AnimNextComponent.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Logging/StructuredLog.h"
#include "UObject/ObjectKey.h"

namespace UE::AnimNext
{
	FInjectionRequestPtr FInjectionUtils::Inject(UAnimNextComponent* InComponent, FInjectionRequestArgs&& InArgs, FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		return Inject(InComponent, InComponent->GetModuleHandle(), MoveTemp(InArgs), MoveTemp(InLifetimeEvents));
	}

	FInjectionRequestPtr FInjectionUtils::Inject(UObject* InHost, FModuleHandle InHandle, FInjectionRequestArgs&& InArgs, FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		check(IsInGameThread());

		FInjectionRequestPtr Request = MakeInjectionRequest();
		if(Request->Inject(MoveTemp(InArgs), MoveTemp(InLifetimeEvents), InHost, InHandle))
		{
			return Request;
		}
		return nullptr;
	}

	void FInjectionUtils::Uninject(FInjectionRequestPtr InInjectionRequest)
	{
		if(!InInjectionRequest.IsValid())
		{
			return;
		}
		InInjectionRequest->Uninject();
	}

	FInjectionRequestPtr FInjectionUtils::PlayAnim(
		UAnimNextComponent* InComponent,
		const FInjectionSite& InSite,
		UAnimSequence* InAnimSequence,
		float InPlayRate,
		float InStartPosition,
		const FInjectionBlendSettings& InBlendInSettings,
		const FInjectionBlendSettings& InBlendOutSettings,
		FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		return PlayAnimHandle(
			InComponent,
			InComponent->GetModuleHandle(),
			InSite,
			InAnimSequence,
			{
				.PlayRate = InPlayRate,
				.StartPosition = InStartPosition
			},
			InBlendInSettings,
			InBlendOutSettings,
			MoveTemp(InLifetimeEvents));
	}

	FInjectionRequestPtr FInjectionUtils::PlayAnim(
		UAnimNextComponent* InComponent,
		const FInjectionSite& InSite,
		UAnimSequence* InAnimSequence,
		const FPlayAnimArgs& InArgs,
		const FInjectionBlendSettings& InBlendInSettings,
		const FInjectionBlendSettings& InBlendOutSettings,
		FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		return PlayAnimHandle(
			InComponent,
			InComponent->GetModuleHandle(),
			InSite,
			InAnimSequence,
			InArgs,
			InBlendInSettings,
			InBlendOutSettings,
			MoveTemp(InLifetimeEvents));
	}

	FInjectionRequestPtr FInjectionUtils::PlayAnimHandle(
		UObject* InHost,
		FModuleHandle InModuleHandle,
		const FInjectionSite& InSite,
		UAnimSequence* InAnimSequence,
		const FPlayAnimArgs& InArgs,
		const FInjectionBlendSettings& InBlendInSettings,
		const FInjectionBlendSettings& InBlendOutSettings,
		FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		check(IsInGameThread());

		FInstancedStruct Payload;
		Payload.InitializeAs<FAnimNextNativeDataInterface_AnimSequencePlayer>();
		FAnimNextNativeDataInterface_AnimSequencePlayer& PlayNativeInterface = Payload.GetMutable<FAnimNextNativeDataInterface_AnimSequencePlayer>();
		PlayNativeInterface.AnimSequence = InAnimSequence;
		PlayNativeInterface.PlayRate = InArgs.PlayRate;
		PlayNativeInterface.StartPosition = InArgs.StartPosition;
		switch (InArgs.LoopMode)
		{
		case ELoopMode::Auto:
			PlayNativeInterface.Loop = InAnimSequence->bLoop;
			break;
		case ELoopMode::ForceLoop:
			PlayNativeInterface.Loop = true;
			break;
		case ELoopMode::ForceNonLoop:
			PlayNativeInterface.Loop = false;
			break;
		}

		FInjectionRequestArgs RequestArgs;
		RequestArgs.Site = InSite;
		RequestArgs.Object = InAnimSequence;
		RequestArgs.Type = EAnimNextInjectionType::InjectObject;
		RequestArgs.BlendInSettings = InBlendInSettings;
		RequestArgs.BlendOutSettings = InBlendOutSettings;
		RequestArgs.Payload.AddNative(MoveTemp(Payload));
		RequestArgs.LifetimeType = InArgs.LifetimeType;

		return Inject(InHost, InModuleHandle, MoveTemp(RequestArgs), MoveTemp(InLifetimeEvents));
	}

	FInjectionRequestPtr FInjectionUtils::InjectAsset(
		UAnimNextComponent* InComponent,
		const FInjectionSite& InSite,
		UObject* InAsset,
		FAnimNextDataInterfacePayload&& InPayload,
		UAnimNextComponent* InBindingComponent,
		const FInjectionBlendSettings& InBlendInSettings,
		const FInjectionBlendSettings& InBlendOutSettings,
		FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		return InjectAsset(
			InComponent,
			InComponent->GetModuleHandle(),
			InSite,
			InAsset,
			MoveTemp(InPayload),
			InBindingComponent ? InBindingComponent->GetModuleHandle() : FModuleHandle(),
			InBlendInSettings,
			InBlendOutSettings,
			MoveTemp(InLifetimeEvents));
	}

	FInjectionRequestPtr FInjectionUtils::InjectAsset(
		UObject* InHost,
		FModuleHandle InModuleHandle,
		const FInjectionSite& InSite,
		UObject* InAsset,
		FAnimNextDataInterfacePayload&& InPayload,
		FModuleHandle InBindingModuleHandle,
		const FInjectionBlendSettings& InBlendInSettings,
		const FInjectionBlendSettings& InBlendOutSettings,
		FInjectionLifetimeEvents&& InLifetimeEvents)
	{
		check(IsInGameThread());

		FInjectionRequestArgs RequestArgs;
		RequestArgs.Site = InSite;
		RequestArgs.Object = InAsset;
		RequestArgs.BindingModuleHandle = InBindingModuleHandle;
		RequestArgs.Type = EAnimNextInjectionType::InjectObject;
		RequestArgs.BlendInSettings = InBlendInSettings;
		RequestArgs.BlendOutSettings = InBlendOutSettings;
		RequestArgs.Payload = MoveTemp(InPayload);

		return Inject(InHost, InModuleHandle, MoveTemp(RequestArgs), MoveTemp(InLifetimeEvents));
	}

	FInjectionRequestPtr FInjectionUtils::InjectEvaluationModifier(
		UAnimNextComponent* InComponent,
		const TSharedRef<IEvaluationModifier>& InEvaluationModifier,
		const FInjectionSite& InSite)
	{
		return InjectEvaluationModifier(
			InComponent,
			InComponent->GetModuleHandle(),
			InEvaluationModifier,
			InSite);
	}

	FInjectionRequestPtr FInjectionUtils::InjectEvaluationModifier(
		UObject* InHost,
		FModuleHandle InModuleHandle,
		const TSharedRef<IEvaluationModifier>& InEvaluationModifier,
		const FInjectionSite& InSite)
	{
		check(IsInGameThread());

		FInjectionRequestArgs RequestArgs;
		RequestArgs.Site = InSite;
		RequestArgs.Type = EAnimNextInjectionType::EvaluationModifier;
		RequestArgs.EvaluationModifier = InEvaluationModifier;

		return Inject(InHost, InModuleHandle, MoveTemp(RequestArgs));
	}
}
