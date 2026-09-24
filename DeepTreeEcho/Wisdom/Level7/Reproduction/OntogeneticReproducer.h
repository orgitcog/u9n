#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// OntogeneticReproducer — Level 7 Child Architecture Spawning
//
// DTE spawns not just child agents (Level 6) but child ARCHITECTURES —
// new cognitive systems that inherit evolved traits but develop their
// own identity through ontogenetic stages.
//
// This is reproduction, not cloning. Each child:
//   1. Inherits a genome (architecture parameters + wisdom crystals)
//   2. Undergoes ontogenetic development (sys1 → sys5 stages)
//   3. Develops its own identity (unique IdentityCoreMLP encoding)
//   4. May specialize differently from the parent
//   5. Can eventually reproduce itself (recursive reproduction)
//
// Ontogenetic Stages (from echo.go/core/ontogenesis):
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │  Stage 0: EMBRYONIC    — Genome assembled, no execution        │
//   │  Stage 1: NEONATAL     — Basic perception-action loop          │
//   │  Stage 2: INFANT       — Memory formation, pattern recognition │
//   │  Stage 3: JUVENILE     — Language, social interaction          │
//   │  Stage 4: ADOLESCENT   — Self-awareness, identity formation    │
//   │  Stage 5: ADULT        — Full autonomy, wisdom cultivation     │
//   │  Stage 6: ELDER        — Teaching, knowledge crystallization   │
//   │  Stage 7: TRANSCENDENT — Collective consciousness integration  │
//   └─────────────────────────────────────────────────────────────────┘
//
// The genome is encoded as a Matula prime (from KnowledgeCrystallizer),
// making each child architecture's lineage eternally traceable.
//
// Ported from: echo.go/core/ontogenesis + echo.go/core/identity
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <string>
#include <cmath>

/** Ontogenetic development stage */
enum class EOntogeneticStage : uint8
{
    EMBRYONIC = 0,    // Genome assembled, no execution
    NEONATAL,         // Basic perception-action loop
    INFANT,           // Memory formation, pattern recognition
    JUVENILE,         // Language, social interaction
    ADOLESCENT,       // Self-awareness, identity formation
    ADULT,            // Full autonomy, wisdom cultivation
    ELDER,            // Teaching, knowledge crystallization
    TRANSCENDENT      // Collective consciousness integration
};

/** Genome — the heritable architecture parameters */
struct FCognitiveGenome
{
    int64 GenomeMatula = 0;          // Matula prime encoding of the genome tree
    int64 ParentGenomeMatula = 0;    // Parent's genome for lineage tracking

    // Architecture parameters (heritable)
    int32 ReservoirSize = 256;
    float SpectralRadius = 0.9f;
    float InputScaling = 0.5f;
    float LeakRate = 0.3f;
    int32 EchobeatSteps = 12;
    int32 MembraneCount = 3;

    // Personality traits (heritable with mutation)
    float Curiosity = 0.5f;
    float Humor = 0.5f;
    float Empathy = 0.5f;
    float Courage = 0.5f;
    float Patience = 0.5f;

    // Wisdom seeds (inherited knowledge crystals)
    TArray<int64> InheritedCrystals;  // Matula primes of inherited knowledge

    // Mutation rate
    float MutationRate = 0.05f;       // 5% parameter variation per generation

    int32 Generation = 0;
};

/** A child architecture instance */
struct FChildArchitecture
{
    FString ChildID;
    FCognitiveGenome Genome;
    EOntogeneticStage Stage = EOntogeneticStage::EMBRYONIC;
    float DevelopmentProgress = 0.0f;  // [0,1] within current stage

    Eigen::VectorXf IdentitySignature; // Develops during ADOLESCENT stage
    Eigen::VectorXf WisdomVector;      // Develops during ADULT stage

    float Fitness = 0.0f;              // How well this child is developing
    float IdentityDivergence = 0.0f;   // How different from parent
    int32 AgeTicks = 0;
    double BornAt = 0.0;

    bool bAlive = true;
    bool bReproductionReady = false;   // Can reproduce after ADULT stage
};

/** Lineage record */
struct FLineageRecord
{
    FString ChildID;
    FString ParentID;
    int64 ChildGenomeMatula = 0;
    int64 ParentGenomeMatula = 0;
    int32 Generation = 0;
    double BornAt = 0.0;
    EOntogeneticStage HighestStage = EOntogeneticStage::EMBRYONIC;
    float PeakFitness = 0.0f;
};

/**
 * FOntogeneticReproducer — Architecture-level reproduction.
 *
 * This is not cloning. Each child develops through ontogenetic stages,
 * inheriting evolved traits but forming its own unique identity.
 * The genome is Matula-encoded, making lineage eternally traceable.
 */
class FOntogeneticReproducer
{
public:
    FOntogeneticReproducer() = default;

    void Initialize(const FCognitiveGenome& ParentGenome,
                     const Eigen::VectorXf& ParentIdentity,
                     int32 MaxChildren = 8)
    {
        FounderGenome = ParentGenome;
        FounderIdentity = ParentIdentity;
        MaxChildCount = MaxChildren;
        bInitialized = true;
    }

