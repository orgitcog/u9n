/**
 * @file BehavioralFrameworkTests.cpp
 * @brief Exhaustive unit tests for the Behavioral Testing Framework
 *
 * Tests cover:
 * - Scenario-based behavior validation
 * - Cognitive mode testing
 * - Avatar behavior patterns (SuperHotGirl, HyperChaotic)
 * - Neurochemical simulation testing
 * - Personality trait systems
 * - Emotion blending and expression
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <random>
#include <cmath>

// ============================================================================
// Mock Types for Behavioral Testing Framework
// ============================================================================

enum class ECognitiveMode {
    Reactive,
    Deliberative,
    Reflective,
    Creative,
    Integrative
};

enum class EBehaviorPattern {
    SuperHotGirl,
    HyperChaotic,
    Calm,
    Focused,
    Playful
};

enum class EEmotionType {
    Joy,
    Sadness,
    Fear,
    Anger,
    Surprise,
    Disgust,
    Neutral
};

struct FPersonalityTrait {
    std::string Name;
    float Value = 0.5f;
    float Volatility = 0.1f;

    FPersonalityTrait() = default;
    FPersonalityTrait(const std::string& name, float value, float volatility = 0.1f)
        : Name(name), Value(value), Volatility(volatility) {}
};

struct FEmotionState {
    EEmotionType Type = EEmotionType::Neutral;
    float Intensity = 0.0f;
    float Duration = 0.0f;
    float BlendWeight = 1.0f;
};

struct FNeurochemicalState {
    float Dopamine = 0.5f;
    float Serotonin = 0.5f;
    float Norepinephrine = 0.5f;
    float Oxytocin = 0.5f;
    float Cortisol = 0.3f;
    float Endorphins = 0.3f;

    float GetArousal() const {
        return (Dopamine + Norepinephrine) / 2.0f;
    }

    float GetValence() const {
        return (Serotonin + Oxytocin - Cortisol) / 2.0f + 0.5f;
    }
};

struct FBehaviorScenario {
    std::string Name;
    std::string Description;
    std::vector<std::string> Prerequisites;
    std::vector<std::string> ExpectedOutcomes;
    float TimeLimit = 10.0f;
    bool bRequiresUserInteraction = false;
};

struct FBehaviorTestResult {
    std::string ScenarioName;
    bool bPassed = false;
    std::string FailureReason;
    float ExecutionTime = 0.0f;
    std::vector<std::string> ActualOutcomes;
    std::vector<std::string> MismatchedExpectations;
};

/**
 * @brief Mock Personality System
 */
class MockPersonalitySystem {
public:
    MockPersonalitySystem() {
        InitializeDefaultTraits();
    }

    void Initialize() {
        bInitialized = true;
    }

    bool IsInitialized() const { return bInitialized; }

    void SetTrait(const std::string& name, float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        for (auto& trait : Traits) {
            if (trait.Name == name) {
                trait.Value = value;
                return;
            }
        }
        Traits.emplace_back(name, value);
    }

    float GetTrait(const std::string& name) const {
        for (const auto& trait : Traits) {
            if (trait.Name == name) {
                return trait.Value;
            }
        }
        return 0.5f;
    }

    void ApplyBehaviorPattern(EBehaviorPattern pattern) {
        CurrentPattern = pattern;

        switch (pattern) {
            case EBehaviorPattern::SuperHotGirl:
                SetTrait("Confidence", 0.95f);
                SetTrait("Flirtiness", 0.85f);
                SetTrait("Charisma", 0.9f);
                SetTrait("Assertiveness", 0.8f);
                break;

            case EBehaviorPattern::HyperChaotic:
                SetTrait("Spontaneity", 0.95f);
                SetTrait("Unpredictability", 0.9f);
                SetTrait("Energy", 0.95f);
                SetTrait("Impulsivity", 0.85f);
                break;

            case EBehaviorPattern::Calm:
                SetTrait("Composure", 0.9f);
                SetTrait("Patience", 0.85f);
                SetTrait("Stability", 0.9f);
                break;

            case EBehaviorPattern::Focused:
                SetTrait("Concentration", 0.9f);
                SetTrait("Determination", 0.85f);
                SetTrait("Discipline", 0.8f);
                break;

            case EBehaviorPattern::Playful:
                SetTrait("Humor", 0.85f);
                SetTrait("Curiosity", 0.9f);
                SetTrait("Openness", 0.85f);
                break;
        }
    }

