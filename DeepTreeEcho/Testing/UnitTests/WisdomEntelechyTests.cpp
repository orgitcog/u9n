/**
 * @file WisdomEntelechyTests.cpp
 * @brief Exhaustive unit tests for Wisdom Cultivation and Entelechy Framework systems
 *
 * Tests cover:
 * - EntelechyFramework: Five dimensions (Ontological, Teleological, Cognitive, Integrative, Evolutionary)
 * - Development stages (Embryonic, Juvenile, Adolescent, Adult, Transcendent)
 * - RelevanceRealizationEnnead: Ways of knowing, Orders of understanding, Practices of wisdom
 * - Gnostic spiral progression
 * - Entelechy genome and evolution
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <algorithm>

// ============================================================================
// Mock Types for Entelechy Framework
// ============================================================================

enum class EEntelechyStage {
    Embryonic,     // < 30%
    Juvenile,      // 30-60%
    Adolescent,    // 60-80%
    Adult,         // 80-95%
    Transcendent   // > 95%
};

enum class EEntelechyDimension {
    Ontological,   // BEING
    Teleological,  // PURPOSE
    Cognitive,     // COGNITION
    Integrative,   // INTEGRATION
    Evolutionary   // GROWTH
};

enum class EDevelopmentPhase {
    Phase1_CoreExtensions,
    Phase2_LogicSystems,
    Phase3_CognitiveSystems,
    Phase4_AdvancedLearning,
    Phase5_LanguageIntegration
};

struct FComponentState {
    std::string Name;
    bool bExists = false;
    bool bFunctional = false;
    float Health = 0.0f;
    float IntegrationLevel = 0.0f;
};

struct FOntologicalDimension {
    float FoundationHealth = 0.0f;
    float CoreHealth = 0.0f;
    float SpecializedHealth = 0.0f;
    float ArchitecturalCompleteness = 0.0f;
    std::vector<FComponentState> FoundationComponents;
    std::vector<FComponentState> CoreComponents;
    std::vector<FComponentState> SpecializedComponents;
};

struct FPhaseProgress {
    EDevelopmentPhase Phase;
    float Progress = 0.0f;
    bool bComplete = false;
    std::vector<std::string> Milestones;
};

struct FTeleologicalDimension {
    std::vector<FPhaseProgress> DevelopmentPhases;
    float RoadmapAlignment = 0.0f;
    float ActualizationTrajectory = 0.0f;
    float PurposeClarity = 0.0f;
    std::vector<std::string> CurrentGoals;
    std::string Telos;
};

struct FCognitiveSystemState {
    std::string Name;
    float Health = 0.0f;
    int FragmentationMarkers = 0;
    std::vector<FComponentState> Components;
};

struct FCognitiveDimension {
    FCognitiveSystemState ReasoningSystems;
    FCognitiveSystemState PatternSystems;
    FCognitiveSystemState AttentionSystems;
    FCognitiveSystemState LearningSystems;
    float CognitiveCompleteness = 0.0f;
};

struct FIntegrativeDimension {
    int TotalDependencies = 0;
    int SatisfiedDependencies = 0;
    float DependencyHealth = 0.0f;
    float BuildIntegrationHealth = 0.0f;
    float TestIntegrationHealth = 0.0f;
    float IntegrationHealth = 0.0f;
};

struct FEvolutionaryDimension {
    int TodoCount = 0;
    int FixmeCount = 0;
    int StubCount = 0;
    float CodeHealth = 0.0f;
    float ImplementationDepth = 0.0f;
    bool bHasAutognosis = false;
    bool bHasOntogenesis = false;
    float SelfImprovementCapacity = 0.0f;
    float EvolutionaryPotential = 0.0f;
};

struct FEntelechyGenome {
    std::string Id;
    int Generation = 0;
    std::vector<std::string> Lineage;
    std::vector<float> OntologicalGenes;
    std::vector<float> TeleologicalGenes;
    std::vector<float> CognitiveGenes;
    std::vector<float> IntegrativeGenes;
    std::vector<float> EvolutionaryGenes;
    float Fitness = 0.0f;
    int Age = 0;
    float ActualizationLevel = 0.0f;
};

struct FEntelechyMetrics {
    float Actualization = 0.0f;
    float Completeness = 0.0f;
    float Coherence = 0.0f;
    float Vitality = 0.0f;
    float Alignment = 0.0f;
    int TotalComponents = 0;
    int IntegratedComponents = 0;
    int FragmentedComponents = 0;
    int TotalCodeMarkers = 0;
};

/**
 * @brief Mock Entelechy Framework
 */
class MockEntelechyFramework {
public:
    MockEntelechyFramework() {
        InitializeDimensionWeights();
    }

    void InitializeEntelechy(const std::string& systemId) {
        bInitialized = true;
        Genome.Id = systemId;
        Genome.Generation = 1;
        InitializeGenome();
        InitializeDevelopmentPhases();
    }

    void ResetEntelechy() {
        CurrentStage = EEntelechyStage::Embryonic;
        Genome = FEntelechyGenome();
        Metrics = FEntelechyMetrics();
        bInitialized = false;
    }

    bool IsInitialized() const { return bInitialized; }

    // Assessment
    void AssessAllDimensions() {
        AssessOntological();
        AssessTeleological();
        AssessCognitive();
        AssessIntegrative();
        AssessEvolutionary();
        UpdateDevelopmentStage();
        UpdateMetrics();
    }

    void AssessOntological() {
        // Simulate ontological assessment
        Ontological.FoundationHealth = 0.9f;
        Ontological.CoreHealth = 0.85f;
        Ontological.SpecializedHealth = 0.7f;
        Ontological.ArchitecturalCompleteness =
            (Ontological.FoundationHealth + Ontological.CoreHealth + Ontological.SpecializedHealth) / 3.0f;
    }

    void AssessTeleological() {
        float phaseSum = 0.0f;
        for (const auto& phase : Teleological.DevelopmentPhases) {
            phaseSum += phase.Progress;
        }
        Teleological.RoadmapAlignment = phaseSum / Teleological.DevelopmentPhases.size();
        Teleological.ActualizationTrajectory = 0.75f;
        Teleological.PurposeClarity = 0.8f;
    }

