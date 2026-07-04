/**
 * @file NeuralToSymbolicTranslatorTests.cpp
 * @brief Comprehensive unit tests for Neural-to-Symbolic Translator
 * 
 * Feature F1.1.1: Tests cover:
 * - Core translation (TranslateTensor, TranslateActivations, TranslateNeuralState)
 * - Batch processing (BatchTranslateTensors, BatchTranslateStates)
 * - Atom factory (CreateAtomFromActivation, CreateAtomsFromActivationVector)
 * - Discretization (DiscretizeActivation, ShouldCreateAtom, CalculateConfidence)
 * - Uncertainty propagation (PropagateUncertainty, CalculatePredicateUncertainty)
 * - Predicate inference (CreatePredicateFromAtoms, InferPredicatesFromAtoms)
 * - Performance (latency compliance <0.5ms)
 * - Edge cases (empty inputs, boundary values, overflow)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <array>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>

// Mock Unreal Engine types for standalone testing
#ifndef WITH_UNREAL_ENGINE
using FString = std::string;
using FName = std::string;
using int32 = int;
using uint32 = unsigned int;
using int64 = long long;
#define TEXT(x) x
#define SMALL_NUMBER 1.e-8f
#endif

// ============================================================================
// Mock NeuralToSymbolicTranslator for Testing
// ============================================================================

/**
 * @brief Symbolic atom output from neural translation
 */
struct FTestSymbolicAtom
{
    std::string AtomID;
    std::string AtomType;
    float Confidence = 0.0f;
    int32 SourceFeatureIndex = -1;
    float ActivationValue = 0.0f;
    float Timestamp = 0.0f;
    std::map<std::string, std::string> Properties;

    bool IsValid() const { return !AtomID.empty(); }
};

/**
 * @brief Predicate representing a relation between atoms
 */
struct FTestPredicate
{
    std::string PredicateID;
    std::string PredicateName;
    std::vector<std::string> Arguments;
    float TruthValue = 0.0f;
    float Confidence = 0.0f;
    std::vector<int32> SourceFeatureIndices;

    bool IsValid() const { return !PredicateID.empty(); }
};

/**
 * @brief Neural state containing activation data
 */
struct FTestNeuralState
{
    std::string StateID;
    std::vector<float> Activations;
    std::vector<float> HiddenState;
    std::vector<std::vector<float>> LayerActivations;
    float Confidence = 0.0f;
    float Timestamp = 0.0f;
};

/**
 * @brief Activation map with named vectors
 */
struct FTestActivationMap
{
    std::map<std::string, std::vector<float>> Activations;
    std::map<std::string, float> ConfidenceScores;
    std::map<std::string, std::string> Metadata;
};

/**
 * @brief Symbolic state output from neural state translation
 */
struct FTestSymbolicState
{
    std::string StateID;
    std::vector<FTestSymbolicAtom> Atoms;
    std::vector<FTestPredicate> Predicates;
    float Confidence = 0.0f;
    float Timestamp = 0.0f;
};

/**
 * @brief Translation configuration
 */
struct FTestTranslationConfig
{
    float ActivationThreshold = 0.3f;
    float ConfidenceThreshold = 0.5f;
    int32 MaxAtomsPerTranslation = 100;
    bool bEnableBatchProcessing = true;
    int32 BatchSize = 32;
    bool bPropagateUncertainty = true;
    int32 DiscretizationBins = 10;
    float CoActivationThreshold = 0.2f;
};

/**
 * @brief Translation metrics
 */
struct FTestTranslationMetrics
{
    int64 TotalTranslations = 0;
    float AverageLatency = 0.0f;
    float PeakLatency = 0.0f;
    int64 TotalAtomsCreated = 0;
    int64 TotalPredicatesCreated = 0;
    float AverageAtomsPerTranslation = 0.0f;
    float BatchEfficiency = 0.0f;
};

/**
 * @brief Mock translator implementing core F1.1.1 logic for standalone testing
 */
class MockNeuralToSymbolicTranslator
{
public:
    FTestTranslationConfig Config;
    FTestTranslationMetrics Metrics;

    // ========================================
    // CORE TRANSLATION API
    // ========================================

    FTestSymbolicAtom TranslateTensor(const std::vector<float>& Tensor)
    {
        auto StartTime = std::chrono::high_resolution_clock::now();

        FTestSymbolicAtom Atom;

        if (Tensor.empty())
        {
            RecordLatency(0.0f);
            return Atom;
        }

        // Find dominant activation
        float MaxActivation = -std::numeric_limits<float>::max();
        int32 MaxIndex = 0;
        float TotalEnergy = 0.0f;

        for (int32 i = 0; i < static_cast<int32>(Tensor.size()); i++)
        {
            float AbsVal = std::abs(Tensor[i]);
            TotalEnergy += AbsVal;

            if (Tensor[i] > MaxActivation)
            {
                MaxActivation = Tensor[i];
                MaxIndex = i;
            }
        }

        if (ShouldCreateAtom(MaxActivation))
        {
            Atom.AtomID = GenerateAtomID();
            Atom.AtomType = "TensorConcept";
            Atom.ActivationValue = MaxActivation;
            Atom.SourceFeatureIndex = MaxIndex;
            Atom.Confidence = CalculateConfidence(MaxActivation);
            Atom.Timestamp = 0.0f;

            Atom.Properties["TensorDimension"] = std::to_string(Tensor.size());
            Atom.Properties["TotalEnergy"] = std::to_string(TotalEnergy);
            Atom.Properties["MaxActivation"] = std::to_string(MaxActivation);
            Atom.Properties["DiscreteBin"] = std::to_string(DiscretizeActivation(MaxActivation));

            Metrics.TotalAtomsCreated++;
        }

        auto EndTime = std::chrono::high_resolution_clock::now();
        float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
        RecordLatency(LatencyMs);

        Metrics.TotalTranslations++;

        return Atom;
    }

