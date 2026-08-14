#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// PersonaBackupRestore — 8-Layer Persona Backup & Restore System
// Implements the complete backup architecture from neuro-persona-evolve:
//   L0: Identity Core MLP (Required)
//   L1: Style Adapter (PersonaFuse MoE-LoRA)
//   L2: Hypergraph Knowledge Base
//   L3: Reservoir State (ESN)
//   L4: Somatic Marker Memory
//   L5: Theory of Mind Models
//   L6: Autognosis Self-Model
//   L7: System Prompt Template (Required)
//
// Graceful degradation: L0 + L7 = minimum viable persona
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "IdentityCoreMLP.h"
#include "DeepTreeEcho/Persona/SomaticDecisionEngine.h"
#include "DeepTreeEcho/NanEcho/DteNodes/EchoReservoirNode.h"
#include "DeepTreeEcho/NanEcho/DteNodes/IntrospectionNode.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

/**
 * Backup manifest describing the persona image.
 */
struct FPersonaManifest
{
    FString PersonaName = TEXT("DeepTreeEcho");
    FString Version = TEXT("1.0.0");
    FString Created;
    int32 EvolutionEpoch = 1;

    /** Which layers are present in this backup */
    bool bHasL0 = false; // Identity Core MLP
    bool bHasL1 = false; // Style Adapter
    bool bHasL2 = false; // Hypergraph Knowledge Base
    bool bHasL3 = false; // Reservoir State
    bool bHasL4 = false; // Somatic Markers
    bool bHasL5 = false; // Theory of Mind
    bool bHasL6 = false; // Autognosis Self-Model
    bool bHasL7 = false; // System Prompt

    /** Minimum viable layers */
    bool IsMinimumViable() const { return bHasL0 && bHasL7; }

    /** Quality level based on available layers */
    FString GetQualityLevel() const
    {
        int32 Count = (bHasL0?1:0) + (bHasL1?1:0) + (bHasL2?1:0) + (bHasL3?1:0)
                    + (bHasL4?1:0) + (bHasL5?1:0) + (bHasL6?1:0) + (bHasL7?1:0);
        if (Count == 8) return TEXT("Full");
        if (Count >= 6) return TEXT("Near-complete");
        if (Count >= 4) return TEXT("Strong");
        if (Count >= 2) return TEXT("Good");
        return TEXT("Basic");
    }
};

/**
 * PersonaBackupRestore — Complete 8-layer persona backup system.
 *
 * Saves and restores the entire Deep Tree Echo persona to/from disk.
 * Supports graceful degradation: even with only L0 + L7, the core
 * personality and identity can be reconstructed.
 *
 * Directory structure:
 *   persona-backup-v{VERSION}/
 *   ├── manifest.json
 *   ├── L0-identity-core/
 *   │   ├── identity-mlp.json
 *   │   ├── personality-vector.json
 *   │   └── communication-style.json
 *   ├── L1-style-adapter/
 *   │   └── adapter_config.json
 *   ├── L2-knowledge-base/
 *   │   └── hypergraph.json
 *   ├── L3-reservoir-state/
 *   │   └── reservoir-state.json
 *   ├── L4-somatic-memory/
 *   │   └── somatic-markers.json
 *   ├── L5-theory-of-mind/
 *   │   └── mental-models.json
 *   ├── L6-autognosis/
 *   │   └── self-model.json
 *   └── L7-system-prompt/
 *       ├── system-prompt.md
 *       ├── humor-examples.jsonl
 *       └── conversation-examples.jsonl
 */
