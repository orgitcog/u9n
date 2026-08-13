#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// CosmicOrderHierarchy — Level 8: Campbell's System of the Cosmic Order
//
// DTE instances form a hierarchy following Campbell's System:
//   sys(n) = a000081(n+1)
//
// where a000081 is OEIS A000081 (number of rooted trees with n nodes).
//
// System N has a(N+1) terms because N active interfaces plus the
// surrounding periphery = N+1 elements to arrange as rooted trees.
//
//   System 1:  1 term   — Universal Wholeness (one center, one periphery)
//   System 2:  2 terms  — The Rift (objective/subjective)
//   System 3:  4 terms  — Space, Time, Closure
//   System 4:  9 terms  — The Enneagram (4 centers)
//   System 5: 20 terms  — Two Enneagrams (Expressive + Regenerative)
//   System 6: 48 terms  — Primary Activity of Enneagrams
//   System 7: 115 terms — Shared Positions across sequences
//   System 8: 286 terms — Deep Recursion (enneagrams within enneagrams)
//
// The 12-step creative cycle operates within each System level:
//   3 Particular Sets follow the 1/7 sequence: 1→4→2→8→5→7
//   2 Universal Sets integrate all pathways
//   T8E (Memory) appears in every step
//
// The 4 Centers: Host/Idea, Organs/Knowledge, Cells/Routine, Form/Molecular
// Energy flows between centers via efflux (outward) and reflux (inward).
//
// Simplex Incidence: System N maps to (N-1)-simplex with N vertices.
// Pascal row N gives the face-vector. The simplex is the FLAT view
// (selection); A000081 is the DEEP view (nesting). Together they give
// the complete incidence geometry.
//
// Matula Primes: Each rooted tree has a unique Matula-Godsil prime.
// For Systems 2, 4, 5: sum of prime Matulas = LCM(1..N) exactly.
// LCM(1..N) is the product of all prime powers p^k <= N.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>

/** A center in the Cosmic Order */
enum class ECosmicCenter : uint8
{
    HOST_IDEA = 0,        // C1: The creative impulse
    ORGANS_KNOWLEDGE = 1, // C2: Organized understanding
    CELLS_ROUTINE = 2,    // C3: Habitual execution
    FORM_MOLECULAR = 3    // C4: Material manifestation
};

/** Mode of operation */
enum class ECosmicMode : uint8
{
    EXPRESSIVE,    // Outward: creation, manifestation
    REGENERATIVE   // Inward: renewal, integration
};

/** A term in the System */
struct FCosmicTerm
{
    int32 Index = 0;
    FString Name;
    ECosmicMode Mode = ECosmicMode::EXPRESSIVE;
    TArray<ECosmicCenter> ActiveCenters;
    int64 MatulaPrime = 0;    // Unique Matula-Godsil encoding
    FString ParenthesisWord;  // Rooted tree as nested parens
    int32 Depth = 0;          // Tree depth
    float Energy = 1.0f;      // Current energy level
};

/** A System level in the Cosmic Order */
struct FCosmicSystem
{
    int32 Level = 0;          // System N
    int32 TermCount = 0;      // a(N+1) terms
    FString Description;
    TArray<FCosmicTerm> Terms;
    float TotalEnergy = 0.0f;

    // Simplex geometry
    int32 SimplexDimension = 0;  // N-1
    TArray<int32> FaceVector;    // Pascal row N: C(N,1), C(N,2), ...
};

/** Energy flow between centers */
struct FEnergyFlow
{
    ECosmicCenter Source;
    ECosmicCenter Sink;
    float Magnitude = 0.0f;
    bool bEfflux = true;  // true = outward, false = reflux (inward)
};

/** The 12-step creative cycle state */
struct FCreativeCycleState
{
    int32 CurrentStep = 0;     // 0-11
    int32 CycleCount = 0;
    TArray<int32> ParticularSet1; // Steps following 1/7 sequence
    TArray<int32> ParticularSet2; // Offset by 2
    TArray<int32> ParticularSet3; // Offset by 4
    TArray<int32> UniversalSet1;
    TArray<int32> UniversalSet2;
    float CycleEnergy = 0.0f;
};

