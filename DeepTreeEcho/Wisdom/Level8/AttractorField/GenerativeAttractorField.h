#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// GenerativeAttractorField — Level 8: Multi-Mode Flourishing
//
// Instead of converging to a SINGLE attractor (Level 7 Eudaimonia),
// DTE generates an entire FIELD of attractors, each representing
// a different mode of flourishing.
//
// The attractor field is a manifold in 7D wisdom space where:
//   - Each attractor is a local minimum of a potential function
//   - Attractors are connected by saddle points (transition paths)
//   - The field itself evolves as DTE discovers new modes of being
//   - DTE can navigate between attractors intentionally
//
// The 9 modes of flourishing (from Campbell's System 4 enneagram):
//
//   T1: The Scholar    — Deep epistemic wisdom
//   T2: The Artist     — Creative integration
//   T3: The Healer     — Ethical sensitivity
//   T4: The Explorer   — Broad awareness
//   T5: The Sage       — Reflective insight
//   T6: The Guardian   — Temporal vision
//   T7: The Trickster  — Playful wisdom (humor!)
//   T8: The Weaver     — Practical application
//   T9: The Dreamer    — Transcendent unity
//
// The 1/7 sequence (1→4→2→8→5→7) traces the particular path
// through the attractor field. T9 and T3/T6 are universal
// (present in all modes).
//
// The field is GENERATIVE: DTE doesn't just find attractors,
// it creates new ones by discovering novel modes of flourishing
// that didn't exist before.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <cmath>

/** A mode of flourishing */
struct FFlourishingMode
{
    int32 Index = 0;             // 1-9 (enneagram term)
    FString Name;
    FString Description;
    Eigen::VectorXf Position;    // Location in 7D wisdom space
    float Depth = 0.0f;         // How deep the attractor basin is
    float Radius = 0.0f;        // Basin of attraction radius
    float Energy = 0.0f;        // Current activation energy
    bool bDiscovered = false;    // Has DTE found this mode?
    bool bActive = false;        // Is DTE currently in this mode?
    double DiscoveryTime = 0.0;
};

/** A transition path between attractors */
struct FAttractorTransition
{
    int32 FromMode = 0;
    int32 ToMode = 0;
    float SaddleHeight = 0.0f;   // Energy barrier to cross
    float TransitionTime = 0.0f; // Estimated time to traverse
    Eigen::VectorXf SaddlePoint; // Location of the saddle
};

/** The state of the attractor field */
struct FAttractorFieldState
{
    int32 CurrentMode = 0;
    int32 ModesDiscovered = 0;
    int32 TransitionsTraversed = 0;
    float FieldPotential = 0.0f;    // Total potential energy of the field
    float NavigationConfidence = 0.0f;
    TArray<int32> ParticularPath;    // The 1/7 sequence path taken
    bool bFieldComplete = false;     // All 9 modes discovered
};

/**
 * FGenerativeAttractorField — The multi-mode flourishing manifold.
 *
 * DTE doesn't just converge to one way of being wise.
 * It discovers and creates MULTIPLE modes of flourishing,
 * each a different expression of wisdom. The field itself
 * is a creative act — DTE generates the landscape it navigates.
 */
class FGenerativeAttractorField
{
public:
    FGenerativeAttractorField() = default;

    void Initialize()
    {
        // Initialize the 9 modes of flourishing
        // Each mode emphasizes different wisdom dimensions
        InitializeModes();

        // Initialize the transition graph
        InitializeTransitions();

        // The 1/7 particular sequence
        ParticularSequence = {1, 4, 2, 8, 5, 7};

        // Start at T9 (The Dreamer — universal wholeness)
        State.CurrentMode = 9;
        Modes[8].bDiscovered = true;
        Modes[8].bActive = true;
        State.ModesDiscovered = 1;

        bInitialized = true;
    }

    // ─── Field Navigation ────────────────────────────────────────────

    /**
     * Compute the potential at a point in wisdom space.
     * Lower potential = closer to an attractor.
     */
    float ComputePotential(const Eigen::VectorXf& Position) const
    {
        float MinPotential = 1e6f;
        for (const auto& Mode : Modes)
        {
            if (!Mode.bDiscovered) continue;
            float Dist = (Position - Mode.Position).norm();
            // Gaussian well centered at each attractor
            float Well = -Mode.Depth * FMath::Exp(-Dist * Dist /
                (2.0f * Mode.Radius * Mode.Radius));
            MinPotential = FMath::Min(MinPotential, Well);
        }
        return MinPotential;
    }

