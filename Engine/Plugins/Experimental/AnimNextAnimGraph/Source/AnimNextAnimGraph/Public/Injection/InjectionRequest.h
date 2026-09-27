// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AlphaBlend.h"
#include "AnimNextDataInterfacePayload.h"
#include "TraitCore/TraitEvent.h"
#include "TraitInterfaces/ITimeline.h"
#include "Injection/InjectionStatus.h"
#include "Injection/InjectionSite.h"
#include "Module/ModuleHandle.h"

#include "InjectionRequest.generated.h"

class FReferenceCollector;
struct FAnimNextModuleInjectionComponent;
struct FAnimNextGraphInstance;
class UInjectionCallbackProxy;
class UWorld;

namespace UE::AnimNext
{
	struct IEvaluationModifier;
	struct FInjection_InjectEvent;
	struct FInjection_StatusUpdateEvent;
	struct FInjection_TimelineUpdateEvent;
	struct FInjectionUtils;
	struct FPlayAnimSlotTrait;
	struct FInjectionSiteTrait;
	struct FInjectionRequest;
	struct FInjectionRequestTracker;
}

UENUM()
enum class EAnimNextInjectionBlendMode : uint8
{
	// Uses standard weight based blend
	Standard,

	// Uses inertialization. Requires an inertialization trait somewhere earlier in the graph.
	Inertialization,
};

/**
 * Injection Blend Settings
 *
 * Encapsulates the blend settings used by injection requests.
 */
USTRUCT(BlueprintType)
struct FAnimNextInjectionBlendSettings
{
	GENERATED_BODY()

	/** Blend Profile to use for this blend */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (DisplayAfter = "Blend"))
	//TObjectPtr<UBlendProfile> BlendProfile;

	/** AlphaBlend options (time, curve, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (DisplayAfter = "BlendMode"))
	FAlphaBlendArgs Blend;

	/** Type of blend mode (Standard vs Inertial) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	EAnimNextInjectionBlendMode BlendMode = EAnimNextInjectionBlendMode::Standard;
};

// What to do when performing an injection
UENUM()
enum class EAnimNextInjectionType : uint8
{
	// Inject an object to be instantiated via factory at the injection site. Optionally this can bind to an external module.
	InjectObject,

	// Apply an evaluation modifier at the specified injection site. Does not affect the currently-provided object.
	EvaluationModifier,
};

// Lifetime behavior for injection requests
UENUM()
enum class EAnimNextInjectionLifetimeType : uint8
{
	// Automatically un-inject this request when the timeline of the injection request expires.
	// Looping/infinite timelines will never auto-expire and must be uninjected manually.
	Auto,

	// Injection request persists and must be un-injected manually.
	// Blending out to the source animation at the injection site does not occur.
	// Useful when you want to just push a chain of animations to an injection site, never seeing the source/passthrough pose.
	ForcePersistent,
};

/**
 * Injection Request Arguments
 *
 * Encapsulates the parameters required to initiate an injection request.
 */
USTRUCT()
struct FAnimNextInjectionRequestArgs
{
	GENERATED_BODY()

private:
	friend UE::AnimNext::FInjectionRequest;
	friend UE::AnimNext::FInjectionSiteTrait;
	friend UE::AnimNext::FPlayAnimSlotTrait;
	friend UE::AnimNext::FInjectionRequestTracker;
	friend UE::AnimNext::FInjectionUtils;
	friend FAnimNextModuleInjectionComponent;
	friend UInjectionCallbackProxy;

	// The injection site to target with this request.
	UPROPERTY()
	FAnimNextInjectionSite Site;

	// The blend settings to use when blending in
	UPROPERTY()
	FAnimNextInjectionBlendSettings BlendInSettings;

	// The blend settings to use when blending out (if not interrupted)
	UPROPERTY()
	FAnimNextInjectionBlendSettings BlendOutSettings;

	// Object to inject. The animation graph to be instantiated for this request will be chosen via a factory
	UPROPERTY()
	TObjectPtr<UObject> Object;

	// Module handle to use when injecting. This will supply the source binding for data interfaces when the graph is instantiated.
	UE::AnimNext::FModuleHandle BindingModuleHandle;

	// Evaluation modifier to apply at the injection site. Shared ownership, references can persist in worker threads until un-injected.
	TSharedPtr<UE::AnimNext::IEvaluationModifier> EvaluationModifier;

	UPROPERTY()
	EAnimNextInjectionType Type = EAnimNextInjectionType::InjectObject;

	// Lifetime behavior for the request
	UPROPERTY()
	EAnimNextInjectionLifetimeType LifetimeType = EAnimNextInjectionLifetimeType::Auto;

	// Whether or not the request should track the timeline progress
	UPROPERTY()
	bool bTrackTimelineProgress = false;

	// Payload that will be applied to the animation graph's variables via its data interfaces.
	UPROPERTY()
	FAnimNextDataInterfacePayload Payload;
};

namespace UE::AnimNext
{
	// Create a namespaced aliases to simplify usage
	using EInjectionBlendMode = EAnimNextInjectionBlendMode;
	using FInjectionBlendSettings = FAnimNextInjectionBlendSettings;
	using FInjectionRequestArgs = FAnimNextInjectionRequestArgs;

	struct FInjectionRequest;

	DECLARE_DELEGATE_OneParam(FAnimNextOnInjectionStarted, const FInjectionRequest&)
	DECLARE_DELEGATE_OneParam(FAnimNextOnInjectionCompleted, const FInjectionRequest&)
	DECLARE_DELEGATE_OneParam(FAnimNextOnInjectionInterrupted, const FInjectionRequest&)
	DECLARE_DELEGATE_OneParam(FAnimNextOnInjectionBlendingOut, const FInjectionRequest&)