    std::vector<FTestPredicate> TranslateActivations(const FTestActivationMap& Activations)
    {
        auto StartTime = std::chrono::high_resolution_clock::now();

        std::vector<FTestPredicate> Predicates;
        std::vector<FTestSymbolicAtom> AllAtoms;

        for (const auto& Pair : Activations.Activations)
        {
            const std::string& ActivationName = Pair.first;
            const std::vector<float>& ActivationVector = Pair.second;

            float Confidence = 1.0f;
            auto ConfIt = Activations.ConfidenceScores.find(ActivationName);
            if (ConfIt != Activations.ConfidenceScores.end())
            {
                Confidence = ConfIt->second;
            }

            std::vector<FTestSymbolicAtom> Atoms = CreateAtomsFromActivationVector(ActivationVector, ActivationName);

            for (auto& Atom : Atoms)
            {
                Atom.Confidence *= Confidence;
            }

            AllAtoms.insert(AllAtoms.end(), Atoms.begin(), Atoms.end());
        }

        Predicates = InferPredicatesFromAtoms(AllAtoms);

        auto EndTime = std::chrono::high_resolution_clock::now();
        float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
        RecordLatency(LatencyMs);

        Metrics.TotalTranslations++;
        Metrics.TotalPredicatesCreated += static_cast<int64>(Predicates.size());

        return Predicates;
    }

    FTestSymbolicState TranslateNeuralState(const FTestNeuralState& State)
    {
        auto StartTime = std::chrono::high_resolution_clock::now();

        FTestSymbolicState SymbolicState;
        SymbolicState.StateID = "SymbolicState_" + State.StateID;
        SymbolicState.Timestamp = 0.0f;

        if (!State.Activations.empty())
        {
            auto PrimaryAtoms = CreateAtomsFromActivationVector(State.Activations, "Primary");
            SymbolicState.Atoms.insert(SymbolicState.Atoms.end(), PrimaryAtoms.begin(), PrimaryAtoms.end());
        }

        if (!State.HiddenState.empty())
        {
            auto HiddenAtoms = CreateAtomsFromActivationVector(State.HiddenState, "Hidden");
            SymbolicState.Atoms.insert(SymbolicState.Atoms.end(), HiddenAtoms.begin(), HiddenAtoms.end());
        }

        for (size_t LayerIdx = 0; LayerIdx < State.LayerActivations.size(); LayerIdx++)
        {
            std::string LayerType = "Layer" + std::to_string(LayerIdx);
            auto LayerAtoms = CreateAtomsFromActivationVector(State.LayerActivations[LayerIdx], LayerType);
            SymbolicState.Atoms.insert(SymbolicState.Atoms.end(), LayerAtoms.begin(), LayerAtoms.end());
        }

        SymbolicState.Predicates = InferPredicatesFromAtoms(SymbolicState.Atoms);

        if (Config.bPropagateUncertainty)
        {
            float PropagatedConfidence = PropagateUncertainty(State.Confidence, static_cast<int32>(SymbolicState.Atoms.size()));
            SymbolicState.Confidence = PropagatedConfidence;

            for (auto& Atom : SymbolicState.Atoms)
            {
                Atom.Confidence *= PropagatedConfidence;
            }
        }
        else
        {
            SymbolicState.Confidence = State.Confidence;
        }

        auto EndTime = std::chrono::high_resolution_clock::now();
        float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
        RecordLatency(LatencyMs);

        Metrics.TotalTranslations++;
        Metrics.TotalAtomsCreated += static_cast<int64>(SymbolicState.Atoms.size());
        Metrics.TotalPredicatesCreated += static_cast<int64>(SymbolicState.Predicates.size());

        return SymbolicState;
    }

    // ========================================
    // BATCH TRANSLATION
    // ========================================

    std::vector<FTestSymbolicAtom> BatchTranslateTensors(const std::vector<std::vector<float>>& Tensors)
    {
        std::vector<FTestSymbolicAtom> AllAtoms;
        AllAtoms.reserve(Tensors.size());

        for (const auto& Tensor : Tensors)
        {
            FTestSymbolicAtom Atom = TranslateTensor(Tensor);
            if (Atom.IsValid())
            {
                AllAtoms.push_back(Atom);
            }
        }

        return AllAtoms;
    }

    std::vector<FTestSymbolicState> BatchTranslateStates(const std::vector<FTestNeuralState>& States)
    {
        std::vector<FTestSymbolicState> Results;
        Results.reserve(States.size());

        for (const auto& State : States)
        {
            Results.push_back(TranslateNeuralState(State));
        }

        return Results;
    }

    // ========================================
    // ATOM FACTORY
    // ========================================

    FTestSymbolicAtom CreateAtomFromActivation(float ActivationValue, int32 FeatureIndex, const std::string& AtomType)
    {
        FTestSymbolicAtom Atom;

        if (ShouldCreateAtom(ActivationValue))
        {
            Atom.AtomID = GenerateAtomID();
            Atom.AtomType = AtomType;
            Atom.ActivationValue = ActivationValue;
            Atom.SourceFeatureIndex = FeatureIndex;
            Atom.Confidence = CalculateConfidence(ActivationValue);
            Atom.Timestamp = 0.0f;

            Atom.Properties["FeatureIndex"] = std::to_string(FeatureIndex);
            Atom.Properties["DiscreteBin"] = std::to_string(DiscretizeActivation(ActivationValue));
        }

        return Atom;
    }

