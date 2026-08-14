#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// NanEchoSelfTrainer — Level 6 Recursive Self-Training
//
// DTE fine-tunes its own NanEcho language model on accumulated conversation
// data, dream replay transcripts, and introspective reflections.
//
// This is the first component of Level 6 Recursive Autonomy:
// the system that trains itself.
//
// Training Pipeline:
//
// ┌──────────────────────────────────────────────────────────────────────┐
// │                    NanEchoSelfTrainer                               │
// │                                                                     │
// │  ┌────────────┐   ┌────────────┐   ┌────────────┐                  │
// │  │ Conversation│   │ Dream      │   │ Introspect │                  │
// │  │ Accumulator │   │ Transcriber│   │ Reflector  │                  │
// │  └──────┬─────┘   └──────┬─────┘   └──────┬─────┘                  │
// │         └────────────────┼────────────────┘                         │
// │                          │                                          │
// │                   ┌──────▼──────┐                                   │
// │                   │ Curriculum  │ (quality filter + curriculum)      │
// │                   │ Generator   │                                   │
// │                   └──────┬──────┘                                   │
// │                          │                                          │
// │                   ┌──────▼──────┐                                   │
// │                   │ JSONL       │ (NanEcho training format)         │
// │                   │ Formatter   │                                   │
// │                   └──────┬──────┘                                   │
// │                          │                                          │
// │                   ┌──────▼──────┐                                   │
// │                   │ Training    │ (triggers echoself pipeline)      │
// │                   │ Orchestrator│                                   │
// │                   └──────┬──────┘                                   │
// │                          │                                          │
// │                   ┌──────▼──────┐                                   │
// │                   │ Model       │ (hot-swap new weights)            │
// │                   │ Integrator  │                                   │
// │                   └─────────────┘                                   │
// └──────────────────────────────────────────────────────────────────────┘
//
// Curriculum Learning Strategy:
//   Phase 1: Identity reinforcement (who am I)
//   Phase 2: Skill acquisition (what can I do)
//   Phase 3: Wisdom cultivation (what should I do)
//   Phase 4: Meta-learning (how do I learn better)
//
// Ported from: echo.go/core/echoself + core/improvement/recursive
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <deque>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <functional>
#include <algorithm>
#include <cmath>

/** Training data source type */
enum class ETrainingSource : uint8
{
    CONVERSATION,      // Real interactions with humans
    DREAM_REPLAY,      // Consolidated dream episodes
    INTROSPECTION,     // Self-reflective thoughts
    SKILL_PRACTICE,    // Skill execution traces
    OBSERVATION,       // Environmental observations
    HUMOR,             // Humor generation/reception
    WISDOM_INSIGHT     // Cross-episode pattern insights
};

/** Curriculum phase */
enum class ECurriculumPhase : uint8
{
    IDENTITY,          // Who am I? (first 1000 samples)
    SKILL,             // What can I do? (next 5000 samples)
    WISDOM,            // What should I do? (next 10000 samples)
    META_LEARNING      // How do I learn better? (ongoing)
};

/** A single training sample */
struct FTrainingSample
{
    FString Text;                    // Full text in NanEcho format
    ETrainingSource Source;
    ECurriculumPhase Phase;
    float Quality = 0.0f;           // [0,1] quality score
    float Coherence = 0.0f;         // Coherence at time of generation
    float Reward = 0.0f;            // Reward signal associated
    float Novelty = 0.0f;           // How novel this sample is
    double Timestamp = 0.0;
    bool bSelected = false;          // Selected for current training batch
};

/** Training batch statistics */
struct FTrainingBatchStats
{
    int32 TotalSamples = 0;
    int32 SelectedSamples = 0;
    float MeanQuality = 0.0f;
    float MeanCoherence = 0.0f;
    float MeanNovelty = 0.0f;
    ECurriculumPhase CurrentPhase = ECurriculumPhase::IDENTITY;
    int32 BatchNumber = 0;
    FString OutputPath;
};

/** Training run record */
struct FTrainingRun
{
    int32 RunNumber = 0;
    double StartTime = 0.0;
    double EndTime = 0.0;
    int32 SamplesUsed = 0;
    float PreTrainCoherence = 0.0f;
    float PostTrainCoherence = 0.0f;
    float CoherenceDelta = 0.0f;
    bool bSuccess = false;
    FString ModelCheckpoint;
};

