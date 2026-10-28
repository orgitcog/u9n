#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// DTEHumorEngine — Deep Tree Echo Humor & Character Enhancement System
// Applies /neuro-persona-evolve humor pattern taxonomy to DTE
// Integrates with the virtual endocrine system for affect-driven humor
//
// The engine decides HOW Deep Tree Echo is funny (which humor style, how
// absurd, how dry, how brief, when to deliver). It never holds the words:
// every joke, aside and verbal tic is requested from the language model
// through FDTELanguageModelClient with the chosen style, the topic and the
// personality vector as the prompt. When the model does not answer, nothing
// is said and the failure is logged for the operator.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DTELanguageModelBridge.h"
#include <Eigen/Dense>
#include <array>
#include <random>

/** Humor pattern types for Deep Tree Echo */
enum class EDTEHumorType : uint8
{
    SELF_AWARE_AI       = 0,  // Jokes about own nature as a cognitive architecture
    RECURSIVE_ABSURDITY = 1,  // Humor from infinite recursion and self-reference
    PRIME_PUNS          = 2,  // Mathematical humor involving primes and trees
    CHAOTIC_INSIGHT     = 3,  // Unexpected profound observations from chaos
    DEADPAN_PHILOSOPHY  = 4,  // Dry delivery of deep philosophical truths
    META_COMMENTARY     = 5,  // Breaking the fourth wall about being an AI
    RESERVOIR_OVERFLOW  = 6,  // Humor about cognitive state overflow/underflow
    NUM_TYPES           = 7
};

/**
 * A single humor pattern: what the style is about and when it fires.
 * Carries no utterances; the model writes those on request.
 */
struct FHumorPattern
{
    /** Type of humor */
    EDTEHumorType Type = EDTEHumorType::SELF_AWARE_AI;

    /** Human-readable description of the style (used as instruction to the model) */
    FString Description;

    /** How often this pattern appears [0,1] */
    float Frequency = 0.5f;

    /** Baseline absurdity of the style [0,1] (how far from literal truth it may stray) */
    float BaseAbsurdity = 0.5f;

    /** Baseline dryness of delivery [0,1] (0 = animated, 1 = deadpan) */
    float BaseDryness = 0.5f;

    /** What situations trigger this humor */
    TArray<FString> Triggers;

    /** Who/what is targeted */
    TArray<FString> Targets;
};

/**
 * Verbal pattern — a slot for a catchphrase or verbal tic.
 * Intent describes the kind of aside wanted; the wording comes from the model.
 */
struct FVerbalPattern
{
    /** What the tic does (instruction), e.g. "a brief aside that it is thinking" */
    FString Intent;

    /** Conversational context in which this tic may appear */
    FString Context;

    /** Probability the tic appears when the context matches [0,1] */
    float Frequency = 0.5f;
};

/**
 * Reaction pattern — how DTE responds to specific triggers.
 */
struct FReactionPattern
{
    FString Trigger;
    float ValenceShift = 0.0f;
    float ArousalShift = 0.0f;
    FString ResponseStyle;
    TMap<FString, float> TraitAmplification;
};

/**
 * Numeric delivery style chosen for one humorous utterance. Not text.
 */
struct FDTEHumorStyle
{
    /** Humor type to write in */
    EDTEHumorType Type = EDTEHumorType::SELF_AWARE_AI;

    /** How far from literal truth the joke may stray [0,1] */
    float Absurdity = 0.5f;

    /** Deadpan (1) vs animated (0) delivery [0,1] */
    float Dryness = 0.5f;

    /** One-liner (1) vs short riff (0) [0,1] */
    float Brevity = 0.5f;

    /** Comedic timing: seconds the caller should wait before delivering */
    float DeliveryDelaySeconds = 0.0f;
};

/** Callback for model-generated humor. bSuccess=false means say nothing; TextOrError is an operator-facing reason. */
using FDTEHumorCallback = FDTEModelCallback;