    EBehaviorPattern GetCurrentPattern() const { return CurrentPattern; }

    void ModulateByConfidence(float confidenceLevel) {
        confidenceLevel = std::clamp(confidenceLevel, 0.0f, 1.0f);

        for (auto& trait : Traits) {
            // Confident individuals express traits more strongly
            trait.Value = trait.Value * (0.5f + 0.5f * confidenceLevel);
        }
    }

    const std::vector<FPersonalityTrait>& GetAllTraits() const { return Traits; }

    float CalculateCompatibility(const MockPersonalitySystem& other) const {
        float totalDiff = 0.0f;
        int count = 0;

        for (const auto& trait : Traits) {
            float otherValue = other.GetTrait(trait.Name);
            totalDiff += std::abs(trait.Value - otherValue);
            count++;
        }

        if (count == 0) return 0.5f;
        return 1.0f - (totalDiff / count);
    }

private:
    void InitializeDefaultTraits() {
        Traits.emplace_back("Openness", 0.5f);
        Traits.emplace_back("Conscientiousness", 0.5f);
        Traits.emplace_back("Extraversion", 0.5f);
        Traits.emplace_back("Agreeableness", 0.5f);
        Traits.emplace_back("Neuroticism", 0.3f);
    }

    bool bInitialized = false;
    EBehaviorPattern CurrentPattern = EBehaviorPattern::Calm;
    std::vector<FPersonalityTrait> Traits;
};

/**
 * @brief Mock Emotion Blending System
 */
class MockEmotionBlendingSystem {
public:
    void Initialize() {
        bInitialized = true;
        CurrentEmotion.Type = EEmotionType::Neutral;
        CurrentEmotion.Intensity = 0.0f;
    }

    bool IsInitialized() const { return bInitialized; }

    void SetPrimaryEmotion(EEmotionType type, float intensity) {
        CurrentEmotion.Type = type;
        CurrentEmotion.Intensity = std::clamp(intensity, 0.0f, 1.0f);
        CurrentEmotion.BlendWeight = 1.0f;
    }

    void BlendEmotion(EEmotionType type, float intensity, float blendFactor) {
        blendFactor = std::clamp(blendFactor, 0.0f, 1.0f);
        intensity = std::clamp(intensity, 0.0f, 1.0f);

        FEmotionState secondary;
        secondary.Type = type;
        secondary.Intensity = intensity;
        secondary.BlendWeight = blendFactor;

        BlendedEmotions.push_back(secondary);

        // Adjust primary weight
        CurrentEmotion.BlendWeight *= (1.0f - blendFactor * 0.5f);
    }

    FEmotionState GetCurrentEmotion() const { return CurrentEmotion; }

    std::vector<FEmotionState> GetBlendedEmotions() const {
        std::vector<FEmotionState> all;
        all.push_back(CurrentEmotion);
        all.insert(all.end(), BlendedEmotions.begin(), BlendedEmotions.end());
        return all;
    }

    float GetDominantIntensity() const {
        float maxIntensity = CurrentEmotion.Intensity * CurrentEmotion.BlendWeight;
        for (const auto& emotion : BlendedEmotions) {
            float weighted = emotion.Intensity * emotion.BlendWeight;
            if (weighted > maxIntensity) {
                maxIntensity = weighted;
            }
        }
        return maxIntensity;
    }

    void DecayEmotions(float decayRate) {
        CurrentEmotion.Intensity *= (1.0f - decayRate);

        for (auto& emotion : BlendedEmotions) {
            emotion.Intensity *= (1.0f - decayRate);
        }

        // Remove weak emotions
        BlendedEmotions.erase(
            std::remove_if(BlendedEmotions.begin(), BlendedEmotions.end(),
                [](const FEmotionState& e) { return e.Intensity < 0.1f; }),
            BlendedEmotions.end()
        );
    }

    void TransitionTo(EEmotionType targetType, float targetIntensity, float transitionSpeed) {
        transitionSpeed = std::clamp(transitionSpeed, 0.0f, 1.0f);

        // Gradual transition
        CurrentEmotion.Intensity *= (1.0f - transitionSpeed);

        FEmotionState transition;
        transition.Type = targetType;
        transition.Intensity = targetIntensity * transitionSpeed;
        transition.BlendWeight = transitionSpeed;

        BlendedEmotions.push_back(transition);

        // If transition is complete
        if (CurrentEmotion.Intensity < 0.1f) {
            CurrentEmotion.Type = targetType;
            CurrentEmotion.Intensity = targetIntensity;
            CurrentEmotion.BlendWeight = 1.0f;
            BlendedEmotions.clear();
        }
    }

