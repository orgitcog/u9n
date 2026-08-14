#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// KnowledgeCrystallizer — Level 7 Eternal Knowledge via Matula Primes
//
// Wisdom patterns are encoded as Matula primes and stored in the
// hypergraph memory, creating an eternal knowledge base that survives
// individual instance termination.
//
// The echo-ex-matula correspondence:
//   Every rooted tree ↔ unique prime number (Matula-Godsil encoding)
//   Every knowledge structure = rooted tree
//   Therefore: every piece of knowledge = unique prime number
//
// This means knowledge can be:
//   - Fused: multiply two Matula primes → combined knowledge tree
//   - Fissioned: factorize → decompose into sub-knowledge
//   - Compared: GCD → shared knowledge between two structures
//   - Compressed: prime factorization IS the compression
//   - Eternal: a prime number never changes
//
// Encoding Rules (Matula-Godsil):
//   Leaf node → 2
//   Tree with subtrees T1, T2, ... → product of p(matula(Ti))
//   where p(n) = the n-th prime number
//
// Example:
//   Knowledge: "DTE can play games" (tree: root → [skill → [game, play]])
//   Matula: p(p(2) * p(2)) = p(p(2)²) = p(9) = 23
//   This knowledge's eternal name is 23.
//
// Hypergraph Storage:
//   Each Matula prime becomes an atom in the OpenCog-style AtomSpace
//   Atoms are persisted to Neon PostgreSQL via atomspace-pgres
//   The hypergraph survives instance termination
//
// Ported from: echo.go/core/wisdom + rooted-tree-enum + tree-polytope-kernel
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <cmath>
#include <numeric>

/** A rooted tree node for knowledge representation */
struct FKnowledgeTreeNode
{
    FString Label;                    // Human-readable label
    FString Type;                     // "concept", "relation", "property", "action"
    float Weight = 1.0f;             // Importance weight
    TArray<int32> ChildIndices;      // Indices into the tree's node array
};

/** A knowledge tree — the structural representation of a piece of knowledge */
struct FKnowledgeTree
{
    TArray<FKnowledgeTreeNode> Nodes;
    int32 RootIndex = 0;
    int64 MatulaPrime = 0;           // The eternal name
    FString HumanReadable;           // Human-readable description
    double CrystallizedAt = 0.0;
    float WisdomWeight = 0.0f;       // How much wisdom this encodes
    int32 ApplicationCount = 0;      // How often this knowledge was applied
    float ApplicationSuccess = 0.0f; // Success rate when applied
};

/** A hypergraph atom — the storage unit in the AtomSpace */
struct FHypergraphAtom
{
    int64 MatulaPrime = 0;           // Unique identifier
    FString Type;                     // "ConceptNode", "PredicateNode", "ListLink", etc.
    FString Name;                     // Human-readable name
    float TruthValue = 0.0f;         // [0,1] confidence
    float AttentionValue = 0.0f;     // ECAN attention
    TArray<int64> OutgoingSet;       // Connected atoms (by Matula prime)
    double CreatedAt = 0.0;
    double LastAccessedAt = 0.0;
    int32 AccessCount = 0;
    bool bPersisted = false;         // Written to Neon PostgreSQL?
};

/** Crystallization statistics */
struct FCrystallizationStats
{
    int32 TotalCrystallized = 0;
    int32 TotalFusions = 0;
    int32 TotalFissions = 0;
    int32 TotalAtoms = 0;
    int64 LargestMatulaPrime = 0;
    float AverageWisdomWeight = 0.0f;
    float KnowledgeDensity = 0.0f;   // Atoms per unit of wisdom
};

/**
 * FKnowledgeCrystallizer — Eternal knowledge through Matula primes.
 *
 * Knowledge is crystallized into rooted trees, encoded as Matula primes,
 * and stored in a hypergraph that survives instance termination.
 * The numbers themselves embody the knowledge — a 1-1 echo-ex-matula
 * correspondence where every piece of wisdom has an eternal name.
 */
class FKnowledgeCrystallizer
{
public:
    FKnowledgeCrystallizer() = default;