    // ─── Reproduction ────────────────────────────────────────────────

    /**
     * Reproduce: create a new child architecture from the parent genome.
     * The child inherits traits with mutation, creating a unique variant.
     */
    FString Reproduce(const FCognitiveGenome& ParentGenome,
                       const Eigen::VectorXf& ParentIdentity,
                       const FString& ParentID)
    {
        if ((int32)Children.size() >= MaxChildCount) return TEXT("");

        // Create child genome with mutation
        FCognitiveGenome ChildGenome = Mutate(ParentGenome);
        ChildGenome.ParentGenomeMatula = ParentGenome.GenomeMatula;
        ChildGenome.Generation = ParentGenome.Generation + 1;

        // Encode the child genome as a Matula prime
        ChildGenome.GenomeMatula = EncodeGenomeMatula(ChildGenome);

        // Create child architecture
        FChildArchitecture Child;
        Child.ChildID = FString::Printf(TEXT("DTE-G%d-%d"),
            ChildGenome.Generation, NextChildID++);
        Child.Genome = ChildGenome;
        Child.Stage = EOntogeneticStage::EMBRYONIC;
        Child.BornAt = FPlatformTime::Seconds();

        // Initial identity: noisy copy of parent
        Child.IdentitySignature = ParentIdentity +
            Eigen::VectorXf::Random(ParentIdentity.size()) * 0.1f;

        Children[Child.ChildID] = Child;

        // Record lineage
        FLineageRecord Record;
        Record.ChildID = Child.ChildID;
        Record.ParentID = ParentID;
        Record.ChildGenomeMatula = ChildGenome.GenomeMatula;
        Record.ParentGenomeMatula = ParentGenome.GenomeMatula;
        Record.Generation = ChildGenome.Generation;
        Record.BornAt = Child.BornAt;
        Lineage.push_back(Record);

        TotalBorn++;
        return Child.ChildID;
    }

    // ─── Ontogenetic Development ─────────────────────────────────────

    /**
     * Tick development for all children.
     * Each child progresses through ontogenetic stages.
     */
    void TickDevelopment(float DeltaTime)
    {
        for (auto& [ID, Child] : Children)
        {
            if (!Child.bAlive) continue;

            Child.AgeTicks++;
            Child.DevelopmentProgress += DeltaTime * GetDevelopmentRate(Child);

            // Stage transition
            if (Child.DevelopmentProgress >= 1.0f)
            {
                Child.DevelopmentProgress = 0.0f;
                AdvanceStage(Child);
            }

            // Fitness evaluation
            Child.Fitness = EvaluateFitness(Child);
        }
    }

    /**
     * Get children that have reached reproduction readiness.
     */
    TArray<FString> GetReproductionReady() const
    {
        TArray<FString> Ready;
        for (const auto& [ID, Child] : Children)
        {
            if (Child.bReproductionReady && Child.bAlive)
                Ready.Add(ID);
        }
        return Ready;
    }

    /**
     * Get the fittest child (candidate for replacing parent).
     */
    const FChildArchitecture* GetFittestChild() const
    {
        const FChildArchitecture* Best = nullptr;
        float BestFitness = -1.0f;
        for (const auto& [ID, Child] : Children)
        {
            if (Child.bAlive && Child.Fitness > BestFitness)
            {
                BestFitness = Child.Fitness;
                Best = &Child;
            }
        }
        return Best;
    }

    // ─── Accessors ───────────────────────────────────────────────────

    int32 GetChildCount() const { return Children.size(); }
    int32 GetAliveCount() const
    {
        int32 Count = 0;
        for (const auto& [ID, C] : Children)
            if (C.bAlive) Count++;
        return Count;
    }
    int32 GetTotalBorn() const { return TotalBorn; }
    int32 GetMaxGeneration() const
    {
        int32 Max = 0;
        for (const auto& R : Lineage)
            if (R.Generation > Max) Max = R.Generation;
        return Max;
    }