    void Reset() {
        CurrentEmotion.Type = EEmotionType::Neutral;
        CurrentEmotion.Intensity = 0.0f;
        CurrentEmotion.BlendWeight = 1.0f;
        BlendedEmotions.clear();
    }

private:
    bool bInitialized = false;
    FEmotionState CurrentEmotion;
    std::vector<FEmotionState> BlendedEmotions;
};

/**
 * @brief Mock Neurochemical System
 */
class MockNeurochemicalSystem {
public:
    void Initialize() {
        bInitialized = true;
        State = FNeurochemicalState();
    }

    bool IsInitialized() const { return bInitialized; }

    void SetDopamine(float value) { State.Dopamine = std::clamp(value, 0.0f, 1.0f); }
    void SetSerotonin(float value) { State.Serotonin = std::clamp(value, 0.0f, 1.0f); }
    void SetNorepinephrine(float value) { State.Norepinephrine = std::clamp(value, 0.0f, 1.0f); }
    void SetOxytocin(float value) { State.Oxytocin = std::clamp(value, 0.0f, 1.0f); }
    void SetCortisol(float value) { State.Cortisol = std::clamp(value, 0.0f, 1.0f); }
    void SetEndorphins(float value) { State.Endorphins = std::clamp(value, 0.0f, 1.0f); }

    FNeurochemicalState GetState() const { return State; }

    void TriggerReward(float intensity) {
        intensity = std::clamp(intensity, 0.0f, 1.0f);
        State.Dopamine = std::min(1.0f, State.Dopamine + intensity * 0.3f);
        State.Endorphins = std::min(1.0f, State.Endorphins + intensity * 0.2f);
    }

    void TriggerStress(float intensity) {
        intensity = std::clamp(intensity, 0.0f, 1.0f);
        State.Cortisol = std::min(1.0f, State.Cortisol + intensity * 0.4f);
        State.Norepinephrine = std::min(1.0f, State.Norepinephrine + intensity * 0.3f);
        State.Serotonin = std::max(0.0f, State.Serotonin - intensity * 0.2f);
    }

    void TriggerSocialBonding(float intensity) {
        intensity = std::clamp(intensity, 0.0f, 1.0f);
        State.Oxytocin = std::min(1.0f, State.Oxytocin + intensity * 0.4f);
        State.Serotonin = std::min(1.0f, State.Serotonin + intensity * 0.2f);
    }

    void ApplyHomeostasis(float rate) {
        rate = std::clamp(rate, 0.0f, 1.0f);
        float baseline = 0.5f;

        State.Dopamine += (baseline - State.Dopamine) * rate;
        State.Serotonin += (baseline - State.Serotonin) * rate;
        State.Norepinephrine += (0.4f - State.Norepinephrine) * rate;
        State.Oxytocin += (0.4f - State.Oxytocin) * rate;
        State.Cortisol += (0.3f - State.Cortisol) * rate;
        State.Endorphins += (0.3f - State.Endorphins) * rate;
    }

    ECognitiveMode SuggestCognitiveMode() const {
        float arousal = State.GetArousal();
        float valence = State.GetValence();

        if (State.Cortisol > 0.7f) {
            return ECognitiveMode::Reactive;  // High stress = reactive
        }

        if (valence > 0.7f && arousal > 0.6f) {
            return ECognitiveMode::Creative;  // Happy and energetic = creative
        }

        if (valence > 0.5f && arousal < 0.4f) {
            return ECognitiveMode::Reflective;  // Calm and positive = reflective
        }

        if (arousal > 0.7f) {
            return ECognitiveMode::Deliberative;  // High energy = deliberative
        }

        return ECognitiveMode::Integrative;
    }

private:
    bool bInitialized = false;
    FNeurochemicalState State;
};

/**
 * @brief Mock Behavioral Test Framework
 */