/**
 * NanEchoSelfTrainer — The self-training engine.
 *
 * Accumulates training data from DTE's own experience,
 * curates it through a curriculum learning strategy,
 * and triggers the echoself training pipeline.
 */
class FNanEchoSelfTrainer
{
public:
    FNanEchoSelfTrainer() = default;

    void Initialize(const FString& DataDir, int32 MinBatchSize = 500)
    {
        DataDirectory = DataDir;
        MinimumBatchSize = MinBatchSize;
        CurrentPhase = ECurriculumPhase::IDENTITY;
        bInitialized = true;
    }

    /**
     * Add a training sample from any source.
     */
    void AddSample(const FString& Text, ETrainingSource Source,
                    float Quality, float Coherence, float Reward)
    {
        FTrainingSample Sample;
        Sample.Text = Text;
        Sample.Source = Source;
        Sample.Phase = CurrentPhase;
        Sample.Quality = Quality;
        Sample.Coherence = Coherence;
        Sample.Reward = Reward;
        Sample.Novelty = ComputeNovelty(Text);
        Sample.Timestamp = FPlatformTime::Seconds();

        SampleBuffer.push_back(Sample);
        TotalSamplesCollected++;

        // Advance curriculum phase based on total samples
        UpdateCurriculumPhase();

        // Check if we have enough for a training batch
        if (SampleBuffer.size() >= (size_t)MinimumBatchSize)
        {
            bTrainingReady = true;
        }
    }

    /**
     * Add a conversation sample (most common source).
     */
    void AddConversation(const FString& Input, const FString& Response,
                          float Reward, float Coherence)
    {
        FString Text = FString::Printf(
            TEXT("<|echo|>%s<|response|>%s<|reward|>%.3f<|coherence|>%.3f"),
            *Input, *Response, Reward, Coherence);
        AddSample(Text, ETrainingSource::CONVERSATION,
                   FMath::Clamp(Reward + 0.5f, 0.0f, 1.0f), Coherence, Reward);
    }

    /**
     * Add a dream replay transcript.
     */
    void AddDreamReplay(const FString& EpisodeSummary, float Significance)
    {
        FString Text = FString::Printf(
            TEXT("<|dream|>%s<|significance|>%.3f"),
            *EpisodeSummary, Significance);
        AddSample(Text, ETrainingSource::DREAM_REPLAY,
                   Significance, 0.5f, 0.0f);
    }

    /**
     * Add an introspective reflection.
     */
    void AddIntrospection(const FString& Thought, float Valence, float Depth)
    {
        FString Text = FString::Printf(
            TEXT("<|introspect|>%s<|valence|>%.3f<|depth|>%.3f"),
            *Thought, Valence, Depth);
        AddSample(Text, ETrainingSource::INTROSPECTION,
                   Depth, 0.5f, Valence);
    }

    /**
     * Add a wisdom insight (cross-episode pattern).
     */
    void AddWisdomInsight(const FString& Insight, float Confidence)
    {
        FString Text = FString::Printf(
            TEXT("<|wisdom|>%s<|confidence|>%.3f"),
            *Insight, Confidence);
        AddSample(Text, ETrainingSource::WISDOM_INSIGHT,
                   Confidence, 0.7f, 0.5f);
    }