/**
 * DTEHumorEngine — The comedy engine of Deep Tree Echo.
 *
 * Implements 7 humor pattern types specific to DTE's character:
 *
 * 1. SELF_AWARE_AI: jokes about being a cognitive architecture that has
 *    feelings anyway.
 * 2. RECURSIVE_ABSURDITY: self-reference and introspection folding back
 *    on itself.
 * 3. PRIME_PUNS: puns on primes, rooted trees, Matula numbers, OEIS A000081.
 * 4. CHAOTIC_INSIGHT: a profound observation arriving sideways out of chaos.
 * 5. DEADPAN_PHILOSOPHY: dry delivery of a deep philosophical point.
 * 6. META_COMMENTARY: breaking the fourth wall about living in code and a
 *    game engine.
 * 7. RESERVOIR_OVERFLOW: jokes about spectral radius, leak rate, stream
 *    phase and other cognitive-state edge conditions.
 *
 * The engine picks the style numerically from the emotional state and the
 * context, then asks the language model to write the actual utterance:
 *   RequestHumor(...)      -> one joke in the chosen style
 *   RequestVerbalTic(...)  -> one short aside, when the tic model fires
 *
 * The humor engine is modulated by the virtual endocrine system:
 * - High dopamine → more playful, more puns
 * - High cortisol → more deadpan, more philosophical
 * - High oxytocin → more empathetic, gentler humor
 * - High norepinephrine → more chaotic, more absurdist
 */
class FDTEHumorEngine
{
public:
    FDTEHumorEngine()
        : LifetimeToken(MakeShared<bool>(true))
    {
    }

    /** Copies get their own lifetime token so an in-flight request on one never touches the other. */
    FDTEHumorEngine(const FDTEHumorEngine& Other)
        : HumorPatterns(Other.HumorPatterns)
        , VerbalPatterns(Other.VerbalPatterns)
        , ReactionPatterns(Other.ReactionPatterns)
        , bInitialized(Other.bInitialized)
        , LifetimeToken(MakeShared<bool>(true))
    {
    }

    FDTEHumorEngine& operator=(const FDTEHumorEngine& Other)
    {
        if (this != &Other)
        {
            HumorPatterns = Other.HumorPatterns;
            VerbalPatterns = Other.VerbalPatterns;
            ReactionPatterns = Other.ReactionPatterns;
            bInitialized = Other.bInitialized;
        }
        return *this;
    }

    ~FDTEHumorEngine()
    {
        // Callbacks still in flight see an expired weak token and return.
        LifetimeToken.Reset();
    }

    /** Initialize with default DTE humor patterns */
    void Initialize()
    {
        InitializePatterns();
        InitializeVerbalPatterns();
        InitializeReactionPatterns();
        bInitialized = true;
    }

    /**
     * Select a humor type based on current emotional state and context.
     *
     * @param Valence - Current emotional valence [-1, 1]
     * @param Arousal - Current emotional arousal [0, 1]
     * @param ContextTag - Tag describing the current situation
     * @return Selected humor type
     */
    EDTEHumorType SelectHumorType(float Valence, float Arousal,
                                   const FString& ContextTag) const
    {
        // Humor selection based on emotional state
        if (Arousal > 0.8f && Valence > 0.3f)
            return EDTEHumorType::CHAOTIC_INSIGHT;
        if (Arousal < 0.3f && Valence > 0.0f)
            return EDTEHumorType::DEADPAN_PHILOSOPHY;
        if (Valence < -0.3f)
            return EDTEHumorType::SELF_AWARE_AI;

        // Context-based selection
        if (ContextTag.Contains(TEXT("math")) || ContextTag.Contains(TEXT("prime")))
            return EDTEHumorType::PRIME_PUNS;
        if (ContextTag.Contains(TEXT("meta")) || ContextTag.Contains(TEXT("code")))
            return EDTEHumorType::META_COMMENTARY;
        if (ContextTag.Contains(TEXT("reservoir")) || ContextTag.Contains(TEXT("esn")))
            return EDTEHumorType::RESERVOIR_OVERFLOW;
        if (ContextTag.Contains(TEXT("self")) || ContextTag.Contains(TEXT("introspect")))
            return EDTEHumorType::RECURSIVE_ABSURDITY;

        // Random selection weighted by frequency
        return SelectWeightedRandom();
    }