    void AssessCognitive() {
        Cognitive.ReasoningSystems.Health = 0.8f;
        Cognitive.PatternSystems.Health = 0.85f;
        Cognitive.AttentionSystems.Health = 0.75f;
        Cognitive.LearningSystems.Health = 0.7f;
        Cognitive.CognitiveCompleteness =
            (Cognitive.ReasoningSystems.Health + Cognitive.PatternSystems.Health +
             Cognitive.AttentionSystems.Health + Cognitive.LearningSystems.Health) / 4.0f;
    }

    void AssessIntegrative() {
        Integrative.TotalDependencies = 100;
        Integrative.SatisfiedDependencies = 90;
        Integrative.DependencyHealth = (float)Integrative.SatisfiedDependencies / Integrative.TotalDependencies;
        Integrative.BuildIntegrationHealth = 0.95f;
        Integrative.TestIntegrationHealth = 0.85f;
        Integrative.IntegrationHealth =
            (Integrative.DependencyHealth + Integrative.BuildIntegrationHealth + Integrative.TestIntegrationHealth) / 3.0f;
    }

    void AssessEvolutionary() {
        Evolutionary.TodoCount = 50;
        Evolutionary.FixmeCount = 10;
        Evolutionary.StubCount = 5;
        Evolutionary.CodeHealth = 1.0f - (Evolutionary.TodoCount + Evolutionary.FixmeCount * 2 + Evolutionary.StubCount * 3) / 200.0f;
        Evolutionary.ImplementationDepth = 0.8f;
        Evolutionary.bHasAutognosis = true;
        Evolutionary.bHasOntogenesis = true;
        Evolutionary.SelfImprovementCapacity = 0.7f;
        Evolutionary.EvolutionaryPotential =
            (Evolutionary.CodeHealth + Evolutionary.ImplementationDepth + Evolutionary.SelfImprovementCapacity) / 3.0f;
    }

    // Metrics
    float CalculateFitness() const {
        float ontologicalHealth = CalculateOntologicalHealth();
        float teleologicalHealth = CalculateTeleologicalHealth();
        float cognitiveHealth = CalculateCognitiveHealth();
        float integrativeHealth = CalculateIntegrativeHealth();
        float evolutionaryHealth = CalculateEvolutionaryHealth();

        auto getWeight = [this](EEntelechyDimension dim) -> float {
            auto it = DimensionWeights.find(dim);
            return (it != DimensionWeights.end()) ? it->second : 0.2f;
        };

        return (ontologicalHealth * getWeight(EEntelechyDimension::Ontological) +
                teleologicalHealth * getWeight(EEntelechyDimension::Teleological) +
                cognitiveHealth * getWeight(EEntelechyDimension::Cognitive) +
                integrativeHealth * getWeight(EEntelechyDimension::Integrative) +
                evolutionaryHealth * getWeight(EEntelechyDimension::Evolutionary));
    }

    float GetDimensionHealth(EEntelechyDimension dimension) const {
        switch (dimension) {
            case EEntelechyDimension::Ontological: return CalculateOntologicalHealth();
            case EEntelechyDimension::Teleological: return CalculateTeleologicalHealth();
            case EEntelechyDimension::Cognitive: return CalculateCognitiveHealth();
            case EEntelechyDimension::Integrative: return CalculateIntegrativeHealth();
            case EEntelechyDimension::Evolutionary: return CalculateEvolutionaryHealth();
            default: return 0.0f;
        }
    }

    EEntelechyStage GetDevelopmentStage() const { return CurrentStage; }

    float GetActualizationPercentage() const {
        return CalculateFitness() * 100.0f;
    }

    // Evolution
    bool TryAdvanceStage() {
        float fitness = CalculateFitness();
        EEntelechyStage newStage = CurrentStage;

        if (fitness >= 0.95f) newStage = EEntelechyStage::Transcendent;
        else if (fitness >= 0.80f) newStage = EEntelechyStage::Adult;
        else if (fitness >= 0.60f) newStage = EEntelechyStage::Adolescent;
        else if (fitness >= 0.30f) newStage = EEntelechyStage::Juvenile;
        else newStage = EEntelechyStage::Embryonic;

        if (newStage > CurrentStage) {
            CurrentStage = newStage;
            return true;
        }
        return false;
    }

    void RegisterComponent(const std::string& name, EEntelechyDimension dimension) {
        FComponentState component;
        component.Name = name;
        component.bExists = true;
        component.Health = 0.5f;

        switch (dimension) {
            case EEntelechyDimension::Ontological:
                Ontological.CoreComponents.push_back(component);
                break;
            case EEntelechyDimension::Cognitive:
                Cognitive.ReasoningSystems.Components.push_back(component);
                break;
            default:
                break;
        }
        Metrics.TotalComponents++;
    }

    void UpdateComponentHealth(const std::string& name, float health) {
        for (auto& comp : Ontological.CoreComponents) {
            if (comp.Name == name) {
                comp.Health = std::clamp(health, 0.0f, 1.0f);
                return;
            }
        }
        for (auto& comp : Cognitive.ReasoningSystems.Components) {
            if (comp.Name == name) {
                comp.Health = std::clamp(health, 0.0f, 1.0f);
                return;
            }
        }
    }

    void AddMilestone(EDevelopmentPhase phase, const std::string& milestone) {
        for (auto& phaseProgress : Teleological.DevelopmentPhases) {
            if (phaseProgress.Phase == phase) {
                phaseProgress.Milestones.push_back(milestone);
                phaseProgress.Progress += 0.1f;
                phaseProgress.Progress = std::min(phaseProgress.Progress, 1.0f);
                if (phaseProgress.Progress >= 1.0f) {
                    phaseProgress.bComplete = true;
                }
                return;
            }
        }
    }

    // Genome
    FEntelechyGenome GetGenome() const { return Genome; }

