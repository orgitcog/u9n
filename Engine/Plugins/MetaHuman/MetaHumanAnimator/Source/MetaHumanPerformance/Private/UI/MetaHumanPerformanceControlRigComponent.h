// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"

#include "MetaHumanPerformanceControlRigComponent.generated.h"

/**
 * Component capable of rendering ControlRigShape actors. It uses the same mechanism as a ChildActorComponent where
 * upon being registered will spawn all control rig shape actors in the world
 */
UCLASS(MinimalAPI)
class UMetaHumanPerformanceControlRigComponent
	: public UPrimitiveComponent
{
	GENERATED_BODY()

public:

	/** Sets which control rig to use */
	void SetControlRig(class UControlRig* InControlRig);

	/** Updates the transform of all control rig shapes */
	void UpdateControlRigShapes();

	/** Get the bounding box of all visible control rig shapes */
	FBox GetShapesBoundingBox() const;

public:

	//~Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bInDestroyingHierarchy) override;
	//~End UActorComponent Interface

public:

	UPROPERTY()
	TObjectPtr<class UControlRig> ControlRig;

	UPROPERTY()
	TArray<TObjectPtr<class AControlRigShapeActor>> ShapeActors;

public:

	static const FName HeadIKControlName;
	static const FName HeadIKSwitchControlName;

protected:

	/** Applies the visibility to all control rig shape actors we are managing */
	virtual void OnVisibilityChanged() override;

private:

	/** Spawn all control rig shapes */
	void SpawnControlRigShapes();

	/** Destroy all control rig shape actors */
	void DestroyControlRigShapes();

};