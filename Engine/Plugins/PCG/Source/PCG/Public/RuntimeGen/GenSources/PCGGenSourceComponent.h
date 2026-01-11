// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RuntimeGen/GenSources/PCGGenSourceBase.h"

#include "Components/ActorComponent.h"

#include "PCGGenSourceComponent.generated.h"

#define UE_API PCG_API

namespace EEndPlayReason { enum Type : int; }

class FPCGGenSourceManager;

/**
 * UPCGGenSourceComponent makes the actor this is attached to act as a PCG runtime generation source.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), DisplayName = "PCG Generation Source", meta = (BlueprintSpawnableComponent, PrioritizeCategories = "PCG"))
class UPCGGenSourceComponent : public UActorComponent, public IPCGGenSourceBase
{
	GENERATED_BODY()

	UPCGGenSourceComponent(const FObjectInitializer& InObjectInitializer) : Super(InObjectInitializer) {}
	~UPCGGenSourceComponent();

public:
	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void OnRegister() override;
	UE_API virtual void OnUnregister() override;
	UE_API virtual void OnComponentCreated() override;
	UE_API virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~End UActorComponent Interface

	//~Begin IPCGGenSourceInterface
	/** Returns the world space position of this gen source. */
	UE_API virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	UE_API virtual TOptional<FVector> GetDirection() const override;
	//~End IPCGGenSourceInterface

protected:
	UE_API FPCGGenSourceManager* GetGenSourceManager() const;
};

#undef UE_API
