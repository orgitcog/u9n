// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "Injection/InjectionRequest.h"

#include "InjectionCallbackProxy.generated.h"

class UAnimNextComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInjectionDelegate);

UENUM()
enum class EUninjectionResult : uint8
{
	Succeeded,
	Failed
};

UCLASS(MinimalAPI, meta=(ExposedAsyncProxy = "AsyncTask", HasDedicatedAsyncNode))
class UInjectionCallbackProxy : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	// Called when the provided animation object finished playing and hasn't been interrupted
	UPROPERTY(BlueprintAssignable)
	FOnInjectionDelegate OnCompleted;

	// Called when the provided animation object starts blending out and hasn't been interrupted
	UPROPERTY(BlueprintAssignable)
	FOnInjectionDelegate OnBlendOut;

	// Called when the provided animation object has been interrupted (or failed to play)
	UPROPERTY(BlueprintAssignable)
	FOnInjectionDelegate OnInterrupted;

	// Called to perform the query internally
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static ANIMNEXTANIMGRAPH_API UInjectionCallbackProxy* CreateProxyObjectForInjection(
		UAnimNextComponent* AnimNextComponent,
		FName SiteName,
		UObject* Object,
		UAnimNextComponent* BindingComponent,
		FInstancedStruct Payload,
		FAnimNextInjectionBlendSettings BlendInSettings = FAnimNextInjectionBlendSettings(),
		FAnimNextInjectionBlendSettings BlendOutSettings = FAnimNextInjectionBlendSettings());

	// Un-inject a previously injected object. Cancelling this async tasks will also un-inject.
	UFUNCTION(BlueprintCallable, Category="Animation|AnimNext", meta=(ExpandEnumAsExecs = "ReturnValue"))
	ANIMNEXTANIMGRAPH_API EUninjectionResult Uninject();

	// UObject Interface
	ANIMNEXTANIMGRAPH_API virtual void BeginDestroy() override;

	// UCancellableAsyncAction interface
	ANIMNEXTANIMGRAPH_API virtual void Cancel() override;

protected:
	ANIMNEXTANIMGRAPH_API void OnInjectionCompleted(const UE::AnimNext::FInjectionRequest& Request);
	ANIMNEXTANIMGRAPH_API void OnInjectionInterrupted(const UE::AnimNext::FInjectionRequest& Request);
	ANIMNEXTANIMGRAPH_API void OnInjectionBlendingOut(const UE::AnimNext::FInjectionRequest& Request);

	// Attempts to play an object with the specified payload. Returns whether it started or not.
	ANIMNEXTANIMGRAPH_API bool Inject(
		UAnimNextComponent* AnimNextComponent,
		FName SiteName,
		UObject* Object,
		UAnimNextComponent* BindingComponent,
		FInstancedStruct&& Payload,
		const UE::AnimNext::FInjectionBlendSettings& BlendInSettings,
		const UE::AnimNext::FInjectionBlendSettings& BlendOutSettings);

private:
	void Reset();

	UE::AnimNext::FInjectionRequestPtr PlayingRequest;
	bool bWasInterrupted = false;
};