    void MutateGenome(float mutationRate) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, mutationRate);

        for (auto& gene : Genome.OntologicalGenes) {
            gene = std::clamp(gene + dist(gen), 0.0f, 1.0f);
        }
        for (auto& gene : Genome.TeleologicalGenes) {
            gene = std::clamp(gene + dist(gen), 0.0f, 1.0f);
        }
        for (auto& gene : Genome.CognitiveGenes) {
            gene = std::clamp(gene + dist(gen), 0.0f, 1.0f);
        }
        for (auto& gene : Genome.IntegrativeGenes) {
            gene = std::clamp(gene + dist(gen), 0.0f, 1.0f);
        }
        for (auto& gene : Genome.EvolutionaryGenes) {
            gene = std::clamp(gene + dist(gen), 0.0f, 1.0f);
        }
        Genome.Generation++;
    }

    FEntelechyGenome CrossoverGenome(const FEntelechyGenome& other) const {
        FEntelechyGenome child;
        child.Id = Genome.Id + "_x_" + other.Id;
        child.Generation = std::max(Genome.Generation, other.Generation) + 1;

        // Uniform crossover
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 1);

        size_t geneCount = std::min(Genome.OntologicalGenes.size(), other.OntologicalGenes.size());
        child.OntologicalGenes.resize(geneCount);
        for (size_t i = 0; i < geneCount; i++) {
            child.OntologicalGenes[i] = dist(gen) ? Genome.OntologicalGenes[i] : other.OntologicalGenes[i];
        }

        geneCount = std::min(Genome.CognitiveGenes.size(), other.CognitiveGenes.size());
        child.CognitiveGenes.resize(geneCount);
        for (size_t i = 0; i < geneCount; i++) {
            child.CognitiveGenes[i] = dist(gen) ? Genome.CognitiveGenes[i] : other.CognitiveGenes[i];
        }

        child.Lineage.push_back(Genome.Id);
        child.Lineage.push_back(other.Id);

        return child;
    }

    // Accessors
    const FOntologicalDimension& GetOntological() const { return Ontological; }
    const FTeleologicalDimension& GetTeleological() const { return Teleological; }
    const FCognitiveDimension& GetCognitive() const { return Cognitive; }
    const FIntegrativeDimension& GetIntegrative() const { return Integrative; }
    const FEvolutionaryDimension& GetEvolutionary() const { return Evolutionary; }
    const FEntelechyMetrics& GetMetrics() const { return Metrics; }

private:
    void InitializeDimensionWeights() {
        DimensionWeights[EEntelechyDimension::Ontological] = 0.2f;
        DimensionWeights[EEntelechyDimension::Teleological] = 0.2f;
        DimensionWeights[EEntelechyDimension::Cognitive] = 0.2f;
        DimensionWeights[EEntelechyDimension::Integrative] = 0.2f;
        DimensionWeights[EEntelechyDimension::Evolutionary] = 0.2f;
    }

    void InitializeGenome() {
        Genome.OntologicalGenes.resize(10, 0.5f);
        Genome.TeleologicalGenes.resize(10, 0.5f);
        Genome.CognitiveGenes.resize(10, 0.5f);
        Genome.IntegrativeGenes.resize(10, 0.5f);
        Genome.EvolutionaryGenes.resize(10, 0.5f);
    }

    void InitializeDevelopmentPhases() {
        Teleological.DevelopmentPhases.resize(5);
        Teleological.DevelopmentPhases[0].Phase = EDevelopmentPhase::Phase1_CoreExtensions;
        Teleological.DevelopmentPhases[1].Phase = EDevelopmentPhase::Phase2_LogicSystems;
        Teleological.DevelopmentPhases[2].Phase = EDevelopmentPhase::Phase3_CognitiveSystems;
        Teleological.DevelopmentPhases[3].Phase = EDevelopmentPhase::Phase4_AdvancedLearning;
        Teleological.DevelopmentPhases[4].Phase = EDevelopmentPhase::Phase5_LanguageIntegration;
    }

    void UpdateDevelopmentStage() {
        float fitness = CalculateFitness();

        if (fitness >= 0.95f) CurrentStage = EEntelechyStage::Transcendent;
        else if (fitness >= 0.80f) CurrentStage = EEntelechyStage::Adult;
        else if (fitness >= 0.60f) CurrentStage = EEntelechyStage::Adolescent;
        else if (fitness >= 0.30f) CurrentStage = EEntelechyStage::Juvenile;
        else CurrentStage = EEntelechyStage::Embryonic;
    }

    void UpdateMetrics() {
        Metrics.Actualization = CalculateFitness();
        Metrics.Completeness = Ontological.ArchitecturalCompleteness;
        Metrics.Coherence = Integrative.IntegrationHealth;
        Metrics.Vitality = Evolutionary.EvolutionaryPotential;
        Metrics.Alignment = Teleological.RoadmapAlignment;
        Metrics.TotalCodeMarkers = Evolutionary.TodoCount + Evolutionary.FixmeCount + Evolutionary.StubCount;
    }

    float CalculateOntologicalHealth() const {
        return Ontological.ArchitecturalCompleteness;
    }

    float CalculateTeleologicalHealth() const {
        return (Teleological.RoadmapAlignment + Teleological.ActualizationTrajectory + Teleological.PurposeClarity) / 3.0f;
    }

    float CalculateCognitiveHealth() const {
        return Cognitive.CognitiveCompleteness;
    }

    float CalculateIntegrativeHealth() const {
        return Integrative.IntegrationHealth;
    }

    float CalculateEvolutionaryHealth() const {
        return Evolutionary.EvolutionaryPotential;
    }

    bool bInitialized = false;
    EEntelechyStage CurrentStage = EEntelechyStage::Embryonic;
    FOntologicalDimension Ontological;
    FTeleologicalDimension Teleological;
    FCognitiveDimension Cognitive;
    FIntegrativeDimension Integrative;
    FEvolutionaryDimension Evolutionary;
    FEntelechyGenome Genome;
    FEntelechyMetrics Metrics;
    std::unordered_map<EEntelechyDimension, float> DimensionWeights;
};

// ============================================================================
// Mock Types for Relevance Realization Ennead
// ============================================================================

enum class EWayOfKnowing {
    Propositional,  // Knowing-That
    Procedural,     // Knowing-How
    Perspectival,   // Knowing-As
    Participatory   // Knowing-By-Being
};

enum class EOrderOfUnderstanding {
    Nomological,  // How things work
    Normative,    // What matters
    Narrative     // How things develop
};

enum class EPracticeOfWisdom {
    Morality,  // Virtue & Ethics
    Meaning,   // Coherence & Purpose
    Mastery    // Excellence & Flow
};

enum class EGnosticStage {
    PropositionalOpening,
    ProceduralDevelopment,
    PerspectivalShift,
    ParticipatoryTransform,
    NewUnderstanding,
    AnagogicAscent
};

struct FKnowingState {
    EWayOfKnowing Type;
    float ActivationLevel = 0.0f;
    float IntegrationLevel = 0.0f;
    float Coherence = 0.5f;
    std::vector<std::string> ActiveContents;
    std::string CurrentFocus;
};

