// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "MoverComponent.h"
#include "GameFramework/NavMovementInterface.h"

#include "NavMoverComponent.generated.h"

#define UE_API MOVER_API

/**
 * NavMoverComponent: Responsible for implementing INavMoveInterface with UMoverComponent so pathfinding and other forms of navigation movement work.
 * This component also caches the input given to it that is then consumed by the mover system.
 * Note: This component relies on the parent actor having a MoverComponent as well. By default this component only has a reference to MoverComponent meaning
 * we use other ways (such as gameplay tags for the active movement mode) to check for state rather than calling specific functions on the active MoverComponent.
 */
UCLASS(MinimalAPI, BlueprintType, meta = (BlueprintSpawnableComponent))
class UNavMoverComponent : public UActorComponent, public INavMovementInterface
{
	GENERATED_BODY()

public:
	UE_API UNavMoverComponent();

	/** Properties that define how the component can move. */
	UPROPERTY(EditAnywhere, Category="Nav Movement", meta = (DisplayName = "Movement Capabilities", Keywords = "Nav Agent"))
	FNavAgentProperties NavAgentProps;

	/** Expresses runtime state of character's movement. Put all temporal changes to movement properties here */
	UPROPERTY()
	FMovementProperties MovementState;
	
protected:
	/** associated properties for nav movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nav Movement")
	FNavMovementProperties NavMovementProperties;

	/** Keeps track of the last game frame we consumed nav movement. */
	uint64 GameFrameNavMovementConsumed = 0;

	/** Keeps track of the last game frame we requested nav movement. */
	uint64 GameFrameNavMovementRequested = 0;
	
	// Used to store movement input intent from requested moves
	FVector CachedNavMoveInputIntent = FVector::ZeroVector;
	// Used to store movement input velocity from requested moves
	FVector CachedNavMoveInputVelocity = FVector::ZeroVector;
	
private:
	/** object implementing IPathFollowingAgentInterface. Is private to control access to it.
	 *	@see SetPathFollowingAgent, GetPathFollowingAgent */
	UPROPERTY()
	TObjectPtr<UObject> PathFollowingComp;

	/** Associated Movement component that will actually move the actor */ 
	UPROPERTY()
	TWeakObjectPtr<UMoverComponent> MoverComponent;
	
public:
	UE_API virtual void InitializeComponent() override;
	UE_API virtual void BeginPlay() override;
	
	/** Get the owner of the object consuming nav movement */
	virtual UObject* GetOwnerAsObject() const override { return GetOwner(); }
	
	/** Get the component this movement component is updating */
	virtual TObjectPtr<UObject> GetUpdatedObject() const override { return MoverComponent.IsValid() ? MoverComponent->GetUpdatedComponent() : nullptr; }

	/** Get axis-aligned cylinder around this actor, used for simple collision checks in nav movement */
	UE_API virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override;

	/** Returns collision extents vector for this object, based on GetSimpleCollisionCylinder. */
	UE_API virtual FVector GetSimpleCollisionCylinderExtent() const override;
	
	/** Get forward vector of the object being driven by nav movement */
	UE_API virtual FVector GetForwardVector() const override;
	
	/** Get the current velocity of the movement component */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	UE_API virtual FVector GetVelocityForNavMovement() const override;

	/** Get the max speed of the movement component */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	UE_API virtual float GetMaxSpeedForNavMovement() const override;

	// Overridden to also call StopActiveMovement().
	UE_API virtual void StopMovementImmediately() override;

	// Writes internal cached requested velocities to the MoveInput passed in. Returns true if it had move input to write.
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	UE_API bool ConsumeNavMovementData(FVector& OutMoveInputIntent, FVector& OutMoveInputVelocity);
	
	/** Returns location of controlled actor - meaning center of collision bounding box */
	UE_API virtual FVector GetLocation() const override;
	/** Returns location of controlled actor's "feet": the center bottom of its collision bounding box at its current location */
	UE_API virtual FVector GetFeetLocation() const override;
	/** Returns location of controlled actor's "feet": the center bottom of its collision bounding box, as if it was at a specific location */
	UE_API virtual FVector GetFeetLocationAt(FVector ComponentLocation) const;
	/** Returns based location of controlled actor */
	UE_API virtual FBasedPosition GetFeetLocationBased() const override;

	/** Set nav agent properties from an object */
	UE_API virtual void UpdateNavAgent(const UObject& ObjectToUpdateFrom) override;
	
	/** path following: request new velocity */
	UE_API virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;

	/** path following: request new move input (normal vector = full strength) */
	UE_API virtual void RequestPathMove(const FVector& MoveInput) override;

	/** check if current move target can be reached right now if positions are matching
	 *  (e.g. performing scripted move and can't stop) */
	UE_API virtual bool CanStopPathFollowing() const override;

	/** Get Nav movement props struct this component uses */
	virtual FNavMovementProperties* GetNavMovementProperties() override { return &NavMovementProperties; }
	/** Returns the NavMovementProps(const) */
	virtual const FNavMovementProperties& GetNavMovementProperties() const override{ return NavMovementProperties; } 
	
	UE_API virtual void SetPathFollowingAgent(IPathFollowingAgentInterface* InPathFollowingAgent) override;
	UE_API virtual IPathFollowingAgentInterface* GetPathFollowingAgent() override;
	UE_API virtual const IPathFollowingAgentInterface* GetPathFollowingAgent() const override;

	/** Returns the NavAgentProps(const) */
	UE_API virtual const FNavAgentProperties& GetNavAgentPropertiesRef() const override;
	/** Returns the NavAgentProps */
	UE_API virtual FNavAgentProperties& GetNavAgentPropertiesRef() override;

	/** Resets runtime movement state to character's movement capabilities */
	UE_API virtual void ResetMoveState() override;

	/** Returns true if path following can start */
	UE_API virtual bool CanStartPathFollowing() const override;

	/** Returns true if currently crouching */
	UE_API virtual bool IsCrouching() const override;
	
	/** Returns true if currently falling (not flying, in a non-fluid volume, and not on the ground) */ 
	UE_API virtual bool IsFalling() const override;

	/** Returns true if currently moving on the ground (e.g. walking or driving) */
	UE_API virtual bool IsMovingOnGround() const override;
	
	/** Returns true if currently swimming (moving through a fluid volume) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	UE_API virtual bool IsSwimming() const override;

	/** Returns true if currently flying (moving through a non-fluid volume without resting on the ground) */
	UE_API virtual bool IsFlying() const override;
};

#undef UE_API