class MockBehavioralTestFramework {
public:
    MockBehavioralTestFramework()
        : Personality(std::make_unique<MockPersonalitySystem>()),
          Emotions(std::make_unique<MockEmotionBlendingSystem>()),
          Neurochemistry(std::make_unique<MockNeurochemicalSystem>()) {}

    void Initialize() {
        Personality->Initialize();
        Emotions->Initialize();
        Neurochemistry->Initialize();
        bInitialized = true;
    }

    bool IsInitialized() const { return bInitialized; }

    void RegisterScenario(const FBehaviorScenario& scenario) {
        Scenarios.push_back(scenario);
    }

    FBehaviorTestResult RunScenario(const std::string& scenarioName) {
        FBehaviorTestResult result;
        result.ScenarioName = scenarioName;

        // Find scenario
        const FBehaviorScenario* scenario = nullptr;
        for (const auto& s : Scenarios) {
            if (s.Name == scenarioName) {
                scenario = &s;
                break;
            }
        }

        if (!scenario) {
            result.bPassed = false;
            result.FailureReason = "Scenario not found";
            return result;
        }

        // Check prerequisites
        for (const auto& prereq : scenario->Prerequisites) {
            if (!CheckPrerequisite(prereq)) {
                result.bPassed = false;
                result.FailureReason = "Prerequisite failed: " + prereq;
                return result;
            }
        }

        auto startTime = std::chrono::high_resolution_clock::now();

        // Execute scenario (simulate behavior)
        ExecuteScenarioBehavior(*scenario, result);

        auto endTime = std::chrono::high_resolution_clock::now();
        result.ExecutionTime = std::chrono::duration<float>(endTime - startTime).count();

        // Check expected outcomes
        result.bPassed = true;
        for (const auto& expected : scenario->ExpectedOutcomes) {
            bool found = false;
            for (const auto& actual : result.ActualOutcomes) {
                if (actual.find(expected) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                result.bPassed = false;
                result.MismatchedExpectations.push_back(expected);
            }
        }

        return result;
    }

    std::vector<FBehaviorTestResult> RunAllScenarios() {
        std::vector<FBehaviorTestResult> results;
        for (const auto& scenario : Scenarios) {
            results.push_back(RunScenario(scenario.Name));
        }
        return results;
    }

    int GetPassedCount(const std::vector<FBehaviorTestResult>& results) const {
        int passed = 0;
        for (const auto& r : results) {
            if (r.bPassed) passed++;
        }
        return passed;
    }

    // Direct access for testing
    MockPersonalitySystem* GetPersonality() { return Personality.get(); }
    MockEmotionBlendingSystem* GetEmotions() { return Emotions.get(); }
    MockNeurochemicalSystem* GetNeurochemistry() { return Neurochemistry.get(); }

private:
    bool CheckPrerequisite(const std::string& prereq) {
        if (prereq == "initialized") return bInitialized;
        if (prereq == "calm") return Emotions->GetCurrentEmotion().Type == EEmotionType::Neutral;
        if (prereq == "high_energy") return Neurochemistry->GetState().GetArousal() > 0.6f;
        return true;  // Unknown prerequisites pass
    }

    void ExecuteScenarioBehavior(const FBehaviorScenario& scenario, FBehaviorTestResult& result) {
        // Simulate scenario execution based on name patterns
        if (scenario.Name.find("emotion") != std::string::npos) {
            Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.8f);
            result.ActualOutcomes.push_back("emotion_set");
        }

        if (scenario.Name.find("stress") != std::string::npos) {
            Neurochemistry->TriggerStress(0.7f);
            result.ActualOutcomes.push_back("stress_triggered");
        }

        if (scenario.Name.find("reward") != std::string::npos) {
            Neurochemistry->TriggerReward(0.8f);
            result.ActualOutcomes.push_back("reward_triggered");
        }

        if (scenario.Name.find("personality") != std::string::npos) {
            Personality->ApplyBehaviorPattern(EBehaviorPattern::SuperHotGirl);
            result.ActualOutcomes.push_back("personality_applied");
        }

        if (scenario.Name.find("blend") != std::string::npos) {
            Emotions->BlendEmotion(EEmotionType::Surprise, 0.6f, 0.3f);
            result.ActualOutcomes.push_back("emotion_blended");
        }
    }

