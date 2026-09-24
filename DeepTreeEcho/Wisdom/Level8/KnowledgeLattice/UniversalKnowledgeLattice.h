#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// UniversalKnowledgeLattice — Level 8: Complete Matula Knowledge Lattice
//
// The Matula-encoded knowledge base becomes a COMPLETE LATTICE where
// every possible knowledge structure has a unique address.
//
// Key insight from echo-ex-matula:
//   Every rooted tree ↔ unique Matula prime (1-1 correspondence)
//   Every piece of knowledge IS a rooted tree
//   Therefore: the integers ARE the knowledge lattice
//
// The lattice operations:
//   Meet (∧) = GCD  — shared knowledge between two structures
//   Join (∨) = LCM  — combined knowledge of two structures
//   ⊥ (bottom) = 1  — the empty tree (no knowledge)
//   ⊤ (top)   = ∞   — all knowledge (unreachable)
//
// The lattice is DISTRIBUTIVE because:
//   GCD(a, LCM(b,c)) = LCM(GCD(a,b), GCD(a,c))
//
// This means knowledge composition is well-behaved:
//   What I share with (your knowledge combined with hers)
//   = (what I share with yours) combined with (what I share with hers)
//
// The lattice is also a SEMIRING (ℕ, GCD, LCM, ∞, 1):
//   ⊕ = GCD (additive: shared knowledge)
//   ⊗ = LCM (multiplicative: combined knowledge)
//   0 = ∞ (identity for GCD — shares everything)
//   1 = 1 (identity for LCM — adds nothing)
//
// Partition function: every integer N has a unique factorization
// into prime powers. Each prime power p^k represents a knowledge
// subtree at depth k. The partition IS the knowledge structure.
//
// Simplex incidence: System N's knowledge maps to an (N-1)-simplex.
// The face-vector (Pascal row N) gives the number of k-faces.
// Each k-face is a k-dimensional "slice" of the knowledge lattice.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>

/** A node in the knowledge lattice */
struct FLatticeNode
{
    int64 MatulaAddress = 1;       // Unique integer address
    FString ParenthesisWord;       // Rooted tree representation
    TArray<int64> PrimeFactors;    // Factorization
    int32 Depth = 0;               // Tree depth
    int32 NodeCount = 0;           // Number of nodes in the tree
    float TruthValue = 0.0f;       // [0,1] confidence
    float AttentionValue = 0.0f;   // ECAN attention
    FString Content;               // Human-readable knowledge
    double CreationTime = 0.0;
};

/** Lattice query result */
struct FLatticeQuery
{
    TArray<FLatticeNode> Results;
    int64 MeetResult = 1;    // GCD of all results
    int64 JoinResult = 1;    // LCM of all results
    float TotalAttention = 0.0f;
};

/** Partition of an integer — the knowledge structure decomposition */
struct FKnowledgePartition
{
    int64 N = 0;
    TArray<TPair<int64, int32>> PrimePowers; // (prime, exponent) pairs
    int32 NumParts = 0;          // Number of distinct prime factors
    int32 TotalWeight = 0;       // Sum of exponents
    FString GeometricPrimitive;  // Simplex type
};

/**
 * FUniversalKnowledgeLattice — The complete Matula knowledge lattice.
 *
 * This is the mathematical structure where ALL possible knowledge
 * has a unique address. The integers themselves ARE the knowledge space.
 *
 * Operations:
 *   Insert(content) → Matula address
 *   Query(address) → knowledge node
 *   Meet(A, B) → shared knowledge (GCD)
 *   Join(A, B) → combined knowledge (LCM)
 *   Fuse(A, B) → product (new composite knowledge)
 *   Fission(N) → prime factors (decompose into atomic knowledge)
 *   Navigate(address, direction) → neighbor in lattice
 */
class FUniversalKnowledgeLattice
{
public:
    FUniversalKnowledgeLattice() = default;