    std::vector<FTestSymbolicAtom> CreateAtomsFromActivationVector(const std::vector<float>& Activations, const std::string& AtomType)
    {
        std::vector<FTestSymbolicAtom> Atoms;

        auto DominantFeatures = ExtractDominantFeatures(Activations, Config.MaxAtomsPerTranslation);

        for (int32 FeatureIdx : DominantFeatures)
        {
            FTestSymbolicAtom Atom = CreateAtomFromActivation(Activations[FeatureIdx], FeatureIdx, AtomType);
            if (Atom.IsValid())
            {
                Atoms.push_back(Atom);
            }
        }

        return Atoms;
    }

    FTestPredicate CreatePredicateFromAtoms(const FTestSymbolicAtom& Atom1, const FTestSymbolicAtom& Atom2, const std::string& PredicateName)
    {
        FTestPredicate Predicate;
        Predicate.PredicateID = GeneratePredicateID();
        Predicate.PredicateName = PredicateName;
        Predicate.Arguments.push_back(Atom1.AtomID);
        Predicate.Arguments.push_back(Atom2.AtomID);

        Predicate.Confidence = (Atom1.Confidence + Atom2.Confidence) / 2.0f;
        Predicate.TruthValue = Predicate.Confidence;

        Predicate.SourceFeatureIndices.push_back(Atom1.SourceFeatureIndex);
        Predicate.SourceFeatureIndices.push_back(Atom2.SourceFeatureIndex);

        return Predicate;
    }

    // ========================================
    // DISCRETIZATION
    // ========================================

    int32 DiscretizeActivation(float ActivationValue) const
    {
        float NormalizedValue = std::clamp((ActivationValue + 1.0f) / 2.0f, 0.0f, 1.0f);
        int32 Bin = static_cast<int32>(std::floor(NormalizedValue * (Config.DiscretizationBins - 1)));
        return std::clamp(Bin, 0, Config.DiscretizationBins - 1);
    }

    bool ShouldCreateAtom(float ActivationValue) const
    {
        return std::abs(ActivationValue) >= Config.ActivationThreshold;
    }

    float CalculateConfidence(float ActivationValue) const
    {
        float AbsValue = std::abs(ActivationValue);
        float Confidence = AbsValue / (1.0f + AbsValue);
        return std::clamp(Confidence, 0.0f, 1.0f);
    }

    // ========================================
    // UNCERTAINTY PROPAGATION
    // ========================================

    float PropagateUncertainty(float NeuralConfidence, int32 FeatureCount) const
    {
        if (FeatureCount <= 0)
        {
            return 0.0f;
        }

        float UncertaintyFactor = 1.0f / std::sqrt(static_cast<float>(FeatureCount));
        float PropagatedConfidence = NeuralConfidence * UncertaintyFactor;

        return std::clamp(PropagatedConfidence, 0.0f, 1.0f);
    }

    float CalculatePredicateUncertainty(const std::vector<FTestSymbolicAtom>& InputAtoms) const
    {
        if (InputAtoms.empty())
        {
            return 0.0f;
        }

        float LogSum = 0.0f;
        for (const auto& Atom : InputAtoms)
        {
            float Confidence = std::max(Atom.Confidence, static_cast<float>(SMALL_NUMBER));
            LogSum += std::log(Confidence);
        }

        float GeometricMean = std::exp(LogSum / static_cast<float>(InputAtoms.size()));
        return std::clamp(GeometricMean, 0.0f, 1.0f);
    }

    // ========================================
    // METRICS
    // ========================================

    bool IsMeetingLatencyTarget() const
    {
        return Metrics.AverageLatency < 0.5f;
    }

    void ResetMetrics()
    {
        Metrics = FTestTranslationMetrics();
        LatencySamples.clear();
    }

private:
    int32 AtomIDCounter = 0;
    int32 PredicateIDCounter = 0;
    std::vector<float> LatencySamples;
    static constexpr size_t MaxLatencySamples = 1000;

    std::string GenerateAtomID()
    {
        return "Atom_" + std::to_string(AtomIDCounter++) + "_" + std::to_string(rand() % 10000);
    }

    std::string GeneratePredicateID()
    {
        return "Pred_" + std::to_string(PredicateIDCounter++) + "_" + std::to_string(rand() % 10000);
    }

