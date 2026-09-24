// GameFramework/CharacterMovementComponent.h - Stub for standalone compilation testing
// In production, the actual Unreal Engine header will be used.

#pragma once
#include "CoreMinimal.h"

class UCharacterMovementComponent : public UActorComponent {
public:
    FVector Velocity;
    float MaxWalkSpeed = 600.0f;
    float MaxAcceleration = 2048.0f;
    float BrakingDecelerationWalking = 2048.0f;
    float GroundFriction = 8.0f;
    float JumpZVelocity = 420.0f;
    float AirControl = 0.05f;
    
    bool IsFalling() const { return false; }
    bool IsMovingOnGround() const { return true; }
    bool IsSwimming() const { return false; }
    bool IsFlying() const { return false; }
    
    void SetMovementMode(int NewMovementMode) {}
    int GetMovementMode() const { return 0; }
    
    void AddImpulse(FVector Impulse, bool bVelocityChange = false) {}
    void AddForce(FVector Force) {}
    void Launch(FVector LaunchVelocity) {}
    
    void StopMovementImmediately() {}
};
