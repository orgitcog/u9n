// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassObserverRegistry.generated.h"

#define UE_API MASSENTITY_API


struct FMassObserverManager;

/**
 * A wrapper type for a TArray to support having map-of-arrays UPROPERTY members in FMassEntityObserverClassesMap
 */
USTRUCT()
struct FMassProcessorClassCollection
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TSubclassOf<UMassProcessor>> ClassCollection;
};

/**
 * A wrapper type for a TMap to support having array-of-maps UPROPERTY members in UMassObserverRegistry
 */
USTRUCT()
struct FMassEntityObserverClassesMap
{
	GENERATED_BODY()

	/** a helper accessor simplifying access while still keeping Container private */
	const TMap<TObjectPtr<const UScriptStruct>, FMassProcessorClassCollection>& operator*() const
	{
		return Container;
	}

	TMap<TObjectPtr<const UScriptStruct>, FMassProcessorClassCollection>& operator*()
	{
		return Container;
	}

private:
	UPROPERTY()
	TMap<TObjectPtr<const UScriptStruct>, FMassProcessorClassCollection> Container;
};

UCLASS(MinimalAPI)
class UMassObserverRegistry : public UObject
{
	GENERATED_BODY()

public:
	UE_API UMassObserverRegistry();

	static UMassObserverRegistry& GetMutable() { return *GetMutableDefault<UMassObserverRegistry>(); }
	static const UMassObserverRegistry& Get() { return *GetDefault<UMassObserverRegistry>(); }

	UE_API void RegisterObserver(const UScriptStruct& ObservedType, const EMassObservedOperation Operation, TSubclassOf<UMassProcessor> ObserverClass);

protected:
	friend FMassObserverManager;

	UPROPERTY()
	FMassEntityObserverClassesMap FragmentObservers[(uint8)EMassObservedOperation::MAX];

	UPROPERTY()
	FMassEntityObserverClassesMap TagObservers[(uint8)EMassObservedOperation::MAX];
};

#undef UE_API
