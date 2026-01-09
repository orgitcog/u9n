// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Module/AnimNextModuleContextData.h"
#include "Graph/AnimNextGraphInstance.h"
#include "AnimNextGraphContextData.generated.h"

namespace UE::AnimNext
{
	struct FLatentPropertyHandle;
}

USTRUCT()
struct FAnimNextGraphContextData : public FAnimNextModuleContextData
{
	GENERATED_BODY()

	FAnimNextGraphContextData() = default;

	FAnimNextGraphContextData(FAnimNextModuleInstance* InModuleInstance, const FAnimNextGraphInstance* InInstance)
		: FAnimNextModuleContextData(InModuleInstance, InInstance)
	{
	}

	const FAnimNextGraphInstance& GetGraphInstance() const { return static_cast<const FAnimNextGraphInstance&>(GetDataInterfaceInstance()); }

private:
	friend struct FAnimNextGraphInstance;
	friend struct FAnimNextExecuteContext;
};
