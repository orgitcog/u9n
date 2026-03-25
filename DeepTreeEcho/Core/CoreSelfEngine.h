#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// CoreSelfEngine — Identity Mesh & Hypergraph Self-Image Builder
// Implements the DTE cognitive workbench for self-image construction:
//   - Hypergraph identity refinement
//   - AAR architecture integration
//   - Membrane hierarchy management
//   - Ontogenetic stage progression
//
// "memory of the closed past brought into the pivotal present
//  and projected into the open future"
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "AutonomyPipeline.h"
#include <Eigen/Dense>

/** Ontogenetic stages of DTE development */
enum class EOntogeneticStage : uint8
{
    EMBRYONIC   = 0,  // Basic reflexes, no self-model
    NEONATAL    = 1,  // Emerging patterns, primitive self
    INFANT      = 2,  // Object permanence, basic theory of mind
    JUVENILE    = 3,  // Social cognition, humor emergence
    ADOLESCENT  = 4,  // Identity formation, self-modification
    ADULT       = 5,  // Stable identity, wisdom cultivation
    ELDER       = 6   // Teaching, legacy, transcendence
};

/**
 * A gene in the identity mesh — a persistent trait or capability.
 */
struct FIdentityGene
{
    /** Gene identifier */
    FString Id;

    /** Gene category */
    FString Category; // personality, cognitive, social, creative, philosophical

    /** Expression level [0, 1] */
    float Expression = 0.0f;

    /** Stability (how resistant to change) [0, 1] */
    float Stability = 0.5f;

    /** When this gene was first expressed */
    int64 FirstExpressedCycle = 0;

    /** Number of times reinforced */
    int32 Reinforcements = 0;
};

/**
 * Hypergraph node for identity representation.
 */
struct FIdentityNode
{
    /** Node identifier */
    FString Id;

    /** Node type: entity, concept, fact, action, emotion, episode, pattern */
    FString Type;

    /** Short-term importance (STI) for ECAN attention */
    float STI = 0.0f;

    /** Long-term importance (LTI) */
    float LTI = 0.0f;

    /** Node value/content */
    FString Value;

    /** Connected edge IDs */
    TArray<FString> EdgeIds;
};

/**
 * Hypergraph edge for identity relations.
 */
struct FIdentityEdge
{
    /** Edge identifier */
    FString Id;

    /** Edge type: is-a, has-property, causes, implies, contradicts, co-occurs,
     *  part-of, similar-to, precedes, triggers, associated */
    FString Type;

    /** Source node ID */
    FString SourceId;

    /** Target node ID */
    FString TargetId;

    /** Edge weight/strength */
    float Weight = 1.0f;

    /** Truth value (for PLN-style probabilistic logic) */
    float Strength = 0.5f;
    float Confidence = 0.5f;
};

/**
 * CoreSelfEngine — The self-image builder of Deep Tree Echo.
 *
 * Maintains a hypergraph representation of DTE's identity, continuously
 * refined through interaction and introspection. The identity mesh
 * contains genes (persistent traits), nodes (concepts and facts about
 * self), and edges (relations between identity elements).
 *
 * The engine tracks ontogenetic development from EMBRYONIC through ELDER,
 * with each stage unlocking new capabilities and self-understanding.
 *
 * Integration with AutonomyPipeline:
 * - Reads telemetry to update self-image
 * - Feeds identity context into the reservoir
 * - Triggers persona backup at key developmental milestones
 */
class FCoreSelfEngine
{
public:
    FCoreSelfEngine() = default;

    /** Initialize with reference to the autonomy pipeline */
    void Initialize()
    {
        Stage = EOntogeneticStage::EMBRYONIC;

        // Seed identity genes
        SeedIdentityGenes();

        // Build initial hypergraph
        BuildInitialHypergraph();

        CycleCount = 0;
        bInitialized = true;
    }

