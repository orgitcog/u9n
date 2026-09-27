// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "MoverTypes.h"

#include "ChaosCharacterInputs.generated.h"

USTRUCT(BlueprintType)
struct FChaosMoverLaunchInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

public:
	// Launch velocity in cm/s or impulse in kg cm/s
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FVector LaunchVelocityOrImpulse = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	EChaosMoverVelocityEffectMode Mode = EChaosMoverVelocityEffectMode::AdditiveVelocity;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}
	virtual FMoverDataStructBase* Clone() const override
	{
		return new FChaosMoverLaunchInputs(*this);
	}
	CHAOSMOVER_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	CHAOSMOVER_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	CHAOSMOVER_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	CHAOSMOVER_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	CHAOSMOVER_API virtual void Merge(const FMoverDataStructBase& From) override;
};

template<>
struct TStructOpsTypeTraits< FChaosMoverLaunchInputs > : public TStructOpsTypeTraitsBase2< FChaosMoverLaunchInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};