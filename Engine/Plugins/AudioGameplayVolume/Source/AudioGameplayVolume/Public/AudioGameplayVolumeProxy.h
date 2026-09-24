// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioGameplayFlags.h"
#include "AudioGameplayVolumeProxy.generated.h"

#define UE_API AUDIOGAMEPLAYVOLUME_API

// Forward Declarations 
class FPrimitiveDrawInterface;
class FProxyVolumeMutator;
class FSceneView;
class UActorComponent;
class UAudioGameplayVolumeComponent;
class UPrimitiveComponent;
struct FAudioProxyMutatorPriorities;
struct FAudioProxyMutatorSearchResult;
struct FBodyInstance;

/**
 *  UAudioGameplayVolumeProxy - Abstract proxy used on audio thread to represent audio gameplay volumes.
 */
UCLASS(MinimalAPI, Abstract, EditInlineNew, HideDropdown)
class UAudioGameplayVolumeProxy : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	using PayloadFlags = AudioGameplay::EComponentPayload;
	using ProxyMutatorList = TArray<TSharedPtr<FProxyVolumeMutator>>;

	virtual ~UAudioGameplayVolumeProxy() = default;

	UE_API virtual bool ContainsPosition(const FVector& Position) const;
	UE_API virtual void InitFromComponent(const UAudioGameplayVolumeComponent* Component);

	UE_API void FindMutatorPriority(FAudioProxyMutatorPriorities& Priorities) const;
	UE_API void GatherMutators(const FAudioProxyMutatorPriorities& Priorities, FAudioProxyMutatorSearchResult& OutResult) const;

	UE_API void AddPayloadType(PayloadFlags InType);
	UE_API bool HasPayloadType(PayloadFlags InType) const;

	UE_API uint32 GetVolumeID() const;
	UE_API uint32 GetWorldID() const;

	/** Used for debug visualization of UAudioGameplayVolumeProxy in the editor */
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) {}

protected:

	ProxyMutatorList ProxyVolumeMutators;

	uint32 VolumeID = INDEX_NONE;
	uint32 WorldID = INDEX_NONE;
	PayloadFlags PayloadType = PayloadFlags::AGCP_None;
};

/**
 *  UAGVPrimitiveComponentProxy - Proxy based on a volume's primitive component
 */
UCLASS(MinimalAPI, meta = (DisplayName = "AGV Primitive Proxy"))
class UAGVPrimitiveComponentProxy : public UAudioGameplayVolumeProxy
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UAGVPrimitiveComponentProxy() = default;

	UE_API virtual bool ContainsPosition(const FVector& Position) const override;
	UE_API virtual void InitFromComponent(const UAudioGameplayVolumeComponent* Component) override;

protected:

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Primitives;
};

/**
 *  UAGVConditionProxy - Proxy for use with the UAudioGameplayCondition interface
 */
UCLASS(MinimalAPI, meta = (DisplayName = "AGV Condition Proxy"))
class UAGVConditionProxy : public UAudioGameplayVolumeProxy
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UAGVConditionProxy() = default;

	UE_API virtual bool ContainsPosition(const FVector& Position) const override;
	UE_API virtual void InitFromComponent(const UAudioGameplayVolumeComponent* Component) override;

protected:

	UPROPERTY(Transient)
	TObjectPtr<const UObject> ObjectPtr;
};

#undef UE_API
