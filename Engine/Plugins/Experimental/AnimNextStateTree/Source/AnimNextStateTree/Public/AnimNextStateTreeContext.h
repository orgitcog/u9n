// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimStateTreeTrait.h"
#include "StructUtils/InstancedStruct.h"

#include "AnimNextStateTreeContext.generated.h"

namespace UE::AnimNext
{
struct FTraitBinding;
struct FExecutionContext;
struct FStateTreeTrait;
}

class UAnimNextAnimationGraph;
struct FAlphaBlendArgs;
struct FAnimNextDataInterfacePayload;

USTRUCT()
struct FAnimNextStateTreeTraitContext
{	
	GENERATED_BODY()
	
	friend UE::AnimNext::FStateTreeTrait;
	friend UE::AnimNext::FStateTreeTrait::FInstanceData;
	
	FAnimNextStateTreeTraitContext() {}

	bool PushAssetOntoBlendStack(TNonNullPtr<UObject> InAsset, const FAlphaBlendArgs& InBlendArguments, FAnimNextDataInterfacePayload&& InPayload) const;
	float QueryPlaybackRatio(TNonNullPtr<UObject> InAsset) const;

	UE::AnimNext::FExecutionContext* GetAnimExecuteContext() { return Context; }
public:
	FAnimNextStateTreeTraitContext(UE::AnimNext::FExecutionContext& InContext, const UE::AnimNext::FTraitBinding* InBinding) : Context(&InContext), Binding(InBinding) {}
protected:
	UE::AnimNext::FExecutionContext* Context = nullptr;
	const UE::AnimNext::FTraitBinding* Binding = nullptr;
};