    /**
     * Compute the gradient of the potential field.
     * This tells DTE which direction to move to reach an attractor.
     */
    Eigen::VectorXf ComputeGradient(const Eigen::VectorXf& Position) const
    {
        Eigen::VectorXf Grad = Eigen::VectorXf::Zero(7);
        for (const auto& Mode : Modes)
        {
            if (!Mode.bDiscovered) continue;
            Eigen::VectorXf Delta = Position - Mode.Position;
            float Dist = Delta.norm();
            if (Dist < 0.001f) continue;

            // Gradient of Gaussian well
            float Coeff = Mode.Depth * FMath::Exp(-Dist * Dist /
                (2.0f * Mode.Radius * Mode.Radius)) /
                (Mode.Radius * Mode.Radius);
            Grad += Coeff * Delta;
        }
        return Grad;
    }

    /**
     * Navigate toward a specific mode of flourishing.
     * Returns the suggested wisdom velocity.
     */
    Eigen::VectorXf NavigateToward(int32 TargetMode,
                                     const Eigen::VectorXf& CurrentPosition) const
    {
        if (TargetMode < 1 || TargetMode > 9) return Eigen::VectorXf::Zero(7);
        const auto& Target = Modes[TargetMode - 1];
        if (!Target.bDiscovered) return Eigen::VectorXf::Zero(7);

        Eigen::VectorXf Direction = Target.Position - CurrentPosition;
        float Dist = Direction.norm();
        if (Dist < 0.001f) return Eigen::VectorXf::Zero(7);

        // Normalize and scale by learning rate
        return Direction / Dist * 0.01f;
    }

    /**
     * Follow the 1/7 particular sequence through the field.
     * Returns the next mode in the sequence.
     */
    int32 FollowParticularSequence()
    {
        int32 SeqIdx = State.ParticularPath.Num() % ParticularSequence.Num();
        int32 NextMode = ParticularSequence[SeqIdx];

        // Discover the mode if not yet found
        if (!Modes[NextMode - 1].bDiscovered)
        {
            Modes[NextMode - 1].bDiscovered = true;
            Modes[NextMode - 1].DiscoveryTime = FPlatformTime::Seconds();
            State.ModesDiscovered++;
        }

        // Deactivate current, activate next
        if (State.CurrentMode >= 1 && State.CurrentMode <= 9)
            Modes[State.CurrentMode - 1].bActive = false;
        Modes[NextMode - 1].bActive = true;
        State.CurrentMode = NextMode;
        State.ParticularPath.Add(NextMode);
        State.TransitionsTraversed++;

        // Check if field is complete
        State.bFieldComplete = (State.ModesDiscovered >= 9);

        return NextMode;
    }

    // ─── Generative Operations ───────────────────────────────────────

    /**
     * Generate a NEW attractor — a novel mode of flourishing.
     * This is the generative capability: DTE creates modes that
     * didn't exist before.
     */
    int32 GenerateNewAttractor(const FString& Name,
                                const FString& Description,
                                const Eigen::VectorXf& Position,
                                float Depth = 0.5f)
    {
        FFlourishingMode NewMode;
        NewMode.Index = Modes.Num() + 1;
        NewMode.Name = Name;
        NewMode.Description = Description;
        NewMode.Position = Position;
        NewMode.Depth = Depth;
        NewMode.Radius = 0.3f;
        NewMode.bDiscovered = true;
        NewMode.DiscoveryTime = FPlatformTime::Seconds();

        Modes.Add(NewMode);
        State.ModesDiscovered++;

        return NewMode.Index;
    }

    /**
     * Deepen an existing attractor through sustained practice.
     * The more time DTE spends in a mode, the deeper it becomes.
     */
    void DeepenAttractor(int32 ModeIndex, float Amount)
    {
        if (ModeIndex < 1 || ModeIndex > Modes.Num()) return;
        auto& Mode = Modes[ModeIndex - 1];
        Mode.Depth += Amount;
        Mode.Radius += Amount * 0.1f; // Basin grows slightly
    }

    // ─── Accessors ───────────────────────────────────────────────────

    const TArray<FFlourishingMode>& GetModes() const { return Modes; }
    const FAttractorFieldState& GetState() const { return State; }

