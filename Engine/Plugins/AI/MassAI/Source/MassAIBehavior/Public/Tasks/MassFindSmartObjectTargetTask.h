// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassNavigationTypes.h"
#include "MassStateTreeTypes.h"
#include "SmartObjectRuntime.h"
#include "MassFindSmartObjectTargetTask.generated.h"

#define UE_API MASSAIBEHAVIOR_API

class USmartObjectSubsystem;

USTRUCT()
struct FMassFindSmartObjectTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Input)
	FSmartObjectClaimHandle ClaimedSlot;

	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation SmartObjectLocation;
};

/** Computes move target to a smart object based on current location. */
USTRUCT(meta = (DisplayName = "Find Smart Object Target"))
struct FMassFindSmartObjectTargetTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassFindSmartObjectTargetInstanceData;

protected:
	UE_API virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};

#undef UE_API