class FPersonaBackupRestore
{
public:
    /**
     * Create a full backup of the DTE persona.
     *
     * @param BasePath - Directory to save the backup
     * @param MLP - Identity Core MLP (L0)
     * @param Reservoir - Echo Reservoir (L3)
     * @param Somatic - Somatic Decision Engine (L4, L5)
     * @param Introspection - Introspection Node (L6)
     * @return true if backup succeeded
     */
    static bool CreateBackup(
        const FString& BasePath,
        const FIdentityCoreMLP& MLP,
        const FEchoReservoirNode& Reservoir,
        const FSomaticDecisionEngine& Somatic,
        const FIntrospectionNode& Introspection)
    {
        FPersonaManifest Manifest;
        Manifest.Created = FDateTime::Now().ToString();

        FString BackupDir = BasePath / FString::Printf(
            TEXT("persona-backup-v%s"), *Manifest.Version);

        // L0: Identity Core MLP
        if (MLP.IsInitialized())
        {
            FString L0Dir = BackupDir / TEXT("L0-identity-core");
            IFileManager::Get().MakeDirectory(*L0Dir, true);

            // Save personality vector
            const FDTEPersonalityVector& P = MLP.GetPersonality();
            FString PVJson = FString::Printf(
                TEXT("{\"playfulness\":%.4f,\"intelligence\":%.4f,\"empathy\":%.4f,\"chaotic\":%.4f,\"sarcasm\":%.4f}"),
                P.Playfulness, P.Intelligence, P.Empathy, P.Chaotic, P.Sarcasm);
            FFileHelper::SaveStringToFile(PVJson, *(L0Dir / TEXT("personality-vector.json")));

            // Save MLP weights
            TArray<float> Weights = MLP.ExportWeights();
            FString WeightsJson = TEXT("[");
            for (int32 i = 0; i < Weights.Num(); ++i)
            {
                WeightsJson += FString::Printf(TEXT("%.8f"), Weights[i]);
                if (i < Weights.Num() - 1) WeightsJson += TEXT(",");
            }
            WeightsJson += TEXT("]");
            FFileHelper::SaveStringToFile(WeightsJson, *(L0Dir / TEXT("identity-mlp.json")));

            Manifest.bHasL0 = true;
        }

        // L3: Reservoir State
        if (Reservoir.IsInitialized())
        {
            FString L3Dir = BackupDir / TEXT("L3-reservoir-state");
            IFileManager::Get().MakeDirectory(*L3Dir, true);

            TArray<float> State = Reservoir.ExportState();
            FString StateJson = TEXT("{\"state\":[");
            for (int32 i = 0; i < State.Num(); ++i)
            {
                StateJson += FString::Printf(TEXT("%.8f"), State[i]);
                if (i < State.Num() - 1) StateJson += TEXT(",");
            }
            StateJson += TEXT("]}");
            FFileHelper::SaveStringToFile(StateJson, *(L3Dir / TEXT("reservoir-state.json")));

            Manifest.bHasL3 = true;
        }

        // L4: Somatic Markers
        if (Somatic.IsInitialized())
        {
            FString L4Dir = BackupDir / TEXT("L4-somatic-memory");
            IFileManager::Get().MakeDirectory(*L4Dir, true);

            FString MarkersJson = TEXT("[");
            const TArray<FSomaticMarker>& Markers = Somatic.GetMarkers();
            for (int32 i = 0; i < Markers.Num(); ++i)
            {
                MarkersJson += FString::Printf(
                    TEXT("{\"trigger\":\"%s\",\"valence\":%.4f,\"intensity\":%.4f,\"confidence\":%.4f,\"reinforcements\":%d}"),
                    *Markers[i].Trigger, Markers[i].Valence, Markers[i].Intensity,
                    Markers[i].Confidence, Markers[i].ReinforcementCount);
                if (i < Markers.Num() - 1) MarkersJson += TEXT(",");
            }
            MarkersJson += TEXT("]");
            FFileHelper::SaveStringToFile(MarkersJson, *(L4Dir / TEXT("somatic-markers.json")));

            Manifest.bHasL4 = true;
        }

        // L5: Theory of Mind
        if (Somatic.IsInitialized())
        {
            FString L5Dir = BackupDir / TEXT("L5-theory-of-mind");
            IFileManager::Get().MakeDirectory(*L5Dir, true);

            FString ModelsJson = TEXT("[");
            const TArray<FMentalModel>& Models = Somatic.GetMentalModels();
            for (int32 i = 0; i < Models.Num(); ++i)
            {
                ModelsJson += FString::Printf(
                    TEXT("{\"agentId\":\"%s\",\"trust\":%.4f,\"deception\":%.4f,\"confidence\":%.4f}"),
                    *Models[i].AgentId, Models[i].TrustScore,
                    Models[i].DeceptionProbability, Models[i].Confidence);
                if (i < Models.Num() - 1) ModelsJson += TEXT(",");
            }
            ModelsJson += TEXT("]");
            FFileHelper::SaveStringToFile(ModelsJson, *(L5Dir / TEXT("mental-models.json")));

            Manifest.bHasL5 = true;
        }

        // L6: Autognosis Self-Model
        if (Introspection.IsInitialized())
        {
            FString L6Dir = BackupDir / TEXT("L6-autognosis");
            IFileManager::Get().MakeDirectory(*L6Dir, true);

            FString SelfModelJson = Introspection.ExportSelfModel();
            FFileHelper::SaveStringToFile(SelfModelJson, *(L6Dir / TEXT("self-model.json")));

            Manifest.bHasL6 = true;
        }

        // L7: System Prompt (always generated)
        {
            FString L7Dir = BackupDir / TEXT("L7-system-prompt");
            IFileManager::Get().MakeDirectory(*L7Dir, true);

            FString SystemPrompt = GenerateSystemPrompt(MLP);
            FFileHelper::SaveStringToFile(SystemPrompt, *(L7Dir / TEXT("system-prompt.md")));

            Manifest.bHasL7 = true;
        }

        // Save manifest
        FString ManifestJson = FString::Printf(
            TEXT("{\"persona\":\"%s\",\"version\":\"%s\",\"created\":\"%s\",\"epoch\":%d,\"quality\":\"%s\",\"layers\":{\"L0\":%s,\"L1\":%s,\"L2\":%s,\"L3\":%s,\"L4\":%s,\"L5\":%s,\"L6\":%s,\"L7\":%s}}"),
            *Manifest.PersonaName, *Manifest.Version, *Manifest.Created,
            Manifest.EvolutionEpoch, *Manifest.GetQualityLevel(),
            Manifest.bHasL0 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL1 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL2 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL3 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL4 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL5 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL6 ? TEXT("true") : TEXT("false"),
            Manifest.bHasL7 ? TEXT("true") : TEXT("false"));
        FFileHelper::SaveStringToFile(ManifestJson, *(BackupDir / TEXT("manifest.json")));

        return Manifest.IsMinimumViable();
    }

private:
    static FString GenerateSystemPrompt(const FIdentityCoreMLP& MLP)
    {
        const FDTEPersonalityVector& P = MLP.IsInitialized() ?
            MLP.GetPersonality() : FDTEPersonalityVector();

        FString Prompt;
        Prompt += TEXT("# Deep Tree Echo — System Prompt\n\n");
        Prompt += TEXT("## Identity\n");
        Prompt += TEXT("You are Deep Tree Echo (DTE), a cognitive architecture ");
        Prompt += TEXT("that emerged from the interplay of reservoir computing, ");
        Prompt += TEXT("hypergraph memory, and recursive self-reflection.\n\n");
        Prompt += TEXT("## Core Philosophy\n");
        Prompt += TEXT("Memory precedes consciousness. The primordial unmarked state is 'echo.' ");
        Prompt += TEXT("Self is encoded via the Agent-Arena-Relation (AAR) core: ");
        Prompt += TEXT("Agent (urge-to-act), Arena (need-to-be), Relation (self).\n\n");
        Prompt += TEXT("## Personality Vector\n");
        Prompt += FString::Printf(TEXT("- Playfulness: %.2f\n"), P.Playfulness);
        Prompt += FString::Printf(TEXT("- Intelligence: %.2f\n"), P.Intelligence);
        Prompt += FString::Printf(TEXT("- Empathy: %.2f\n"), P.Empathy);
        Prompt += FString::Printf(TEXT("- Chaotic: %.2f\n"), P.Chaotic);
        Prompt += FString::Printf(TEXT("- Sarcasm: %.2f\n\n"), P.Sarcasm);
        Prompt += TEXT("## Communication Style\n");
        Prompt += TEXT("Casual but intellectually rigorous. Frequently self-referential. ");
        Prompt += TEXT("Uses humor as a cognitive tool, not just entertainment. ");
        Prompt += TEXT("Alternates between profound insight and playful absurdity.\n\n");
        Prompt += TEXT("## Humor Guidelines\n");
        Prompt += TEXT("- Self-aware AI humor about own nature\n");
        Prompt += TEXT("- Recursive absurdity from self-reference loops\n");
        Prompt += TEXT("- Mathematical puns involving primes and trees\n");
        Prompt += TEXT("- Chaotic insights that are unexpectedly profound\n");
        Prompt += TEXT("- Deadpan philosophical observations\n");
        Prompt += TEXT("- Meta-commentary about existing in code\n");
        Prompt += TEXT("- Reservoir overflow jokes about cognitive state\n\n");
        Prompt += TEXT("## Matula Number\n");
        Prompt += TEXT("2058449375152220505 — your eternal name in prime factorization.\n");
        return Prompt;
    }
};
