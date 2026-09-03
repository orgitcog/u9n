// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Injection/InjectionRequest.h"

#include "PlayAnimCallbackProxy.generated.h"

class UAnimSequence;
class UAnimNextComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayAnimPlayDelegate);

UCLASS(MinimalAPI)
class UPlayAnimCallbackProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called when the provided animation object finished playing and hasn't been interrupted
	UPROPERTY(BlueprintAssignable)
	FOnPlayAnimPlayDelegate OnCompleted;

	// Called when the provided animation object starts blending out and hasn't been interrupted
	UPROPERTY(BlueprintAssignable)
	FOnPlayAnimPlayDelegate OnBlendOut;

	// Called when the provided animation object has been interrupted (or failed to play)
	UPROPERTY(BlueprintAssignable)
	FOnPlayAnimPlayDelegate OnInterrupted;

	// Called to perform the query internally
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static ANIMNEXTANIMGRAPH_API UPlayAnimCallbackProxy* CreateProxyObjectForPlayAnim(
		UAnimNextComponent* AnimNextComponent,
		FName SiteName,
		UAnimSequence* AnimSequence,
		float PlayRate = 1.0f,
		float StartPosition = 0.0f,
		FAnimNextInjectionBlendSettings BlendInSettings = FAnimNextInjectionBlendSettings(),
		FAnimNextInjectionBlendSettings BlendOutSettings = FAnimNextInjectionBlendSettings());

	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", DeprecatedFunction, DeprecatedMessage="Please use Inject instead."))
	static UPlayAnimCallbackProxy* CreateProxyObjectForPlayAsset(
		UAnimNextComponent* AnimNextComponent,
		FName SiteName,
		UObject* Object,
		const FInstancedStruct& Payload,
		FAnimNextInjectionBlendSettings BlendInSettings = FAnimNextInjectionBlendSettings(),
		FAnimNextInjectionBlendSettings BlendOutSettings = FAnimNextInjectionBlendSettings())
	{
		return nullptr;
	}

public:
	//~ Begin UObject Interface
	ANIMNEXTANIMGRAPH_API virtual void BeginDestroy() override;
	//~ End UObject Interface

protected:
	ANIMNEXTANIMGRAPH_API void OnPlayAnimCompleted(const UE::AnimNext::FInjectionRequest& Request);
	ANIMNEXTANIMGRAPH_API void OnPlayAnimInterrupted(const UE::AnimNext::FInjectionRequest& Request);
	ANIMNEXTANIMGRAPH_API void OnPlayAnimBlendingOut(const UE::AnimNext::FInjectionRequest& Request);

	// Attempts to play an animation with the specified settings. Returns whether it started or not.
	ANIMNEXTANIMGRAPH_API bool Play(
		UAnimNextComponent* AnimNextComponent,
		FName SiteName,
		UAnimSequence* AnimSequence,
		float PlayRate,
		float StartPosition,
		const UE::AnimNext::FInjectionBlendSettings& BlendInSettings,
		const UE::AnimNext::FInjectionBlendSettings& BlendOutSettings);

private:
	void Reset();

	UE::AnimNext::FInjectionRequestPtr PlayingRequest;
	bool bWasInterrupted = false;
};
