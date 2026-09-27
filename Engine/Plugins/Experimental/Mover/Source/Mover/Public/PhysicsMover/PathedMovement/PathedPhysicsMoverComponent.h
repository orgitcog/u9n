// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverComponent.h"
#include "PathedMovementTypes.h"
#include "PathedPhysicsMoverComponent.generated.h"

#define UE_API MOVER_API

class UPathedPhysicsDebugDrawComponent;
class UMoverPathedPhysicsLiaisonComponent;

UCLASS(MinimalAPI, BlueprintType, meta = (BlueprintSpawnableComponent))
class UPathedPhysicsMoverComponent : public UMoverComponent
{
	GENERATED_BODY()

public:
	UE_API UPathedPhysicsMoverComponent();

	UE_API virtual void OnRegister() override;
	UE_API virtual void OnUnregister() override;
	UE_API virtual void InitializeComponent() override;
	UE_API virtual void UninitializeComponent() override;

#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API bool IsMoving() const;

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API void SetIsMoving(bool bShouldMove);

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API bool IsInReverse() const;

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API void SetPlaybackDirection(bool bPlayForward);

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	EPathedPhysicsPlaybackBehavior GetDefaultPlaybackBehavior() const { return DefaultPlaybackBehavior; }

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API void SetDefaultPlaybackBehavior(EPathedPhysicsPlaybackBehavior PlaybackBehavior);

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API bool IsJointEnabled() const;

	bool ShouldMoveOnSpawn() const { return bAutoMoveOnSpawn; }

	//@todo DanH: SetPathProgress, GetPathProgress 
	//@todo DanH: GetTimeAlongPath (setting this is impossible for nonlinear easing)
	
	/**
	 * Changes path origin (i.e. the transform at the very beginning), causing the whole path to shift accordlingly.
	 * The updated component is immediately teleported by the delta between the old and new origins, retaining all other physics state
	 * If a more gradual origin shift is desired, it's up to the caller to set this with a small lerp delta each frame.
	 */
	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API void SetPathOriginTransform(const FTransform& NewPathOrigin);

	UFUNCTION(BlueprintCallable, Category = PathedMover)
	UE_API const FTransform& GetPathOriginTransform() const;

	//@todo DanH: See if Core will let me move DEFINE_PASSKEY into the engine
	//DEFINE_PASSKEY(UMoverPathedPhysicsLiaisonComponent, bool, IsMoving);
	//void NotifyIsMovingChanged(FIsMovingPasskey bIsMoving);
	UE_API void NotifyIsMovingChanged(bool bIsMoving);

protected:
	/** Whether to start moving along the path immediately upon beginning play */
	UPROPERTY(EditAnywhere, Category = "Pathed Movement")
	bool bAutoMoveOnSpawn = true;

	/** Default follow behavior for the path as a whole */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathed Movement")
	EPathedPhysicsPlaybackBehavior DefaultPlaybackBehavior = EPathedPhysicsPlaybackBehavior::OneShot;

	/** How long after being told to StartMoving() to delay before actually moving. Generally used to stagger the start times of a handful of autoplaying instances. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pathed Movement")
	float MovementStartDelay = 0.f;

	/**  */
	
	//@todo DanH: TimeDilation
	
	//@todo DanH: Allow disabling the joint at this level regardless of what the movement mode wants?

	UPROPERTY(Transient)
	TObjectPtr<UMoverPathedPhysicsLiaisonComponent> PathedPhysicsLiaison;

protected:
	friend class UPathedPhysicsDebugDrawComponent;

	/** When true and the root component of the actor is a mesh, a duplicate mesh will be shown at PreviewMeshProgress along the starting/default path */
	UPROPERTY(EditAnywhere, Category = "Pathed Movement|Debug")
	bool bDisplayProgressPreviewMesh = true;

	/** How far along the starting/default path to preview the controlled mesh */
	UPROPERTY(EditAnywhere, Category = "Pathed Movement|Debug", meta = (EditCondition = bDisplayProgressPreviewMesh, UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float PreviewMeshProgress = 1.f;

	/** The material to apply to the preview mesh displayed along the path at PreviewMeshProgress (leave empty for an exact duplicate) */
	UPROPERTY(EditAnywhere, Category = "Pathed Movement|Debug", meta = (EditCondition = bDisplayProgressPreviewMesh))
	TObjectPtr<UMaterialInterface> ProgressPreviewMeshMaterial;
	
	UPROPERTY(Transient)
	TObjectPtr<UPathedPhysicsDebugDrawComponent> DebugDrawComp;
};

#undef UE_API