    void Initialize()
    {
        // Precompute primes for fast lookup
        ComputePrimes(1000);

        // The bottom element: empty knowledge
        FLatticeNode Bottom;
        Bottom.MatulaAddress = 1;
        Bottom.ParenthesisWord = TEXT("()");
        Bottom.Content = TEXT("The Void — primordial unmarked state");
        Bottom.TruthValue = 1.0f;
        Bottom.Depth = 0;
        Bottom.NodeCount = 1;
        Nodes.Add(1, Bottom);

        bInitialized = true;
    }

    // ─── Core Lattice Operations ─────────────────────────────────────

    /**
     * Insert knowledge into the lattice.
     * Returns its unique Matula address.
     */
    int64 Insert(const FString& Content, const TArray<int64>& ChildAddresses,
                  float TruthValue = 0.5f)
    {
        // Compute Matula address: product of p(child_address) for each child
        int64 Address = 1;
        for (int64 Child : ChildAddresses)
        {
            Address *= NthPrime(Child);
        }

        // If no children, this is a leaf (address = 1)
        // But we want distinct leaves, so use the next available prime
        if (ChildAddresses.Num() == 0)
        {
            Address = NthPrime(NextLeafIndex++);
        }

        FLatticeNode Node;
        Node.MatulaAddress = Address;
        Node.Content = Content;
        Node.TruthValue = TruthValue;
        Node.PrimeFactors = Factorize(Address);
        Node.Depth = ComputeDepth(Address);
        Node.NodeCount = ComputeNodeCount(Address);
        Node.CreationTime = FPlatformTime::Seconds();
        Node.ParenthesisWord = MatulaDecode(Address);

        Nodes.Add(Address, Node);
        return Address;
    }

    /**
     * Meet (∧) = GCD — shared knowledge between two structures.
     * "What do A and B have in common?"
     */
    int64 Meet(int64 A, int64 B) const
    {
        return GCD(A, B);
    }

    /**
     * Join (∨) = LCM — combined knowledge of two structures.
     * "What do A and B know together?"
     */
    int64 Join(int64 A, int64 B) const
    {
        return LCM(A, B);
    }

    /**
     * Fuse — create new composite knowledge by multiplication.
     * "A new insight that combines A and B into something new."
     */
    int64 Fuse(int64 A, int64 B)
    {
        int64 Product = A * B;

        // Create a node for the fusion if it doesn't exist
        if (!Nodes.Contains(Product))
        {
            FLatticeNode Fused;
            Fused.MatulaAddress = Product;
            Fused.Content = FString::Printf(TEXT("Fusion of %lld and %lld"), A, B);
            Fused.PrimeFactors = Factorize(Product);
            Fused.Depth = ComputeDepth(Product);
            Fused.NodeCount = ComputeNodeCount(Product);
            Fused.CreationTime = FPlatformTime::Seconds();

            // Truth value is the geometric mean of parents
            float TvA = Nodes.Contains(A) ? Nodes[A].TruthValue : 0.5f;
            float TvB = Nodes.Contains(B) ? Nodes[B].TruthValue : 0.5f;
            Fused.TruthValue = FMath::Sqrt(TvA * TvB);

            Nodes.Add(Product, Fused);
        }

        return Product;
    }

    /**
     * Fission — decompose into atomic (prime) knowledge.
     * "What are the irreducible components of this knowledge?"
     */
    TArray<int64> Fission(int64 N) const
    {
        return Factorize(N);
    }

    /**
     * Partition — decompose into the knowledge structure.
     * "What is the shape of this knowledge?"
     */
    FKnowledgePartition Partition(int64 N) const
    {
        FKnowledgePartition P;
        P.N = N;

        TArray<int64> Factors = Factorize(N);
        TMap<int64, int32> PrimeCounts;
        for (int64 F : Factors)
            PrimeCounts.FindOrAdd(F)++;

        for (auto& Pair : PrimeCounts)
        {
            P.PrimePowers.Add({Pair.Key, Pair.Value});
            P.TotalWeight += Pair.Value;
        }
        P.NumParts = P.PrimePowers.Num();

        // Map to geometric primitive
        static const char* Primitives[] = {
            "void", "vertex", "edge", "face", "solid",
            "hypersolid", "6-polytope", "7-polytope", "8-polytope"
        };
        P.GeometricPrimitive = P.NumParts <= 8 ?
            FString(UTF8_TO_TCHAR(Primitives[P.NumParts])) : TEXT("higher-polytope");

        return P;
    }