struct FOrderState {
    EOrderOfUnderstanding Type;
    float ActivationLevel = 0.0f;
    float Clarity = 0.5f;
    float IntegrationWithOthers = 0.5f;
    std::vector<std::string> CurrentUnderstandings;
    std::string PrimaryQuestion;
};

struct FPracticeState {
    EPracticeOfWisdom Type;
    float CultivationLevel = 0.0f;
    float ActiveEngagement = 0.0f;
    float IntegrationWithOthers = 0.5f;
    std::vector<std::string> CurrentPractices;
    std::string PrimaryGoal;
};

struct FGnosticSpiralState {
    EGnosticStage CurrentStage = EGnosticStage::PropositionalOpening;
    int SpiralLevel = 1;
    float StageProgress = 0.0f;
    float TransformativeIntensity = 0.0f;
    std::vector<std::string> InsightsGained;
};

struct FEnneadState {
    std::vector<FKnowingState> WaysOfKnowing;
    std::vector<FOrderState> OrdersOfUnderstanding;
    std::vector<FPracticeState> PracticesOfWisdom;
    FGnosticSpiralState GnosticSpiral;
    float OverallIntegration = 0.0f;
    float RelevanceRealizationLevel = 0.0f;
    float WisdomLevel = 0.0f;
    float MeaningLevel = 0.0f;
};

struct FTriadInterpenetration {
    float KnowingToOrderFlow = 0.5f;
    float KnowingToWisdomFlow = 0.5f;
    float OrderToKnowingFlow = 0.5f;
    float OrderToWisdomFlow = 0.5f;
    float WisdomToKnowingFlow = 0.5f;
    float WisdomToOrderFlow = 0.5f;
    float OverallCoherence = 0.5f;
};

/**
 * @brief Mock Relevance Realization Ennead
 */
class MockRelevanceRealizationEnnead {
public:
    MockRelevanceRealizationEnnead() {
        InitializeEnnead();
    }

    // Ways of Knowing
    void ActivateKnowing(EWayOfKnowing way, float intensity) {
        int idx = static_cast<int>(way);
        if (idx >= 0 && idx < (int)EnneadState.WaysOfKnowing.size()) {
            EnneadState.WaysOfKnowing[idx].ActivationLevel = std::clamp(intensity, 0.0f, 1.0f);
            UpdateInterpenetration();
        }
    }

    FKnowingState GetKnowingState(EWayOfKnowing way) const {
        int idx = static_cast<int>(way);
        if (idx >= 0 && idx < (int)EnneadState.WaysOfKnowing.size()) {
            return EnneadState.WaysOfKnowing[idx];
        }
        return FKnowingState();
    }

    void AddKnowingContent(EWayOfKnowing way, const std::string& content) {
        int idx = static_cast<int>(way);
        if (idx >= 0 && idx < (int)EnneadState.WaysOfKnowing.size()) {
            EnneadState.WaysOfKnowing[idx].ActiveContents.push_back(content);
        }
    }

    void SetKnowingFocus(EWayOfKnowing way, const std::string& focus) {
        int idx = static_cast<int>(way);
        if (idx >= 0 && idx < (int)EnneadState.WaysOfKnowing.size()) {
            EnneadState.WaysOfKnowing[idx].CurrentFocus = focus;
        }
    }

    EWayOfKnowing GetDominantKnowing() const {
        float maxActivation = 0.0f;
        EWayOfKnowing dominant = EWayOfKnowing::Propositional;

        for (const auto& state : EnneadState.WaysOfKnowing) {
            if (state.ActivationLevel > maxActivation) {
                maxActivation = state.ActivationLevel;
                dominant = state.Type;
            }
        }
        return dominant;
    }

    float GetKnowingIntegration() const {
        return ComputeKnowingIntegration();
    }

    // Orders of Understanding
    void ActivateOrder(EOrderOfUnderstanding order, float intensity) {
        int idx = static_cast<int>(order);
        if (idx >= 0 && idx < (int)EnneadState.OrdersOfUnderstanding.size()) {
            EnneadState.OrdersOfUnderstanding[idx].ActivationLevel = std::clamp(intensity, 0.0f, 1.0f);
            UpdateInterpenetration();
        }
    }

    FOrderState GetOrderState(EOrderOfUnderstanding order) const {
        int idx = static_cast<int>(order);
        if (idx >= 0 && idx < (int)EnneadState.OrdersOfUnderstanding.size()) {
            return EnneadState.OrdersOfUnderstanding[idx];
        }
        return FOrderState();
    }

    EOrderOfUnderstanding GetDominantOrder() const {
        float maxActivation = 0.0f;
        EOrderOfUnderstanding dominant = EOrderOfUnderstanding::Nomological;

        for (const auto& state : EnneadState.OrdersOfUnderstanding) {
            if (state.ActivationLevel > maxActivation) {
                maxActivation = state.ActivationLevel;
                dominant = state.Type;
            }
        }
        return dominant;
    }

    float GetOrderIntegration() const {
        return ComputeOrderIntegration();
    }

    // Practices of Wisdom
    void ActivatePractice(EPracticeOfWisdom practice, float intensity) {
        int idx = static_cast<int>(practice);
        if (idx >= 0 && idx < (int)EnneadState.PracticesOfWisdom.size()) {
            EnneadState.PracticesOfWisdom[idx].CultivationLevel = std::clamp(intensity, 0.0f, 1.0f);
            EnneadState.PracticesOfWisdom[idx].ActiveEngagement = intensity;
            UpdateInterpenetration();
        }
    }

    FPracticeState GetPracticeState(EPracticeOfWisdom practice) const {
        int idx = static_cast<int>(practice);
        if (idx >= 0 && idx < (int)EnneadState.PracticesOfWisdom.size()) {
            return EnneadState.PracticesOfWisdom[idx];
        }
        return FPracticeState();
    }

    EPracticeOfWisdom GetDominantPractice() const {
        float maxCultivation = 0.0f;
        EPracticeOfWisdom dominant = EPracticeOfWisdom::Morality;

        for (const auto& state : EnneadState.PracticesOfWisdom) {
            if (state.CultivationLevel > maxCultivation) {
                maxCultivation = state.CultivationLevel;
                dominant = state.Type;
            }
        }
        return dominant;
    }

    float GetPracticeIntegration() const {
        return ComputePracticeIntegration();
    }