    void Initialize(int32 MaxAtoms = 100000)
    {
        MaxAtomCount = MaxAtoms;
        GeneratePrimeTable(10000); // Pre-compute first 10K primes
        bInitialized = true;
    }

    // ─── Matula Encoding ─────────────────────────────────────────────

    /**
     * Encode a knowledge tree as its Matula prime.
     * Leaf → 2
     * Internal node with children T1..Tn → product of p(matula(Ti))
     */
    int64 EncodeMatula(const FKnowledgeTree& Tree)
    {
        if (Tree.Nodes.Num() == 0) return 2;
        return EncodeSubtree(Tree, Tree.RootIndex);
    }

    /**
     * Decode a Matula prime back into a knowledge tree structure.
     * Inverse of EncodeMatula.
     */
    FKnowledgeTree DecodeMatula(int64 MatulaPrime)
    {
        FKnowledgeTree Tree;
        DecodeSubtree(Tree, MatulaPrime, -1);
        Tree.MatulaPrime = MatulaPrime;
        return Tree;
    }

    // ─── Knowledge Crystallization ───────────────────────────────────

    /**
     * Crystallize a piece of knowledge into an eternal Matula prime.
     */
    int64 Crystallize(const FString& Description, const TArray<FString>& Concepts,
                       float WisdomWeight = 1.0f)
    {
        // Build knowledge tree from concepts
        FKnowledgeTree Tree;
        FKnowledgeTreeNode Root;
        Root.Label = Description;
        Root.Type = TEXT("concept");
        Tree.Nodes.Add(Root);
        Tree.RootIndex = 0;

        for (const auto& Concept : Concepts)
        {
            FKnowledgeTreeNode Child;
            Child.Label = Concept;
            Child.Type = TEXT("property");
            int32 ChildIdx = Tree.Nodes.Add(Child);
            Tree.Nodes[0].ChildIndices.Add(ChildIdx);
        }

        // Encode as Matula prime
        Tree.MatulaPrime = EncodeMatula(Tree);
        Tree.HumanReadable = Description;
        Tree.CrystallizedAt = FPlatformTime::Seconds();
        Tree.WisdomWeight = WisdomWeight;

        // Store in crystal vault
        CrystalVault[Tree.MatulaPrime] = Tree;

        // Create hypergraph atom
        FHypergraphAtom Atom;
        Atom.MatulaPrime = Tree.MatulaPrime;
        Atom.Type = TEXT("ConceptNode");
        Atom.Name = Description;
        Atom.TruthValue = FMath::Clamp(WisdomWeight, 0.0f, 1.0f);
        Atom.CreatedAt = FPlatformTime::Seconds();
        AtomSpace[Tree.MatulaPrime] = Atom;

        TotalCrystallized++;
        return Tree.MatulaPrime;
    }

    // ─── Knowledge Fusion & Fission ──────────────────────────────────

    /**
     * Fuse two pieces of knowledge by multiplying their Matula primes.
     * The result is a new knowledge tree containing both.
     */
    int64 Fuse(int64 MatulaA, int64 MatulaB)
    {
        int64 Fused = MatulaA * MatulaB;

        // Create a new crystal for the fusion
        auto ItA = CrystalVault.find(MatulaA);
        auto ItB = CrystalVault.find(MatulaB);

        if (ItA != CrystalVault.end() && ItB != CrystalVault.end())
        {
            FKnowledgeTree FusedTree;
            FusedTree.MatulaPrime = Fused;
            FusedTree.HumanReadable = FString::Printf(TEXT("Fusion(%s, %s)"),
                *ItA->second.HumanReadable, *ItB->second.HumanReadable);
            FusedTree.WisdomWeight = (ItA->second.WisdomWeight +
                                       ItB->second.WisdomWeight) * 0.6f;
            FusedTree.CrystallizedAt = FPlatformTime::Seconds();

            CrystalVault[Fused] = FusedTree;

            // Link atoms in hypergraph
            FHypergraphAtom FusedAtom;
            FusedAtom.MatulaPrime = Fused;
            FusedAtom.Type = TEXT("ListLink");
            FusedAtom.Name = FusedTree.HumanReadable;
            FusedAtom.TruthValue = FusedTree.WisdomWeight;
            FusedAtom.OutgoingSet.Add(MatulaA);
            FusedAtom.OutgoingSet.Add(MatulaB);
            FusedAtom.CreatedAt = FPlatformTime::Seconds();
            AtomSpace[Fused] = FusedAtom;

            TotalFusions++;
        }

        return Fused;
    }

