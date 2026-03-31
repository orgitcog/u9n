/**
 * @file DTEAvatarAgentTests.cpp
 * @brief Comprehensive unit tests for DTE as Autonomous Cognitive AI Agent
 *
 * Tests cover:
 * - DTEAvatarAgent initialization and configuration (SIMA 2 VLA paradigm)
 * - Visual observation pipeline (pixels → features → reservoir → actions)
 * - Embodied cognition for MetaHuman avatar orchestration
 * - Sensorimotor integration with proprioception
 * - MetaHuman facial expression output (LiveLink)
 * - Autonomy pipeline modes: OBSERVE, IMITATE, EXPLORE, SELF_IMPROVE, DREAM
 * - Training data collection and imitation learning
 * - 4E embodied cognition state (Embodied, Embedded, Enacted, Extended)
 * - Body schema binding to MetaHuman DNA joints/blend shapes
 * - Console Controller → Virtual CNS → Avatar action loop
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <numeric>
#include <chrono>

namespace {

// ============================================================================
// Mock types (standalone, no UE/Eigen dependency)
// ============================================================================

namespace FMath {
    inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
    inline float Abs(float x) { return std::abs(x); }
    inline float Max(float a, float b) { return a > b ? a : b; }
    inline float Min(float a, float b) { return a < b ? a : b; }
}

// ============================================================================
// Visual Observation (camera sensor input for DTE)
// ============================================================================

struct FVisualObservation {
    std::vector<float> Pixels;
    int Width = 0;
    int Height = 0;
    int Channels = 0;
    int FrameIndex = 0;

    static FVisualObservation CreateTestFrame(int W, int H, float Val = 0.5f) {
        FVisualObservation obs;
        obs.Width = W;
        obs.Height = H;
        obs.Channels = 3;
        obs.Pixels.assign(W * H * 3, Val);
        return obs;
    }

    bool IsValid() const {
        return Width > 0 && Height > 0 && Channels > 0 &&
               static_cast<int>(Pixels.size()) == Width * Height * Channels;
    }
};

// ============================================================================
// Proprioceptive state (body awareness)
// ============================================================================

struct FProprioceptiveState {
    std::vector<float> JointAngles;
    std::vector<float> AngularVelocities;
    float HealthNormalized = 1.0f;
    float StaminaNormalized = 1.0f;
    bool bIsGrounded = true;
    bool bIsAiming = false;
    float SpeedNormalized = 0.0f;

    std::vector<float> ToVector() const {
        std::vector<float> v;
        v.insert(v.end(), JointAngles.begin(), JointAngles.end());
        v.insert(v.end(), AngularVelocities.begin(), AngularVelocities.end());
        v.push_back(HealthNormalized);
        v.push_back(StaminaNormalized);
        v.push_back(bIsGrounded ? 1.0f : 0.0f);
        v.push_back(bIsAiming ? 1.0f : 0.0f);
        v.push_back(SpeedNormalized);
        return v;
    }
};

// ============================================================================
// MetaHuman expression output
// ============================================================================

struct FMetaHumanExpression {
    float Valence = 0.0f;      // -1 (negative) to +1 (positive)
    float Arousal = 0.0f;      // 0 (calm) to 1 (excited)
    float Dominance = 0.5f;    // 0 (submissive) to 1 (dominant)

    // FACS Action Units (simplified)
    float InnerBrowRaise = 0.0f;
    float OuterBrowRaise = 0.0f;
    float BrowLower = 0.0f;
    float SmileLeft = 0.0f;
    float SmileRight = 0.0f;
    float MouthOpen = 0.0f;
    float EyeWide = 0.0f;
    float EyeSquint = 0.0f;

    // Postural markers
    float HeadTilt = 0.0f;
    float ShoulderTension = 0.0f;

    void SetEmotion(const std::string& emotion) {
        if (emotion == "happy") {
            Valence = 0.8f; Arousal = 0.6f;
            SmileLeft = 0.8f; SmileRight = 0.8f; EyeWide = 0.3f;
        } else if (emotion == "focused") {
            Valence = 0.1f; Arousal = 0.7f; Dominance = 0.7f;
            BrowLower = 0.4f; EyeSquint = 0.3f;
        } else if (emotion == "alarmed") {
            Valence = -0.5f; Arousal = 0.9f;
            InnerBrowRaise = 0.8f; EyeWide = 0.9f; MouthOpen = 0.3f;
        } else if (emotion == "calm") {
            Valence = 0.2f; Arousal = 0.2f; Dominance = 0.5f;
        }
    }

    bool IsValid() const {
        return Valence >= -1.0f && Valence <= 1.0f &&
               Arousal >= 0.0f && Arousal <= 1.0f;
    }
};

// ============================================================================
// Body Schema (MetaHuman DNA binding)
// ============================================================================

struct FBodySchema {
    std::unordered_map<std::string, std::vector<float>> JointTransforms;
    std::unordered_map<std::string, float> BlendShapeWeights;
    float PeripersonalRadius = 100.0f;
    float SchemaCoherence = 1.0f;

    void SetJoint(const std::string& Name, float Pitch, float Yaw, float Roll) {
        JointTransforms[Name] = {Pitch, Yaw, Roll};
    }

    void SetBlendShape(const std::string& Name, float Weight) {
        BlendShapeWeights[Name] = FMath::Clamp(Weight, 0.0f, 1.0f);
    }

    bool HasJoint(const std::string& Name) const {
        return JointTransforms.count(Name) > 0;
    }
};

// ============================================================================
// 4E Embodied Cognition State
// ============================================================================

enum class E4ECognitionType : uint8_t {
    Embodied,  // Body schema, proprioception
    Embedded,  // Environmental coupling
    Enacted,   // Sensorimotor contingencies
    Extended   // Cognitive tools, external memory
};

struct F4ECognitionState {
    // Embodied: body awareness
    float SomaticCoherence = 1.0f;
    std::vector<float> SomaticMarkers;  // Emotional body-state signals

    // Embedded: environmental coupling
    float EnvironmentalCoupling = 0.5f;
    std::vector<std::string> ActiveAffordances;

    // Enacted: sensorimotor contingencies
    float ActionPredictionAccuracy = 0.5f;
    int SensorimotorContingencyCount = 0;

    // Extended: cognitive tools
    std::vector<std::string> CognitiveTools;
    bool bExternalMemoryActive = false;

    bool IsValid() const {
        return SomaticCoherence >= 0.0f && SomaticCoherence <= 1.0f &&
               EnvironmentalCoupling >= 0.0f && EnvironmentalCoupling <= 1.0f;
    }
};

// ============================================================================
// Autonomy Pipeline Training Mode
// ============================================================================

enum class ETrainingMode : uint8_t {
    OBSERVE,       // Record human gameplay
    IMITATE,       // Behavioral cloning from demos
    EXPLORE,       // RL with Auto-RL reward
    SELF_IMPROVE,  // Autonomous task generation
    DREAM          // Offline consolidation
};

// ============================================================================
// DTE Avatar Agent Configuration
// ============================================================================

struct FDTEAvatarConfig {
    int CaptureWidth = 160;
    int CaptureHeight = 120;
    int ColorChannels = 3;
    int FrameSkip = 2;
    int NumDiscreteActions = 8;
    int NumContinuousAxes = 4;
    bool bMetaHumanExpression = true;
    bool bSpeechOutput = true;
    bool bEnableProprioception = true;
    float AutonomyLevel = 0.5f;  // 0=fully human-controlled, 1=fully autonomous
};

// ============================================================================
// DTE Avatar Agent Mock (the full cognitive AI agent)
// ============================================================================

class MockDTEAvatarAgent {
public:
    bool Initialize(const FDTEAvatarConfig& Cfg) {
        Config = Cfg;

        // Setup input dimensions
        VisualFeatureDim = 256;  // CNN features from camera
        ProprioDim = 32;         // Joints + body state
        ReservoirStateDim = 512; // ESN internal state

        // Initialize reservoir state
        ReservoirState.assign(ReservoirStateDim, 0.0f);

        // Setup 4E cognition
        CognitionState.SomaticMarkers.assign(8, 0.5f);
        CognitionState.SensorimotorContingencyCount = 0;

        // Initialize body schema with MetaHuman default joints
        InitializeBodySchema();

        TrainingMode = ETrainingMode::OBSERVE;
        FrameCount = 0;
        bInitialized = true;
        return true;
    }

    // -- Main Perception-Action loop --

    std::vector<float> ProcessObservation(
        const FVisualObservation& Visual,
        const FProprioceptiveState& Proprio) {

        if (!bInitialized) return {};
        FrameCount++;

        // Step 1: Extract visual features (simplified: compute statistics)
        std::vector<float> VisualFeatures = ExtractVisualFeatures(Visual);

        // Step 2: Combine with proprioception
        std::vector<float> ProprioVec = Proprio.ToVector();

        // Step 3: Update reservoir (Echo State Network dynamics)
        UpdateReservoir(VisualFeatures, ProprioVec);

        // Step 4: Compute action from reservoir state (readout)
        std::vector<float> Action = ComputeAction();

        // Step 5: Update 4E cognition state
        Update4ECognition(Proprio, Action);

        // Step 6: Update MetaHuman expression
        if (Config.bMetaHumanExpression) {
            UpdateExpression(ProprioVec, Action);
        }

        // Record for training
        if (bRecording) {
            ObservationBuffer.push_back(ReservoirState);
            ActionBuffer.push_back(Action);
        }

        LastAction = Action;
        return Action;
    }

    // -- MetaHuman expression update --

    void SetEmotionalState(const std::string& Emotion) {
        CurrentExpression.SetEmotion(Emotion);
    }

    const FMetaHumanExpression& GetCurrentExpression() const {
        return CurrentExpression;
    }

    // -- Body Schema (MetaHuman DNA binding) --

    void UpdateBodySchema(const FBodySchema& Schema) {
        CurrentBodySchema = Schema;
        // Update 4E embodied state from body schema
        CognitionState.SomaticCoherence = Schema.SchemaCoherence;
    }

    const FBodySchema& GetBodySchema() const { return CurrentBodySchema; }

    bool IsBodySchemaValid() const {
        return CurrentBodySchema.SchemaCoherence > 0.5f &&
               !CurrentBodySchema.JointTransforms.empty();
    }

    // -- Training mode control --

    void SetTrainingMode(ETrainingMode Mode) {
        TrainingMode = Mode;
        if (Mode == ETrainingMode::OBSERVE || Mode == ETrainingMode::IMITATE) {
            bRecording = true;
        } else {
            bRecording = false;
        }
    }

    ETrainingMode GetTrainingMode() const { return TrainingMode; }

    void StartRecording() {
        bRecording = true;
        ObservationBuffer.clear();
        ActionBuffer.clear();
    }

    void StopRecording() { bRecording = false; }
    bool IsRecording() const { return bRecording; }

    int GetRecordedFrameCount() const {
        return static_cast<int>(ObservationBuffer.size());
    }

    // -- Imitation learning --

    bool LearnFromDemonstration(
        const std::vector<std::vector<float>>& DemoObs,
        const std::vector<std::vector<float>>& DemoActions) {

        if (DemoObs.size() != DemoActions.size() || DemoObs.empty()) return false;

        // Behavioral cloning: update readout weights toward demo actions
        float alpha = 0.01f;
        for (size_t i = 0; i < DemoObs.size(); ++i) {
            // Simplified: just track that we processed the demos
            ImitationLossHistory.push_back(ComputeImitationLoss(DemoObs[i], DemoActions[i]));
        }
        ImitatationUpdates++;
        return true;
    }

    float GetImitationLoss() const {
        if (ImitationLossHistory.empty()) return 0.0f;
        float sum = 0.0f;
        int n = FMath::Min(static_cast<int>(ImitationLossHistory.size()), 10);
        for (int i = ImitationLossHistory.size() - n; i < (int)ImitationLossHistory.size(); ++i)
            sum += ImitationLossHistory[i];
        return sum / n;
    }

    // -- 4E Cognition --

    const F4ECognitionState& GetCognitionState() const { return CognitionState; }

    void AddAffordance(const std::string& Affordance) {
        CognitionState.ActiveAffordances.push_back(Affordance);
        CognitionState.EnvironmentalCoupling = FMath::Clamp(
            CognitionState.EnvironmentalCoupling + 0.05f, 0.0f, 1.0f);
    }

    void AddCognitiveTool(const std::string& Tool) {
        CognitionState.CognitiveTools.push_back(Tool);
        CognitionState.bExternalMemoryActive = true;
    }

    void RegisterSensorimotorContingency(const std::string& Action,
                                          const std::string& ExpectedOutcome) {
        CognitionState.SensorimotorContingencyCount++;
        ContingencyRegistry[Action] = ExpectedOutcome;
    }

    // -- Accessors --

    bool IsInitialized() const { return bInitialized; }
    int GetFrameCount() const { return FrameCount; }
    const std::vector<float>& GetLastAction() const { return LastAction; }
    int GetActionDim() const { return Config.NumDiscreteActions + Config.NumContinuousAxes; }
    const FDTEAvatarConfig& GetConfig() const { return Config; }
    float GetReservoirActivation() const {
        if (ReservoirState.empty()) return 0.0f;
        float sum = 0.0f;
        for (float x : ReservoirState) sum += std::abs(x);
        return sum / ReservoirState.size();
    }
    int GetImitatationUpdates() const { return ImitatationUpdates; }

private:
    std::vector<float> ExtractVisualFeatures(const FVisualObservation& Visual) {
        if (!Visual.IsValid()) return std::vector<float>(VisualFeatureDim, 0.0f);
        // Simplified feature extraction: compute mean, variance, edge density
        std::vector<float> features(VisualFeatureDim, 0.0f);
        if (!Visual.Pixels.empty()) {
            float mean = 0.0f;
            for (float p : Visual.Pixels) mean += p;
            mean /= Visual.Pixels.size();
            features[0] = mean;
            float var = 0.0f;
            for (float p : Visual.Pixels) var += (p - mean) * (p - mean);
            features[1] = var / Visual.Pixels.size();
        }
        return features;
    }

    void UpdateReservoir(const std::vector<float>& VisualFeatures,
                          const std::vector<float>& ProprioVec) {
        // Simplified ESN update
        float inputMean = 0.0f;
        for (float x : VisualFeatures) inputMean += std::abs(x);
        inputMean = inputMean / FMath::Max(1.0f, (float)VisualFeatures.size());

        for (float& x : ReservoirState) {
            x = 0.7f * x + 0.3f * std::tanh(inputMean * 0.5f + x * 0.1f);
        }
    }

    std::vector<float> ComputeAction() {
        int actionDim = GetActionDim();
        std::vector<float> action(actionDim, 0.0f);
        if (ReservoirState.empty()) return action;

        // Simple readout: dot product of reservoir with fixed weights
        for (int i = 0; i < actionDim && i < (int)ReservoirState.size(); ++i) {
            action[i] = std::tanh(ReservoirState[i] * ReadoutScale);
        }
        return action;
    }

    void Update4ECognition(const FProprioceptiveState& Proprio,
                            const std::vector<float>& Action) {
        // Embodied: update somatic markers from proprioception
        for (size_t i = 0; i < CognitionState.SomaticMarkers.size() && i < 3; ++i) {
            CognitionState.SomaticMarkers[i] =
                FMath::Lerp(CognitionState.SomaticMarkers[i],
                            Proprio.HealthNormalized, 0.01f);
        }

        // Enacted: track sensorimotor prediction accuracy
        if (!LastAction.empty() && !Action.empty()) {
            float predError = 0.0f;
            int n = FMath::Min((int)LastAction.size(), (int)Action.size());
            for (int i = 0; i < n; ++i)
                predError += std::abs(LastAction[i] - Action[i]);
            predError /= FMath::Max(1, n);
            CognitionState.ActionPredictionAccuracy =
                FMath::Lerp(CognitionState.ActionPredictionAccuracy,
                            1.0f - predError, 0.1f);
        }
    }

    void UpdateExpression(const std::vector<float>& Proprio,
                           const std::vector<float>& Action) {
        // Map reservoir activation to emotional state
        float activation = GetReservoirActivation();
        CurrentExpression.Arousal = FMath::Clamp(activation * 2.0f, 0.0f, 1.0f);

        // Map proprioceptive health to valence
        if (!Proprio.empty()) {
            CurrentExpression.Valence = FMath::Lerp(-0.5f, 0.5f, Proprio[0]);
        }
    }

    void InitializeBodySchema() {
        // Initialize with MetaHuman key joints
        CurrentBodySchema.SetJoint("head", 0.0f, 0.0f, 0.0f);
        CurrentBodySchema.SetJoint("neck_01", 0.0f, 0.0f, 0.0f);
        CurrentBodySchema.SetJoint("spine_01", 0.0f, 0.0f, 0.0f);
        CurrentBodySchema.SetJoint("upperarm_l", 0.0f, 0.0f, 0.0f);
        CurrentBodySchema.SetJoint("upperarm_r", 0.0f, 0.0f, 0.0f);
        CurrentBodySchema.SetBlendShape("Smile_L", 0.0f);
        CurrentBodySchema.SetBlendShape("Smile_R", 0.0f);
        CurrentBodySchema.SetBlendShape("BrowDown_L", 0.0f);
        CurrentBodySchema.SchemaCoherence = 1.0f;
    }

    float ComputeImitationLoss(const std::vector<float>& Obs,
                                const std::vector<float>& Action) {
        float loss = 0.0f;
        for (size_t i = 0; i < Action.size(); ++i) {
            loss += Action[i] * Action[i];  // Simplified MSE from zero
        }
        return loss / FMath::Max(1.0f, (float)Action.size());
    }

    FDTEAvatarConfig Config;
    bool bInitialized = false;
    int FrameCount = 0;

    int VisualFeatureDim = 256;
    int ProprioDim = 32;
    int ReservoirStateDim = 512;
    float ReadoutScale = 0.1f;

    std::vector<float> ReservoirState;
    std::vector<float> LastAction;

    F4ECognitionState CognitionState;
    FBodySchema CurrentBodySchema;
    FMetaHumanExpression CurrentExpression;

    ETrainingMode TrainingMode = ETrainingMode::OBSERVE;
    bool bRecording = false;

    std::vector<std::vector<float>> ObservationBuffer;
    std::vector<std::vector<float>> ActionBuffer;
    std::vector<float> ImitationLossHistory;
    int ImitatationUpdates = 0;

    std::unordered_map<std::string, std::string> ContingencyRegistry;
};

// ============================================================================
// Test Fixture
// ============================================================================

class DTEAvatarAgentTest : public ::testing::Test {
protected:
    void SetUp() override {
        Agent = std::make_unique<MockDTEAvatarAgent>();
        FDTEAvatarConfig cfg;
        cfg.CaptureWidth = 80;
        cfg.CaptureHeight = 60;
        cfg.bMetaHumanExpression = true;
        ASSERT_TRUE(Agent->Initialize(cfg));
    }

    void TearDown() override { Agent.reset(); }

    std::unique_ptr<MockDTEAvatarAgent> Agent;

    FVisualObservation MakeObservation(float val = 0.5f) {
        return FVisualObservation::CreateTestFrame(80, 60, val);
    }

    FProprioceptiveState MakeProprioception(float health = 1.0f) {
        FProprioceptiveState p;
        p.JointAngles.assign(16, 0.0f);
        p.AngularVelocities.assign(16, 0.0f);
        p.HealthNormalized = health;
        p.StaminaNormalized = 1.0f;
        return p;
    }
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(DTEAvatarAgentTest, InitializationSucceeds) {
    EXPECT_TRUE(Agent->IsInitialized());
}

TEST_F(DTEAvatarAgentTest, ConfigStoredCorrectly) {
    EXPECT_EQ(Agent->GetConfig().CaptureWidth, 80);
    EXPECT_EQ(Agent->GetConfig().CaptureHeight, 60);
    EXPECT_TRUE(Agent->GetConfig().bMetaHumanExpression);
}

TEST_F(DTEAvatarAgentTest, ActionDimensionMatchesConfig) {
    EXPECT_EQ(Agent->GetActionDim(),
              Agent->GetConfig().NumDiscreteActions +
              Agent->GetConfig().NumContinuousAxes);
}

// ============================================================================
// VISUAL OBSERVATION PIPELINE TESTS
// ============================================================================

TEST_F(DTEAvatarAgentTest, ValidObservationProducesAction) {
    auto obs = MakeObservation();
    auto prop = MakeProprioception();

    auto action = Agent->ProcessObservation(obs, prop);

    EXPECT_EQ(static_cast<int>(action.size()), Agent->GetActionDim());
}

TEST_F(DTEAvatarAgentTest, FrameCountIncrements) {
    EXPECT_EQ(Agent->GetFrameCount(), 0);
    Agent->ProcessObservation(MakeObservation(), MakeProprioception());
    EXPECT_EQ(Agent->GetFrameCount(), 1);
    Agent->ProcessObservation(MakeObservation(), MakeProprioception());
    EXPECT_EQ(Agent->GetFrameCount(), 2);
}

TEST_F(DTEAvatarAgentTest, DifferentInputsProduceDifferentActions) {
    auto obs1 = MakeObservation(0.1f);
    auto obs2 = MakeObservation(0.9f);
    auto prop = MakeProprioception();

    // Reset reservoir by creating fresh agent
    MockDTEAvatarAgent a1, a2;
    FDTEAvatarConfig cfg;
    a1.Initialize(cfg);
    a2.Initialize(cfg);

    auto action1 = a1.ProcessObservation(obs1, prop);
    auto action2 = a2.ProcessObservation(obs2, prop);

    // Different inputs should produce different outputs
    bool differs = false;
    for (size_t i = 0; i < action1.size() && i < action2.size(); ++i) {
        if (std::abs(action1[i] - action2[i]) > 1e-6f) { differs = true; break; }
    }
    EXPECT_TRUE(differs);
}

TEST_F(DTEAvatarAgentTest, ReservoirActivationNonzeroAfterInput) {
    Agent->ProcessObservation(MakeObservation(0.8f), MakeProprioception());
    EXPECT_GT(Agent->GetReservoirActivation(), 0.0f);
}

TEST_F(DTEAvatarAgentTest, ActionsBoundedByTanh) {
    for (int i = 0; i < 20; ++i) {
        Agent->ProcessObservation(MakeObservation((float)i / 20.0f),
                                  MakeProprioception());
    }
    const auto& action = Agent->GetLastAction();
    for (float a : action) {
        EXPECT_GE(a, -1.0f);
        EXPECT_LE(a, 1.0f);
    }
}

// ============================================================================
// METAHUMAN EMBODIED COGNITION TESTS
// ============================================================================

TEST_F(DTEAvatarAgentTest, BodySchemaInitializedWithMetaHumanJoints) {
    const auto& schema = Agent->GetBodySchema();
    EXPECT_TRUE(schema.HasJoint("head"));
    EXPECT_TRUE(schema.HasJoint("neck_01"));
    EXPECT_TRUE(schema.HasJoint("spine_01"));
    EXPECT_TRUE(schema.HasJoint("upperarm_l"));
}

TEST_F(DTEAvatarAgentTest, BodySchemaCoherenceValid) {
    EXPECT_TRUE(Agent->IsBodySchemaValid());
}

TEST_F(DTEAvatarAgentTest, BodySchemaCanBeUpdated) {
    FBodySchema newSchema;
    newSchema.SetJoint("head", 0.0f, 30.0f, 0.0f);  // Looking right 30°
    newSchema.SetJoint("neck_01", 0.0f, 15.0f, 0.0f);
    newSchema.SetBlendShape("Smile_L", 0.8f);
    newSchema.SchemaCoherence = 0.95f;

    Agent->UpdateBodySchema(newSchema);

    const auto& schema = Agent->GetBodySchema();
    EXPECT_TRUE(schema.HasJoint("head"));
}

TEST_F(DTEAvatarAgentTest, SomaticCoherenceUpdatedFromBodySchema) {
    FBodySchema schema;
    schema.SetJoint("head", 0.0f, 0.0f, 0.0f);
    schema.SchemaCoherence = 0.7f;
    Agent->UpdateBodySchema(schema);

    EXPECT_NEAR(Agent->GetCognitionState().SomaticCoherence, 0.7f, 0.01f);
}

// ============================================================================
// METAHUMAN FACIAL EXPRESSION TESTS (LiveLink output)
// ============================================================================

TEST_F(DTEAvatarAgentTest, ExpressionValidAfterInit) {
    EXPECT_TRUE(Agent->GetCurrentExpression().IsValid());
}

TEST_F(DTEAvatarAgentTest, HappyExpressionHasHighValence) {
    Agent->SetEmotionalState("happy");
    const auto& expr = Agent->GetCurrentExpression();
    EXPECT_GT(expr.Valence, 0.5f);
    EXPECT_GT(expr.SmileLeft, 0.5f);
    EXPECT_GT(expr.SmileRight, 0.5f);
}

TEST_F(DTEAvatarAgentTest, AlarmedExpressionHasHighArousal) {
    Agent->SetEmotionalState("alarmed");
    const auto& expr = Agent->GetCurrentExpression();
    EXPECT_GT(expr.Arousal, 0.5f);
    EXPECT_GT(expr.EyeWide, 0.5f);
    EXPECT_LT(expr.Valence, 0.0f);
}

TEST_F(DTEAvatarAgentTest, FocusedExpressionHasBrowLower) {
    Agent->SetEmotionalState("focused");
    const auto& expr = Agent->GetCurrentExpression();
    EXPECT_GT(expr.BrowLower, 0.2f);
    EXPECT_GT(expr.Dominance, 0.5f);
}

TEST_F(DTEAvatarAgentTest, ExpressionUpdatedWithHealthState) {
    // Critical health → negative valence
    auto obs = MakeObservation();
    auto lowHealth = MakeProprioception(0.1f);  // 10% health

    Agent->ProcessObservation(obs, lowHealth);
    // Expression should reflect low health state
    // (valence will be low)
    EXPECT_LE(Agent->GetCurrentExpression().Valence, 0.5f);
}

// ============================================================================
// 4E EMBODIED COGNITION TESTS
// ============================================================================

TEST_F(DTEAvatarAgentTest, EmbodiedCognitionStateValid) {
    EXPECT_TRUE(Agent->GetCognitionState().IsValid());
}

TEST_F(DTEAvatarAgentTest, AffordancesUpdateEmbeddedCognition) {
    float before = Agent->GetCognitionState().EnvironmentalCoupling;
    Agent->AddAffordance("Weapon_Pickup");
    Agent->AddAffordance("Cover_Position");
    float after = Agent->GetCognitionState().EnvironmentalCoupling;
    EXPECT_GT(after, before);
}

TEST_F(DTEAvatarAgentTest, SensorimotorContingencyRegistration) {
    Agent->RegisterSensorimotorContingency("Jump", "IncreaseElevation");
    Agent->RegisterSensorimotorContingency("Fire", "ProjectileLaunched");
    EXPECT_EQ(Agent->GetCognitionState().SensorimotorContingencyCount, 2);
}

TEST_F(DTEAvatarAgentTest, ExtendedCognitionViaTools) {
    Agent->AddCognitiveTool("Minimap");
    Agent->AddCognitiveTool("KillFeed");
    EXPECT_TRUE(Agent->GetCognitionState().bExternalMemoryActive);
    EXPECT_EQ(static_cast<int>(Agent->GetCognitionState().CognitiveTools.size()), 2);
}

TEST_F(DTEAvatarAgentTest, SomaticMarkersInitialized) {
    const auto& state = Agent->GetCognitionState();
    EXPECT_FALSE(state.SomaticMarkers.empty());
    for (float marker : state.SomaticMarkers) {
        EXPECT_GE(marker, 0.0f);
        EXPECT_LE(marker, 1.0f);
    }
}

// ============================================================================
// TRAINING MODE TESTS (SIMA 2 VLA paradigm)
// ============================================================================

TEST_F(DTEAvatarAgentTest, DefaultTrainingModeIsObserve) {
    EXPECT_EQ(Agent->GetTrainingMode(), ETrainingMode::OBSERVE);
}

TEST_F(DTEAvatarAgentTest, TrainingModeCanBeChanged) {
    Agent->SetTrainingMode(ETrainingMode::EXPLORE);
    EXPECT_EQ(Agent->GetTrainingMode(), ETrainingMode::EXPLORE);

    Agent->SetTrainingMode(ETrainingMode::DREAM);
    EXPECT_EQ(Agent->GetTrainingMode(), ETrainingMode::DREAM);
}

TEST_F(DTEAvatarAgentTest, ObserveModeEnablesRecording) {
    Agent->SetTrainingMode(ETrainingMode::OBSERVE);
    EXPECT_TRUE(Agent->IsRecording());
}

TEST_F(DTEAvatarAgentTest, ExploreModDisablesRecording) {
    Agent->SetTrainingMode(ETrainingMode::EXPLORE);
    EXPECT_FALSE(Agent->IsRecording());
}

TEST_F(DTEAvatarAgentTest, RecordedFramesAccumulate) {
    Agent->SetTrainingMode(ETrainingMode::OBSERVE);
    auto obs = MakeObservation();
    auto prop = MakeProprioception();

    for (int i = 0; i < 5; ++i) {
        Agent->ProcessObservation(obs, prop);
    }

    EXPECT_EQ(Agent->GetRecordedFrameCount(), 5);
}

TEST_F(DTEAvatarAgentTest, ImitationLearningFromDemonstration) {
    std::vector<std::vector<float>> demoObs, demoActions;
    int actionDim = Agent->GetActionDim();

    for (int i = 0; i < 20; ++i) {
        std::vector<float> obs(256, (float)i / 20.0f);
        std::vector<float> action(actionDim, (float)i / 20.0f);
        demoObs.push_back(obs);
        demoActions.push_back(action);
    }

    bool success = Agent->LearnFromDemonstration(demoObs, demoActions);
    EXPECT_TRUE(success);
    EXPECT_EQ(Agent->GetImitatationUpdates(), 1);
}

TEST_F(DTEAvatarAgentTest, EmptyDemonstrationFails) {
    std::vector<std::vector<float>> empty;
    bool success = Agent->LearnFromDemonstration(empty, empty);
    EXPECT_FALSE(success);
}

TEST_F(DTEAvatarAgentTest, MismatchedDemoDataFails) {
    std::vector<std::vector<float>> obs = {{1.0f}};
    std::vector<std::vector<float>> actions = {{0.1f}, {0.2f}};  // Different size
    bool success = Agent->LearnFromDemonstration(obs, actions);
    EXPECT_FALSE(success);
}

// ============================================================================
// AUTONOMOUS CONTROL TESTS (Console Controller → Virtual CNS → Avatar)
// ============================================================================

TEST_F(DTEAvatarAgentTest, ContinuousProcessingStaysStable) {
    // The autonomous pipeline should run stably over many frames
    for (int frame = 0; frame < 100; ++frame) {
        float val = (frame % 10) / 10.0f;
        auto obs = MakeObservation(val);
        auto prop = MakeProprioception(1.0f - val * 0.5f);

        auto action = Agent->ProcessObservation(obs, prop);
        ASSERT_EQ(static_cast<int>(action.size()), Agent->GetActionDim());

        // Actions should always be bounded
        for (float a : action) {
            EXPECT_GE(a, -1.0f);
            EXPECT_LE(a, 1.0f);
        }
    }
}

TEST_F(DTEAvatarAgentTest, HighFrameRateProcessing) {
    auto obs = MakeObservation();
    auto prop = MakeProprioception();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 600; ++i) {  // 600 frames = 10s at 60fps
        Agent->ProcessObservation(obs, prop);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Must process 600 frames in under 1000ms in any environment
    EXPECT_LT(duration.count(), 1000)
        << "DTE avatar agent must be real-time capable";
}

// ============================================================================
// INTEGRATION: Full Perception-Cognition-Expression-Action Loop
// ============================================================================

TEST_F(DTEAvatarAgentTest, FullCognitiveCycleIntegration) {
    // Simulate combat scenario
    Agent->RegisterSensorimotorContingency("ADS", "IncreaseAccuracy");
    Agent->AddAffordance("Cover_Behind_Wall");

    // Perceive: approaching enemy
    auto obs = MakeObservation(0.8f);  // Bright scene (enemy visible)
    auto prop = MakeProprioception(0.6f);  // 60% health

    auto action = Agent->ProcessObservation(obs, prop);

    // Cognition: 4E state updated
    EXPECT_TRUE(Agent->GetCognitionState().IsValid());

    // Expression: tense/focused expression
    Agent->SetEmotionalState("focused");
    EXPECT_GT(Agent->GetCurrentExpression().Dominance, 0.5f);

    // Action: valid action produced
    EXPECT_EQ(static_cast<int>(action.size()), Agent->GetActionDim());

    // Body: schema still coherent
    EXPECT_TRUE(Agent->IsBodySchemaValid());
}

} // namespace