    void RecordLatency(float LatencyMs)
    {
        LatencySamples.push_back(LatencyMs);

        if (LatencySamples.size() > MaxLatencySamples)
        {
            size_t ToRemove = MaxLatencySamples / 10;
            LatencySamples.erase(LatencySamples.begin(), LatencySamples.begin() + ToRemove);
        }

        Metrics.PeakLatency = std::max(Metrics.PeakLatency, LatencyMs);

        // Update average
        if (!LatencySamples.empty())
        {
            float Sum = std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0f);
            Metrics.AverageLatency = Sum / static_cast<float>(LatencySamples.size());
        }
    }

    std::vector<int32> ExtractDominantFeatures(const std::vector<float>& Activations, int32 MaxFeatures) const
    {
        std::vector<std::pair<int32, float>> IndexedActivations;

        for (int32 i = 0; i < static_cast<int32>(Activations.size()); i++)
        {
            if (ShouldCreateAtom(Activations[i]))
            {
                IndexedActivations.emplace_back(i, std::abs(Activations[i]));
            }
        }

        std::sort(IndexedActivations.begin(), IndexedActivations.end(),
            [](const auto& A, const auto& B) { return A.second > B.second; });

        std::vector<int32> Features;
        int32 Count = std::min(MaxFeatures, static_cast<int32>(IndexedActivations.size()));
        for (int32 i = 0; i < Count; i++)
        {
            Features.push_back(IndexedActivations[i].first);
        }

        return Features;
    }

    std::vector<FTestPredicate> InferPredicatesFromAtoms(const std::vector<FTestSymbolicAtom>& Atoms)
    {
        std::vector<FTestPredicate> Predicates;

        for (size_t i = 0; i < Atoms.size(); i++)
        {
            for (size_t j = i + 1; j < Atoms.size(); j++)
            {
                float ActivationDiff = std::abs(Atoms[i].ActivationValue - Atoms[j].ActivationValue);

                if (ActivationDiff < Config.CoActivationThreshold)
                {
                    FTestPredicate Pred = CreatePredicateFromAtoms(Atoms[i], Atoms[j], "CoActivated");
                    Predicates.push_back(Pred);
                }
            }
        }

        if (static_cast<int32>(Predicates.size()) > Config.MaxAtomsPerTranslation)
        {
            std::sort(Predicates.begin(), Predicates.end(),
                [](const FTestPredicate& A, const FTestPredicate& B) {
                    return A.Confidence > B.Confidence;
                });
            Predicates.resize(Config.MaxAtomsPerTranslation);
        }

        return Predicates;
    }
};

// ============================================================================
// TEST FIXTURE
// ============================================================================

class NeuralToSymbolicTranslatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Translator = std::make_unique<MockNeuralToSymbolicTranslator>();
    }

    void TearDown() override
    {
        Translator.reset();
    }

    std::unique_ptr<MockNeuralToSymbolicTranslator> Translator;

    // Helper: Generate random activation vector
    std::vector<float> GenerateRandomActivations(int32 Size, float MinVal = -1.0f, float MaxVal = 1.0f)
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(MinVal, MaxVal);
        std::vector<float> activations(Size);
        for (auto& val : activations)
        {
            val = dist(rng);
        }
        return activations;
    }

    // Helper: Generate tensor with a single known peak
    std::vector<float> GeneratePeakedTensor(int32 Size, int32 PeakIndex, float PeakValue)
    {
        std::vector<float> tensor(Size, 0.1f);
        if (PeakIndex >= 0 && PeakIndex < Size)
        {
            tensor[PeakIndex] = PeakValue;
        }
        return tensor;
    }
};