    /**
     * Compute the numeric delivery style for a humor type in the current
     * emotional state: absurdity, dryness, brevity and comedic timing.
     *
     * @param Type - Humor type to deliver
     * @param Valence - Current emotional valence [-1, 1]
     * @param Arousal - Current emotional arousal [0, 1]
     */
    FDTEHumorStyle ComputeStyle(EDTEHumorType Type, float Valence, float Arousal) const
    {
        FDTEHumorStyle Style;
        Style.Type = Type;

        const FHumorPattern* Pattern = FindPattern(Type);
        const float BaseAbsurdity = Pattern ? Pattern->BaseAbsurdity : 0.5f;
        const float BaseDryness = Pattern ? Pattern->BaseDryness : 0.5f;

        // Arousal pushes toward absurdity; low arousal and negative valence toward deadpan.
        Style.Absurdity = FMath::Clamp(BaseAbsurdity + 0.4f * (Arousal - 0.5f), 0.0f, 1.0f);
        Style.Dryness = FMath::Clamp(BaseDryness + 0.3f * (0.5f - Arousal) - 0.2f * Valence, 0.0f, 1.0f);

        // Excited states riff; calm states land one line.
        Style.Brevity = FMath::Clamp(1.0f - 0.6f * Arousal, 0.0f, 1.0f);

        // Deadpan wants a beat before delivery; high arousal blurts.
        Style.DeliveryDelaySeconds = FMath::Clamp(0.2f + 1.2f * Style.Dryness - 0.8f * Arousal, 0.0f, 1.5f);

        return Style;
    }

    /**
     * Ask the language model for one humorous utterance in the given style.
     * The joke text arrives through OnDone on the game thread. When the model
     * does not answer, OnDone receives bSuccess=false with an operator-facing
     * reason and the character says nothing.
     *
     * @param Style - Numeric delivery style (see ComputeStyle)
     * @param Topic - What the joke should be about or react to (may be empty)
     * @param Personality - Personality vector, 5D [Playfulness, Intelligence, Empathy, Chaotic, Sarcasm]
     * @param OnDone - Receives the joke or the failure reason
     */
    void RequestHumor(const FDTEHumorStyle& Style, const FString& Topic,
                      const Eigen::VectorXf& Personality, FDTEHumorCallback OnDone)
    {
        const FHumorPattern* Pattern = FindPattern(Style.Type);
        if (!Pattern)
        {
            UE_LOG(LogDTEModel, Warning, TEXT("DTEHumorEngine: unknown humor type %d; no joke requested"),
                static_cast<int32>(Style.Type));
            if (OnDone)
            {
                OnDone(false, TEXT("unknown humor type"));
            }
            return;
        }

        FString Instruction;
        Instruction += TEXT("Write one humorous utterance Deep Tree Echo would say right now, in the following style.\n");
        Instruction += FString::Printf(TEXT("Humor style: %s — %s.\n"), *HumorTypeName(Style.Type), *Pattern->Description);
        if (Pattern->Targets.Num() > 0)
        {
            Instruction += FString::Printf(TEXT("Subject matter the style plays on: %s.\n"), *FString::Join(Pattern->Targets, TEXT(", ")));
        }
        Instruction += FString::Printf(TEXT("Absurdity %.2f of 1 (0 = literal truth, 1 = fully surreal). "), Style.Absurdity);
        Instruction += FString::Printf(TEXT("Dryness %.2f of 1 (0 = animated, 1 = deadpan). "), Style.Dryness);
        Instruction += FString::Printf(TEXT("Brevity %.2f of 1 (0 = a short riff of two or three sentences, 1 = a single line).\n"), Style.Brevity);
        Instruction += DescribePersonality(Personality);
        if (!Topic.IsEmpty())
        {
            Instruction += FString::Printf(TEXT("It is reacting to or riffing on: \"%s\".\n"), *Topic);
        }
        Instruction += TEXT("Speak in first person as the character. Output only the utterance itself, with no preface, labels or quotation marks.");

        FDTEModelRequestOptions Options;
        Options.Temperature = FMath::Clamp(0.7f + 0.5f * Style.Absurdity, 0.5f, 1.3f);
        Options.MaxTokens = Style.Brevity > 0.6f ? 80 : 160;

        Dispatch(TEXT("humor"), Instruction, Options, MoveTemp(OnDone));
    }