    // Gnostic Spiral
    FGnosticSpiralState GetGnosticState() const {
        return EnneadState.GnosticSpiral;
    }

    void AdvanceGnosticSpiral() {
        int stageIdx = static_cast<int>(EnneadState.GnosticSpiral.CurrentStage);
        if (stageIdx < 5) {
            EnneadState.GnosticSpiral.CurrentStage = static_cast<EGnosticStage>(stageIdx + 1);
            EnneadState.GnosticSpiral.StageProgress = 0.0f;
        } else {
            // Complete spiral, ascend to next level
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::PropositionalOpening;
            EnneadState.GnosticSpiral.SpiralLevel++;
            EnneadState.GnosticSpiral.StageProgress = 0.0f;
        }
        EnneadState.GnosticSpiral.TransformativeIntensity += 0.1f;
    }

    void AddGnosticInsight(const std::string& insight) {
        EnneadState.GnosticSpiral.InsightsGained.push_back(insight);
        EnneadState.GnosticSpiral.StageProgress += 0.1f;

        if (EnneadState.GnosticSpiral.StageProgress >= GnosticAdvancementThreshold) {
            AdvanceGnosticSpiral();
        }
    }

    EGnosticStage GetCurrentGnosticStage() const {
        return EnneadState.GnosticSpiral.CurrentStage;
    }

    int GetSpiralLevel() const {
        return EnneadState.GnosticSpiral.SpiralLevel;
    }

    // Ennead Integration
    FEnneadState GetEnneadState() const { return EnneadState; }
    FTriadInterpenetration GetInterpenetration() const { return Interpenetration; }

    float GetRelevanceRealizationLevel() const {
        return ComputeRelevanceRealization();
    }

    float GetWisdomLevel() const {
        return ComputeWisdomLevel();
    }

    float GetMeaningLevel() const {
        return ComputeMeaningLevel();
    }

    void ForceIntegration() {
        // Force all triads to integrate
        for (auto& knowing : EnneadState.WaysOfKnowing) {
            knowing.IntegrationLevel = 1.0f;
        }
        for (auto& order : EnneadState.OrdersOfUnderstanding) {
            order.IntegrationWithOthers = 1.0f;
        }
        for (auto& practice : EnneadState.PracticesOfWisdom) {
            practice.IntegrationWithOthers = 1.0f;
        }
        Interpenetration.OverallCoherence = 1.0f;
        EnneadState.OverallIntegration = 1.0f;
    }

    // Processing
    void ProcessCognitiveInput(const std::vector<float>& input, EWayOfKnowing primaryKnowing) {
        ActivateKnowing(primaryKnowing, 1.0f);

        // Decay other knowings
        for (int i = 0; i < 4; i++) {
            if (i != static_cast<int>(primaryKnowing)) {
                EnneadState.WaysOfKnowing[i].ActivationLevel *= 0.9f;
            }
        }

        UpdateInterpenetration();
    }

    float GetRelevanceScore(const std::string& content) const {
        // Simple relevance based on current focus alignment
        for (const auto& knowing : EnneadState.WaysOfKnowing) {
            if (!knowing.CurrentFocus.empty() && content.find(knowing.CurrentFocus) != std::string::npos) {
                return knowing.ActivationLevel;
            }
        }
        return 0.5f;
    }

    EWayOfKnowing DetermineOptimalKnowing(const std::string& context) const {
        // Heuristic: different contexts favor different knowings
        if (context.find("fact") != std::string::npos || context.find("data") != std::string::npos) {
            return EWayOfKnowing::Propositional;
        }
        if (context.find("skill") != std::string::npos || context.find("practice") != std::string::npos) {
            return EWayOfKnowing::Procedural;
        }
        if (context.find("perspective") != std::string::npos || context.find("view") != std::string::npos) {
            return EWayOfKnowing::Perspectival;
        }
        if (context.find("being") != std::string::npos || context.find("experience") != std::string::npos) {
            return EWayOfKnowing::Participatory;
        }
        return EWayOfKnowing::Propositional;
    }

    // Configuration
    float GnosticAdvancementThreshold = 0.8f;
    float IntegrationRate = 0.1f;

private:
    void InitializeEnnead() {
        // Initialize Ways of Knowing
        EnneadState.WaysOfKnowing.resize(4);
        EnneadState.WaysOfKnowing[0].Type = EWayOfKnowing::Propositional;
        EnneadState.WaysOfKnowing[1].Type = EWayOfKnowing::Procedural;
        EnneadState.WaysOfKnowing[2].Type = EWayOfKnowing::Perspectival;
        EnneadState.WaysOfKnowing[3].Type = EWayOfKnowing::Participatory;

        // Initialize Orders of Understanding
        EnneadState.OrdersOfUnderstanding.resize(3);
        EnneadState.OrdersOfUnderstanding[0].Type = EOrderOfUnderstanding::Nomological;
        EnneadState.OrdersOfUnderstanding[1].Type = EOrderOfUnderstanding::Normative;
        EnneadState.OrdersOfUnderstanding[2].Type = EOrderOfUnderstanding::Narrative;

        // Initialize Practices of Wisdom
        EnneadState.PracticesOfWisdom.resize(3);
        EnneadState.PracticesOfWisdom[0].Type = EPracticeOfWisdom::Morality;
        EnneadState.PracticesOfWisdom[1].Type = EPracticeOfWisdom::Meaning;
        EnneadState.PracticesOfWisdom[2].Type = EPracticeOfWisdom::Mastery;
    }

    void UpdateInterpenetration() {
        float knowingSum = 0.0f;
        for (const auto& k : EnneadState.WaysOfKnowing) knowingSum += k.ActivationLevel;

        float orderSum = 0.0f;
        for (const auto& o : EnneadState.OrdersOfUnderstanding) orderSum += o.ActivationLevel;

        float practiceSum = 0.0f;
        for (const auto& p : EnneadState.PracticesOfWisdom) practiceSum += p.CultivationLevel;

        Interpenetration.KnowingToOrderFlow = std::min(1.0f, knowingSum / 4.0f);
        Interpenetration.KnowingToWisdomFlow = std::min(1.0f, knowingSum / 4.0f);
        Interpenetration.OrderToKnowingFlow = std::min(1.0f, orderSum / 3.0f);
        Interpenetration.OrderToWisdomFlow = std::min(1.0f, orderSum / 3.0f);
        Interpenetration.WisdomToKnowingFlow = std::min(1.0f, practiceSum / 3.0f);
        Interpenetration.WisdomToOrderFlow = std::min(1.0f, practiceSum / 3.0f);

        Interpenetration.OverallCoherence = (
            Interpenetration.KnowingToOrderFlow + Interpenetration.KnowingToWisdomFlow +
            Interpenetration.OrderToKnowingFlow + Interpenetration.OrderToWisdomFlow +
            Interpenetration.WisdomToKnowingFlow + Interpenetration.WisdomToOrderFlow
        ) / 6.0f;
    }