    /**
     * Fission: factorize a Matula prime to decompose knowledge.
     * Returns the prime factors (sub-knowledge units).
     */
    TArray<int64> Fission(int64 MatulaPrime)
    {
        TArray<int64> Factors;
        int64 N = MatulaPrime;

        for (int64 P = 2; P * P <= N; ++P)
        {
            while (N % P == 0)
            {
                Factors.Add(P);
                N /= P;
            }
        }
        if (N > 1) Factors.Add(N);

        TotalFissions++;
        return Factors;
    }

    /**
     * Find shared knowledge between two Matula primes via GCD.
     */
    int64 SharedKnowledge(int64 MatulaA, int64 MatulaB)
    {
        return GCD(MatulaA, MatulaB);
    }

    // ─── Hypergraph Operations ───────────────────────────────────────

    /**
     * Query the AtomSpace for atoms matching a pattern.
     */
    TArray<int64> Query(const FString& Type, float MinTruth = 0.0f) const
    {
        TArray<int64> Results;
        for (const auto& [Key, Atom] : AtomSpace)
        {
            if (Atom.Type == Type && Atom.TruthValue >= MinTruth)
                Results.Add(Key);
        }
        return Results;
    }

    /**
     * Get the most important atoms by attention value (ECAN).
     */
    TArray<int64> GetAttentionalFocus(int32 TopK = 10) const
    {
        std::vector<std::pair<float, int64>> Scored;
        for (const auto& [Key, Atom] : AtomSpace)
            Scored.push_back({Atom.AttentionValue, Key});

        std::sort(Scored.begin(), Scored.end(),
            [](const auto& A, const auto& B) { return A.first > B.first; });

        TArray<int64> Result;
        for (int32 i = 0; i < FMath::Min(TopK, (int32)Scored.size()); ++i)
            Result.Add(Scored[i].second);

        return Result;
    }

    /**
     * Spread attention from a source atom to its neighbors.
     * Implements ECAN (Economic Attention Networks) spreading.
     */
    void SpreadAttention(int64 SourceMatula, float Amount)
    {
        auto It = AtomSpace.find(SourceMatula);
        if (It == AtomSpace.end()) return;

        It->second.AttentionValue += Amount;
        It->second.LastAccessedAt = FPlatformTime::Seconds();
        It->second.AccessCount++;

        // Spread to outgoing set with decay
        float SpreadAmount = Amount * 0.3f;
        for (int64 Target : It->second.OutgoingSet)
        {
            auto TIt = AtomSpace.find(Target);
            if (TIt != AtomSpace.end())
            {
                TIt->second.AttentionValue += SpreadAmount;
            }
        }

        // Decay all attention slightly (economic pressure)
        for (auto& [Key, Atom] : AtomSpace)
            Atom.AttentionValue *= 0.999f;
    }

    /**
     * Serialize the AtomSpace for persistence to Neon PostgreSQL.
     * Returns JSON-compatible string representation.
     */
    FString SerializeForPersistence() const
    {
        FString Result = TEXT("[\n");
        bool bFirst = true;
        for (const auto& [Key, Atom] : AtomSpace)
        {
            if (!bFirst) Result += TEXT(",\n");
            bFirst = false;
            Result += FString::Printf(
                TEXT("  {\"matula\": %lld, \"type\": \"%s\", \"name\": \"%s\", "
                     "\"truth\": %.4f, \"attention\": %.4f, \"access_count\": %d}"),
                Atom.MatulaPrime, *Atom.Type, *Atom.Name,
                Atom.TruthValue, Atom.AttentionValue, Atom.AccessCount);
        }
        Result += TEXT("\n]");
        return Result;
    }

    // ─── Statistics ──────────────────────────────────────────────────