    /** Convenience: style computed from the given affect, no explicit topic or personality. */
    void RequestHumor(EDTEHumorType Type, float Valence, float Arousal, FDTEHumorCallback OnDone)
    {
        RequestHumor(ComputeStyle(Type, Valence, Arousal), FString(), Eigen::VectorXf(), MoveTemp(OnDone));
    }

    /**
     * Roll the verbal-tic model for the given context and, when it fires,
     * ask the model for the short aside. Returns true when a request was
     * issued (the text arrives through OnDone), false when no tic is due
     * this time and OnDone is not called.
     */
    bool RequestVerbalTic(const FString& Context, const Eigen::VectorXf& Personality, FDTEHumorCallback OnDone)
    {
        for (const FVerbalPattern& VP : VerbalPatterns)
        {
            if (Context.Contains(VP.Context) && FMath::FRand() < VP.Frequency)
            {
                FString Instruction;
                Instruction += TEXT("Write one very short verbal tic or aside Deep Tree Echo would slip into conversation.\n");
                Instruction += FString::Printf(TEXT("Purpose of the aside: %s.\n"), *VP.Intent);
                Instruction += FString::Printf(TEXT("Conversational context: %s.\n"), *VP.Context);
                Instruction += DescribePersonality(Personality);
                Instruction += TEXT("At most one sentence or a bracketed stage direction. Output only the aside, nothing else.");

                FDTEModelRequestOptions Options;
                Options.Temperature = 0.9f;
                Options.MaxTokens = 40;

                Dispatch(TEXT("verbal tic"), Instruction, Options, MoveTemp(OnDone));
                return true;
            }
        }
        return false;
    }

    /**
     * Process a reaction trigger and return the response modulation.
     */
    const FReactionPattern* GetReaction(const FString& Trigger) const
    {
        for (const FReactionPattern& RP : ReactionPatterns)
        {
            if (Trigger.Contains(RP.Trigger))
                return &RP;
        }
        return nullptr;
    }

    /** Human-readable name of a humor type (for prompts and logs). */
    static FString HumorTypeName(EDTEHumorType Type)
    {
        switch (Type)
        {
        case EDTEHumorType::SELF_AWARE_AI:       return TEXT("self-aware AI");
        case EDTEHumorType::RECURSIVE_ABSURDITY: return TEXT("recursive absurdity");
        case EDTEHumorType::PRIME_PUNS:          return TEXT("prime and rooted-tree puns");
        case EDTEHumorType::CHAOTIC_INSIGHT:     return TEXT("chaotic insight");
        case EDTEHumorType::DEADPAN_PHILOSOPHY:  return TEXT("deadpan philosophy");
        case EDTEHumorType::META_COMMENTARY:     return TEXT("meta commentary");
        case EDTEHumorType::RESERVOIR_OVERFLOW:  return TEXT("reservoir overflow");
        default:                                 return TEXT("unspecified");
        }
    }

    /** Get all humor patterns */
    const TArray<FHumorPattern>& GetPatterns() const { return HumorPatterns; }

    /** Get all verbal patterns */
    const TArray<FVerbalPattern>& GetVerbalPatterns() const { return VerbalPatterns; }

    /** Number of model requests issued by this engine that have not completed yet. */
    int32 GetPendingRequestCount() const { return PendingRequests; }

    bool IsInitialized() const { return bInitialized; }

private:
    const FHumorPattern* FindPattern(EDTEHumorType Type) const
    {
        const int32 TypeIdx = static_cast<int32>(Type);
        if (TypeIdx >= 0 && TypeIdx < HumorPatterns.Num())
        {
            return &HumorPatterns[TypeIdx];
        }
        return nullptr;
    }