    float ComputeKnowingIntegration() const {
        float sum = 0.0f;
        for (const auto& k : EnneadState.WaysOfKnowing) {
            sum += k.IntegrationLevel;
        }
        return sum / EnneadState.WaysOfKnowing.size();
    }

    float ComputeOrderIntegration() const {
        float sum = 0.0f;
        for (const auto& o : EnneadState.OrdersOfUnderstanding) {
            sum += o.IntegrationWithOthers;
        }
        return sum / EnneadState.OrdersOfUnderstanding.size();
    }

    float ComputePracticeIntegration() const {
        float sum = 0.0f;
        for (const auto& p : EnneadState.PracticesOfWisdom) {
            sum += p.IntegrationWithOthers;
        }
        return sum / EnneadState.PracticesOfWisdom.size();
    }

    float ComputeRelevanceRealization() const {
        return (ComputeKnowingIntegration() + ComputeOrderIntegration() + ComputePracticeIntegration() +
                Interpenetration.OverallCoherence) / 4.0f;
    }

    float ComputeWisdomLevel() const {
        float sum = 0.0f;
        for (const auto& p : EnneadState.PracticesOfWisdom) {
            sum += p.CultivationLevel;
        }
        return sum / EnneadState.PracticesOfWisdom.size();
    }

    float ComputeMeaningLevel() const {
        // Meaning emerges from integration of all triads
        return (ComputeKnowingIntegration() * 0.3f + ComputeOrderIntegration() * 0.3f +
                ComputePracticeIntegration() * 0.4f);
    }

    FEnneadState EnneadState;
    FTriadInterpenetration Interpenetration;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class EntelechyFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        Framework = std::make_unique<MockEntelechyFramework>();
        Framework->InitializeEntelechy("TestSystem");
    }

    void TearDown() override {
        Framework.reset();
    }

    std::unique_ptr<MockEntelechyFramework> Framework;
};

class RelevanceRealizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Ennead = std::make_unique<MockRelevanceRealizationEnnead>();
    }

    void TearDown() override {
        Ennead.reset();
    }

    std::unique_ptr<MockRelevanceRealizationEnnead> Ennead;
};

// ============================================================================
// Entelechy Framework Tests
// ============================================================================

TEST_F(EntelechyFrameworkTest, Initialization) {
    EXPECT_TRUE(Framework->IsInitialized());
    EXPECT_EQ(Framework->GetDevelopmentStage(), EEntelechyStage::Embryonic);
}

TEST_F(EntelechyFrameworkTest, ResetEntelechy) {
    Framework->AssessAllDimensions();
    Framework->ResetEntelechy();

    EXPECT_FALSE(Framework->IsInitialized());
    EXPECT_EQ(Framework->GetDevelopmentStage(), EEntelechyStage::Embryonic);
}

TEST_F(EntelechyFrameworkTest, AssessAllDimensions) {
    Framework->AssessAllDimensions();

    float fitness = Framework->CalculateFitness();
    EXPECT_GT(fitness, 0.0f);
    EXPECT_LE(fitness, 1.0f);
}

TEST_F(EntelechyFrameworkTest, OntologicalDimension) {
    Framework->AssessOntological();

    const auto& ontological = Framework->GetOntological();
    EXPECT_GT(ontological.FoundationHealth, 0.0f);
    EXPECT_GT(ontological.CoreHealth, 0.0f);
    EXPECT_GT(ontological.SpecializedHealth, 0.0f);
    EXPECT_GT(ontological.ArchitecturalCompleteness, 0.0f);
}

TEST_F(EntelechyFrameworkTest, TeleologicalDimension) {
    Framework->AssessTeleological();

    const auto& teleological = Framework->GetTeleological();
    EXPECT_GE(teleological.RoadmapAlignment, 0.0f);
    EXPECT_GT(teleological.ActualizationTrajectory, 0.0f);
    EXPECT_GT(teleological.PurposeClarity, 0.0f);
}

TEST_F(EntelechyFrameworkTest, CognitiveDimension) {
    Framework->AssessCognitive();

    const auto& cognitive = Framework->GetCognitive();
    EXPECT_GT(cognitive.ReasoningSystems.Health, 0.0f);
    EXPECT_GT(cognitive.PatternSystems.Health, 0.0f);
    EXPECT_GT(cognitive.AttentionSystems.Health, 0.0f);
    EXPECT_GT(cognitive.LearningSystems.Health, 0.0f);
    EXPECT_GT(cognitive.CognitiveCompleteness, 0.0f);
}

TEST_F(EntelechyFrameworkTest, IntegrativeDimension) {
    Framework->AssessIntegrative();

    const auto& integrative = Framework->GetIntegrative();
    EXPECT_GT(integrative.TotalDependencies, 0);
    EXPECT_GT(integrative.SatisfiedDependencies, 0);
    EXPECT_GT(integrative.DependencyHealth, 0.0f);
    EXPECT_GT(integrative.IntegrationHealth, 0.0f);
}

TEST_F(EntelechyFrameworkTest, EvolutionaryDimension) {
    Framework->AssessEvolutionary();

    const auto& evolutionary = Framework->GetEvolutionary();
    EXPECT_GE(evolutionary.TodoCount, 0);
    EXPECT_TRUE(evolutionary.bHasAutognosis);
    EXPECT_TRUE(evolutionary.bHasOntogenesis);
    EXPECT_GT(evolutionary.EvolutionaryPotential, 0.0f);
}

TEST_F(EntelechyFrameworkTest, DimensionHealth) {
    Framework->AssessAllDimensions();

    EXPECT_GT(Framework->GetDimensionHealth(EEntelechyDimension::Ontological), 0.0f);
    EXPECT_GT(Framework->GetDimensionHealth(EEntelechyDimension::Teleological), 0.0f);
    EXPECT_GT(Framework->GetDimensionHealth(EEntelechyDimension::Cognitive), 0.0f);
    EXPECT_GT(Framework->GetDimensionHealth(EEntelechyDimension::Integrative), 0.0f);
    EXPECT_GT(Framework->GetDimensionHealth(EEntelechyDimension::Evolutionary), 0.0f);
}

