// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextStateTreeContext.h"
#include "AnimNextStateTreeTypes.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "AlphaBlend.h"
#include "StructUtils/PropertyBag.h"

#include "AnimNextStateTreeGraphInstanceTask.generated.h"

USTRUCT()
struct ANIMNEXTSTATETREE_API FAnimNextGraphInstanceTaskInstanceData
{
	GENERATED_BODY()

	// The asset to instantiate
	UPROPERTY(EditAnywhere, Category = Animation, meta=(GetAllowedClasses="/Script/AnimNextAnimGraph.AnimNextAnimGraphSettings:GetAllowedAssetClasses"))
	TObjectPtr<UObject> Asset = nullptr;

	// The payload to use for the asset when instanced
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional, FixedLayout))
	FInstancedPropertyBag Payload;

	// Blend options for when the state is pushed
	UPROPERTY(EditAnywhere, Category = Animation)
	FAlphaBlendArgs BlendOptions;

	// Whether this task should continue to tick once state is entered
	UPROPERTY(EditAnywhere, Category = Animation)
	bool bContinueTicking = true;

	// Current playback ratio (debug)
	UPROPERTY(VisibleAnywhere, Category = Animation)
	float PlaybackRatio = 1.0f;
};

// Basic task pushing AnimationGraph onto blend stack
USTRUCT(meta = (DisplayName = "AnimNext Graph"))
struct ANIMNEXTSTATETREE_API FAnimNextStateTreeGraphInstanceTask : public FAnimNextStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnimNextGraphInstanceTaskInstanceData;

	FAnimNextStateTreeGraphInstanceTask();
	
	virtual bool Link(FStateTreeLinker& Linker) override;
protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
#if WITH_EDITOR
	virtual void PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView) override;
	virtual void GetObjectReferences(TArray<const UObject*>& OutReferencedObjects, const FStateTreeDataView InstanceDataView) const override;
#endif
public:
	TStateTreeExternalDataHandle<FAnimNextStateTreeTraitContext> TraitContextHandle;
};