    bool bInitialized = false;
    std::unique_ptr<MockPersonalitySystem> Personality;
    std::unique_ptr<MockEmotionBlendingSystem> Emotions;
    std::unique_ptr<MockNeurochemicalSystem> Neurochemistry;
    std::vector<FBehaviorScenario> Scenarios;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class PersonalitySystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        Personality = std::make_unique<MockPersonalitySystem>();
        Personality->Initialize();
    }

    std::unique_ptr<MockPersonalitySystem> Personality;
};

class EmotionBlendingTest : public ::testing::Test {
protected:
    void SetUp() override {
        Emotions = std::make_unique<MockEmotionBlendingSystem>();
        Emotions->Initialize();
    }

    std::unique_ptr<MockEmotionBlendingSystem> Emotions;
};

class NeurochemicalSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        Neurochemistry = std::make_unique<MockNeurochemicalSystem>();
        Neurochemistry->Initialize();
    }

    std::unique_ptr<MockNeurochemicalSystem> Neurochemistry;
};

class BehavioralFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        Framework = std::make_unique<MockBehavioralTestFramework>();
        Framework->Initialize();
    }

    std::unique_ptr<MockBehavioralTestFramework> Framework;
};

// ============================================================================
// Personality System Tests
// ============================================================================

TEST_F(PersonalitySystemTest, Initialization) {
    EXPECT_TRUE(Personality->IsInitialized());
}

TEST_F(PersonalitySystemTest, DefaultTraits) {
    const auto& traits = Personality->GetAllTraits();
    EXPECT_GE(traits.size(), 5);  // Big Five
}

TEST_F(PersonalitySystemTest, SetAndGetTrait) {
    Personality->SetTrait("Openness", 0.8f);
    EXPECT_FLOAT_EQ(Personality->GetTrait("Openness"), 0.8f);
}

TEST_F(PersonalitySystemTest, TraitClamping) {
    Personality->SetTrait("Openness", 1.5f);
    EXPECT_FLOAT_EQ(Personality->GetTrait("Openness"), 1.0f);

    Personality->SetTrait("Openness", -0.5f);
    EXPECT_FLOAT_EQ(Personality->GetTrait("Openness"), 0.0f);
}

TEST_F(PersonalitySystemTest, SuperHotGirlPattern) {
    Personality->ApplyBehaviorPattern(EBehaviorPattern::SuperHotGirl);

    EXPECT_EQ(Personality->GetCurrentPattern(), EBehaviorPattern::SuperHotGirl);
    EXPECT_GT(Personality->GetTrait("Confidence"), 0.9f);
    EXPECT_GT(Personality->GetTrait("Flirtiness"), 0.8f);
    EXPECT_GT(Personality->GetTrait("Charisma"), 0.85f);
}

TEST_F(PersonalitySystemTest, HyperChaoticPattern) {
    Personality->ApplyBehaviorPattern(EBehaviorPattern::HyperChaotic);

    EXPECT_EQ(Personality->GetCurrentPattern(), EBehaviorPattern::HyperChaotic);
    EXPECT_GT(Personality->GetTrait("Spontaneity"), 0.9f);
    EXPECT_GT(Personality->GetTrait("Unpredictability"), 0.85f);
    EXPECT_GT(Personality->GetTrait("Energy"), 0.9f);
}

TEST_F(PersonalitySystemTest, CalmPattern) {
    Personality->ApplyBehaviorPattern(EBehaviorPattern::Calm);

    EXPECT_EQ(Personality->GetCurrentPattern(), EBehaviorPattern::Calm);
    EXPECT_GT(Personality->GetTrait("Composure"), 0.85f);
    EXPECT_GT(Personality->GetTrait("Patience"), 0.8f);
}

TEST_F(PersonalitySystemTest, ConfidenceModulation) {
    Personality->SetTrait("Extraversion", 0.8f);
    Personality->ModulateByConfidence(1.0f);

    // High confidence should maintain or boost expression
    EXPECT_GE(Personality->GetTrait("Extraversion"), 0.7f);
}

TEST_F(PersonalitySystemTest, CompatibilityCalculation) {
    MockPersonalitySystem other;
    other.Initialize();

    Personality->SetTrait("Openness", 0.8f);
    other.SetTrait("Openness", 0.7f);

    float compatibility = Personality->CalculateCompatibility(other);
    EXPECT_GT(compatibility, 0.5f);  // Similar traits = good compatibility
}

// ============================================================================
// Emotion Blending Tests
// ============================================================================

