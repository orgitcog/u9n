#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhysicsDeformer.generated.h"

USTRUCT(BlueprintType)
struct FPhysicsParticle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Force = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float Mass = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float Damping = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    bool bFixed = false;
};

USTRUCT(BlueprintType)
struct FPhysicsConstraint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    int32 ParticleIndexA = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    int32 ParticleIndexB = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float RestLength = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float Stiffness = 0.8f;
};

USTRUCT(BlueprintType)
struct FPhysicsGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FName GroupName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    TArray<FPhysicsParticle> Particles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    TArray<FPhysicsConstraint> Constraints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector Wind = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float AirResistance = 0.1f;
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

    /** Add a physics group */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void AddPhysicsGroup(const FName& GroupName, int32 ParticleCount);

    /** Set gravity for a physics group */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetGravity(const FName& GroupName, const FVector& NewGravity);

    /** Set wind for a physics group */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetWind(const FName& GroupName, const FVector& NewWind);

    /** Add constraint between particles */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void AddConstraint(const FName& GroupName, int32 ParticleA, int32 ParticleB, float RestLength, float Stiffness);

    /** Get particle position */
    UFUNCTION(BlueprintPure, Category = "Live2D")
    FVector GetParticlePosition(const FName& GroupName, int32 ParticleIndex) const;

    /** Set particle position */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetParticlePosition(const FName& GroupName, int32 ParticleIndex, const FVector& Position);

    /** Reset physics simulation */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void ResetPhysics();

private:
    /** Update particle forces */
    void UpdateForces(FPhysicsGroup& Group, float DeltaTime);

    /** Integrate particle motion */
    void IntegrateMotion(FPhysicsGroup& Group, float DeltaTime);

    /** Solve constraints */
    void SolveConstraints(FPhysicsGroup& Group);

    /** Apply collision detection */
    void ApplyCollisions(FPhysicsGroup& Group);

    /** Physics groups for different body parts */
    UPROPERTY()
    TMap<FName, FPhysicsGroup> PhysicsGroups;

    /** Constraint solver iterations */
    UPROPERTY(EditAnywhere, Category = "Physics")
    int32 ConstraintIterations = 3;

    /** Enable collision detection */
    UPROPERTY(EditAnywhere, Category = "Physics")
    bool bEnableCollisions = true;

    /** Ground plane height for collision */
    UPROPERTY(EditAnywhere, Category = "Physics")
    float GroundPlaneHeight = 0.0f;
};