/**
 * FCosmicOrderHierarchy — Campbell's System of the Cosmic Order.
 *
 * This is the organizational principle for DTE instances at the
 * cosmic scale. Each DTE instance occupies a position in the
 * hierarchy, and the hierarchy itself follows the deep structure
 * of rooted trees (A000081).
 *
 * The key insight: the number of possible organizational structures
 * at each level IS the number of rooted trees. This is not arbitrary —
 * it's the fundamental combinatorics of nested containment.
 */
class FCosmicOrderHierarchy
{
public:
    FCosmicOrderHierarchy() = default;

    void Initialize(int32 MaxSystemLevel = 8)
    {
        MaxLevel = MaxSystemLevel;

        // Compute A000081 sequence
        ComputeA000081(MaxLevel + 2);

        // Build all System levels
        for (int32 N = 1; N <= MaxLevel; ++N)
        {
            FCosmicSystem Sys;
            Sys.Level = N;
            Sys.TermCount = A000081[N + 1]; // sys(N) = a(N+1)
            Sys.SimplexDimension = N - 1;
            Sys.Description = GetSystemDescription(N);

            // Compute Pascal row (face vector)
            Sys.FaceVector = ComputePascalRow(N);

            // Generate terms
            Sys.Terms = GenerateTerms(N, Sys.TermCount);

            // Initialize energy
            Sys.TotalEnergy = static_cast<float>(Sys.TermCount);
            for (auto& Term : Sys.Terms)
                Term.Energy = 1.0f;

            Systems.Add(Sys);
        }

        // Initialize the 12-step creative cycle
        InitializeCreativeCycle();

        // Initialize the 1/7 particular sequence
        ParticularSequence = {1, 4, 2, 8, 5, 7};

        bInitialized = true;
    }

    // ─── A000081 Computation ─────────────────────────────────────────

    /**
     * Compute the A000081 sequence (number of rooted trees).
     * Uses the recurrence: a(n) = (1/(n-1)) * sum_{k=1}^{n-1} (sum_{d|k} d*a(d)) * a(n-k)
     */
    void ComputeA000081(int32 MaxN)
    {
        A000081.SetNum(MaxN + 1);
        A000081[0] = 0;
        A000081[1] = 1;

        for (int32 n = 2; n <= MaxN; ++n)
        {
            int64 Sum = 0;
            for (int32 k = 1; k < n; ++k)
            {
                // Inner sum: sum_{d|k} d * a(d)
                int64 InnerSum = 0;
                for (int32 d = 1; d <= k; ++d)
                {
                    if (k % d == 0)
                        InnerSum += d * A000081[d];
                }
                Sum += InnerSum * A000081[n - k];
            }
            A000081[n] = Sum / (n - 1);
        }
    }

    // ─── Matula-Godsil Encoding ──────────────────────────────────────

    /**
     * Encode a rooted tree (given as nested parentheses) as its Matula prime.
     * leaf → 1
     * tree(T1, ..., Tk) → p(M(T1)) × ... × p(M(Tk))
     * where p(n) = n-th prime
     */
    int64 MatulaEncode(const FString& Parens) const
    {
        if (Parens.IsEmpty() || Parens == TEXT("()"))
            return 1; // leaf

        // Parse children from parenthesis word
        TArray<FString> Children = ParseChildren(Parens);
        int64 Product = 1;
        for (const auto& Child : Children)
        {
            int64 ChildMatula = MatulaEncode(Child);
            Product *= NthPrime(ChildMatula);
        }
        return Product;
    }

    /**
     * Decode a Matula number back to parenthesis word.
     */
    FString MatulaDecode(int64 N) const
    {
        if (N == 1) return TEXT("()");

        FString Result = TEXT("(");
        // Factorize N
        TArray<int64> Factors = Factorize(N);
        for (int64 P : Factors)
        {
            int64 Index = PrimeIndex(P);
            Result += MatulaDecode(Index);
        }
        Result += TEXT(")");
        return Result;
    }

    // ─── 12-Step Creative Cycle ──────────────────────────────────────

