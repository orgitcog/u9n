// AIController/AIController.h - Stub for standalone compilation testing
// In production, the actual Unreal Engine header (AIModule) will be used.

#pragma once
#include "CoreMinimal.h"

/**
 * Stub for UE's UNavigationSystemV1-style move request result.
 */
struct FAIMoveRequest
{
    FAIMoveRequest() = default;
    explicit FAIMoveRequest(AActor* InGoalActor) : GoalActor(InGoalActor) {}

    FAIMoveRequest& SetAcceptanceRadius(float Radius) { AcceptanceRadius = Radius; return *this; }
    FAIMoveRequest& SetUsePathfinding(bool bUse) { bUsePathfinding = bUse; return *this; }

    AActor* GoalActor = nullptr;
    float AcceptanceRadius = 50.0f;
    bool bUsePathfinding = true;
};

enum class EPathFollowingRequestResult : uint8
{
    Failed,
    AlreadyAtGoal,
    RequestSuccessful
};

enum class EPathFollowingResult : uint8
{
    Success,
    Blocked,
    OffPath,
    Aborted,
    Skipped,
    Invalid
};

/**
 * Stub for Unreal Engine's AAIController (AIModule).
 *
 * AAIController is the base class for AI-controlled pawns.  It wires together
 * the Behavior Tree / Blackboard, navigation, and perception subsystems.  This
 * stub provides the minimal interface required so UE-style C++ can be parsed
 * in the standalone CMake build.
 */
class AAIController : public AController
{
public:
    // -----------------------------------------------------------------------
    // Navigation
    // -----------------------------------------------------------------------

    /** Request a move to a specific actor. */
    EPathFollowingRequestResult MoveToActor(AActor* Goal,
                                             float AcceptanceRadius = -1.0f,
                                             bool bStopOnOverlap = true,
                                             bool bUsePathfinding = true)
    { return EPathFollowingRequestResult::Failed; }

    /** Request a move to a world-space location. */
    EPathFollowingRequestResult MoveToLocation(const FVector& Dest,
                                                float AcceptanceRadius = -1.0f,
                                                bool bStopOnOverlap = true,
                                                bool bUsePathfinding = true)
    { return EPathFollowingRequestResult::Failed; }

    /** Stop any in-progress movement. */
    void StopMovement() {}

    // -----------------------------------------------------------------------
    // Focus / gaze
    // -----------------------------------------------------------------------

    /** Make the controller face a world location. */
    void SetFocalPoint(const FVector& FP) {}

    /** Make the controller face an actor. */
    void SetFocus(AActor* NewFocus) {}

    /** Clear focus. */
    void ClearFocus(int Priority = 0) {}

    FVector GetFocalPoint() const { return FVector(); }
    AActor* GetFocusActor() const { return nullptr; }

    // -----------------------------------------------------------------------
    // Pawn possession
    // -----------------------------------------------------------------------

    /** Called when the controller possesses a pawn. Override to set up components. */
    virtual void OnPossess(APawn* InPawn) {}

    /** Called when the controller un-possesses a pawn. */
    virtual void OnUnPossess() {}

    // -----------------------------------------------------------------------
    // Blackboard
    // -----------------------------------------------------------------------

    class UBlackboardComponent* GetBlackboardComponent() const { return nullptr; }
};