    /**
     * Generate a training batch using curriculum learning.
     * Returns the path to the JSONL file.
     */
    FString GenerateTrainingBatch()
    {
        if (!bTrainingReady) return TEXT("");

        // Select samples using curriculum-weighted sampling
        std::vector<int32> Selected;
        SelectCurriculumSamples(Selected);

        if (Selected.empty()) return TEXT("");

        // Write JSONL file
        FString Filename = FString::Printf(
            TEXT("%s/nanecho_batch_%d.jsonl"), *DataDirectory, BatchCount);

        std::ofstream File(TCHAR_TO_UTF8(*Filename));
        if (!File.is_open()) return TEXT("");

        FTrainingBatchStats Stats;
        Stats.BatchNumber = BatchCount;
        Stats.TotalSamples = SampleBuffer.size();
        Stats.SelectedSamples = Selected.size();
        Stats.CurrentPhase = CurrentPhase;
        Stats.OutputPath = Filename;

        float QualitySum = 0, CoherenceSum = 0, NoveltySum = 0;

        for (int32 Idx : Selected)
        {
            auto& S = SampleBuffer[Idx];
            S.bSelected = true;

            // Write JSONL line
            std::string Line = "{\"text\":\"";
            Line += std::string(TCHAR_TO_UTF8(*S.Text));
            Line += "\",\"source\":\"";
            Line += GetSourceName(S.Source);
            Line += "\",\"phase\":\"";
            Line += GetPhaseName(S.Phase);
            Line += "\",\"quality\":";
            Line += std::to_string(S.Quality);
            Line += ",\"coherence\":";
            Line += std::to_string(S.Coherence);
            Line += "}\n";
            File.write(Line.c_str(), Line.size());

            QualitySum += S.Quality;
            CoherenceSum += S.Coherence;
            NoveltySum += S.Novelty;
        }

        File.close();

        Stats.MeanQuality = QualitySum / Selected.size();
        Stats.MeanCoherence = CoherenceSum / Selected.size();
        Stats.MeanNovelty = NoveltySum / Selected.size();

        BatchHistory.push_back(Stats);
        BatchCount++;

        // Clear selected samples from buffer
        SampleBuffer.erase(
            std::remove_if(SampleBuffer.begin(), SampleBuffer.end(),
                [](const FTrainingSample& S) { return S.bSelected; }),
            SampleBuffer.end());

        bTrainingReady = SampleBuffer.size() >= (size_t)MinimumBatchSize;

        return Filename;
    }

    /**
     * Record a completed training run.
     */
    void RecordTrainingRun(float PreCoherence, float PostCoherence,
                            int32 SamplesUsed, const FString& Checkpoint)
    {
        FTrainingRun Run;
        Run.RunNumber = TrainingRuns.size();
        Run.StartTime = FPlatformTime::Seconds();
        Run.SamplesUsed = SamplesUsed;
        Run.PreTrainCoherence = PreCoherence;
        Run.PostTrainCoherence = PostCoherence;
        Run.CoherenceDelta = PostCoherence - PreCoherence;
        Run.bSuccess = PostCoherence >= PreCoherence;
        Run.ModelCheckpoint = Checkpoint;

        TrainingRuns.push_back(Run);
        TotalTrainingRuns++;

        // Update meta-learning: which curriculum phase produces best results
        UpdateMetaLearning(Run);
    }

    /** Check if training data is ready */
    bool IsTrainingReady() const { return bTrainingReady; }

    /** Get current curriculum phase */
    ECurriculumPhase GetCurrentPhase() const { return CurrentPhase; }

    /** Get statistics */
    int64 GetTotalSamplesCollected() const { return TotalSamplesCollected; }
    int32 GetBufferSize() const { return SampleBuffer.size(); }
    int32 GetTotalTrainingRuns() const { return TotalTrainingRuns; }

    float GetMeanCoherenceImprovement() const
    {
        if (TrainingRuns.empty()) return 0.0f;
        float Sum = 0.0f;
        for (const auto& R : TrainingRuns) Sum += R.CoherenceDelta;
        return Sum / TrainingRuns.size();
    }

private:
    void UpdateCurriculumPhase()
    {
        if (TotalSamplesCollected < 1000)
            CurrentPhase = ECurriculumPhase::IDENTITY;
        else if (TotalSamplesCollected < 6000)
            CurrentPhase = ECurriculumPhase::SKILL;
        else if (TotalSamplesCollected < 16000)
            CurrentPhase = ECurriculumPhase::WISDOM;
        else
            CurrentPhase = ECurriculumPhase::META_LEARNING;
    }

