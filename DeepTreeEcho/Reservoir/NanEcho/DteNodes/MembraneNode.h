#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// MembraneNode — P-System Membrane Hierarchy Manager
// Ported from echoself/NanEcho/dte_nodes/membrane_node.py
// Implements the nested containment: Root → Security → Extension → Cognitive
// cogpy layer: L2 coglux (lightweight cognitive runtime)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

/** Membrane types in the DTE hierarchy */
enum class EMembraneType : uint8
{
    ROOT       = 0,  // System boundary
    SECURITY   = 1,  // Authentication, validation, emergency
    EXTENSION  = 2,  // Browser, ML, introspection
    COGNITIVE  = 3,  // Memory, reasoning, grammar
    MEMORY     = 4,  // Sub-membrane: declarative, procedural, episodic, intentional
    REASONING  = 5,  // Sub-membrane: symbolic reasoning, neural-symbolic
    GRAMMAR    = 6   // Sub-membrane: cognitive grammar kernel
};

/** State of a membrane boundary */
enum class EMembraneState : uint8
{
    OPEN       = 0,  // Allows free passage
    SELECTIVE  = 1,  // Filters based on rules
    CLOSED     = 2,  // Blocks all passage
    EMERGENCY  = 3   // Emergency lockdown
};

/**
 * A single membrane in the P-system hierarchy.
 */
struct FMembrane
{
    /** Unique identifier */
    FString Id;

    /** Type of membrane */
    EMembraneType Type = EMembraneType::ROOT;

    /** Current state */
    EMembraneState State = EMembraneState::SELECTIVE;

    /** Parent membrane ID (empty for root) */
    FString ParentId;

    /** Child membrane IDs */
    TArray<FString> ChildIds;

    /** Internal state vector (contents of this membrane region) */
    Eigen::VectorXf InternalState;

    /** Permeability: how easily signals cross this boundary [0,1] */
    float Permeability = 0.5f;

    /** Energy cost of crossing this membrane */
    float CrossingCost = 0.1f;

    /** Number of objects currently inside this membrane */
    int32 ObjectCount = 0;
};

/**
 * Signal crossing a membrane boundary.
 */
struct FMembraneSignal
{
    /** Source membrane ID */
    FString SourceId;

    /** Target membrane ID */
    FString TargetId;

    /** Signal payload */
    Eigen::VectorXf Payload;

    /** Signal priority (higher = more likely to cross) */
    float Priority = 0.5f;

    /** Signal type tag */
    FString TypeTag;
};

/**
 * MembraneNode — P-System membrane hierarchy for Deep Tree Echo.
 *
 * Implements the nested containment architecture:
 *   Root (System Boundary)
 *   ├── Security Membrane (auth, validation, emergency)
 *   ├── Extension Membrane (browser, ML, introspection)
 *   └── Cognitive Membrane
 *       ├── Memory (declarative, procedural, episodic, intentional)
 *       ├── Reasoning (symbolic, neural-symbolic)
 *       └── Grammar (cognitive grammar kernel)
 *
 * Signals must cross membrane boundaries to propagate between regions.
 * Each boundary has permeability and crossing cost, implementing the
 * "nested shells" constraint from OEIS A000081:
 *   N=1→1, N=2→2, N=3→4, N=4→9 execution contexts.
 *
 * The 5 cognitive extension sections from ion-cognitive-shell map to:
 *   .fptable  → Cognitive dispatch IVT (32 slots for 12 echobeats + 20 aux)
 *   CPADinfo  → Integrity seal ROM (protection posture)
 *   LZMADEC   → Memory decompression DMA (hypergraph inflation)
 *   malloc_h  → Hypergraph memory allocator (AtomSpace node/link patterns)
 *   prot      → Membrane security boundary (this node's enforcement)
 */
class FMembraneNode
{
public:
    FMembraneNode() = default;

    /** Initialize the membrane hierarchy */
    void Initialize(int32 StateDim = 128)
    {
        Dim = StateDim;

        // Create the standard DTE membrane hierarchy
        CreateMembrane(TEXT("root"), EMembraneType::ROOT, TEXT(""), 1.0f);
        CreateMembrane(TEXT("security"), EMembraneType::SECURITY, TEXT("root"), 0.3f);
        CreateMembrane(TEXT("extension"), EMembraneType::EXTENSION, TEXT("root"), 0.6f);
        CreateMembrane(TEXT("cognitive"), EMembraneType::COGNITIVE, TEXT("root"), 0.5f);
        CreateMembrane(TEXT("memory"), EMembraneType::MEMORY, TEXT("cognitive"), 0.7f);
        CreateMembrane(TEXT("reasoning"), EMembraneType::REASONING, TEXT("cognitive"), 0.5f);
        CreateMembrane(TEXT("grammar"), EMembraneType::GRAMMAR, TEXT("cognitive"), 0.4f);

        bInitialized = true;
    }