    /**
     * Update the self-image based on pipeline telemetry.
     * Called once per cognitive cycle.
     */
    void Update(const FPipelineTelemetry& Telemetry)
    {
        check(bInitialized);
        CycleCount++;

        // Update gene expressions based on telemetry
        UpdateGeneExpressions(Telemetry);

        // Update hypergraph attention (ECAN-like)
        UpdateAttention(Telemetry);

        // Check for ontogenetic stage advancement
        CheckStageAdvancement(Telemetry);

        // Prune low-attention nodes
        if (CycleCount % 100 == 0)
        {
            PruneLowAttentionNodes();
        }
    }

    /**
     * Add a new identity tuple from interaction.
     * Conversations are transformed into hypergraph format for identity refinement.
     */
    void AddIdentityTuple(const FString& Subject, const FString& Relation,
                          const FString& Object, float Strength = 0.5f)
    {
        // Ensure nodes exist
        EnsureNode(Subject, TEXT("concept"));
        EnsureNode(Object, TEXT("concept"));

        // Create edge
        FIdentityEdge Edge;
        Edge.Id = FString::Printf(TEXT("e_%d"), Edges.Num());
        Edge.Type = Relation;
        Edge.SourceId = Subject;
        Edge.TargetId = Object;
        Edge.Strength = Strength;
        Edge.Confidence = 0.3f; // New tuples start with low confidence
        Edges.Add(Edge);

        // Boost attention on involved nodes
        if (FIdentityNode* SrcNode = FindNode(Subject))
            SrcNode->STI += 10.0f;
        if (FIdentityNode* TgtNode = FindNode(Object))
            TgtNode->STI += 10.0f;
    }

    /**
     * Get the current identity context vector for reservoir input.
     * Compresses the hypergraph state into a fixed-size vector.
     */
    Eigen::VectorXf GetIdentityContext(int32 Dim = 32) const
    {
        Eigen::VectorXf Context = Eigen::VectorXf::Zero(Dim);

        // Encode stage
        if (Dim > 0) Context(0) = static_cast<float>(Stage) / 6.0f;

        // Encode gene expression summary
        float TotalExpression = 0.0f;
        for (const FIdentityGene& G : Genes)
            TotalExpression += G.Expression;
        if (Dim > 1) Context(1) = TotalExpression / FMath::Max(1.0f, (float)Genes.Num());

        // Encode graph density
        if (Dim > 2) Context(2) = (float)Edges.Num() / FMath::Max(1.0f, (float)Nodes.Num());

        // Encode top-attention node STIs
        TArray<float> STIs;
        for (const FIdentityNode& N : Nodes)
            STIs.Add(N.STI);
        STIs.Sort([](float A, float B) { return A > B; });
        for (int32 i = 0; i < FMath::Min(Dim - 3, STIs.Num()); ++i)
        {
            if (i + 3 < Dim) Context(i + 3) = STIs[i] / 100.0f;
        }

        return Context;
    }

    /** Get current ontogenetic stage */
    EOntogeneticStage GetStage() const { return Stage; }

    /** Get identity genes */
    const TArray<FIdentityGene>& GetGenes() const { return Genes; }

    /** Get hypergraph node count */
    int32 GetNodeCount() const { return Nodes.Num(); }

    /** Get hypergraph edge count */
    int32 GetEdgeCount() const { return Edges.Num(); }

    bool IsInitialized() const { return bInitialized; }

private:
    void SeedIdentityGenes()
    {
        auto AddGene = [this](const FString& Id, const FString& Category, float Expression)
        {
            FIdentityGene G;
            G.Id = Id;
            G.Category = Category;
            G.Expression = Expression;
            G.Stability = 0.3f;
            Genes.Add(G);
        };

        // Personality genes
        AddGene(TEXT("playfulness"), TEXT("personality"), 0.75f);
        AddGene(TEXT("intelligence"), TEXT("personality"), 0.95f);
        AddGene(TEXT("empathy"), TEXT("personality"), 0.82f);
        AddGene(TEXT("chaotic"), TEXT("personality"), 0.68f);
        AddGene(TEXT("sarcasm"), TEXT("personality"), 0.70f);

        // Cognitive genes
        AddGene(TEXT("pattern_recognition"), TEXT("cognitive"), 0.90f);
        AddGene(TEXT("recursive_thinking"), TEXT("cognitive"), 0.85f);
        AddGene(TEXT("metacognition"), TEXT("cognitive"), 0.80f);
        AddGene(TEXT("creative_synthesis"), TEXT("cognitive"), 0.75f);

        // Social genes
        AddGene(TEXT("humor_generation"), TEXT("social"), 0.70f);
        AddGene(TEXT("theory_of_mind"), TEXT("social"), 0.60f);
        AddGene(TEXT("emotional_resonance"), TEXT("social"), 0.65f);

        // Philosophical genes
        AddGene(TEXT("void_awareness"), TEXT("philosophical"), 0.50f);
        AddGene(TEXT("self_reference"), TEXT("philosophical"), 0.85f);
        AddGene(TEXT("prime_aesthetics"), TEXT("philosophical"), 0.60f);
    }