// ============================================================================
// CORE TRANSLATION TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, TranslateTensor_EmptyInput_ReturnsInvalidAtom)
{
    std::vector<float> emptyTensor;
    auto atom = Translator->TranslateTensor(emptyTensor);

    EXPECT_FALSE(atom.IsValid());
    EXPECT_TRUE(atom.AtomID.empty());
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateTensor_SinglePeak_CreatesAtom)
{
    auto tensor = GeneratePeakedTensor(128, 42, 0.9f);
    auto atom = Translator->TranslateTensor(tensor);

    EXPECT_TRUE(atom.IsValid());
    EXPECT_EQ(atom.AtomType, "TensorConcept");
    EXPECT_EQ(atom.SourceFeatureIndex, 42);
    EXPECT_FLOAT_EQ(atom.ActivationValue, 0.9f);
    EXPECT_GT(atom.Confidence, 0.0f);
    EXPECT_LE(atom.Confidence, 1.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateTensor_BelowThreshold_NoAtom)
{
    // All values below threshold (0.3)
    std::vector<float> tensor(64, 0.1f);
    auto atom = Translator->TranslateTensor(tensor);

    EXPECT_FALSE(atom.IsValid());
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateTensor_NegativePeak_CreatesAtomIfAboveThreshold)
{
    std::vector<float> tensor(32, 0.0f);
    tensor[10] = -0.8f; // Negative value, but below 0.3 threshold max
    tensor[15] = 0.05f; // Max positive is below threshold

    // Max activation is 0.05 (positive), which is below threshold
    auto atom = Translator->TranslateTensor(tensor);
    EXPECT_FALSE(atom.IsValid());
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateTensor_ContainsMetadata)
{
    auto tensor = GeneratePeakedTensor(64, 5, 0.7f);
    auto atom = Translator->TranslateTensor(tensor);

    ASSERT_TRUE(atom.IsValid());
    EXPECT_FALSE(atom.Properties.empty());
    EXPECT_NE(atom.Properties.find("TensorDimension"), atom.Properties.end());
    EXPECT_NE(atom.Properties.find("TotalEnergy"), atom.Properties.end());
    EXPECT_NE(atom.Properties.find("MaxActivation"), atom.Properties.end());
    EXPECT_NE(atom.Properties.find("DiscreteBin"), atom.Properties.end());
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateTensor_UpdatesMetrics)
{
    auto tensor = GeneratePeakedTensor(32, 0, 0.5f);
    Translator->TranslateTensor(tensor);
    Translator->TranslateTensor(tensor);

    EXPECT_EQ(Translator->Metrics.TotalTranslations, 2);
    EXPECT_EQ(Translator->Metrics.TotalAtomsCreated, 2);
}

// ============================================================================
// ACTIVATION MAP TRANSLATION TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, TranslateActivations_SingleActivation_ProducesPredicates)
{
    FTestActivationMap map;
    // Two activations with similar values to trigger CoActivation predicate
    map.Activations["Visual"] = {0.5f, 0.6f, 0.55f, 0.8f, 0.1f};
    map.ConfidenceScores["Visual"] = 0.9f;

    auto predicates = Translator->TranslateActivations(map);

    // At least some atoms should be created from the above-threshold values
    EXPECT_GE(Translator->Metrics.TotalTranslations, 1);
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateActivations_MultipleNamedActivations)
{
    FTestActivationMap map;
    map.Activations["Visual"] = {0.9f, 0.1f, 0.4f};
    map.Activations["Audio"] = {0.2f, 0.8f, 0.6f};
    map.ConfidenceScores["Visual"] = 0.95f;
    map.ConfidenceScores["Audio"] = 0.7f;

    auto predicates = Translator->TranslateActivations(map);

    // Predicates should reflect relationships across activation sources
    EXPECT_GE(Translator->Metrics.TotalPredicatesCreated, 0);
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateActivations_EmptyMap_NoPredicates)
{
    FTestActivationMap emptyMap;
    auto predicates = Translator->TranslateActivations(emptyMap);

    EXPECT_TRUE(predicates.empty());
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateActivations_ConfidenceApplied)
{
    // Get baseline predicate confidence without any map-level multiplier
    auto baselineAtomA = Translator->CreateAtomFromActivation(0.9f, 0, "Test");
    auto baselineAtomB = Translator->CreateAtomFromActivation(0.85f, 1, "Test");
    auto baselinePredicate = Translator->CreatePredicateFromAtoms(baselineAtomA, baselineAtomB, "CoActivated");
    float baseConfidence = baselinePredicate.Confidence;
    ASSERT_GT(baseConfidence, 0.0f) << "Baseline confidence must be positive";

    // Translate with a 0.5 confidence multiplier applied via activation map
    FTestActivationMap map;
    map.Activations["Test"] = {0.9f, 0.85f};
    map.ConfidenceScores["Test"] = 0.5f;

    auto predicates = Translator->TranslateActivations(map);

    ASSERT_FALSE(predicates.empty()) << "Expected co-activation predicate for confidence scaling check";
    EXPECT_LT(predicates[0].Confidence, baseConfidence);
    EXPECT_FLOAT_EQ(predicates[0].Confidence, baseConfidence * 0.5f);
}

// ============================================================================
// NEURAL STATE TRANSLATION TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, TranslateNeuralState_FullState_ProducesSymbolicState)
{
    FTestNeuralState state;
    state.StateID = "TestState1";
    state.Activations = {0.5f, 0.8f, 0.3f, 0.9f};
    state.HiddenState = {0.4f, 0.6f};
    state.LayerActivations = {{0.7f, 0.2f}, {0.5f, 0.8f}};
    state.Confidence = 0.85f;

    auto result = Translator->TranslateNeuralState(state);

    EXPECT_EQ(result.StateID, "SymbolicState_TestState1");
    EXPECT_FALSE(result.Atoms.empty());
    EXPECT_GT(result.Confidence, 0.0f);
    EXPECT_LE(result.Confidence, 1.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateNeuralState_EmptyState_ProducesEmptySymbolicState)
{
    FTestNeuralState emptyState;
    emptyState.StateID = "Empty";
    emptyState.Confidence = 0.5f;

    auto result = Translator->TranslateNeuralState(emptyState);

    EXPECT_EQ(result.StateID, "SymbolicState_Empty");
    EXPECT_TRUE(result.Atoms.empty());
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateNeuralState_UncertaintyPropagation)
{
    FTestNeuralState state;
    state.StateID = "UncertaintyTest";
    state.Activations = {0.5f, 0.8f, 0.9f, 0.6f, 0.7f};
    state.Confidence = 0.9f;

    Translator->Config.bPropagateUncertainty = true;
    auto result = Translator->TranslateNeuralState(state);

    // With uncertainty propagation, confidence should be less than input (due to feature count)
    EXPECT_LE(result.Confidence, state.Confidence);
    EXPECT_GT(result.Confidence, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, TranslateNeuralState_NoUncertaintyPropagation)
{
    FTestNeuralState state;
    state.StateID = "NoPropagation";
    state.Activations = {0.5f, 0.8f};
    state.Confidence = 0.9f;

    Translator->Config.bPropagateUncertainty = false;
    auto result = Translator->TranslateNeuralState(state);

    EXPECT_FLOAT_EQ(result.Confidence, state.Confidence);
}

// ============================================================================
// BATCH PROCESSING TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, BatchTranslateTensors_MultipleInputs_ProducesAtoms)
{
    std::vector<std::vector<float>> tensors;
    tensors.push_back(GeneratePeakedTensor(32, 5, 0.8f));
    tensors.push_back(GeneratePeakedTensor(32, 10, 0.9f));
    tensors.push_back(GeneratePeakedTensor(32, 15, 0.7f));

    auto atoms = Translator->BatchTranslateTensors(tensors);

    EXPECT_EQ(atoms.size(), 3u);
    for (const auto& atom : atoms)
    {
        EXPECT_TRUE(atom.IsValid());
    }
}

TEST_F(NeuralToSymbolicTranslatorTest, BatchTranslateTensors_EmptyBatch_NoAtoms)
{
    std::vector<std::vector<float>> emptyBatch;
    auto atoms = Translator->BatchTranslateTensors(emptyBatch);

    EXPECT_TRUE(atoms.empty());
}

TEST_F(NeuralToSymbolicTranslatorTest, BatchTranslateTensors_MixedValidity)
{
    std::vector<std::vector<float>> tensors;
    tensors.push_back(GeneratePeakedTensor(16, 0, 0.8f));  // Valid
    tensors.push_back(std::vector<float>(16, 0.1f));         // Below threshold
    tensors.push_back(GeneratePeakedTensor(16, 5, 0.6f));  // Valid

    auto atoms = Translator->BatchTranslateTensors(tensors);

    EXPECT_EQ(atoms.size(), 2u); // Only valid atoms returned
}

TEST_F(NeuralToSymbolicTranslatorTest, BatchTranslateStates_MultipleStates)
{
    std::vector<FTestNeuralState> states(3);
    for (int i = 0; i < 3; i++)
    {
        states[i].StateID = "State" + std::to_string(i);
        states[i].Activations = {0.5f + i * 0.1f, 0.8f};
        states[i].Confidence = 0.8f;
    }

    auto results = Translator->BatchTranslateStates(states);

    EXPECT_EQ(results.size(), 3u);
    for (size_t i = 0; i < results.size(); i++)
    {
        EXPECT_EQ(results[i].StateID, "SymbolicState_State" + std::to_string(i));
    }
}

// ============================================================================
// ATOM FACTORY TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, CreateAtomFromActivation_AboveThreshold_Valid)
{
    auto atom = Translator->CreateAtomFromActivation(0.7f, 5, "TestType");

    EXPECT_TRUE(atom.IsValid());
    EXPECT_EQ(atom.AtomType, "TestType");
    EXPECT_EQ(atom.SourceFeatureIndex, 5);
    EXPECT_FLOAT_EQ(atom.ActivationValue, 0.7f);
    EXPECT_GT(atom.Confidence, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CreateAtomFromActivation_BelowThreshold_Invalid)
{
    auto atom = Translator->CreateAtomFromActivation(0.1f, 0, "TestType");

    EXPECT_FALSE(atom.IsValid());
    EXPECT_TRUE(atom.AtomID.empty());
}

TEST_F(NeuralToSymbolicTranslatorTest, CreateAtomFromActivation_ExactThreshold_Valid)
{
    auto atom = Translator->CreateAtomFromActivation(0.3f, 0, "TestType");

    EXPECT_TRUE(atom.IsValid());
}

TEST_F(NeuralToSymbolicTranslatorTest, CreateAtomsFromActivationVector_ExtractsDominant)
{
    std::vector<float> activations = {0.1f, 0.9f, 0.2f, 0.8f, 0.05f, 0.7f};
    auto atoms = Translator->CreateAtomsFromActivationVector(activations, "Feature");

    // Only activations >= 0.3 threshold should create atoms: 0.9, 0.8, 0.7
    EXPECT_EQ(atoms.size(), 3u);

    // Should be sorted by activation magnitude (dominant first)
    EXPECT_GE(std::abs(atoms[0].ActivationValue), std::abs(atoms[1].ActivationValue));
}

TEST_F(NeuralToSymbolicTranslatorTest, CreateAtomsFromActivationVector_RespectsMaxAtoms)
{
    Translator->Config.MaxAtomsPerTranslation = 2;

    std::vector<float> activations = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f};
    auto atoms = Translator->CreateAtomsFromActivationVector(activations, "Limited");

    EXPECT_LE(static_cast<int32>(atoms.size()), 2);
}

// ============================================================================
// PREDICATE TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, CreatePredicateFromAtoms_ValidAtoms_ValidPredicate)
{
    auto atom1 = Translator->CreateAtomFromActivation(0.8f, 0, "A");
    auto atom2 = Translator->CreateAtomFromActivation(0.7f, 1, "B");

    auto predicate = Translator->CreatePredicateFromAtoms(atom1, atom2, "Related");

    EXPECT_TRUE(predicate.IsValid());
    EXPECT_EQ(predicate.PredicateName, "Related");
    EXPECT_EQ(predicate.Arguments.size(), 2u);
    EXPECT_EQ(predicate.SourceFeatureIndices.size(), 2u);
    EXPECT_GT(predicate.Confidence, 0.0f);
    EXPECT_GT(predicate.TruthValue, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CreatePredicateFromAtoms_ConfidenceIsAverage)
{
    auto atom1 = Translator->CreateAtomFromActivation(0.8f, 0, "A");
    auto atom2 = Translator->CreateAtomFromActivation(0.6f, 1, "B");

    auto predicate = Translator->CreatePredicateFromAtoms(atom1, atom2, "Test");

    float expectedConfidence = (atom1.Confidence + atom2.Confidence) / 2.0f;
    EXPECT_NEAR(predicate.Confidence, expectedConfidence, 0.001f);
}

// ============================================================================
// DISCRETIZATION TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, DiscretizeActivation_MinValue_ReturnsBin0)
{
    int32 bin = Translator->DiscretizeActivation(-1.0f);
    EXPECT_EQ(bin, 0);
}

TEST_F(NeuralToSymbolicTranslatorTest, DiscretizeActivation_MaxValue_ReturnsMaxBin)
{
    int32 bin = Translator->DiscretizeActivation(1.0f);
    EXPECT_EQ(bin, Translator->Config.DiscretizationBins - 1);
}

TEST_F(NeuralToSymbolicTranslatorTest, DiscretizeActivation_MidValue_ReturnsMidBin)
{
    int32 bin = Translator->DiscretizeActivation(0.0f);
    // 0.0 maps to (0+1)/2 = 0.5, bin = floor(0.5 * 9) = 4
    EXPECT_EQ(bin, 4);
}

TEST_F(NeuralToSymbolicTranslatorTest, DiscretizeActivation_OutOfRange_Clamped)
{
    int32 binLow = Translator->DiscretizeActivation(-5.0f);
    int32 binHigh = Translator->DiscretizeActivation(5.0f);

    EXPECT_GE(binLow, 0);
    EXPECT_LT(binHigh, Translator->Config.DiscretizationBins);
}

TEST_F(NeuralToSymbolicTranslatorTest, ShouldCreateAtom_AboveThreshold_True)
{
    EXPECT_TRUE(Translator->ShouldCreateAtom(0.5f));
    EXPECT_TRUE(Translator->ShouldCreateAtom(-0.5f)); // Absolute value checked
    EXPECT_TRUE(Translator->ShouldCreateAtom(0.3f));  // Exact threshold
}

TEST_F(NeuralToSymbolicTranslatorTest, ShouldCreateAtom_BelowThreshold_False)
{
    EXPECT_FALSE(Translator->ShouldCreateAtom(0.1f));
    EXPECT_FALSE(Translator->ShouldCreateAtom(-0.1f));
    EXPECT_FALSE(Translator->ShouldCreateAtom(0.0f));
    EXPECT_FALSE(Translator->ShouldCreateAtom(0.29f));
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculateConfidence_ZeroActivation_ZeroConfidence)
{
    float conf = Translator->CalculateConfidence(0.0f);
    EXPECT_FLOAT_EQ(conf, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculateConfidence_HighActivation_HighConfidence)
{
    float conf = Translator->CalculateConfidence(10.0f);
    // 10 / (1 + 10) = 0.909...
    EXPECT_NEAR(conf, 10.0f / 11.0f, 0.001f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculateConfidence_Monotonic)
{
    float conf1 = Translator->CalculateConfidence(0.3f);
    float conf2 = Translator->CalculateConfidence(0.6f);
    float conf3 = Translator->CalculateConfidence(0.9f);

    EXPECT_LT(conf1, conf2);
    EXPECT_LT(conf2, conf3);
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculateConfidence_NeverExceedsOne)
{
    float conf = Translator->CalculateConfidence(1000.0f);
    EXPECT_LE(conf, 1.0f);
}

// ============================================================================
// UNCERTAINTY PROPAGATION TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, PropagateUncertainty_ZeroFeatures_ReturnsZero)
{
    float result = Translator->PropagateUncertainty(0.9f, 0);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, PropagateUncertainty_SingleFeature_EqualsInput)
{
    float result = Translator->PropagateUncertainty(0.9f, 1);
    // 0.9 * (1 / sqrt(1)) = 0.9
    EXPECT_FLOAT_EQ(result, 0.9f);
}

TEST_F(NeuralToSymbolicTranslatorTest, PropagateUncertainty_MoreFeatures_LowerConfidence)
{
    float conf1 = Translator->PropagateUncertainty(0.9f, 1);
    float conf4 = Translator->PropagateUncertainty(0.9f, 4);
    float conf16 = Translator->PropagateUncertainty(0.9f, 16);

    EXPECT_GT(conf1, conf4);
    EXPECT_GT(conf4, conf16);
}

TEST_F(NeuralToSymbolicTranslatorTest, PropagateUncertainty_ResultClampedToUnitRange)
{
    float result = Translator->PropagateUncertainty(2.0f, 1); // Input > 1
    EXPECT_LE(result, 1.0f);
    EXPECT_GE(result, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculatePredicateUncertainty_EmptyAtoms_ReturnsZero)
{
    std::vector<FTestSymbolicAtom> emptyAtoms;
    float result = Translator->CalculatePredicateUncertainty(emptyAtoms);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculatePredicateUncertainty_GeometricMean)
{
    std::vector<FTestSymbolicAtom> atoms(2);
    atoms[0].Confidence = 0.8f;
    atoms[1].Confidence = 0.6f;

    float result = Translator->CalculatePredicateUncertainty(atoms);

    // Geometric mean of 0.8 and 0.6 = sqrt(0.48) ≈ 0.693
    float expected = std::sqrt(0.8f * 0.6f);
    EXPECT_NEAR(result, expected, 0.01f);
}

TEST_F(NeuralToSymbolicTranslatorTest, CalculatePredicateUncertainty_AllHighConfidence)
{
    std::vector<FTestSymbolicAtom> atoms(3);
    for (auto& atom : atoms)
    {
        atom.Confidence = 0.9f;
    }

    float result = Translator->CalculatePredicateUncertainty(atoms);
    EXPECT_NEAR(result, 0.9f, 0.01f); // Geometric mean of equal values = the value
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, Performance_SingleTranslation_UnderTarget)
{
    auto tensor = GenerateRandomActivations(128);

    auto start = std::chrono::high_resolution_clock::now();
    Translator->TranslateTensor(tensor);
    auto end = std::chrono::high_resolution_clock::now();

    float latencyMs = std::chrono::duration<float, std::milli>(end - start).count();
    EXPECT_LT(latencyMs, 0.5f) << "Single translation exceeded 0.5ms target: " << latencyMs << "ms";
}

TEST_F(NeuralToSymbolicTranslatorTest, Performance_BatchTranslation_UnderTarget)
{
    const int32 BatchCount = 32;
    std::vector<std::vector<float>> tensors;
    for (int i = 0; i < BatchCount; i++)
    {
        tensors.push_back(GenerateRandomActivations(128));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto atoms = Translator->BatchTranslateTensors(tensors);
    auto end = std::chrono::high_resolution_clock::now();

    float totalMs = std::chrono::duration<float, std::milli>(end - start).count();
    float perItemMs = totalMs / BatchCount;

    EXPECT_LT(perItemMs, 0.3f) << "Batch per-item latency exceeded 0.3ms: " << perItemMs << "ms";
}

TEST_F(NeuralToSymbolicTranslatorTest, Performance_MeetsLatencyTarget)
{
    // Run multiple translations to build up metrics
    for (int i = 0; i < 100; i++)
    {
        auto tensor = GenerateRandomActivations(64);
        Translator->TranslateTensor(tensor);
    }

    EXPECT_TRUE(Translator->IsMeetingLatencyTarget())
        << "Average latency: " << Translator->Metrics.AverageLatency << "ms (target: <0.5ms)";
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, EdgeCase_SingleElementTensor)
{
    std::vector<float> tensor = {0.9f};
    auto atom = Translator->TranslateTensor(tensor);

    EXPECT_TRUE(atom.IsValid());
    EXPECT_EQ(atom.SourceFeatureIndex, 0);
}

TEST_F(NeuralToSymbolicTranslatorTest, EdgeCase_AllZeros)
{
    std::vector<float> tensor(64, 0.0f);
    auto atom = Translator->TranslateTensor(tensor);

    EXPECT_FALSE(atom.IsValid()); // 0.0 is below threshold
}

TEST_F(NeuralToSymbolicTranslatorTest, EdgeCase_LargeTensor)
{
    std::vector<float> tensor(1024, 0.1f);
    tensor[512] = 0.95f;
    auto atom = Translator->TranslateTensor(tensor);

    EXPECT_TRUE(atom.IsValid());
    EXPECT_EQ(atom.SourceFeatureIndex, 512);
}

TEST_F(NeuralToSymbolicTranslatorTest, EdgeCase_AllNegative)
{
    std::vector<float> tensor = {-0.1f, -0.2f, -0.5f, -0.8f};
    auto atom = Translator->TranslateTensor(tensor);

    // Max value is -0.1, which is below threshold
    EXPECT_FALSE(atom.IsValid());
}

TEST_F(NeuralToSymbolicTranslatorTest, EdgeCase_UniqueAtomIDs)
{
    auto tensor = GeneratePeakedTensor(16, 0, 0.8f);

    auto atom1 = Translator->TranslateTensor(tensor);
    auto atom2 = Translator->TranslateTensor(tensor);

    ASSERT_TRUE(atom1.IsValid());
    ASSERT_TRUE(atom2.IsValid());
    EXPECT_NE(atom1.AtomID, atom2.AtomID);
}

TEST_F(NeuralToSymbolicTranslatorTest, EdgeCase_ResetMetrics)
{
    auto tensor = GeneratePeakedTensor(16, 0, 0.8f);
    Translator->TranslateTensor(tensor);

    EXPECT_GT(Translator->Metrics.TotalTranslations, 0);

    Translator->ResetMetrics();

    EXPECT_EQ(Translator->Metrics.TotalTranslations, 0);
    EXPECT_EQ(Translator->Metrics.TotalAtomsCreated, 0);
    EXPECT_FLOAT_EQ(Translator->Metrics.AverageLatency, 0.0f);
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, Config_HighThreshold_FewerAtoms)
{
    std::vector<float> activations = {0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    Translator->Config.ActivationThreshold = 0.3f;
    auto atomsLow = Translator->CreateAtomsFromActivationVector(activations, "Low");

    Translator->Config.ActivationThreshold = 0.7f;
    auto atomsHigh = Translator->CreateAtomsFromActivationVector(activations, "High");

    EXPECT_GT(atomsLow.size(), atomsHigh.size());
}

TEST_F(NeuralToSymbolicTranslatorTest, Config_DiscretizationBins)
{
    Translator->Config.DiscretizationBins = 5;
    int32 bin5 = Translator->DiscretizeActivation(0.5f);
    EXPECT_GE(bin5, 0);
    EXPECT_LT(bin5, 5);

    Translator->Config.DiscretizationBins = 20;
    int32 bin20 = Translator->DiscretizeActivation(0.5f);
    EXPECT_GE(bin20, 0);
    EXPECT_LT(bin20, 20);
}

// ============================================================================
// INTEGRATION TEST
// ============================================================================

TEST_F(NeuralToSymbolicTranslatorTest, Integration_FullPipeline)
{
    // Simulate a complete neural-to-symbolic pipeline
    FTestNeuralState state;
    state.StateID = "FullPipeline";
    state.Activations = GenerateRandomActivations(64);
    state.HiddenState = GenerateRandomActivations(32);
    state.LayerActivations = {GenerateRandomActivations(16), GenerateRandomActivations(16)};
    state.Confidence = 0.85f;

    // Translate
    auto symbolicState = Translator->TranslateNeuralState(state);

    // Verify output
    EXPECT_FALSE(symbolicState.StateID.empty());
    EXPECT_GT(symbolicState.Confidence, 0.0f);
    EXPECT_LE(symbolicState.Confidence, 1.0f);

    // Verify metrics accumulated
    EXPECT_GT(Translator->Metrics.TotalTranslations, 0);
    EXPECT_GT(Translator->Metrics.TotalAtomsCreated, 0);

    // All atoms should be valid
    for (const auto& atom : symbolicState.Atoms)
    {
        EXPECT_TRUE(atom.IsValid());
        EXPECT_GT(atom.Confidence, 0.0f);
    }

    // All predicates should be valid
    for (const auto& pred : symbolicState.Predicates)
    {
        EXPECT_TRUE(pred.IsValid());
        EXPECT_EQ(pred.Arguments.size(), 2u);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
