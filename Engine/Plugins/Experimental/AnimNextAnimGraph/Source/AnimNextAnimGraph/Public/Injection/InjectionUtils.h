// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InjectionRequest.h"
#include "Injection/InjectionSite.h"

struct FAnimNextDataInterfaceInstance;
class UAnimNextComponent;

namespace UE::AnimNext
{
	struct IEvaluationModifier;
}

namespace UE::AnimNext
{
	// How to loop an animation when playing back
	enum class ELoopMode : uint8
	{
		// Use looping flag on the animation
		Auto,

		// Force looping
		ForceLoop,

		// Force non-looping
		ForceNonLoop,

		// TODO: Specific fractional loop count
	};

	// Optional arguments to PlayAnim
	struct FPlayAnimArgs
	{
		float PlayRate = 1.0f;
		float StartPosition = 0.0f;
		ELoopMode LoopMode = ELoopMode::Auto;
		EAnimNextInjectionLifetimeType LifetimeType = EAnimNextInjectionLifetimeType::Auto;
	};

	// A collection of wrapper functions for common injection operations
	struct FInjectionUtils
	{
		UE_DEPRECATED(5.6, "Use PlayAnim that takes FPlayAnimArgs")
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr PlayAnim(
			UAnimNextComponent* InComponent,
			const FInjectionSite& InSite,
			UAnimSequence* InAnimSequence,
			float InPlayRate = 1.0f,
			float InStartPosition = 0.0f,
			const FInjectionBlendSettings& InBlendInSettings = FInjectionBlendSettings(),
			const FInjectionBlendSettings& InBlendOutSettings = FInjectionBlendSettings(),
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// Injects an animation sequence into a running AnimNext module
		// @param InComponent          The UAnimNextComponent that hosts the module being played on
		// @param InHost               The object that hosts the module (e.g. UAnimNextComponent) being played on
		// @param InModuleHandle       The module handle being injected into
		// @param InSite               The injection site to 'play' the animation sequence on
		// @param InAnimSequence       The animation sequence to play
		// @param InArgs               Optional args to use
		// @param InBlendInSettings    The blend in settings
		// @param InBlendOutSettings   The blend in settings
		// @param InLifetimeEvents     Delegates called for various request lifetime events
		// @return the new injection request for tracking, or an invalid request if injection failed. This can be discarded for fire-and-forget style behavior.
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr PlayAnim(
			UAnimNextComponent* InComponent,
			const FInjectionSite& InSite,
			UAnimSequence* InAnimSequence,
			const FPlayAnimArgs& InArgs = FPlayAnimArgs(),
			const FInjectionBlendSettings& InBlendInSettings = FInjectionBlendSettings(),
			const FInjectionBlendSettings& InBlendOutSettings = FInjectionBlendSettings(),
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// @see PlayAnim
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr PlayAnimHandle(
			UObject* InHost,
			FModuleHandle InModuleHandle,
			const FInjectionSite& InSite,
			UAnimSequence* InAnimSequence,
			const FPlayAnimArgs& InArgs = FPlayAnimArgs(),
			const FInjectionBlendSettings& InBlendInSettings = FInjectionBlendSettings(),
			const FInjectionBlendSettings& InBlendOutSettings = FInjectionBlendSettings(),
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// Injects an asset into a running AnimNext module
		// @param InComponent          The UAnimNextComponent that hosts the module being played on
		// @param InHost               The object that hosts the module (e.g. UAnimNextComponent) being played on
		// @param InHandle             The module handle being injected into
		// @param InSite               The injection site to 'play' the asset on
		// @param InAsset              The asset to 'play'
		// @param InPayload            The payload for the asset to use
		// @param InBindingComponent   A component to use to supply additional data interface bindings at the injection site
		// @param InBindingModuleHandle A module to use to supply additional data interface bindings at the injection site
		// @param InBlendInSettings    The blend in settings
		// @param InBlendOutSettings   The blend in settings
		// @param InLifetimeEvents     Delegates called for various request lifetime events
		// @return the new injection request for tracking, or an invalid request if injection failed. This can be discarded for fire-and-forget style behavior.
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr InjectAsset(
			UAnimNextComponent* InComponent,
			const FInjectionSite& InSite,
			UObject* InAsset,
			FAnimNextDataInterfacePayload&& InPayload = FAnimNextDataInterfacePayload(),
			UAnimNextComponent* InBindingComponent = nullptr,
			const FInjectionBlendSettings& InBlendInSettings = FInjectionBlendSettings(),
			const FInjectionBlendSettings& InBlendOutSettings = FInjectionBlendSettings(),
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// @see InjectAsset
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr InjectAsset(
			UObject* InHost,
			FModuleHandle InModuleHandle,
			const FInjectionSite& InSite,
			UObject* InAsset,
			FAnimNextDataInterfacePayload&& InPayload = FAnimNextDataInterfacePayload(),
			FModuleHandle InBindingModuleHandle = FModuleHandle(),
			const FInjectionBlendSettings& InBlendInSettings = FInjectionBlendSettings(),
			const FInjectionBlendSettings& InBlendOutSettings = FInjectionBlendSettings(),
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// Inject an evaluation modifier into a running AnimNext module to control animation evaluation at a low level.
		// Does not require that injection sites are publicly visible.
		// @param InComponent          The UAnimNextComponent that hosts the module being injected into
		// @param InHost               The object that hosts the module (e.g. UAnimNextComponent) being injected into
		// @param InModuleHandle       The module handle being injected into
		// @param InEvaluationModifier The evaluation modifier to inject
		// @param InSite               The injection site to use. If this is NAME_None, then the first site will be used.
		// @return the new injection request for tracking, or an invalid request if injection failed. This can be discarded for fire-and-forget style behavior.
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr InjectEvaluationModifier(
			UAnimNextComponent* InComponent,
			const TSharedRef<IEvaluationModifier>& InEvaluationModifier,
			const FInjectionSite& InSite = FInjectionSite());

		// @see InjectExternalGraph
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr InjectEvaluationModifier(
			UObject* InHost,
			FModuleHandle InModuleHandle,
			const TSharedRef<IEvaluationModifier>& InEvaluationModifier,
			const FInjectionSite& InSite = FInjectionSite());

		// Inject into a running AnimNext module (raw custom args)
		// @param InComponent          The UAnimNextComponent that hosts the module being injected into
		// @param InHost               The object that hosts the module (e.g. UAnimNextComponent) being injected into
		// @param InHandle             The module handle being injected into
		// @param InArgs               Arguments to the injection request
		// @param InLifetimeEvents     Delegates called for various request lifetime events
		// @return the new injection request for tracking, or an invalid request if injection failed. This can be discarded for fire-and-forget style behavior.
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr Inject(
			UAnimNextComponent* InComponent,
			FInjectionRequestArgs&& InArgs,
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// @see Inject
		ANIMNEXTANIMGRAPH_API static FInjectionRequestPtr Inject(
			UObject* InHost,
			FModuleHandle InHandle,
			FInjectionRequestArgs&& InArgs,
			FInjectionLifetimeEvents&& InLifetimeEvents = FInjectionLifetimeEvents());

		// Uninject a previously injected request
		// @param InInjectionRequest   The injection request to uninject
		ANIMNEXTANIMGRAPH_API static void Uninject(FInjectionRequestPtr InInjectionRequest);
	};
}