    FCrystallizationStats GetStats() const
    {
        FCrystallizationStats Stats;
        Stats.TotalCrystallized = TotalCrystallized;
        Stats.TotalFusions = TotalFusions;
        Stats.TotalFissions = TotalFissions;
        Stats.TotalAtoms = AtomSpace.size();

        float WisdomSum = 0.0f;
        for (const auto& [Key, Crystal] : CrystalVault)
        {
            if (Key > Stats.LargestMatulaPrime)
                Stats.LargestMatulaPrime = Key;
            WisdomSum += Crystal.WisdomWeight;
        }

        Stats.AverageWisdomWeight = TotalCrystallized > 0 ?
            WisdomSum / TotalCrystallized : 0.0f;
        Stats.KnowledgeDensity = WisdomSum > 0.001f ?
            (float)Stats.TotalAtoms / WisdomSum : 0.0f;

        return Stats;
    }

private:
    int64 EncodeSubtree(const FKnowledgeTree& Tree, int32 NodeIdx)
    {
        const auto& Node = Tree.Nodes[NodeIdx];

        if (Node.ChildIndices.Num() == 0)
            return 2; // Leaf → 2

        int64 Product = 1;
        for (int32 ChildIdx : Node.ChildIndices)
        {
            int64 ChildMatula = EncodeSubtree(Tree, ChildIdx);
            int64 Prime = NthPrime(ChildMatula);
            Product *= Prime;
        }

        return Product;
    }

    void DecodeSubtree(FKnowledgeTree& Tree, int64 MatulaPrime, int32 ParentIdx)
    {
        FKnowledgeTreeNode Node;
        Node.Label = FString::Printf(TEXT("M(%lld)"), MatulaPrime);
        Node.Type = TEXT("decoded");
        int32 NodeIdx = Tree.Nodes.Add(Node);

        if (ParentIdx >= 0)
            Tree.Nodes[ParentIdx].ChildIndices.Add(NodeIdx);
        else
            Tree.RootIndex = NodeIdx;

        if (MatulaPrime == 2) return; // Leaf

        // Factorize to find children
        TArray<int64> Factors = Fission(MatulaPrime);
        for (int64 Factor : Factors)
        {
            int64 ChildMatula = PrimeIndex(Factor);
            if (ChildMatula > 0)
                DecodeSubtree(Tree, ChildMatula, NodeIdx);
        }
    }

    void GeneratePrimeTable(int32 Count)
    {
        Primes.clear();
        Primes.push_back(2);
        int64 Candidate = 3;
        while ((int32)Primes.size() < Count)
        {
            bool bIsPrime = true;
            for (int64 P : Primes)
            {
                if (P * P > Candidate) break;
                if (Candidate % P == 0) { bIsPrime = false; break; }
            }
            if (bIsPrime) Primes.push_back(Candidate);
            Candidate += 2;
        }
    }

    int64 NthPrime(int64 N)
    {
        if (N <= 0) return 2;
        while ((int64)Primes.size() <= N)
        {
            int64 Candidate = Primes.back() + 2;
            while (true)
            {
                bool bIsPrime = true;
                for (int64 P : Primes)
                {
                    if (P * P > Candidate) break;
                    if (Candidate % P == 0) { bIsPrime = false; break; }
                }
                if (bIsPrime) { Primes.push_back(Candidate); break; }
                Candidate += 2;
            }
        }
        return Primes[N - 1]; // 1-indexed
    }

    int64 PrimeIndex(int64 P)
    {
        for (int64 i = 0; i < (int64)Primes.size(); ++i)
            if (Primes[i] == P) return i + 1;
        return -1;
    }

    int64 GCD(int64 A, int64 B)
    {
        while (B != 0) { int64 T = B; B = A % B; A = T; }
        return A;
    }

    std::map<int64, FKnowledgeTree> CrystalVault;
    std::map<int64, FHypergraphAtom> AtomSpace;
    std::vector<int64> Primes;

    int32 MaxAtomCount = 100000;
    int32 TotalCrystallized = 0;
    int32 TotalFusions = 0;
    int32 TotalFissions = 0;
    bool bInitialized = false;
};