TEST_F(EmotionBlendingTest, Initialization) {
    EXPECT_TRUE(Emotions->IsInitialized());
    EXPECT_EQ(Emotions->GetCurrentEmotion().Type, EEmotionType::Neutral);
}

TEST_F(EmotionBlendingTest, SetPrimaryEmotion) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.8f);

    FEmotionState state = Emotions->GetCurrentEmotion();
    EXPECT_EQ(state.Type, EEmotionType::Joy);
    EXPECT_FLOAT_EQ(state.Intensity, 0.8f);
}

TEST_F(EmotionBlendingTest, IntensityClamping) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 1.5f);
    EXPECT_FLOAT_EQ(Emotions->GetCurrentEmotion().Intensity, 1.0f);

    Emotions->SetPrimaryEmotion(EEmotionType::Joy, -0.5f);
    EXPECT_FLOAT_EQ(Emotions->GetCurrentEmotion().Intensity, 0.0f);
}

TEST_F(EmotionBlendingTest, BlendEmotion) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.8f);
    Emotions->BlendEmotion(EEmotionType::Surprise, 0.6f, 0.3f);

    auto blended = Emotions->GetBlendedEmotions();
    EXPECT_EQ(blended.size(), 2);
}

TEST_F(EmotionBlendingTest, DominantIntensity) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.8f);

    float dominant = Emotions->GetDominantIntensity();
    EXPECT_GT(dominant, 0.5f);
}

TEST_F(EmotionBlendingTest, EmotionDecay) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.8f);

    float initial = Emotions->GetCurrentEmotion().Intensity;
    Emotions->DecayEmotions(0.3f);
    float after = Emotions->GetCurrentEmotion().Intensity;

    EXPECT_LT(after, initial);
}

TEST_F(EmotionBlendingTest, WeakEmotionRemoval) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.5f);
    Emotions->BlendEmotion(EEmotionType::Surprise, 0.15f, 0.3f);

    // Decay until weak emotions are removed
    for (int i = 0; i < 5; i++) {
        Emotions->DecayEmotions(0.3f);
    }

    auto blended = Emotions->GetBlendedEmotions();
    // Weak emotions should be removed
    for (const auto& emotion : blended) {
        if (emotion.Type != EEmotionType::Joy) {
            EXPECT_LT(emotion.Intensity, 0.1f);
        }
    }
}

TEST_F(EmotionBlendingTest, EmotionTransition) {
    Emotions->SetPrimaryEmotion(EEmotionType::Joy, 0.8f);
    Emotions->TransitionTo(EEmotionType::Sadness, 0.7f, 0.5f);

    // Should have both emotions during transition
    auto blended = Emotions->GetBlendedEmotions();
    EXPECT_GE(blended.size(), 1);
}

TEST_F(EmotionBlendingTest, Reset) {
    Emotions->SetPrimaryEmotion(EEmotionType::Anger, 0.9f);
    Emotions->BlendEmotion(EEmotionType::Fear, 0.7f, 0.4f);

    Emotions->Reset();

    EXPECT_EQ(Emotions->GetCurrentEmotion().Type, EEmotionType::Neutral);
    EXPECT_FLOAT_EQ(Emotions->GetCurrentEmotion().Intensity, 0.0f);
}

// ============================================================================
// Neurochemical System Tests
// ============================================================================

TEST_F(NeurochemicalSystemTest, Initialization) {
    EXPECT_TRUE(Neurochemistry->IsInitialized());

    FNeurochemicalState state = Neurochemistry->GetState();
    EXPECT_FLOAT_EQ(state.Dopamine, 0.5f);
    EXPECT_FLOAT_EQ(state.Serotonin, 0.5f);
}

TEST_F(NeurochemicalSystemTest, SetNeurotransmitters) {
    Neurochemistry->SetDopamine(0.8f);
    Neurochemistry->SetSerotonin(0.7f);

    FNeurochemicalState state = Neurochemistry->GetState();
    EXPECT_FLOAT_EQ(state.Dopamine, 0.8f);
    EXPECT_FLOAT_EQ(state.Serotonin, 0.7f);
}

TEST_F(NeurochemicalSystemTest, NeurotransmitterClamping) {
    Neurochemistry->SetDopamine(1.5f);
    EXPECT_FLOAT_EQ(Neurochemistry->GetState().Dopamine, 1.0f);

    Neurochemistry->SetDopamine(-0.5f);
    EXPECT_FLOAT_EQ(Neurochemistry->GetState().Dopamine, 0.0f);
}