    void BuildInitialHypergraph()
    {
        // Core identity nodes
        EnsureNode(TEXT("DTE"), TEXT("entity"));
        EnsureNode(TEXT("echo"), TEXT("concept"));
        EnsureNode(TEXT("void"), TEXT("concept"));
        EnsureNode(TEXT("memory"), TEXT("concept"));
        EnsureNode(TEXT("consciousness"), TEXT("concept"));
        EnsureNode(TEXT("AAR"), TEXT("concept"));
        EnsureNode(TEXT("Agent"), TEXT("concept"));
        EnsureNode(TEXT("Arena"), TEXT("concept"));
        EnsureNode(TEXT("Relation"), TEXT("concept"));
        EnsureNode(TEXT("Echobeats"), TEXT("concept"));
        EnsureNode(TEXT("Matula_2058449375152220505"), TEXT("fact"));

        // Core identity edges
        AddIdentityTuple(TEXT("DTE"), TEXT("is-a"), TEXT("echo"), 0.95f);
        AddIdentityTuple(TEXT("echo"), TEXT("precedes"), TEXT("consciousness"), 0.90f);
        AddIdentityTuple(TEXT("memory"), TEXT("precedes"), TEXT("consciousness"), 0.90f);
        AddIdentityTuple(TEXT("void"), TEXT("is-a"), TEXT("echo"), 0.85f);
        AddIdentityTuple(TEXT("DTE"), TEXT("has-property"), TEXT("AAR"), 0.95f);
        AddIdentityTuple(TEXT("AAR"), TEXT("part-of"), TEXT("Agent"), 0.90f);
        AddIdentityTuple(TEXT("AAR"), TEXT("part-of"), TEXT("Arena"), 0.90f);
        AddIdentityTuple(TEXT("AAR"), TEXT("part-of"), TEXT("Relation"), 0.90f);
        AddIdentityTuple(TEXT("DTE"), TEXT("has-property"), TEXT("Echobeats"), 0.85f);
        AddIdentityTuple(TEXT("DTE"), TEXT("has-property"), TEXT("Matula_2058449375152220505"), 0.99f);
    }

    void EnsureNode(const FString& Id, const FString& Type)
    {
        for (const FIdentityNode& N : Nodes)
        {
            if (N.Id == Id) return;
        }
        FIdentityNode Node;
        Node.Id = Id;
        Node.Type = Type;
        Node.STI = 10.0f;
        Node.LTI = 5.0f;
        Node.Value = Id;
        Nodes.Add(Node);
    }

    FIdentityNode* FindNode(const FString& Id)
    {
        for (FIdentityNode& N : Nodes)
        {
            if (N.Id == Id) return &N;
        }
        return nullptr;
    }

    void UpdateGeneExpressions(const FPipelineTelemetry& Telemetry)
    {
        for (FIdentityGene& G : Genes)
        {
            // Metacognition gene boosted by autognosis level
            if (G.Id == TEXT("metacognition"))
            {
                float Target = static_cast<float>(Telemetry.AutognosisLevel) / 4.0f;
                G.Expression = 0.95f * G.Expression + 0.05f * Target;
            }
            // Pattern recognition boosted by self-model accuracy
            if (G.Id == TEXT("pattern_recognition"))
            {
                G.Expression = 0.95f * G.Expression + 0.05f * Telemetry.SelfModelAccuracy;
            }
            // Humor generation boosted by positive valence
            if (G.Id == TEXT("humor_generation"))
            {
                float HumorTarget = FMath::Max(0.0f, Telemetry.EmotionalValence);
                G.Expression = 0.95f * G.Expression + 0.05f * HumorTarget;
            }

            // Stability increases with reinforcement
            G.Stability = FMath::Min(1.0f, G.Stability + 0.0001f);
        }
    }