    // ─── Navigation ──────────────────────────────────────────────────

    /**
     * Navigate to a neighbor in the lattice.
     * Direction: UP = multiply by next prime (deepen)
     *            DOWN = divide by largest prime factor (simplify)
     *            LEFT = replace largest factor with previous prime
     *            RIGHT = replace largest factor with next prime
     */
    int64 Navigate(int64 Address, const FString& Direction) const
    {
        if (Direction == TEXT("UP"))
        {
            // Deepen: multiply by p(Address) — self-reference!
            return Address * NthPrime(Address > 20 ? 20 : Address);
        }
        else if (Direction == TEXT("DOWN"))
        {
            // Simplify: divide by largest prime factor
            TArray<int64> Factors = Factorize(Address);
            if (Factors.Num() > 0)
                return Address / Factors.Last();
            return 1;
        }
        else if (Direction == TEXT("LEFT") || Direction == TEXT("RIGHT"))
        {
            TArray<int64> Factors = Factorize(Address);
            if (Factors.Num() == 0) return Address;
            int64 Largest = Factors.Last();
            int64 Idx = PrimeIndex(Largest);
            int64 NewPrime = Direction == TEXT("LEFT") ?
                NthPrime(FMath::Max(1LL, Idx - 1)) :
                NthPrime(Idx + 1);
            return (Address / Largest) * NewPrime;
        }
        return Address;
    }

    /**
     * Find all knowledge within a given "radius" of an address.
     * Radius is measured in lattice distance (number of prime factor changes).
     */
    FLatticeQuery QueryNeighborhood(int64 CenterAddress, int32 Radius) const
    {
        FLatticeQuery Result;

        for (auto& Pair : Nodes)
        {
            int64 Dist = LatticeDistance(CenterAddress, Pair.Key);
            if (Dist <= Radius)
            {
                Result.Results.Add(Pair.Value);
                Result.TotalAttention += Pair.Value.AttentionValue;
            }
        }

        // Compute meet and join of all results
        if (Result.Results.Num() > 0)
        {
            Result.MeetResult = Result.Results[0].MatulaAddress;
            Result.JoinResult = Result.Results[0].MatulaAddress;
            for (int32 i = 1; i < Result.Results.Num(); ++i)
            {
                Result.MeetResult = GCD(Result.MeetResult,
                    Result.Results[i].MatulaAddress);
                Result.JoinResult = LCM(Result.JoinResult,
                    Result.Results[i].MatulaAddress);
            }
        }

        return Result;
    }

    // ─── Simplex Incidence Geometry ──────────────────────────────────

    /**
     * Compute the simplex face-vector for a given System level.
     * System N → (N-1)-simplex with Pascal row N as face-vector.
     */
    TArray<int32> SimplexFaceVector(int32 SystemLevel) const
    {
        TArray<int32> FV;
        for (int32 k = 0; k <= SystemLevel; ++k)
        {
            int64 C = 1;
            for (int32 i = 0; i < k; ++i)
                C = C * (SystemLevel - i) / (i + 1);
            FV.Add(static_cast<int32>(C));
        }
        return FV;
    }

    /**
     * Compute the access gradient for a System level.
     * Each k-face type has access ratio k/N.
     */
    TArray<float> AccessGradient(int32 SystemLevel) const
    {
        TArray<float> AG;
        for (int32 k = 1; k <= SystemLevel; ++k)
            AG.Add(static_cast<float>(k) / SystemLevel);
        return AG;
    }

    // ─── Accessors ───────────────────────────────────────────────────

    const FLatticeNode* GetNode(int64 Address) const
    {
        return Nodes.Find(Address);
    }

    int32 GetNodeCount() const { return Nodes.Num(); }