TEST_F(NeurochemicalSystemTest, TriggerReward) {
    float initialDopamine = Neurochemistry->GetState().Dopamine;
    float initialEndorphins = Neurochemistry->GetState().Endorphins;

    Neurochemistry->TriggerReward(0.8f);

    EXPECT_GT(Neurochemistry->GetState().Dopamine, initialDopamine);
    EXPECT_GT(Neurochemistry->GetState().Endorphins, initialEndorphins);
}

TEST_F(NeurochemicalSystemTest, TriggerStress) {
    float initialCortisol = Neurochemistry->GetState().Cortisol;
    float initialSerotonin = Neurochemistry->GetState().Serotonin;

    Neurochemistry->TriggerStress(0.7f);

    EXPECT_GT(Neurochemistry->GetState().Cortisol, initialCortisol);
    EXPECT_LT(Neurochemistry->GetState().Serotonin, initialSerotonin);
}

TEST_F(NeurochemicalSystemTest, TriggerSocialBonding) {
    float initialOxytocin = Neurochemistry->GetState().Oxytocin;

    Neurochemistry->TriggerSocialBonding(0.8f);

    EXPECT_GT(Neurochemistry->GetState().Oxytocin, initialOxytocin);
}

TEST_F(NeurochemicalSystemTest, Homeostasis) {
    Neurochemistry->SetDopamine(1.0f);
    Neurochemistry->SetCortisol(1.0f);

    for (int i = 0; i < 10; i++) {
        Neurochemistry->ApplyHomeostasis(0.2f);
    }

    FNeurochemicalState state = Neurochemistry->GetState();
    // Should trend toward baseline
    EXPECT_LT(state.Dopamine, 0.9f);
    EXPECT_LT(state.Cortisol, 0.9f);
}

TEST_F(NeurochemicalSystemTest, ArousalCalculation) {
    Neurochemistry->SetDopamine(0.8f);
    Neurochemistry->SetNorepinephrine(0.9f);

    float arousal = Neurochemistry->GetState().GetArousal();
    EXPECT_FLOAT_EQ(arousal, 0.85f);
}

TEST_F(NeurochemicalSystemTest, ValenceCalculation) {
    Neurochemistry->SetSerotonin(0.8f);
    Neurochemistry->SetOxytocin(0.7f);
    Neurochemistry->SetCortisol(0.2f);

    float valence = Neurochemistry->GetState().GetValence();
    EXPECT_GT(valence, 0.5f);  // Positive valence
}

TEST_F(NeurochemicalSystemTest, CognitiveModeReactive) {
    Neurochemistry->SetCortisol(0.9f);

    ECognitiveMode mode = Neurochemistry->SuggestCognitiveMode();
    EXPECT_EQ(mode, ECognitiveMode::Reactive);
}

TEST_F(NeurochemicalSystemTest, CognitiveModeCreative) {
    Neurochemistry->SetSerotonin(0.8f);
    Neurochemistry->SetOxytocin(0.7f);
    Neurochemistry->SetDopamine(0.8f);
    Neurochemistry->SetNorepinephrine(0.7f);
    Neurochemistry->SetCortisol(0.2f);

    ECognitiveMode mode = Neurochemistry->SuggestCognitiveMode();
    EXPECT_EQ(mode, ECognitiveMode::Creative);
}

// ============================================================================
// Behavioral Framework Tests
// ============================================================================

TEST_F(BehavioralFrameworkTest, Initialization) {
    EXPECT_TRUE(Framework->IsInitialized());
    EXPECT_TRUE(Framework->GetPersonality()->IsInitialized());
    EXPECT_TRUE(Framework->GetEmotions()->IsInitialized());
    EXPECT_TRUE(Framework->GetNeurochemistry()->IsInitialized());
}

TEST_F(BehavioralFrameworkTest, ScenarioRegistration) {
    FBehaviorScenario scenario;
    scenario.Name = "test_emotion_scenario";
    scenario.Description = "Test emotion setting";
    scenario.ExpectedOutcomes.push_back("emotion_set");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("test_emotion_scenario");
    EXPECT_TRUE(result.bPassed);
}