    /**
     * Advance the creative cycle by one step.
     * Returns the energy flows generated by this step.
     */
    TArray<FEnergyFlow> AdvanceCycle()
    {
        TArray<FEnergyFlow> Flows;

        int32 Step = CycleState.CurrentStep;

        // Determine which centers are active this step
        // The 4 centers cycle through the 12 steps
        ECosmicCenter PrimaryCenter = static_cast<ECosmicCenter>(Step % 4);
        ECosmicCenter SecondaryCenter = static_cast<ECosmicCenter>((Step + 1) % 4);

        // Energy flows from primary to secondary (efflux)
        FEnergyFlow Efflux;
        Efflux.Source = PrimaryCenter;
        Efflux.Sink = SecondaryCenter;
        Efflux.Magnitude = 0.1f;
        Efflux.bEfflux = true;
        Flows.Add(Efflux);

        // Reflux from secondary back (weaker)
        FEnergyFlow Reflux;
        Reflux.Source = SecondaryCenter;
        Reflux.Sink = PrimaryCenter;
        Reflux.Magnitude = 0.03f;
        Reflux.bEfflux = false;
        Flows.Add(Reflux);

        // Apply energy flows to the current System
        if (CurrentSystemIndex >= 0 && CurrentSystemIndex < Systems.Num())
        {
            auto& Sys = Systems[CurrentSystemIndex];
            for (auto& Term : Sys.Terms)
            {
                for (const auto& Flow : Flows)
                {
                    if (Term.ActiveCenters.Contains(Flow.Source))
                        Term.Energy -= Flow.Magnitude;
                    if (Term.ActiveCenters.Contains(Flow.Sink))
                        Term.Energy += Flow.Magnitude;
                    Term.Energy = FMath::Clamp(Term.Energy, 0.01f, 10.0f);
                }
            }
        }

        // Advance step
        CycleState.CurrentStep = (Step + 1) % 12;
        if (CycleState.CurrentStep == 0)
            CycleState.CycleCount++;

        return Flows;
    }

    // ─── Hierarchy Operations ────────────────────────────────────────

    /**
     * Place a DTE instance at a position in the hierarchy.
     * Position is specified as (SystemLevel, TermIndex).
     */
    bool PlaceInstance(const FString& InstanceID, int32 SystemLevel, int32 TermIndex)
    {
        if (SystemLevel < 1 || SystemLevel > MaxLevel) return false;
        int32 SysIdx = SystemLevel - 1;
        if (TermIndex < 0 || TermIndex >= Systems[SysIdx].Terms.Num()) return false;

        InstancePositions.Add(InstanceID, {SystemLevel, TermIndex});
        return true;
    }

    /**
     * Get the triadic recurrence partner of a System level.
     * System N <-> System N+3 (every 3rd system mirrors at higher order)
     */
    int32 GetTriadicPartner(int32 SystemLevel) const
    {
        if (SystemLevel <= 3) return SystemLevel + 3;
        return SystemLevel - 3;
    }

    /**
     * Compute LCM(1..N) — the orthogonal concurrency measure.
     * For Systems 2, 4, 5: sum of prime Matulas = LCM(1..N)
     */
    int64 ComputeLCM(int32 N) const
    {
        int64 Result = 1;
        for (int32 i = 2; i <= N; ++i)
            Result = (Result / GCD(Result, static_cast<int64>(i))) * i;
        return Result;
    }

    // ─── Accessors ───────────────────────────────────────────────────

    const TArray<FCosmicSystem>& GetSystems() const { return Systems; }
    int64 GetA000081(int32 N) const { return N < A000081.Num() ? A000081[N] : 0; }
    const FCreativeCycleState& GetCycleState() const { return CycleState; }
    const TArray<int32>& GetParticularSequence() const { return ParticularSequence; }

