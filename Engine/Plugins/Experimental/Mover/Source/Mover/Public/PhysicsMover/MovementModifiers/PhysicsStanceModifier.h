// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverComponent.h"
#include "DefaultMovementSet/MovementModifiers/StanceModifier.h"
#include "Components/SceneComponent.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsStanceModifier.generated.h"

#define UE_API MOVER_API

/**
 * Physics specialized version of FStanceModifier
 */
USTRUCT(BlueprintType)
struct FPhysicsStanceModifier : public FStanceModifier
{
	GENERATED_BODY()

	// @return newly allocated copy of this FMovementModifier. Must be overridden by child classes
	UE_API virtual FMovementModifierBase* Clone() const override;

	UE_API virtual void NetSerialize(FArchive& Ar) override;

	UE_API virtual UScriptStruct* GetScriptStruct() const override;

	UE_API virtual FString ToSimpleString() const override;

	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;

	UE_API virtual bool CanExpand_Internal(UMoverComponent* MoverComp, USceneComponent* UpdatedComponent, const FMoverSyncState& InSyncState) const;

	static UE_API void OnPostSimulationTick(const FStanceModifier* Modifier, UMoverComponent* MoverComp, UPrimitiveComponent* UpdatedPrimitive,  bool bIsCrouching, bool& bPostProcessed, OUT bool& bStanceChanged);

	UE_API virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;

	UE_API virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;

protected:

	template <class T>
	void UpdateTargetHeight(UMoverComponent* MoverComp, float OriginalHalfHeight, float CrouchedHalfHeight, bool bCrouching)
	{
		if (T* MovementMode = MoverComp->FindMode_Mutable<T>())
		{
			if (MovementMode->template Implements<UPhysicsCharacterMovementModeInterface>())
			{
				const float OriginalTargetHeight = GetDefault<T>(MovementMode->GetClass())->GetTargetHeight();
				const float Offset = OriginalTargetHeight - OriginalHalfHeight;
				
				MovementMode->SetTargetHeightOverride(Offset + (bCrouching ? CrouchedHalfHeight : OriginalHalfHeight));
			}
		}
	}
};

#undef UE_API