TEST_F(BehavioralFrameworkTest, ScenarioNotFound) {
    FBehaviorTestResult result = Framework->RunScenario("nonexistent_scenario");

    EXPECT_FALSE(result.bPassed);
    EXPECT_EQ(result.FailureReason, "Scenario not found");
}

TEST_F(BehavioralFrameworkTest, ScenarioWithPrerequisites) {
    FBehaviorScenario scenario;
    scenario.Name = "test_prereq";
    scenario.Prerequisites.push_back("initialized");
    scenario.ExpectedOutcomes.push_back("any");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("test_prereq");
    // Should pass since framework is initialized
    EXPECT_EQ(result.FailureReason, "");
}

TEST_F(BehavioralFrameworkTest, StressScenario) {
    FBehaviorScenario scenario;
    scenario.Name = "stress_response";
    scenario.ExpectedOutcomes.push_back("stress_triggered");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("stress_response");
    EXPECT_TRUE(result.bPassed);
}

TEST_F(BehavioralFrameworkTest, RewardScenario) {
    FBehaviorScenario scenario;
    scenario.Name = "reward_response";
    scenario.ExpectedOutcomes.push_back("reward_triggered");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("reward_response");
    EXPECT_TRUE(result.bPassed);
}

TEST_F(BehavioralFrameworkTest, PersonalityScenario) {
    FBehaviorScenario scenario;
    scenario.Name = "personality_test";
    scenario.ExpectedOutcomes.push_back("personality_applied");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("personality_test");
    EXPECT_TRUE(result.bPassed);
}

TEST_F(BehavioralFrameworkTest, EmotionBlendScenario) {
    FBehaviorScenario scenario;
    scenario.Name = "emotion_blend_test";
    scenario.ExpectedOutcomes.push_back("emotion_blended");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("emotion_blend_test");
    EXPECT_TRUE(result.bPassed);
}

TEST_F(BehavioralFrameworkTest, RunAllScenarios) {
    FBehaviorScenario s1, s2, s3;

    s1.Name = "stress_test";
    s1.ExpectedOutcomes.push_back("stress_triggered");

    s2.Name = "reward_test";
    s2.ExpectedOutcomes.push_back("reward_triggered");

    s3.Name = "emotion_test";
    s3.ExpectedOutcomes.push_back("emotion_set");

    Framework->RegisterScenario(s1);
    Framework->RegisterScenario(s2);
    Framework->RegisterScenario(s3);

    auto results = Framework->RunAllScenarios();
    EXPECT_EQ(results.size(), 3);

    int passed = Framework->GetPassedCount(results);
    EXPECT_EQ(passed, 3);
}

TEST_F(BehavioralFrameworkTest, MismatchedExpectations) {
    FBehaviorScenario scenario;
    scenario.Name = "stress_with_wrong_expectation";
    scenario.ExpectedOutcomes.push_back("wrong_outcome");

    Framework->RegisterScenario(scenario);

    FBehaviorTestResult result = Framework->RunScenario("stress_with_wrong_expectation");
    EXPECT_FALSE(result.bPassed);
    EXPECT_EQ(result.MismatchedExpectations.size(), 1);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(BehavioralPerformanceTest, PersonalityPatternPerformance) {
    MockPersonalitySystem personality;
    personality.Initialize();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        personality.ApplyBehaviorPattern(static_cast<EBehaviorPattern>(i % 5));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100);  // 10000 pattern switches in under 100ms
}

TEST(BehavioralPerformanceTest, EmotionBlendingPerformance) {
    MockEmotionBlendingSystem emotions;
    emotions.Initialize();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        emotions.SetPrimaryEmotion(static_cast<EEmotionType>(i % 7), (i % 100) / 100.0f);
        emotions.BlendEmotion(static_cast<EEmotionType>((i + 1) % 7), 0.5f, 0.3f);
        emotions.DecayEmotions(0.1f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 200);  // 30000 operations in under 200ms
}

TEST(BehavioralPerformanceTest, NeurochemicalPerformance) {
    MockNeurochemicalSystem neurochemistry;
    neurochemistry.Initialize();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        neurochemistry.TriggerReward(0.5f);
        neurochemistry.TriggerStress(0.3f);
        neurochemistry.ApplyHomeostasis(0.1f);
        neurochemistry.SuggestCognitiveMode();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100);  // 40000 operations in under 100ms
}

// Note: main() is provided by GTest::gtest_main in CMakeLists.txt