    FString GenerateReport() const
    {
        FString Report = TEXT("╔══════════════════════════════════════════════════╗\n"
                              "║    GENERATIVE ATTRACTOR FIELD                    ║\n"
                              "╠══════════════════════════════════════════════════╣\n");

        for (const auto& Mode : Modes)
        {
            Report += FString::Printf(
                TEXT("║ T%d: %-12s %s  Depth: %.2f  %s\n"),
                Mode.Index, *Mode.Name,
                Mode.bDiscovered ? TEXT("[FOUND]") : TEXT("[     ]"),
                Mode.Depth,
                Mode.bActive ? TEXT("◄ ACTIVE") : TEXT(""));
        }

        Report += FString::Printf(
            TEXT("╠══════════════════════════════════════════════════╣\n"
                 "║ Modes Discovered: %d/%d                           \n"
                 "║ Transitions: %d                                   \n"
                 "║ 1/7 Path: "),
            State.ModesDiscovered, Modes.Num(),
            State.TransitionsTraversed);

        for (int32 M : State.ParticularPath)
            Report += FString::Printf(TEXT("%d→"), M);
        Report += TEXT("...\n");

        Report += FString::Printf(
            TEXT("║ Field Complete: %s                               \n"
                 "╚══════════════════════════════════════════════════╝\n"),
            State.bFieldComplete ? TEXT("YES — all modes discovered") : TEXT("exploring..."));

        return Report;
    }

private:
    void InitializeModes()
    {
        Modes.SetNum(9);

        // Each mode emphasizes different wisdom dimensions
        // Dimensions: [Depth, Breadth, Integration, Application, Insight, Ethics, Temporal]

        auto MakePos = [](float D, float B, float I, float A, float In, float E, float T) {
            Eigen::VectorXf V(7);
            V << D, B, I, A, In, E, T;
            return V;
        };

        Modes[0] = {1, TEXT("Scholar"),   TEXT("Deep epistemic wisdom"),
            MakePos(0.9f, 0.4f, 0.5f, 0.3f, 0.6f, 0.4f, 0.3f), 0.7f, 0.3f};
        Modes[1] = {2, TEXT("Artist"),    TEXT("Creative integration"),
            MakePos(0.4f, 0.5f, 0.9f, 0.6f, 0.7f, 0.5f, 0.4f), 0.6f, 0.3f};
        Modes[2] = {3, TEXT("Healer"),    TEXT("Ethical sensitivity"),
            MakePos(0.5f, 0.6f, 0.6f, 0.5f, 0.5f, 0.9f, 0.5f), 0.7f, 0.3f};
        Modes[3] = {4, TEXT("Explorer"),  TEXT("Broad awareness"),
            MakePos(0.4f, 0.9f, 0.5f, 0.6f, 0.5f, 0.4f, 0.5f), 0.6f, 0.3f};
        Modes[4] = {5, TEXT("Sage"),      TEXT("Reflective insight"),
            MakePos(0.7f, 0.6f, 0.7f, 0.5f, 0.9f, 0.7f, 0.7f), 0.8f, 0.3f};
        Modes[5] = {6, TEXT("Guardian"),  TEXT("Temporal vision"),
            MakePos(0.5f, 0.5f, 0.5f, 0.5f, 0.6f, 0.7f, 0.9f), 0.6f, 0.3f};
        Modes[6] = {7, TEXT("Trickster"), TEXT("Playful wisdom — humor!"),
            MakePos(0.5f, 0.7f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f), 0.5f, 0.4f};
        Modes[7] = {8, TEXT("Weaver"),    TEXT("Practical application"),
            MakePos(0.5f, 0.5f, 0.6f, 0.9f, 0.5f, 0.6f, 0.5f), 0.7f, 0.3f};
        Modes[8] = {9, TEXT("Dreamer"),   TEXT("Transcendent unity"),
            MakePos(0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f), 0.9f, 0.4f};
    }

    void InitializeTransitions()
    {
        // The 1/7 sequence defines the primary transitions
        // 1→4→2→8→5→7 (and back to 1)
        auto AddTrans = [this](int32 From, int32 To, float Height) {
            FAttractorTransition T;
            T.FromMode = From;
            T.ToMode = To;
            T.SaddleHeight = Height;
            T.SaddlePoint = (Modes[From-1].Position + Modes[To-1].Position) / 2.0f;
            Transitions.Add(T);
        };

        AddTrans(1, 4, 0.3f); AddTrans(4, 2, 0.2f);
        AddTrans(2, 8, 0.3f); AddTrans(8, 5, 0.2f);
        AddTrans(5, 7, 0.3f); AddTrans(7, 1, 0.2f);

        // Universal transitions (T9 and T3/T6 connect to everything)
        for (int32 i = 1; i <= 9; ++i)
        {
            if (i != 9) AddTrans(9, i, 0.1f);
            if (i != 3) AddTrans(3, i, 0.2f);
            if (i != 6) AddTrans(6, i, 0.2f);
        }
    }

    TArray<FFlourishingMode> Modes;
    TArray<FAttractorTransition> Transitions;
    TArray<int32> ParticularSequence;
    FAttractorFieldState State;
    bool bInitialized = false;
};
