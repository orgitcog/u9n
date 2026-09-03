#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhysicsDeformer.generated.h"

/** Represents a single physics particle in the deformation chain */
USTRUCT(BlueprintType)
struct FPhysicsParticle
{
    GENERATED_BODY()

    /** Current position of the particle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Position = FVector::ZeroVector;

    /** Previous position for Verlet integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector PreviousPosition = FVector::ZeroVector;

    /** Velocity of the particle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Velocity = FVector::ZeroVector;

    /** Mass of the particle (affects inertia) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float Mass = 1.0f;

    /** Damping factor for velocity decay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Damping = 0.9f;

    /** Whether this particle is pinned (immovable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bIsPinned = false;
};

/** Constraint between two physics particles */
USTRUCT(BlueprintType)
struct FPhysicsConstraint
{
    GENERATED_BODY()

    /** Index of the first particle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    int32 ParticleIndexA = 0;

    /** Index of the second particle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    int32 ParticleIndexB = 1;

    /** Rest length of the constraint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0"))
    float RestLength = 10.0f;

    /** Stiffness of the constraint (0.0 = loose, 1.0 = rigid) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Stiffness = 0.8f;

    /** Whether this constraint can stretch beyond rest length */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bAllowStretch = true;

    /** Maximum stretch ratio (1.0 = no stretch allowed beyond rest) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "1.0", ClampMax = "2.0"))
    float MaxStretchRatio = 1.2f;
};

/** A group of particles and constraints forming a physics chain (e.g., hair strand, clothing edge) */
USTRUCT(BlueprintType)
struct FPhysicsGroup
{
    GENERATED_BODY()

    /** Name identifier for this physics group */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FName GroupName;

    /** Particles in this physics group */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    TArray<FPhysicsParticle> Particles;

    /** Constraints connecting particles in this group */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    TArray<FPhysicsConstraint> Constraints;

    /** Whether this group is currently active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bIsActive = true;

    /** Gravity multiplier for this group */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float GravityScale = 1.0f;

    /** Wind influence multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float WindInfluence = 1.0f;

    /** Collision radius for particles in this group */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0"))
    float CollisionRadius = 5.0f;
};

/** Physics simulation settings */
USTRUCT(BlueprintType)
struct FPhysicsSimulationSettings
{
    GENERATED_BODY()

    /** Global gravity vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

    /** Global wind direction and strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Wind = FVector::ZeroVector;

    /** Wind turbulence frequency */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float WindTurbulence = 1.0f;

    /** Number of constraint solver iterations per update */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "1", ClampMax = "32"))
    int32 SolverIterations = 4;

    /** Fixed timestep for physics simulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.001", ClampMax = "0.1"))
    float FixedTimeStep = 0.016667f;

    /** Maximum simulation steps per frame (prevents spiral of death) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxStepsPerFrame = 4;
};

UCLASS(Blueprintable)
class DEEPTREECHO_API UPhysicsDeformer : public UObject
{
    GENERATED_BODY()

public:
    UPhysicsDeformer();

    /** Updates the physics simulation for hair, clothing, etc. */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void UpdatePhysicsSimulation(float DeltaTime);

    /** Adds a new physics group to the simulation */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void AddPhysicsGroup(const FPhysicsGroup& Group);

    /** Removes a physics group by name */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    bool RemovePhysicsGroup(FName GroupName);

    /** Gets a physics group by name */
    UFUNCTION(BlueprintPure, Category = "Live2D")
    FPhysicsGroup GetPhysicsGroup(FName GroupName) const;

    /** Sets wind parameters */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetWind(const FVector& WindDirection, float Strength);

    /** Applies an impulse to all particles in a group */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void ApplyImpulse(FName GroupName, const FVector& Impulse);

    /** Resets all physics groups to their rest positions */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void ResetSimulation();

protected:
    /** Performs Verlet integration step for a particle */
    void IntegrateParticle(FPhysicsParticle& Particle, float DeltaTime);

    /** Solves constraints for a physics group */
    void SolveConstraints(FPhysicsGroup& Group);

    /** Applies collision detection and response */
    void ApplyCollisions(FPhysicsGroup& Group);

    /** Calculates wind force with turbulence */
    FVector CalculateWindForce(float Time) const;

private:
    /** All physics groups in the simulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (AllowPrivateAccess = "true"))
    TArray<FPhysicsGroup> PhysicsGroups;

    /** Global simulation settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (AllowPrivateAccess = "true"))
    FPhysicsSimulationSettings SimulationSettings;

    /** Accumulated time for fixed timestep simulation */
    float AccumulatedTime = 0.0f;

    /** Current simulation time for wind turbulence */
    float SimulationTime = 0.0f;
};