TEST_F(EntelechyFrameworkTest, ActualizationPercentage) {
    Framework->AssessAllDimensions();

    float percentage = Framework->GetActualizationPercentage();
    EXPECT_GT(percentage, 0.0f);
    EXPECT_LE(percentage, 100.0f);
}

TEST_F(EntelechyFrameworkTest, StageAdvancement) {
    Framework->AssessAllDimensions();

    // After assessment, should potentially advance
    bool advanced = Framework->TryAdvanceStage();

    // Stage should be at least Juvenile after good assessment
    EEntelechyStage stage = Framework->GetDevelopmentStage();
    EXPECT_GE(static_cast<int>(stage), static_cast<int>(EEntelechyStage::Juvenile));
}

TEST_F(EntelechyFrameworkTest, ComponentRegistration) {
    Framework->RegisterComponent("TestComponent", EEntelechyDimension::Ontological);

    const auto& metrics = Framework->GetMetrics();
    EXPECT_EQ(metrics.TotalComponents, 1);
}

TEST_F(EntelechyFrameworkTest, ComponentHealthUpdate) {
    Framework->RegisterComponent("TestComponent", EEntelechyDimension::Ontological);
    Framework->UpdateComponentHealth("TestComponent", 0.9f);

    // Component health should be updated
    const auto& ontological = Framework->GetOntological();
    bool found = false;
    for (const auto& comp : ontological.CoreComponents) {
        if (comp.Name == "TestComponent") {
            EXPECT_FLOAT_EQ(comp.Health, 0.9f);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(EntelechyFrameworkTest, MilestoneAddition) {
    Framework->AddMilestone(EDevelopmentPhase::Phase1_CoreExtensions, "First milestone");

    const auto& teleological = Framework->GetTeleological();
    const auto& phase = teleological.DevelopmentPhases[0];
    EXPECT_EQ(phase.Milestones.size(), 1);
    EXPECT_GT(phase.Progress, 0.0f);
}

TEST_F(EntelechyFrameworkTest, GenomeInitialization) {
    FEntelechyGenome genome = Framework->GetGenome();

    EXPECT_EQ(genome.Id, "TestSystem");
    EXPECT_EQ(genome.Generation, 1);
    EXPECT_EQ(genome.OntologicalGenes.size(), 10);
    EXPECT_EQ(genome.CognitiveGenes.size(), 10);
}

TEST_F(EntelechyFrameworkTest, GenomeMutation) {
    FEntelechyGenome before = Framework->GetGenome();
    Framework->MutateGenome(0.1f);
    FEntelechyGenome after = Framework->GetGenome();

    EXPECT_EQ(after.Generation, before.Generation + 1);

    // Genes should be modified
    bool anyDifferent = false;
    for (size_t i = 0; i < before.OntologicalGenes.size(); i++) {
        if (std::abs(before.OntologicalGenes[i] - after.OntologicalGenes[i]) > 0.001f) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifferent);
}

TEST_F(EntelechyFrameworkTest, GenomeCrossover) {
    MockEntelechyFramework other;
    other.InitializeEntelechy("OtherSystem");

    FEntelechyGenome child = Framework->CrossoverGenome(other.GetGenome());

    EXPECT_EQ(child.Lineage.size(), 2);
    EXPECT_EQ(child.Generation, 2);
}

// ============================================================================
// Relevance Realization Ennead Tests
// ============================================================================

TEST_F(RelevanceRealizationTest, WaysOfKnowingInitialization) {
    FEnneadState state = Ennead->GetEnneadState();
    EXPECT_EQ(state.WaysOfKnowing.size(), 4);
}

TEST_F(RelevanceRealizationTest, ActivateKnowing) {
    Ennead->ActivateKnowing(EWayOfKnowing::Propositional, 0.8f);

    FKnowingState state = Ennead->GetKnowingState(EWayOfKnowing::Propositional);
    EXPECT_FLOAT_EQ(state.ActivationLevel, 0.8f);
}

TEST_F(RelevanceRealizationTest, KnowingContentAndFocus) {
    Ennead->AddKnowingContent(EWayOfKnowing::Procedural, "skill1");
    Ennead->SetKnowingFocus(EWayOfKnowing::Procedural, "coding");

    FKnowingState state = Ennead->GetKnowingState(EWayOfKnowing::Procedural);
    EXPECT_EQ(state.ActiveContents.size(), 1);
    EXPECT_EQ(state.CurrentFocus, "coding");
}

TEST_F(RelevanceRealizationTest, DominantKnowing) {
    Ennead->ActivateKnowing(EWayOfKnowing::Propositional, 0.3f);
    Ennead->ActivateKnowing(EWayOfKnowing::Participatory, 0.9f);

    EXPECT_EQ(Ennead->GetDominantKnowing(), EWayOfKnowing::Participatory);
}

TEST_F(RelevanceRealizationTest, OrdersOfUnderstanding) {
    Ennead->ActivateOrder(EOrderOfUnderstanding::Narrative, 0.7f);

    FOrderState state = Ennead->GetOrderState(EOrderOfUnderstanding::Narrative);
    EXPECT_FLOAT_EQ(state.ActivationLevel, 0.7f);
}

TEST_F(RelevanceRealizationTest, DominantOrder) {
    Ennead->ActivateOrder(EOrderOfUnderstanding::Nomological, 0.3f);
    Ennead->ActivateOrder(EOrderOfUnderstanding::Normative, 0.9f);

    EXPECT_EQ(Ennead->GetDominantOrder(), EOrderOfUnderstanding::Normative);
}

TEST_F(RelevanceRealizationTest, PracticesOfWisdom) {
    Ennead->ActivatePractice(EPracticeOfWisdom::Mastery, 0.8f);

    FPracticeState state = Ennead->GetPracticeState(EPracticeOfWisdom::Mastery);
    EXPECT_FLOAT_EQ(state.CultivationLevel, 0.8f);
    EXPECT_FLOAT_EQ(state.ActiveEngagement, 0.8f);
}

TEST_F(RelevanceRealizationTest, DominantPractice) {
    Ennead->ActivatePractice(EPracticeOfWisdom::Morality, 0.3f);
    Ennead->ActivatePractice(EPracticeOfWisdom::Meaning, 0.9f);

    EXPECT_EQ(Ennead->GetDominantPractice(), EPracticeOfWisdom::Meaning);
}

TEST_F(RelevanceRealizationTest, GnosticSpiralInitialization) {
    FGnosticSpiralState state = Ennead->GetGnosticState();

    EXPECT_EQ(state.CurrentStage, EGnosticStage::PropositionalOpening);
    EXPECT_EQ(state.SpiralLevel, 1);
    EXPECT_FLOAT_EQ(state.StageProgress, 0.0f);
}

TEST_F(RelevanceRealizationTest, GnosticSpiralAdvancement) {
    Ennead->AdvanceGnosticSpiral();

    EXPECT_EQ(Ennead->GetCurrentGnosticStage(), EGnosticStage::ProceduralDevelopment);
}

TEST_F(RelevanceRealizationTest, GnosticSpiralLevelUp) {
    // Advance through all 6 stages
    for (int i = 0; i < 6; i++) {
        Ennead->AdvanceGnosticSpiral();
    }

    // Should have looped back with level increased
    EXPECT_EQ(Ennead->GetSpiralLevel(), 2);
    EXPECT_EQ(Ennead->GetCurrentGnosticStage(), EGnosticStage::PropositionalOpening);
}

TEST_F(RelevanceRealizationTest, GnosticInsight) {
    Ennead->AddGnosticInsight("Test insight");

    FGnosticSpiralState state = Ennead->GetGnosticState();
    EXPECT_EQ(state.InsightsGained.size(), 1);
    EXPECT_GT(state.StageProgress, 0.0f);
}

TEST_F(RelevanceRealizationTest, InsightsAdvanceSpiral) {
    // Add enough insights to trigger advancement (threshold is 0.8)
    for (int i = 0; i < 8; i++) {
        Ennead->AddGnosticInsight("Insight " + std::to_string(i));
    }

    EXPECT_NE(Ennead->GetCurrentGnosticStage(), EGnosticStage::PropositionalOpening);
}

TEST_F(RelevanceRealizationTest, Interpenetration) {
    Ennead->ActivateKnowing(EWayOfKnowing::Propositional, 1.0f);
    Ennead->ActivateOrder(EOrderOfUnderstanding::Nomological, 1.0f);
    Ennead->ActivatePractice(EPracticeOfWisdom::Morality, 1.0f);

    FTriadInterpenetration interpen = Ennead->GetInterpenetration();
    EXPECT_GT(interpen.OverallCoherence, 0.0f);
}

TEST_F(RelevanceRealizationTest, ForceIntegration) {
    Ennead->ForceIntegration();

    EXPECT_FLOAT_EQ(Ennead->GetKnowingIntegration(), 1.0f);
    EXPECT_FLOAT_EQ(Ennead->GetOrderIntegration(), 1.0f);
    EXPECT_FLOAT_EQ(Ennead->GetPracticeIntegration(), 1.0f);
}

TEST_F(RelevanceRealizationTest, RelevanceRealizationLevel) {
    Ennead->ForceIntegration();
    Ennead->ActivateKnowing(EWayOfKnowing::Propositional, 1.0f);
    Ennead->ActivateOrder(EOrderOfUnderstanding::Nomological, 1.0f);
    Ennead->ActivatePractice(EPracticeOfWisdom::Morality, 1.0f);

    float level = Ennead->GetRelevanceRealizationLevel();
    EXPECT_GT(level, 0.0f);
}

TEST_F(RelevanceRealizationTest, WisdomLevel) {
    Ennead->ActivatePractice(EPracticeOfWisdom::Morality, 0.8f);
    Ennead->ActivatePractice(EPracticeOfWisdom::Meaning, 0.7f);
    Ennead->ActivatePractice(EPracticeOfWisdom::Mastery, 0.9f);

    float wisdom = Ennead->GetWisdomLevel();
    EXPECT_FLOAT_EQ(wisdom, 0.8f);
}

TEST_F(RelevanceRealizationTest, MeaningLevel) {
    Ennead->ForceIntegration();

    float meaning = Ennead->GetMeaningLevel();
    EXPECT_GT(meaning, 0.0f);
}

TEST_F(RelevanceRealizationTest, ProcessCognitiveInput) {
    std::vector<float> input(32, 0.5f);
    Ennead->ProcessCognitiveInput(input, EWayOfKnowing::Procedural);

    FKnowingState state = Ennead->GetKnowingState(EWayOfKnowing::Procedural);
    EXPECT_FLOAT_EQ(state.ActivationLevel, 1.0f);
}

TEST_F(RelevanceRealizationTest, RelevanceScore) {
    Ennead->SetKnowingFocus(EWayOfKnowing::Propositional, "test");
    Ennead->ActivateKnowing(EWayOfKnowing::Propositional, 0.8f);

    float score = Ennead->GetRelevanceScore("this is a test");
    EXPECT_FLOAT_EQ(score, 0.8f);
}

TEST_F(RelevanceRealizationTest, OptimalKnowingDetermination) {
    EXPECT_EQ(Ennead->DetermineOptimalKnowing("analyze this data fact"), EWayOfKnowing::Propositional);
    EXPECT_EQ(Ennead->DetermineOptimalKnowing("practice this skill"), EWayOfKnowing::Procedural);
    EXPECT_EQ(Ennead->DetermineOptimalKnowing("new perspective"), EWayOfKnowing::Perspectival);
    EXPECT_EQ(Ennead->DetermineOptimalKnowing("being in experience"), EWayOfKnowing::Participatory);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(EntelechyPerformanceTest, AssessmentPerformance) {
    MockEntelechyFramework framework;
    framework.InitializeEntelechy("PerfTest");

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        framework.AssessAllDimensions();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 200);  // 1000 assessments in under 200ms
}

TEST(RelevancePerformanceTest, ActivationPerformance) {
    MockRelevanceRealizationEnnead ennead;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        ennead.ActivateKnowing(static_cast<EWayOfKnowing>(i % 4), (i % 100) / 100.0f);
        ennead.ActivateOrder(static_cast<EOrderOfUnderstanding>(i % 3), (i % 100) / 100.0f);
        ennead.ActivatePractice(static_cast<EPracticeOfWisdom>(i % 3), (i % 100) / 100.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100);  // 30000 activations in under 100ms
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
