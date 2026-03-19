// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextGraphContextData.h"
#include "Module/AnimNextModuleContextData.h"
#include "Graph/AnimNextGraphInstance.h"
#include "AnimNextGraphLatentPropertiesContextData.generated.h"

namespace UE::AnimNext
{
	struct FLatentPropertyHandle;
}

USTRUCT()
struct FAnimNextGraphLatentPropertiesContextData : public FAnimNextGraphContextData
{
	GENERATED_BODY()

	FAnimNextGraphLatentPropertiesContextData() = default;

	FAnimNextGraphLatentPropertiesContextData(FAnimNextModuleInstance* InModuleInstance, const FAnimNextGraphInstance* InInstance, const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& InLatentHandles, void* InDestinationBasePtr, bool bInIsFrozen)
		: FAnimNextGraphContextData(InModuleInstance, InInstance)
		, LatentHandles(InLatentHandles)
		, DestinationBasePtr(InDestinationBasePtr)
		, bIsFrozen(bInIsFrozen)
	{
	}

	const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& GetLatentHandles() const { return LatentHandles; }
	void* GetDestinationBasePtr() const { return DestinationBasePtr; }
	bool IsFrozen() const { return bIsFrozen; }

private:
	TConstArrayView<UE::AnimNext::FLatentPropertyHandle> LatentHandles;
	void* DestinationBasePtr = nullptr;
	bool bIsFrozen = false;

	friend struct FAnimNextGraphInstance;
	friend struct FAnimNextExecuteContext;
};
