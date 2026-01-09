#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhysicsDeformer.generated.h"

UCLASS(Blueprintable)
class DEEPTREECHO_API UPhysicsDeformer : public UObject
{
    GENERATED_BODY()

public:
    /** Updates the physics simulation for hair, clothing, etc. */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void UpdatePhysicsSimulation(float DeltaTime);

private:
    // TODO: Add properties for physics groups, particles, and constraints
};