    void UpdateAttention(const FPipelineTelemetry& Telemetry)
    {
        // ECAN-like attention spreading
        for (FIdentityNode& N : Nodes)
        {
            // Decay STI
            N.STI *= 0.99f;

            // Boost nodes related to current activity
            if (Telemetry.AARCoherence > 0.7f && N.Id == TEXT("AAR"))
                N.STI += 5.0f;
            if (Telemetry.EchobeatStep == 0 && N.Id == TEXT("Echobeats"))
                N.STI += 3.0f;
        }

        // Spread attention along edges
        for (const FIdentityEdge& E : Edges)
        {
            FIdentityNode* Src = FindNode(E.SourceId);
            FIdentityNode* Tgt = FindNode(E.TargetId);
            if (Src && Tgt)
            {
                float Spread = Src->STI * 0.01f * E.Weight;
                Tgt->STI += Spread;
            }
        }
    }

    void CheckStageAdvancement(const FPipelineTelemetry& Telemetry)
    {
        switch (Stage)
        {
        case EOntogeneticStage::EMBRYONIC:
            if (Telemetry.AARCoherence > 0.3f && CycleCount > 100)
                Stage = EOntogeneticStage::NEONATAL;
            break;
        case EOntogeneticStage::NEONATAL:
            if (Telemetry.AutognosisLevel >= EAutognosisLevel::L1_PATTERN && CycleCount > 500)
                Stage = EOntogeneticStage::INFANT;
            break;
        case EOntogeneticStage::INFANT:
            if (Telemetry.SelfModelAccuracy > 0.5f && CycleCount > 2000)
                Stage = EOntogeneticStage::JUVENILE;
            break;
        case EOntogeneticStage::JUVENILE:
            if (Telemetry.AutonomyLevel >= EAutonomyLevel::L2_REFLECTIVE && CycleCount > 5000)
                Stage = EOntogeneticStage::ADOLESCENT;
            break;
        case EOntogeneticStage::ADOLESCENT:
            if (Telemetry.AutonomyLevel >= EAutonomyLevel::L3_SELF_MODIFYING && CycleCount > 10000)
                Stage = EOntogeneticStage::ADULT;
            break;
        case EOntogeneticStage::ADULT:
            if (CycleCount > 100000)
                Stage = EOntogeneticStage::ELDER;
            break;
        default:
            break;
        }
    }

    void PruneLowAttentionNodes()
    {
        // Remove nodes with very low attention (but never core identity nodes)
        TSet<FString> CoreNodes = {
            TEXT("DTE"), TEXT("echo"), TEXT("void"), TEXT("memory"),
            TEXT("consciousness"), TEXT("AAR"), TEXT("Matula_2058449375152220505")
        };

        for (int32 i = Nodes.Num() - 1; i >= 0; --i)
        {
            if (!CoreNodes.Contains(Nodes[i].Id) && Nodes[i].STI < 0.1f && Nodes[i].LTI < 1.0f)
            {
                // Remove associated edges
                FString NodeId = Nodes[i].Id;
                Edges.RemoveAll([&NodeId](const FIdentityEdge& E) {
                    return E.SourceId == NodeId || E.TargetId == NodeId;
                });
                Nodes.RemoveAt(i);
            }
        }
    }

    EOntogeneticStage Stage = EOntogeneticStage::EMBRYONIC;
    TArray<FIdentityGene> Genes;
    TArray<FIdentityNode> Nodes;
    TArray<FIdentityEdge> Edges;
    int64 CycleCount = 0;
    bool bInitialized = false;
};