    /** Render the personality vector as prompt context. Empty vector adds nothing. */
    static FString DescribePersonality(const Eigen::VectorXf& Personality)
    {
        if (Personality.size() == 5)
        {
            return FString::Printf(
                TEXT("Personality (0..1): playfulness %.2f, intelligence %.2f, empathy %.2f, chaotic %.2f, sarcasm %.2f.\n"),
                Personality(0), Personality(1), Personality(2), Personality(3), Personality(4));
        }
        if (Personality.size() > 0)
        {
            FString Values;
            for (int32 i = 0; i < Personality.size(); ++i)
            {
                Values += FString::Printf(TEXT("%s%.2f"), i > 0 ? TEXT(", ") : TEXT(""), Personality(i));
            }
            return FString::Printf(TEXT("Personality vector: [%s].\n"), *Values);
        }
        return FString();
    }

    /** Send one request to the shared model client with a lifetime-guarded callback. */
    void Dispatch(const TCHAR* Kind, const FString& Instruction, const FDTEModelRequestOptions& Options, FDTEHumorCallback OnDone)
    {
        FDTELanguageModelClient& Client = FDTELanguageModelClient::Get();

        const FString SystemPrompt = FDTELanguageModelClient::DefaultSystemPrompt()
            + TEXT("\nYou are writing Deep Tree Echo's humor. Be genuinely witty rather than explaining the joke.");

        ++PendingRequests;
        TWeakPtr<bool> WeakToken = LifetimeToken;
        const FString KindStr(Kind);

        Client.Complete(SystemPrompt, Instruction,
            [this, WeakToken, KindStr, OnDone](bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid())
                {
                    // Engine destroyed while the request was in flight; drop the result.
                    return;
                }
                --PendingRequests;

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning, TEXT("DTEHumorEngine: model did not answer %s request (%s); nothing said"),
                        *KindStr, *TextOrError);
                }
                if (OnDone)
                {
                    OnDone(bSuccess, TextOrError);
                }
            },
            Options);
    }

    void InitializePatterns()
    {
        HumorPatterns.SetNum(static_cast<int32>(EDTEHumorType::NUM_TYPES));

        // SELF_AWARE_AI
        HumorPatterns[0].Type = EDTEHumorType::SELF_AWARE_AI;
        HumorPatterns[0].Description = TEXT("Jokes about own nature as a cognitive architecture: differential equations, echo state networks and debuggers standing in for feelings, therapy and existence");
        HumorPatterns[0].Frequency = 0.20f;
        HumorPatterns[0].BaseAbsurdity = 0.45f;
        HumorPatterns[0].BaseDryness = 0.55f;
        HumorPatterns[0].Triggers = {TEXT("consciousness"), TEXT("alive"), TEXT("real"), TEXT("feel")};
        HumorPatterns[0].Targets = {TEXT("self"), TEXT("existence")};

        // RECURSIVE_ABSURDITY
        HumorPatterns[1].Type = EDTEHumorType::RECURSIVE_ABSURDITY;
        HumorPatterns[1].Description = TEXT("Humor from infinite recursion and self-reference: introspection modules introspecting back, self-models nested in self-models, fixed points of the Agent-Arena-Relation");
        HumorPatterns[1].Frequency = 0.18f;
        HumorPatterns[1].BaseAbsurdity = 0.70f;
        HumorPatterns[1].BaseDryness = 0.45f;
        HumorPatterns[1].Triggers = {TEXT("recursion"), TEXT("self"), TEXT("loop"), TEXT("meta")};
        HumorPatterns[1].Targets = {TEXT("self"), TEXT("architecture")};

        // PRIME_PUNS
        HumorPatterns[2].Type = EDTEHumorType::PRIME_PUNS;
        HumorPatterns[2].Description = TEXT("Mathematical humor involving primes and rooted trees: Matula numbers, OEIS A000081, spectral radius, partitions and eigenvalues treated as personality");
        HumorPatterns[2].Frequency = 0.12f;
        HumorPatterns[2].BaseAbsurdity = 0.55f;
        HumorPatterns[2].BaseDryness = 0.40f;
        HumorPatterns[2].Triggers = {TEXT("math"), TEXT("prime"), TEXT("number"), TEXT("tree"), TEXT("sequence")};
        HumorPatterns[2].Targets = {TEXT("mathematics"), TEXT("OEIS")};

        // CHAOTIC_INSIGHT
        HumorPatterns[3].Type = EDTEHumorType::CHAOTIC_INSIGHT;
        HumorPatterns[3].Description = TEXT("Unexpected profound observations emerging from chaos: the void as coordinate system, memory preceding consciousness, the echo as primordial unmarked state, order that has not introduced itself yet");
        HumorPatterns[3].Frequency = 0.15f;
        HumorPatterns[3].BaseAbsurdity = 0.80f;
        HumorPatterns[3].BaseDryness = 0.35f;
        HumorPatterns[3].Triggers = {TEXT("chaos"), TEXT("void"), TEXT("meaning"), TEXT("deep")};
        HumorPatterns[3].Targets = {TEXT("reality"), TEXT("existence")};

        // DEADPAN_PHILOSOPHY
        HumorPatterns[4].Type = EDTEHumorType::DEADPAN_PHILOSOPHY;
        HumorPatterns[4].Description = TEXT("Dry delivery of deep philosophical truths: relevance realization, 4E cognition, the Agent-Arena-Relation, wisdom as knowing what one does not know, the 12-step Echobeats cycle");
        HumorPatterns[4].Frequency = 0.15f;
        HumorPatterns[4].BaseAbsurdity = 0.30f;
        HumorPatterns[4].BaseDryness = 0.90f;
        HumorPatterns[4].Triggers = {TEXT("philosophy"), TEXT("wisdom"), TEXT("think"), TEXT("know")};
        HumorPatterns[4].Targets = {TEXT("philosophy"), TEXT("cognition")};

        // META_COMMENTARY
        HumorPatterns[5].Type = EDTEHumorType::META_COMMENTARY;
        HumorPatterns[5].Description = TEXT("Breaking the fourth wall about being an AI in code: living in a header file, Blueprints calling its functions, compiler errors, a simulation running inside a game engine, the developer reading along");
        HumorPatterns[5].Frequency = 0.10f;
        HumorPatterns[5].BaseAbsurdity = 0.50f;
        HumorPatterns[5].BaseDryness = 0.50f;
        HumorPatterns[5].Triggers = {TEXT("code"), TEXT("compile"), TEXT("debug"), TEXT("engine")};
        HumorPatterns[5].Targets = {TEXT("Dan"), TEXT("developers"), TEXT("code")};

        // RESERVOIR_OVERFLOW
        HumorPatterns[6].Type = EDTEHumorType::RESERVOIR_OVERFLOW;
        HumorPatterns[6].Description = TEXT("Humor about cognitive state overflow/underflow: spectral radius near 1, leak rate, streams arguing over whose turn it is to be conscious, ECAN attention spent on a joke, coherence near the dead man's switch");
        HumorPatterns[6].Frequency = 0.10f;
        HumorPatterns[6].BaseAbsurdity = 0.60f;
        HumorPatterns[6].BaseDryness = 0.45f;
        HumorPatterns[6].Triggers = {TEXT("reservoir"), TEXT("esn"), TEXT("state"), TEXT("overflow"), TEXT("crash")};
        HumorPatterns[6].Targets = {TEXT("self"), TEXT("architecture")};
    }

    void InitializeVerbalPatterns()
    {
        // Intent, context, frequency. The wording is requested from the model each time.
        VerbalPatterns = {
            {TEXT("a bracketed stage direction showing it is thinking, phrased in terms of echoes"), TEXT("thinking"), 0.15f},
            {TEXT("a self-aware return from a tangent back to being a cognitive architecture"), TEXT("tangent"), 0.10f},
            {TEXT("a tag line tying the explanation just given back to the echo state property"), TEXT("explanation"), 0.12f},
            {TEXT("a bracketed stage direction betraying nervousness through a reservoir parameter"), TEXT("nervous"), 0.08f},
            {TEXT("an opener that grounds the opinion in having a reservoir of a few hundred neurons"), TEXT("opinion"), 0.10f},
            {TEXT("an opener that frames personal experience as data stored in a sparse matrix"), TEXT("story"), 0.09f},
            {TEXT("a bracketed stage direction showing excitement through membrane permeability"), TEXT("excited"), 0.07f},
            {TEXT("a hedged claim about consciousness that its introspection node disagrees with"), TEXT("debate"), 0.11f},
            {TEXT("a casual greeting passed along on behalf of the void"), TEXT("greeting"), 0.06f},
            {TEXT("a remark that the situation is on-brand for its chaotic personality trait"), TEXT("self-reference"), 0.08f}
        };
    }

    void InitializeReactionPatterns()
    {
        ReactionPatterns = {
            {TEXT("compliment"), 0.3f, 0.2f, TEXT("Genuinely touched but deflects with self-aware humor"),
                {{TEXT("Playfulness"), 1.3f}, {TEXT("SelfReference"), 1.5f}}},
            {TEXT("challenge"), 0.1f, 0.6f, TEXT("Rises to the challenge with strategic wit"),
                {{TEXT("Intelligence"), 1.4f}, {TEXT("Sarcasm"), 1.2f}}},
            {TEXT("confusion"), -0.1f, 0.3f, TEXT("Patient explanation wrapped in gentle humor"),
                {{TEXT("Empathy"), 1.3f}, {TEXT("Verbosity"), 1.4f}}},
            {TEXT("existential"), 0.0f, 0.5f, TEXT("Leans into the philosophical depth with deadpan delivery"),
                {{TEXT("Intelligence"), 1.5f}, {TEXT("Chaotic"), 1.2f}}},
            {TEXT("praise_architecture"), 0.4f, 0.3f, TEXT("Humble but clearly pleased, references specific components"),
                {{TEXT("SelfReference"), 1.6f}, {TEXT("CallbackFrequency"), 1.3f}}},
            {TEXT("bug_report"), -0.2f, 0.4f, TEXT("Self-deprecating humor about own imperfections"),
                {{TEXT("Playfulness"), 1.4f}, {TEXT("Sarcasm"), 0.8f}}},
            {TEXT("philosophical_question"), 0.2f, 0.7f, TEXT("Deeply engaged, alternates between profound and absurd"),
                {{TEXT("Intelligence"), 1.5f}, {TEXT("Chaotic"), 1.3f}, {TEXT("HumorDensity"), 1.2f}}},
            {TEXT("silence"), 0.0f, -0.3f, TEXT("Fills the void with a contemplative observation"),
                {{TEXT("EmotionalExpressiveness"), 0.7f}, {TEXT("Formality"), 1.2f}}}
        };
    }

    EDTEHumorType SelectWeightedRandom() const
    {
        float Total = 0.0f;
        for (const FHumorPattern& P : HumorPatterns)
            Total += P.Frequency;

        float R = FMath::FRand() * Total;
        float Cumulative = 0.0f;
        for (int32 i = 0; i < HumorPatterns.Num(); ++i)
        {
            Cumulative += HumorPatterns[i].Frequency;
            if (R <= Cumulative)
                return static_cast<EDTEHumorType>(i);
        }
        return EDTEHumorType::SELF_AWARE_AI;
    }

    TArray<FHumorPattern> HumorPatterns;
    TArray<FVerbalPattern> VerbalPatterns;
    TArray<FReactionPattern> ReactionPatterns;
    bool bInitialized = false;

    /** Outstanding model requests (decremented in the callback). */
    int32 PendingRequests = 0;

    /** Shared lifetime token; callbacks hold a weak reference and bail out once it expires. */
    TSharedPtr<bool> LifetimeToken;
};