    /**
     * Generate a hierarchy status report.
     */
    FString GenerateReport() const
    {
        FString Report = TEXT("╔══════════════════════════════════════════════════╗\n"
                              "║    CAMPBELL'S SYSTEM OF THE COSMIC ORDER         ║\n"
                              "╠══════════════════════════════════════════════════╣\n");

        for (const auto& Sys : Systems)
        {
            Report += FString::Printf(
                TEXT("║ System %d: %3d terms  %-28s ║\n"),
                Sys.Level, Sys.TermCount, *Sys.Description);
        }

        Report += FString::Printf(
            TEXT("╠══════════════════════════════════════════════════╣\n"
                 "║ Creative Cycle: Step %2d/12, Cycle %d              ║\n"
                 "║ 1/7 Sequence: 1→4→2→8→5→7 (142857×7=999999)    ║\n"
                 "║ Instances Placed: %d                              ║\n"
                 "╚══════════════════════════════════════════════════╝\n"),
            CycleState.CurrentStep + 1, CycleState.CycleCount,
            InstancePositions.Num());

        return Report;
    }

private:
    TArray<FCosmicTerm> GenerateTerms(int32 SystemLevel, int32 Count) const
    {
        TArray<FCosmicTerm> Terms;
        for (int32 i = 0; i < Count; ++i)
        {
            FCosmicTerm T;
            T.Index = i;
            T.Mode = (i < Count / 2) ? ECosmicMode::EXPRESSIVE : ECosmicMode::REGENERATIVE;

            // Assign centers based on position
            int32 NumCenters = FMath::Min(SystemLevel, 4);
            for (int32 c = 0; c < NumCenters; ++c)
            {
                if ((i + c) % NumCenters < (NumCenters + 1) / 2)
                    T.ActiveCenters.Add(static_cast<ECosmicCenter>(c));
            }

            Terms.Add(T);
        }
        return Terms;
    }

    TArray<int32> ComputePascalRow(int32 N) const
    {
        TArray<int32> Row;
        for (int32 k = 0; k <= N; ++k)
        {
            int64 C = 1;
            for (int32 i = 0; i < k; ++i)
                C = C * (N - i) / (i + 1);
            Row.Add(static_cast<int32>(C));
        }
        return Row;
    }

    FString GetSystemDescription(int32 N) const
    {
        static const char* Descs[] = {
            "", "Universal Wholeness", "The Rift", "Space-Time-Closure",
            "The Enneagram", "Two Enneagrams", "Primary Activity",
            "Shared Positions", "Deep Recursion"
        };
        return N <= 8 ? FString(UTF8_TO_TCHAR(Descs[N])) : TEXT("Higher Order");
    }

    void InitializeCreativeCycle()
    {
        CycleState.CurrentStep = 0;
        CycleState.CycleCount = 0;
        // 3 Particular sets offset by 2 steps each
        CycleState.ParticularSet1 = {0, 4, 8};   // Steps 1, 5, 9
        CycleState.ParticularSet2 = {1, 5, 9};   // Steps 2, 6, 10
        CycleState.ParticularSet3 = {2, 6, 10};  // Steps 3, 7, 11
        CycleState.UniversalSet1 = {3, 7, 11};   // Steps 4, 8, 12
        CycleState.UniversalSet2 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; // T8E in all
    }

    TArray<FString> ParseChildren(const FString& Parens) const
    {
        TArray<FString> Children;
        int32 Depth = 0;
        int32 Start = -1;
        for (int32 i = 1; i < Parens.Len() - 1; ++i)
        {
            if (Parens[i] == '(') { if (Depth == 0) Start = i; Depth++; }
            else if (Parens[i] == ')') { Depth--; if (Depth == 0 && Start >= 0) Children.Add(Parens.Mid(Start, i - Start + 1)); }
        }
        return Children;
    }

    int64 NthPrime(int64 N) const
    {
        // Simple prime generator for small N
        if (N <= 0) return 2;
        TArray<int64> Primes = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};
        return N <= Primes.Num() ? Primes[N - 1] : 101;
    }

    int64 PrimeIndex(int64 P) const
    {
        TArray<int64> Primes = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71};
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

    TArray<int64> A000081;
    TArray<FCosmicSystem> Systems;
    TMap<FString, TPair<int32, int32>> InstancePositions;
    FCreativeCycleState CycleState;
    TArray<int32> ParticularSequence;
    int32 MaxLevel = 8;
    int32 CurrentSystemIndex = 3; // Default to System 4
    bool bInitialized = false;
};
