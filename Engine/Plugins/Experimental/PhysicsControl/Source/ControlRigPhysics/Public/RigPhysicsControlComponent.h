// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "RigPhysicsBodyComponent.h"

#include "Rigs/RigPhysics.h"

#include "RigPhysicsControlComponent.generated.h"

#define UE_API CONTROLRIGPHYSICS_API

/**
 * A component that can be added to hierarchy elements (joints) to add the data required to control 
 * the simulation of them
 */
USTRUCT(BlueprintType)
struct FRigPhysicsControlComponent : public FRigBaseComponent
{
public:

	GENERATED_BODY()
	DECLARE_RIG_COMPONENT_METHODS(FRigPhysicsControlComponent)

	FRigPhysicsControlComponent()
	{
		ParentBodyComponentKey.ElementKey.Type = ERigElementType::Bone;
		ParentBodyComponentKey.Name = FRigPhysicsBodyComponent::GetDefaultName();
		ChildBodyComponentKey.ElementKey.Type = ERigElementType::Bone;
		ChildBodyComponentKey.Name = FRigPhysicsBodyComponent::GetDefaultName();
	}

	// The body that controls the body being controlled. If this is dynamic, it will be affected
	// too. If unset, then it implies a global control.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Control)
	FRigComponentKey ParentBodyComponentKey;

	// If true, then if the parent body component key is not set, the the default parent body comes
	// from the parent joint. If it is false, then this search is not done, so the control will be
	// in simulation space.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Control)
	bool bUseParentBodyAsDefault = false;

	// The body being controlled
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Control)
	FRigComponentKey ChildBodyComponentKey;

	/** Describes the initial strength etc of the new control */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	FPhysicsControlData ControlData;

	// This is the currently active control multiplier. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	FPhysicsControlMultiplier ControlMultiplier;

	/** Describes the initial target for the new control */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Control)
	FPhysicsControlTarget ControlTarget;

	UE_API virtual void Save(FArchive& A) override;
	UE_API virtual void Load(FArchive& Ar) override;

	UE_API virtual bool CanBeAddedTo(
		const FRigElementKey& InElementKey, const URigHierarchy* InHierarchy, FString* OutFailureReason) const override;

	UE_API virtual void OnRigHierarchyKeyChanged(const FRigHierarchyKey& InOldKey, const FRigHierarchyKey& InNewKey) override;

	virtual FName GetDefaultComponentName() const override { return GetDefaultName(); }

	static FName GetDefaultName() { return TEXT("PhysicsControl"); }
};

#undef UE_API
