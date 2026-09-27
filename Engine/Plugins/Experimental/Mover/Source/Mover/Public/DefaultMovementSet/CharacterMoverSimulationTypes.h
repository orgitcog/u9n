// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"

#include "CharacterMoverSimulationTypes.generated.h"

USTRUCT()
struct FLandedEventData : public FMoverSimulationEventData
{
	GENERATED_BODY()

	FLandedEventData(float InEventTimeMs, const FHitResult& InHitResult, const FName InNewModeName)
		: FMoverSimulationEventData(InEventTimeMs)
		, HitResult(InHitResult)
		, NewModeName(InNewModeName)
	{
	}
	FLandedEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FLandedEventData::StaticStruct();
	}

	FHitResult HitResult;
	FName NewModeName = NAME_None;
};

USTRUCT(BlueprintType)
struct FFloorResultData : public FMoverDataStructBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FFloorCheckResult FloorResult;

	FFloorResultData() = default;
	virtual ~FFloorResultData() = default;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	virtual FMoverDataStructBase* Clone() const override
	{
		return new FFloorResultData(*this);
	}

	MOVER_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	MOVER_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	MOVER_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	MOVER_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	MOVER_API virtual void Merge(const FMoverDataStructBase& From) override;
	MOVER_API virtual void Decay(float DecayAmount) override;
};

template<>
struct TStructOpsTypeTraits< FFloorResultData > : public TStructOpsTypeTraitsBase2< FFloorResultData >
{
	enum
	{
		WithCopy = true
	};
};