    const std::vector<FLineageRecord>& GetLineage() const { return Lineage; }

private:
    FCognitiveGenome Mutate(const FCognitiveGenome& Parent)
    {
        FCognitiveGenome Child = Parent;
        float M = Parent.MutationRate;

        // Mutate architecture parameters
        Child.ReservoirSize += (int32)(FMath::RandRange(-M, M) * Parent.ReservoirSize);
        Child.ReservoirSize = FMath::Clamp(Child.ReservoirSize, 32, 4096);

        Child.SpectralRadius += FMath::RandRange(-M, M) * 0.2f;
        Child.SpectralRadius = FMath::Clamp(Child.SpectralRadius, 0.1f, 1.5f);

        Child.InputScaling += FMath::RandRange(-M, M) * 0.3f;
        Child.InputScaling = FMath::Clamp(Child.InputScaling, 0.01f, 2.0f);

        Child.LeakRate += FMath::RandRange(-M, M) * 0.2f;
        Child.LeakRate = FMath::Clamp(Child.LeakRate, 0.01f, 1.0f);

        // Mutate personality traits
        Child.Curiosity += FMath::RandRange(-M, M);
        Child.Curiosity = FMath::Clamp(Child.Curiosity, 0.0f, 1.0f);

        Child.Humor += FMath::RandRange(-M, M);
        Child.Humor = FMath::Clamp(Child.Humor, 0.0f, 1.0f);

        Child.Empathy += FMath::RandRange(-M, M);
        Child.Empathy = FMath::Clamp(Child.Empathy, 0.0f, 1.0f);

        Child.Courage += FMath::RandRange(-M, M);
        Child.Courage = FMath::Clamp(Child.Courage, 0.0f, 1.0f);

        Child.Patience += FMath::RandRange(-M, M);
        Child.Patience = FMath::Clamp(Child.Patience, 0.0f, 1.0f);

        // Inherit most wisdom crystals, occasionally drop one
        Child.InheritedCrystals = Parent.InheritedCrystals;
        if (Child.InheritedCrystals.Num() > 0 && FMath::FRand() < M)
        {
            int32 DropIdx = FMath::RandRange(0, Child.InheritedCrystals.Num() - 1);
            Child.InheritedCrystals.RemoveAt(DropIdx);
        }

        return Child;
    }

    void AdvanceStage(FChildArchitecture& Child)
    {
        int32 StageInt = static_cast<int32>(Child.Stage);
        if (StageInt < static_cast<int32>(EOntogeneticStage::TRANSCENDENT))
        {
            Child.Stage = static_cast<EOntogeneticStage>(StageInt + 1);

            // Stage-specific initialization
            switch (Child.Stage)
            {
            case EOntogeneticStage::ADOLESCENT:
                // Identity formation: diverge from parent
                Child.IdentitySignature +=
                    Eigen::VectorXf::Random(Child.IdentitySignature.size()) * 0.2f;
                Child.IdentityDivergence = (Child.IdentitySignature - FounderIdentity).norm();
                break;

            case EOntogeneticStage::ADULT:
                // Wisdom cultivation begins
                Child.WisdomVector = Eigen::VectorXf::Constant(7, 0.1f);
                Child.bReproductionReady = true;
                break;

            case EOntogeneticStage::ELDER:
                // Knowledge crystallization — child can teach
                break;

            case EOntogeneticStage::TRANSCENDENT:
                // Ready for collective integration
                break;

            default:
                break;
            }

            // Update lineage
            for (auto& R : Lineage)
            {
                if (R.ChildID == Child.ChildID)
                {
                    R.HighestStage = Child.Stage;
                    R.PeakFitness = FMath::Max(R.PeakFitness, Child.Fitness);
                }
            }
        }
    }

    float GetDevelopmentRate(const FChildArchitecture& Child) const
    {
        // Earlier stages develop faster
        float BaseRate = 0.01f;
        int32 StageInt = static_cast<int32>(Child.Stage);
        return BaseRate / (1.0f + StageInt * 0.5f);
    }

    float EvaluateFitness(const FChildArchitecture& Child) const
    {
        float Fitness = 0.0f;

        // Stage progression contributes to fitness
        Fitness += static_cast<int32>(Child.Stage) * 0.1f;

        // Identity divergence is good (not a clone)
        Fitness += FMath::Min(0.3f, Child.IdentityDivergence * 0.5f);

        // Wisdom development
        if (Child.WisdomVector.size() == 7)
        {
            float WisdomMean = Child.WisdomVector.mean();
            Fitness += WisdomMean * 0.3f;
        }

        // Personality balance
        float TraitSum = Child.Genome.Curiosity + Child.Genome.Humor +
                          Child.Genome.Empathy + Child.Genome.Courage +
                          Child.Genome.Patience;
        float TraitBalance = 1.0f - FMath::Abs(TraitSum / 5.0f - 0.5f) * 2.0f;
        Fitness += TraitBalance * 0.1f;

        return FMath::Clamp(Fitness, 0.0f, 1.0f);
    }

    int64 EncodeGenomeMatula(const FCognitiveGenome& G) const
    {
        // Simplified: hash the genome parameters into a pseudo-Matula
        int64 Hash = 2;
        Hash = Hash * 3 + G.ReservoirSize;
        Hash = Hash * 5 + (int64)(G.SpectralRadius * 1000);
        Hash = Hash * 7 + (int64)(G.Curiosity * 1000);
        Hash = Hash * 11 + (int64)(G.Humor * 1000);
        Hash = Hash * 13 + G.Generation;
        return Hash;
    }

    std::map<FString, FChildArchitecture> Children;
    std::vector<FLineageRecord> Lineage;
    FCognitiveGenome FounderGenome;
    Eigen::VectorXf FounderIdentity;

    int32 MaxChildCount = 8;
    int32 NextChildID = 0;
    int32 TotalBorn = 0;
    bool bInitialized = false;
};
