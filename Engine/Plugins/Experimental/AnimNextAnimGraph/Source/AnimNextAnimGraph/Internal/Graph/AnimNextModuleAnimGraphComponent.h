// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Injection/InjectionInfo.h"
#include "Module/AnimNextModuleInstanceComponent.h"
#include "AnimNextModuleAnimGraphComponent.generated.h"

struct FAnimNextGraphInstance;
class UAnimNextAnimationGraph;

// Module component that owns/allocates/release all animation graph instances on a module
USTRUCT()
struct FAnimNextModuleAnimGraphComponent : public FAnimNextModuleInstanceComponent
{
	GENERATED_BODY()

	FAnimNextModuleAnimGraphComponent() = default;

	TWeakPtr<FAnimNextGraphInstance> AllocateInstance(const UAnimNextAnimationGraph* InAnimationGraph, FAnimNextGraphInstance* InParentInstance = nullptr, FName InEntryPoint = NAME_None);

	void ReleaseInstance(TWeakPtr<FAnimNextGraphInstance> InInstance);

	void AddStructReferencedObjects(class FReferenceCollector& Collector);

private:
	// All the owned graph instances for the module
	TArray<TSharedPtr<FAnimNextGraphInstance>> GraphInstances;
};

template<>
struct TStructOpsTypeTraits<FAnimNextModuleAnimGraphComponent> : public TStructOpsTypeTraitsBase2<FAnimNextModuleAnimGraphComponent>
{
	enum
	{
		WithAddStructReferencedObjects = true,
	};
};