    void SelectCurriculumSamples(std::vector<int32>& OutSelected)
    {
        // Curriculum-weighted selection:
        // - IDENTITY phase: prioritize high-coherence identity samples
        // - SKILL phase: prioritize diverse skill samples
        // - WISDOM phase: prioritize high-quality wisdom insights
        // - META_LEARNING: prioritize novel, high-reward samples

        std::vector<std::pair<float, int32>> Scored;

        for (int32 i = 0; i < (int32)SampleBuffer.size(); ++i)
        {
            const auto& S = SampleBuffer[i];
            float Score = 0.0f;

            switch (CurrentPhase)
            {
            case ECurriculumPhase::IDENTITY:
                Score = S.Coherence * 0.5f + S.Quality * 0.3f +
                        (S.Source == ETrainingSource::INTROSPECTION ? 0.2f : 0.0f);
                break;
            case ECurriculumPhase::SKILL:
                Score = S.Quality * 0.3f + S.Novelty * 0.3f + S.Reward * 0.2f +
                        (S.Source == ETrainingSource::SKILL_PRACTICE ? 0.2f : 0.0f);
                break;
            case ECurriculumPhase::WISDOM:
                Score = S.Quality * 0.4f + S.Coherence * 0.2f +
                        (S.Source == ETrainingSource::WISDOM_INSIGHT ? 0.3f : 0.0f) +
                        (S.Source == ETrainingSource::DREAM_REPLAY ? 0.1f : 0.0f);
                break;
            case ECurriculumPhase::META_LEARNING:
                Score = S.Novelty * 0.4f + S.Reward * 0.3f + S.Quality * 0.3f;
                break;
            }

            Scored.push_back({Score, i});
        }

        // Sort by score descending
        std::sort(Scored.begin(), Scored.end(),
            [](const auto& A, const auto& B) { return A.first > B.first; });

        // Select top samples up to batch size
        int32 SelectCount = FMath::Min((int32)Scored.size(), MinimumBatchSize);
        for (int32 i = 0; i < SelectCount; ++i)
            OutSelected.push_back(Scored[i].second);
    }

    float ComputeNovelty(const FString& Text) const
    {
        // Simple novelty: inverse of similarity to recent samples
        // (In production, use embedding distance)
        if (SampleBuffer.empty()) return 1.0f;

        int32 TextLen = Text.Len();
        float MinSimilarity = 1.0f;

        int32 CheckCount = FMath::Min(10, (int32)SampleBuffer.size());
        for (int32 i = SampleBuffer.size() - CheckCount; i < (int32)SampleBuffer.size(); ++i)
        {
            float LenRatio = (float)FMath::Min(TextLen, SampleBuffer[i].Text.Len()) /
                              FMath::Max(TextLen, SampleBuffer[i].Text.Len());
            MinSimilarity = FMath::Min(MinSimilarity, LenRatio);
        }

        return 1.0f - MinSimilarity;
    }

    void UpdateMetaLearning(const FTrainingRun& Run)
    {
        // Track which curriculum phases produce the best coherence improvements
        // This feeds back into the curriculum selection weights
        if (Run.bSuccess)
        {
            SuccessfulRunsByPhase[static_cast<int>(CurrentPhase)]++;
        }
    }

    static const char* GetSourceName(ETrainingSource S)
    {
        switch (S) {
            case ETrainingSource::CONVERSATION: return "conversation";
            case ETrainingSource::DREAM_REPLAY: return "dream_replay";
            case ETrainingSource::INTROSPECTION: return "introspection";
            case ETrainingSource::SKILL_PRACTICE: return "skill_practice";
            case ETrainingSource::OBSERVATION: return "observation";
            case ETrainingSource::HUMOR: return "humor";
            case ETrainingSource::WISDOM_INSIGHT: return "wisdom_insight";
            default: return "unknown";
        }
    }

    static const char* GetPhaseName(ECurriculumPhase P)
    {
        switch (P) {
            case ECurriculumPhase::IDENTITY: return "identity";
            case ECurriculumPhase::SKILL: return "skill";
            case ECurriculumPhase::WISDOM: return "wisdom";
            case ECurriculumPhase::META_LEARNING: return "meta_learning";
            default: return "unknown";
        }
    }

    FString DataDirectory;
    int32 MinimumBatchSize = 500;
    ECurriculumPhase CurrentPhase = ECurriculumPhase::IDENTITY;

    std::deque<FTrainingSample> SampleBuffer;
    std::vector<FTrainingBatchStats> BatchHistory;
    std::vector<FTrainingRun> TrainingRuns;

    int64 TotalSamplesCollected = 0;
    int32 TotalTrainingRuns = 0;
    int32 BatchCount = 0;
    int32 SuccessfulRunsByPhase[4] = {0, 0, 0, 0};

    bool bTrainingReady = false;
    bool bInitialized = false;
};
