// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "Engine/World.h"

#include "SlateFXSubsystem.generated.h"

#define UE_API SLATERHIRENDERER_API

class FSlateRHIPostBufferProcessorProxy;
class USlateRHIPostBufferProcessor;

UCLASS(MinimalAPI, DisplayName = "Slate FX Subsystem")
class USlateFXSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	static UE_API USlateRHIPostBufferProcessor* GetPostProcessor(ESlatePostRT InSlatePostBufferBit);
	static UE_API TSharedPtr<FSlateRHIPostBufferProcessorProxy> GetPostProcessorProxy(ESlatePostRT InSlatePostBufferBit);

	//~ Begin UObject Interface.
	UE_API virtual void BeginDestroy() override;
	//~ End UObject Interface.

	//~Begin UGameInstanceSubsystem Interface
	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End UGameInstanceSubsystem Interface

	/** Get post processor proxy for a particular post buffer index, if it exists */
	UE_API TSharedPtr<FSlateRHIPostBufferProcessorProxy> GetSlatePostProcessorProxy(ESlatePostRT InPostBufferBit);

public:

	/** Get post processor for a particular post buffer index, if it exists */
	UFUNCTION(BlueprintCallable, Category = "SlateFX")
	UE_API USlateRHIPostBufferProcessor* GetSlatePostProcessor(ESlatePostRT InPostBufferBit);

private:

	/** Map of post RT buffer index to buffer processors, if they exist */
	UPROPERTY(Transient)
	TMap<ESlatePostRT, TObjectPtr<USlateRHIPostBufferProcessor>> SlatePostBufferProcessors;

private:

	/** Callback to create processors on world init */
	void OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	/** Callback to remove processors on world cleanup */
	void OnPostWorldCleanup(UWorld* World, bool SessionEnded, bool bCleanupResources);

private:

	/** Map of post RT buffer index to buffer processor renderthread proxies, if they exist */
	TMap<ESlatePostRT, TSharedPtr<FSlateRHIPostBufferProcessorProxy>> SlatePostBufferProcessorProxies;
};

#undef UE_API