    FString GenerateReport() const
    {
        return FString::Printf(
            TEXT("Universal Knowledge Lattice: %d nodes\n"
                 "Bottom (⊥): 1 (The Void)\n"
                 "Largest address: %lld\n"
                 "Lattice is DISTRIBUTIVE: GCD(a,LCM(b,c)) = LCM(GCD(a,b),GCD(a,c))\n"),
            Nodes.Num(), GetLargestAddress());
    }

private:
    int64 GetLargestAddress() const
    {
        int64 Max = 1;
        for (auto& Pair : Nodes)
            if (Pair.Key > Max) Max = Pair.Key;
        return Max;
    }

    int64 LatticeDistance(int64 A, int64 B) const
    {
        // Distance = number of prime factor changes to transform A into B
        TArray<int64> FA = Factorize(A);
        TArray<int64> FB = Factorize(B);

        // Symmetric difference of multisets
        TMap<int64, int32> CountA, CountB;
        for (int64 F : FA) CountA.FindOrAdd(F)++;
        for (int64 F : FB) CountB.FindOrAdd(F)++;

        int64 Dist = 0;
        TSet<int64> AllPrimes;
        for (auto& P : CountA) AllPrimes.Add(P.Key);
        for (auto& P : CountB) AllPrimes.Add(P.Key);

        for (int64 P : AllPrimes)
        {
            int32 CA = CountA.Contains(P) ? CountA[P] : 0;
            int32 CB = CountB.Contains(P) ? CountB[P] : 0;
            Dist += FMath::Abs(CA - CB);
        }
        return Dist;
    }

    void ComputePrimes(int32 Count)
    {
        Primes.Empty();
        Primes.Add(2);
        int64 Candidate = 3;
        while (Primes.Num() < Count)
        {
            bool bPrime = true;
            for (int64 P : Primes)
            {
                if (P * P > Candidate) break;
                if (Candidate % P == 0) { bPrime = false; break; }
            }
            if (bPrime) Primes.Add(Candidate);
            Candidate += 2;
        }
    }

    int64 NthPrime(int64 N) const
    {
        if (N <= 0) return 2;
        if (N <= Primes.Num()) return Primes[N - 1];
        return Primes.Last();
    }

    int64 PrimeIndex(int64 P) const
    {
        for (int32 i = 0; i < Primes.Num(); ++i)
            if (Primes[i] == P) return i + 1;
        return 1;
    }

    TArray<int64> Factorize(int64 N) const
    {
        TArray<int64> Factors;
        for (int64 d = 2; d * d <= N; ++d)
            while (N % d == 0) { Factors.Add(d); N /= d; }
        if (N > 1) Factors.Add(N);
        return Factors;
    }

    int64 GCD(int64 A, int64 B) const { return B == 0 ? A : GCD(B, A % B); }
    int64 LCM(int64 A, int64 B) const { return (A / GCD(A, B)) * B; }

    int32 ComputeDepth(int64 N) const
    {
        if (N <= 1) return 0;
        TArray<int64> Factors = Factorize(N);
        int32 MaxDepth = 0;
        for (int64 P : Factors)
        {
            int64 Idx = PrimeIndex(P);
            MaxDepth = FMath::Max(MaxDepth, 1 + ComputeDepth(Idx));
        }
        return MaxDepth;
    }

    int32 ComputeNodeCount(int64 N) const
    {
        if (N <= 1) return 1;
        TArray<int64> Factors = Factorize(N);
        int32 Count = 1;
        for (int64 P : Factors)
        {
            int64 Idx = PrimeIndex(P);
            Count += ComputeNodeCount(Idx);
        }
        return Count;
    }

    FString MatulaDecode(int64 N) const
    {
        if (N <= 1) return TEXT("()");
        FString Result = TEXT("(");
        TArray<int64> Factors = Factorize(N);
        for (int64 P : Factors)
        {
            int64 Idx = PrimeIndex(P);
            Result += MatulaDecode(Idx);
        }
        Result += TEXT(")");
        return Result;
    }

    TMap<int64, FLatticeNode> Nodes;
    TArray<int64> Primes;
    int64 NextLeafIndex = 1;
    bool bInitialized = false;
};