    /**
     * Attempt to send a signal across membrane boundaries.
     * The signal must cross each boundary between source and target.
     *
     * @param Signal - The signal to transmit
     * @return true if the signal successfully reached the target
     */
    bool TransmitSignal(const FMembraneSignal& Signal)
    {
        check(bInitialized);

        if (!Membranes.Contains(Signal.SourceId) || !Membranes.Contains(Signal.TargetId))
            return false;

        // Find path between source and target
        TArray<FString> Path = FindPath(Signal.SourceId, Signal.TargetId);
        if (Path.Num() == 0) return false;

        // Attempt to cross each boundary
        float RemainingPriority = Signal.Priority;
        for (const FString& MemId : Path)
        {
            const FMembrane& Mem = Membranes[MemId];

            // Check membrane state
            if (Mem.State == EMembraneState::CLOSED || Mem.State == EMembraneState::EMERGENCY)
                return false;

            // Apply permeability filter
            if (Mem.State == EMembraneState::SELECTIVE)
            {
                RemainingPriority -= Mem.CrossingCost;
                if (RemainingPriority < 0.0f)
                    return false;

                // Stochastic crossing based on permeability
                if (FMath::FRand() > Mem.Permeability)
                    return false;
            }
        }

        // Signal reached target — inject payload
        FMembrane& Target = Membranes[Signal.TargetId];
        if (Target.InternalState.size() == Signal.Payload.size())
        {
            Target.InternalState += Signal.Payload * RemainingPriority;
        }
        Target.ObjectCount++;

        return true;
    }

    /** Get a membrane by ID */
    const FMembrane* GetMembrane(const FString& Id) const
    {
        return Membranes.Find(Id);
    }

    /** Set membrane state (e.g., for emergency lockdown) */
    void SetMembraneState(const FString& Id, EMembraneState NewState)
    {
        if (FMembrane* Mem = Membranes.Find(Id))
        {
            Mem->State = NewState;
        }
    }

    /** Emergency lockdown: close all membranes */
    void EmergencyLockdown()
    {
        for (auto& Pair : Membranes)
        {
            Pair.Value.State = EMembraneState::EMERGENCY;
        }
    }

    /** Get total object count across all membranes */
    int32 GetTotalObjectCount() const
    {
        int32 Total = 0;
        for (const auto& Pair : Membranes)
            Total += Pair.Value.ObjectCount;
        return Total;
    }

    bool IsInitialized() const { return bInitialized; }

private:
    void CreateMembrane(const FString& Id, EMembraneType Type,
                        const FString& ParentId, float Permeability)
    {
        FMembrane Mem;
        Mem.Id = Id;
        Mem.Type = Type;
        Mem.State = EMembraneState::SELECTIVE;
        Mem.ParentId = ParentId;
        Mem.InternalState = Eigen::VectorXf::Zero(Dim);
        Mem.Permeability = Permeability;
        Mem.CrossingCost = 0.1f * (1.0f - Permeability);

        if (!ParentId.IsEmpty() && Membranes.Contains(ParentId))
        {
            Membranes[ParentId].ChildIds.Add(Id);
        }

        Membranes.Add(Id, Mem);
    }

    TArray<FString> FindPath(const FString& From, const FString& To) const
    {
        // Simple BFS path finding through membrane hierarchy
        TArray<FString> Path;
        TSet<FString> Visited;
        TArray<TPair<FString, TArray<FString>>> Queue;

        Queue.Add(TPair<FString, TArray<FString>>(From, {From}));
        Visited.Add(From);

        while (Queue.Num() > 0)
        {
            auto Current = Queue[0];
            Queue.RemoveAt(0);

            if (Current.Key == To)
            {
                Path = Current.Value;
                return Path;
            }

            const FMembrane* Mem = Membranes.Find(Current.Key);
            if (!Mem) continue;

            // Explore parent
            if (!Mem->ParentId.IsEmpty() && !Visited.Contains(Mem->ParentId))
            {
                Visited.Add(Mem->ParentId);
                TArray<FString> NewPath = Current.Value;
                NewPath.Add(Mem->ParentId);
                Queue.Add(TPair<FString, TArray<FString>>(Mem->ParentId, NewPath));
            }

            // Explore children
            for (const FString& ChildId : Mem->ChildIds)
            {
                if (!Visited.Contains(ChildId))
                {
                    Visited.Add(ChildId);
                    TArray<FString> NewPath = Current.Value;
                    NewPath.Add(ChildId);
                    Queue.Add(TPair<FString, TArray<FString>>(ChildId, NewPath));
                }
            }
        }

        return Path; // Empty if no path found
    }

    TMap<FString, FMembrane> Membranes;
    int32 Dim = 128;
    bool bInitialized = false;
};