	// Delegates called for various lifetime events
	struct FInjectionLifetimeEvents
	{
		// Callback called when the request starts playing (status transitions from pending to playing)
		FAnimNextOnInjectionStarted OnStarted;

		// Callback called when the request completes (status transitions from playing to completed)
		FAnimNextOnInjectionCompleted OnCompleted;

		// Callback called when the request is interrupted (either by calling Stop on it or by another request)
		FAnimNextOnInjectionInterrupted OnInterrupted;

		// Callback called when the request starts blending out (if it wasn't interrupted)
		FAnimNextOnInjectionBlendingOut OnBlendingOut;
	};

	/**
	 * Injection Request
	 *
	 * Instances of this class represent individual requests to the injection system.
	 * They are allocated as shared pointers and ownership is split between gameplay (until
	 * it no longer cares about a particular request) and the injection site that hosts it
	 * (until the request completes).
	 * 
	 * Use MakeInjectionRequest(...) to construct instances of this type.
	 */
	struct FInjectionRequest : public TSharedFromThis<FInjectionRequest, ESPMode::ThreadSafe>
	{
		// Returns the arguments this request is using
		ANIMNEXTANIMGRAPH_API const FInjectionRequestArgs& GetArgs() const;

		// Returns the lifetime delegates this request is using
		ANIMNEXTANIMGRAPH_API const FInjectionLifetimeEvents& GetLifetimeEvents() const;

		// Returns the request status
		ANIMNEXTANIMGRAPH_API EInjectionStatus GetStatus() const;

		// Returns the current timeline state (make sure to enable FInjectionRequestArgs::bTrackTimelineProgress to use this)
		ANIMNEXTANIMGRAPH_API const FTimelineState& GetTimelineState() const;

		// Returns whether or not this request has expired
		ANIMNEXTANIMGRAPH_API bool HasExpired() const;

		// Returns whether or not this request has completed (might have been interrupted)
		ANIMNEXTANIMGRAPH_API bool HasCompleted() const;

		// Returns whether or not this request is playing (might be blending out or interrupted)
		ANIMNEXTANIMGRAPH_API bool IsPlaying() const;

		// Returns whether or not this request is blending out
		ANIMNEXTANIMGRAPH_API bool IsBlendingOut() const;

		// Returns whether or not this request was interrupted (by Stop or by another request)
		ANIMNEXTANIMGRAPH_API bool WasInterrupted() const;

		ANIMNEXTANIMGRAPH_API void ExternalAddReferencedObjects(FReferenceCollector& Collector);

	private:
		// Sends this request to the specified host and it will attempt to play with the requested arguments
		bool Inject(FInjectionRequestArgs&& InRequestArgs, FInjectionLifetimeEvents&& InLifetimeEvents, UObject* InHost, FModuleHandle InHandle);

		// Interrupts this request and request that we transition to the source input on the playing injection site
		void Uninject();

		// Returns the arguments this request is using
		ANIMNEXTANIMGRAPH_API FInjectionRequestArgs& GetMutableArgs();

		// Returns the lifetime delegates this request is using
		ANIMNEXTANIMGRAPH_API FInjectionLifetimeEvents& GetMutableLifetimeEvents();

		void OnStatusUpdate(EInjectionStatus NewStatus);

		void OnTimelineUpdate(const FTimelineState& NewTimelineState);

		// GC API
		void AddReferencedObjects(FReferenceCollector& Collector);

		// Validate this set of args is set up correctly for injection
		static bool ValidateArgs(const FInjectionRequestArgs& InRequestArgs);

		// Setup dependencies between the module being injected into and any module used for data interface bindings
		static void SetupBindingModuleDependencies(UWorld* InWorld, FModuleHandle InHandle, const FInjectionRequestArgs& InRequestArgs);

		// Remove dependencies between the module being injected into and any module used for data interface bindings
		static void RemoveBindingModuleDependencies(UWorld* InWorld, FModuleHandle InHandle, const FInjectionRequestArgs& InRequestArgs);

		// The request arguments
		FInjectionRequestArgs RequestArgs;

		// Callbacks for lifetime events 
		FInjectionLifetimeEvents LifetimeEvents;

		// The object we are playing on
		TWeakObjectPtr<UObject> WeakHost;

		// The world within which we are playing
		TWeakObjectPtr<UWorld> WeakWorld;

		// Handle to the module instance
		FModuleHandle Handle;

		// The injection event if we have injected already
		TWeakPtr<FAnimNextTraitEvent> InjectionEvent;

		// The current request status
		EInjectionStatus Status = EInjectionStatus::None;

		// The current timeline state
		FTimelineState TimelineState;

		friend FInjection_InjectEvent;
		friend FInjection_StatusUpdateEvent;
		friend FInjection_TimelineUpdateEvent;
		friend FInjectionUtils;
		friend FPlayAnimSlotTrait;
		friend FInjectionSiteTrait;
		friend ::FAnimNextModuleInjectionComponent;
	};

	// Create a shared pointer alias for injection requests
	using FInjectionRequestPtr = TSharedPtr<FInjectionRequest, ESPMode::ThreadSafe>;

	// Constructs a injection request object
	inline FInjectionRequestPtr MakeInjectionRequest()
	{
		return MakeShared<FInjectionRequest>();
	}
}
