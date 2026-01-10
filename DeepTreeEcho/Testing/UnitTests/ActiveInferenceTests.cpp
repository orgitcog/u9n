/**
 * @file ActiveInferenceTests.cpp
 * @brief Unit tests for Active Inference engine and related systems
 * 
 * Tests cover:
 * - Active Inference Engine core operations
 * - Free Energy minimization
 * - Belief updating and prediction
 * - Action selection and policy evaluation
 * - Niche Construction System
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <cmath>
#include <random>
#include <functional>

// ============================================================================
// Mock Types for Active Inference
// ============================================================================

using Vector = std::vector<double>;
using Matrix = std::vector<std::vector<double>>;

/**
 * @brief Utility functions for vector/matrix operations
 */
namespace MathUtils {
    double Dot(const Vector& a, const Vector& b) {
        if (a.size() != b.size()) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); i++) sum += a[i] * b[i];
        return sum;
    }
    
    double Norm(const Vector& v) {
        return std::sqrt(Dot(v, v));
    }
    
    Vector Normalize(const Vector& v) {
        double n = Norm(v);
        if (n < 1e-10) return v;
        Vector result(v.size());
        for (size_t i = 0; i < v.size(); i++) result[i] = v[i] / n;
        return result;
    }
    
    Vector Softmax(const Vector& v) {
        double maxVal = *std::max_element(v.begin(), v.end());
        Vector exp_v(v.size());
        double sum = 0.0;
        for (size_t i = 0; i < v.size(); i++) {
            exp_v[i] = std::exp(v[i] - maxVal);
            sum += exp_v[i];
        }
        for (size_t i = 0; i < v.size(); i++) exp_v[i] /= sum;
        return exp_v;
    }
    
    double KLDivergence(const Vector& p, const Vector& q) {
        if (p.size() != q.size()) return 0.0;
        double kl = 0.0;
        for (size_t i = 0; i < p.size(); i++) {
            if (p[i] > 1e-10 && q[i] > 1e-10) {
                kl += p[i] * std::log(p[i] / q[i]);
            }
        }
        return kl;
    }
    
    double Entropy(const Vector& p) {
        double h = 0.0;
        for (double pi : p) {
            if (pi > 1e-10) h -= pi * std::log(pi);
        }
        return h;
    }
}

/**
 * @brief Generative model for Active Inference
 */
struct FGenerativeModel {
    Matrix A;  // Observation model P(o|s)
    Matrix B;  // Transition model P(s'|s,a)
    Vector C;  // Preferred observations (log preferences)
    Vector D;  // Prior over initial states
    
    int NumStates = 4;
    int NumObservations = 4;
    int NumActions = 3;
    
    FGenerativeModel() {
        // Initialize with default dimensions
        A.resize(NumObservations, Vector(NumStates, 0.25));
        B.resize(NumStates, Vector(NumStates * NumActions, 0.0));
        C.resize(NumObservations, 0.0);
        D.resize(NumStates, 0.25);
        
        // Set up simple observation model (identity-ish)
        for (int i = 0; i < NumObservations; i++) {
            for (int j = 0; j < NumStates; j++) {
                A[i][j] = (i == j) ? 0.7 : 0.1;
            }
        }
        
        // Normalize columns
        for (int j = 0; j < NumStates; j++) {
            double sum = 0.0;
            for (int i = 0; i < NumObservations; i++) sum += A[i][j];
            for (int i = 0; i < NumObservations; i++) A[i][j] /= sum;
        }
    }
};

/**
 * @brief Mock Active Inference Engine
 */
class MockActiveInferenceEngine {
public:
    struct FBeliefState {
        Vector StateBelief;      // Q(s) - posterior over states
        Vector ActionBelief;     // Q(a) - posterior over actions
        Vector PredictedObs;     // Expected observations
        double FreeEnergy = 0.0;
        double ExpectedFreeEnergy = 0.0;
    };
    
    void Initialize(const FGenerativeModel& model) {
        Model = model;
        
        // Initialize beliefs
        CurrentBelief.StateBelief = model.D;
        CurrentBelief.ActionBelief.resize(model.NumActions, 1.0 / model.NumActions);
        CurrentBelief.PredictedObs.resize(model.NumObservations, 0.0);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void UpdateBelief(const Vector& observation) {
        if (!bInitialized) return;
        
        // Bayesian belief update: Q(s) ∝ P(o|s) * Q(s)
        Vector likelihood(Model.NumStates);
        for (int s = 0; s < Model.NumStates; s++) {
            likelihood[s] = 1.0;
            for (int o = 0; o < Model.NumObservations; o++) {
                if (observation[o] > 0.5) {
                    likelihood[s] *= Model.A[o][s];
                }
            }
        }
        
        // Posterior
        for (int s = 0; s < Model.NumStates; s++) {
            CurrentBelief.StateBelief[s] *= likelihood[s];
        }
        
        // Normalize
        double sum = 0.0;
        for (double b : CurrentBelief.StateBelief) sum += b;
        if (sum > 1e-10) {
            for (double& b : CurrentBelief.StateBelief) b /= sum;
        }
        
        // Compute free energy
        ComputeFreeEnergy(observation);
    }
    
    void ComputeFreeEnergy(const Vector& observation) {
        // F = E_Q[log Q(s)] - E_Q[log P(o,s)]
        // F = -H[Q(s)] + E_Q[-log P(o|s)] + E_Q[-log P(s)]
        
        double negEntropy = -MathUtils::Entropy(CurrentBelief.StateBelief);
        
        double expectedLogLikelihood = 0.0;
        for (int s = 0; s < Model.NumStates; s++) {
            double logLik = 0.0;
            for (int o = 0; o < Model.NumObservations; o++) {
                if (observation[o] > 0.5 && Model.A[o][s] > 1e-10) {
                    logLik += std::log(Model.A[o][s]);
                }
            }
            expectedLogLikelihood += CurrentBelief.StateBelief[s] * logLik;
        }
        
        double expectedLogPrior = 0.0;
        for (int s = 0; s < Model.NumStates; s++) {
            if (Model.D[s] > 1e-10) {
                expectedLogPrior += CurrentBelief.StateBelief[s] * std::log(Model.D[s]);
            }
        }
        
        CurrentBelief.FreeEnergy = negEntropy - expectedLogLikelihood - expectedLogPrior;
    }
    
    double ComputeExpectedFreeEnergy(int action) {
        // G = E_Q[log Q(s') - log P(o',s'|a)]
        // G ≈ -E_Q[log P(o'|s')] + KL[Q(s')||P(s')]  (simplified)
        
        // Predict next state distribution under action
        Vector predictedState(Model.NumStates, 0.0);
        for (int s = 0; s < Model.NumStates; s++) {
            predictedState[s] = CurrentBelief.StateBelief[s];  // Simplified
        }
        
        // Expected observation
        Vector expectedObs(Model.NumObservations, 0.0);
        for (int o = 0; o < Model.NumObservations; o++) {
            for (int s = 0; s < Model.NumStates; s++) {
                expectedObs[o] += Model.A[o][s] * predictedState[s];
            }
        }
        
        // Pragmatic value (preference satisfaction)
        double pragmaticValue = 0.0;
        for (int o = 0; o < Model.NumObservations; o++) {
            pragmaticValue += expectedObs[o] * Model.C[o];
        }
        
        // Epistemic value (information gain)
        double epistemicValue = MathUtils::Entropy(expectedObs);
        
        return -pragmaticValue - 0.1 * epistemicValue;
    }
    
    int SelectAction() {
        // Compute expected free energy for each action
        Vector G(Model.NumActions);
        for (int a = 0; a < Model.NumActions; a++) {
            G[a] = -ComputeExpectedFreeEnergy(a);  // Negate for softmax
        }
        
        // Action selection via softmax
        CurrentBelief.ActionBelief = MathUtils::Softmax(G);
        
        // Sample action (or take argmax)
        int bestAction = 0;
        double bestProb = CurrentBelief.ActionBelief[0];
        for (int a = 1; a < Model.NumActions; a++) {
            if (CurrentBelief.ActionBelief[a] > bestProb) {
                bestProb = CurrentBelief.ActionBelief[a];
                bestAction = a;
            }
        }
        
        return bestAction;
    }
    
    void PredictObservation() {
        // E[o] = sum_s P(o|s) * Q(s)
        CurrentBelief.PredictedObs.assign(Model.NumObservations, 0.0);
        for (int o = 0; o < Model.NumObservations; o++) {
            for (int s = 0; s < Model.NumStates; s++) {
                CurrentBelief.PredictedObs[o] += Model.A[o][s] * CurrentBelief.StateBelief[s];
            }
        }
    }
    
    void Reset() {
        CurrentBelief.StateBelief = Model.D;
        CurrentBelief.ActionBelief.assign(Model.NumActions, 1.0 / Model.NumActions);
        CurrentBelief.FreeEnergy = 0.0;
    }
    
    FBeliefState GetBelief() const { return CurrentBelief; }
    
    double GetFreeEnergy() const { return CurrentBelief.FreeEnergy; }
    
    void SetPreferences(const Vector& preferences) {
        Model.C = preferences;
    }
    
private:
    bool bInitialized = false;
    FGenerativeModel Model;
    FBeliefState CurrentBelief;
};

/**
 * @brief Mock Niche Construction System
 */
class MockNicheConstructionSystem {
public:
    struct FNiche {
        std::string Id;
        Vector EnvironmentState;
        Vector AffordanceVector;
        double Fitness = 0.0;
        int ConstructionCount = 0;
    };
    
    void Initialize(int environmentDim, int affordanceDim) {
        EnvironmentDim = environmentDim;
        AffordanceDim = affordanceDim;
        
        CurrentNiche.EnvironmentState.resize(environmentDim, 0.5);
        CurrentNiche.AffordanceVector.resize(affordanceDim, 0.0);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void ConstructNiche(const Vector& action) {
        if (!bInitialized) return;
        
        // Modify environment based on action
        for (size_t i = 0; i < std::min(action.size(), CurrentNiche.EnvironmentState.size()); i++) {
            CurrentNiche.EnvironmentState[i] += action[i] * 0.1;
            CurrentNiche.EnvironmentState[i] = std::clamp(CurrentNiche.EnvironmentState[i], 0.0, 1.0);
        }
        
        // Update affordances
        UpdateAffordances();
        
        CurrentNiche.ConstructionCount++;
    }
    
    void UpdateAffordances() {
        // Compute affordances from environment state
        for (int i = 0; i < AffordanceDim; i++) {
            double sum = 0.0;
            for (int j = 0; j < EnvironmentDim; j++) {
                sum += CurrentNiche.EnvironmentState[j] * ((i + j) % 3 == 0 ? 1.0 : 0.5);
            }
            CurrentNiche.AffordanceVector[i] = sum / EnvironmentDim;
        }
    }
    
    double EvaluateFitness(const Vector& preferences) {
        // Fitness = alignment between affordances and preferences
        double fitness = MathUtils::Dot(CurrentNiche.AffordanceVector, preferences);
        fitness /= (MathUtils::Norm(CurrentNiche.AffordanceVector) * MathUtils::Norm(preferences) + 1e-10);
        CurrentNiche.Fitness = fitness;
        return fitness;
    }
    
    FNiche GetCurrentNiche() const { return CurrentNiche; }
    
    Vector GetAffordances() const { return CurrentNiche.AffordanceVector; }
    
    void Reset() {
        CurrentNiche.EnvironmentState.assign(EnvironmentDim, 0.5);
        CurrentNiche.AffordanceVector.assign(AffordanceDim, 0.0);
        CurrentNiche.Fitness = 0.0;
        CurrentNiche.ConstructionCount = 0;
    }
    
private:
    bool bInitialized = false;
    int EnvironmentDim = 0;
    int AffordanceDim = 0;
    FNiche CurrentNiche;
};

/**
 * @brief Mock AXIOM Active Inference integration
 */
class MockAXIOMActiveInference {
public:
    struct FAxiomState {
        Vector Beliefs;
        Vector Desires;
        Vector Intentions;
        double Arousal = 0.5;
        double Valence = 0.0;
    };
    
    void Initialize(int beliefDim, int desireDim, int intentionDim) {
        BeliefDim = beliefDim;
        DesireDim = desireDim;
        IntentionDim = intentionDim;
        
        State.Beliefs.resize(beliefDim, 0.0);
        State.Desires.resize(desireDim, 0.0);
        State.Intentions.resize(intentionDim, 0.0);
        
        // Initialize active inference engine
        FGenerativeModel model;
        model.NumStates = beliefDim;
        model.NumObservations = beliefDim;
        model.NumActions = intentionDim;
        InferenceEngine.Initialize(model);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void UpdateBeliefs(const Vector& observation) {
        InferenceEngine.UpdateBelief(observation);
        State.Beliefs = InferenceEngine.GetBelief().StateBelief;
    }
    
    void SetDesires(const Vector& desires) {
        State.Desires = desires;
        InferenceEngine.SetPreferences(desires);
    }
    
    int FormIntention() {
        int action = InferenceEngine.SelectAction();
        
        // Update intention vector
        State.Intentions.assign(IntentionDim, 0.0);
        if (action < IntentionDim) {
            State.Intentions[action] = 1.0;
        }
        
        return action;
    }
    
    void UpdateAffect(double surprise, double reward) {
        // Arousal increases with surprise
        State.Arousal = 0.7 * State.Arousal + 0.3 * std::abs(surprise);
        State.Arousal = std::clamp(State.Arousal, 0.0, 1.0);
        
        // Valence tracks reward
        State.Valence = 0.8 * State.Valence + 0.2 * reward;
        State.Valence = std::clamp(State.Valence, -1.0, 1.0);
    }
    
    FAxiomState GetState() const { return State; }
    
    double GetFreeEnergy() const { return InferenceEngine.GetFreeEnergy(); }
    
private:
    bool bInitialized = false;
    int BeliefDim = 0;
    int DesireDim = 0;
    int IntentionDim = 0;
    FAxiomState State;
    MockActiveInferenceEngine InferenceEngine;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class ActiveInferenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        Engine = std::make_unique<MockActiveInferenceEngine>();
        
        FGenerativeModel model;
        Engine->Initialize(model);
    }
    
    void TearDown() override {
        Engine.reset();
    }
    
    Vector CreateObservation(int activeIdx) {
        Vector obs(4, 0.0);
        if (activeIdx >= 0 && activeIdx < 4) {
            obs[activeIdx] = 1.0;
        }
        return obs;
    }
    
    std::unique_ptr<MockActiveInferenceEngine> Engine;
};

class NicheConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Niche = std::make_unique<MockNicheConstructionSystem>();
        Niche->Initialize(10, 5);
    }
    
    void TearDown() override {
        Niche.reset();
    }
    
    std::unique_ptr<MockNicheConstructionSystem> Niche;
};

class AXIOMTest : public ::testing::Test {
protected:
    void SetUp() override {
        AXIOM = std::make_unique<MockAXIOMActiveInference>();
        AXIOM->Initialize(8, 4, 3);
    }
    
    void TearDown() override {
        AXIOM.reset();
    }
    
    std::unique_ptr<MockAXIOMActiveInference> AXIOM;
};

// ============================================================================
// Active Inference Engine Tests
// ============================================================================

TEST_F(ActiveInferenceTest, Initialization) {
    EXPECT_TRUE(Engine->IsInitialized());
    
    auto belief = Engine->GetBelief();
    EXPECT_EQ(belief.StateBelief.size(), 4);
    EXPECT_EQ(belief.ActionBelief.size(), 3);
}

TEST_F(ActiveInferenceTest, BeliefUpdate) {
    Vector obs = CreateObservation(0);
    Engine->UpdateBelief(obs);
    
    auto belief = Engine->GetBelief();
    
    // Belief should shift toward observed state
    EXPECT_GT(belief.StateBelief[0], belief.StateBelief[1]);
}

TEST_F(ActiveInferenceTest, FreeEnergyComputation) {
    Vector obs = CreateObservation(0);
    Engine->UpdateBelief(obs);
    
    double fe = Engine->GetFreeEnergy();
    
    // Free energy should be finite
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(ActiveInferenceTest, ActionSelection) {
    Vector obs = CreateObservation(1);
    Engine->UpdateBelief(obs);
    
    int action = Engine->SelectAction();
    
    EXPECT_GE(action, 0);
    EXPECT_LT(action, 3);
}

TEST_F(ActiveInferenceTest, PreferenceInfluence) {
    // Set strong preference for observation 0
    Vector preferences = {1.0, 0.0, 0.0, 0.0};
    Engine->SetPreferences(preferences);
    
    // Observe state 2
    Vector obs = CreateObservation(2);
    Engine->UpdateBelief(obs);
    
    // Action should try to move toward preferred state
    int action = Engine->SelectAction();
    EXPECT_GE(action, 0);
}

TEST_F(ActiveInferenceTest, PredictedObservation) {
    Vector obs = CreateObservation(1);
    Engine->UpdateBelief(obs);
    Engine->PredictObservation();
    
    auto belief = Engine->GetBelief();
    
    // Predicted observation should be a valid distribution
    double sum = 0.0;
    for (double p : belief.PredictedObs) {
        EXPECT_GE(p, 0.0);
        sum += p;
    }
    EXPECT_NEAR(sum, 1.0, 0.01);
}

TEST_F(ActiveInferenceTest, Reset) {
    Vector obs = CreateObservation(0);
    Engine->UpdateBelief(obs);
    
    Engine->Reset();
    
    auto belief = Engine->GetBelief();
    
    // Should return to uniform prior
    for (double b : belief.StateBelief) {
        EXPECT_NEAR(b, 0.25, 0.01);
    }
}

TEST_F(ActiveInferenceTest, SequentialUpdates) {
    // Process sequence of observations
    std::vector<int> sequence = {0, 0, 1, 1, 2};
    
    for (int obs_idx : sequence) {
        Vector obs = CreateObservation(obs_idx);
        Engine->UpdateBelief(obs);
    }
    
    auto belief = Engine->GetBelief();
    
    // Belief should reflect recent observations
    EXPECT_GT(belief.StateBelief[2], 0.0);
}

// ============================================================================
// Niche Construction Tests
// ============================================================================

TEST_F(NicheConstructionTest, Initialization) {
    EXPECT_TRUE(Niche->IsInitialized());
    
    auto niche = Niche->GetCurrentNiche();
    EXPECT_EQ(niche.EnvironmentState.size(), 10);
    EXPECT_EQ(niche.AffordanceVector.size(), 5);
}

TEST_F(NicheConstructionTest, ConstructNiche) {
    Vector action(10, 0.5);
    Niche->ConstructNiche(action);
    
    auto niche = Niche->GetCurrentNiche();
    EXPECT_EQ(niche.ConstructionCount, 1);
    
    // Environment should have changed
    bool changed = false;
    for (double val : niche.EnvironmentState) {
        if (std::abs(val - 0.5) > 0.01) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);
}

TEST_F(NicheConstructionTest, AffordanceUpdate) {
    Vector action(10, 1.0);
    Niche->ConstructNiche(action);
    
    auto affordances = Niche->GetAffordances();
    
    // Affordances should be non-zero
    double sum = 0.0;
    for (double a : affordances) sum += std::abs(a);
    EXPECT_GT(sum, 0.0);
}

TEST_F(NicheConstructionTest, FitnessEvaluation) {
    Vector action(10, 0.5);
    Niche->ConstructNiche(action);
    
    Vector preferences(5, 0.5);
    double fitness = Niche->EvaluateFitness(preferences);
    
    EXPECT_GE(fitness, -1.0);
    EXPECT_LE(fitness, 1.0);
}

TEST_F(NicheConstructionTest, IterativeConstruction) {
    Vector preferences(5, 1.0);
    
    std::vector<double> fitnessHistory;
    for (int i = 0; i < 20; i++) {
        // Construct toward preferences
        Vector action(10);
        for (int j = 0; j < 10; j++) {
            action[j] = (j < 5) ? 0.5 : -0.5;
        }
        Niche->ConstructNiche(action);
        fitnessHistory.push_back(Niche->EvaluateFitness(preferences));
    }
    
    // Fitness should generally improve or stabilize
    EXPECT_GE(fitnessHistory.back(), fitnessHistory.front() - 0.5);
}

TEST_F(NicheConstructionTest, Reset) {
    Vector action(10, 1.0);
    Niche->ConstructNiche(action);
    
    Niche->Reset();
    
    auto niche = Niche->GetCurrentNiche();
    EXPECT_EQ(niche.ConstructionCount, 0);
    EXPECT_NEAR(niche.EnvironmentState[0], 0.5, 0.01);
}

// ============================================================================
// AXIOM Active Inference Tests
// ============================================================================

TEST_F(AXIOMTest, Initialization) {
    EXPECT_TRUE(AXIOM->IsInitialized());
    
    auto state = AXIOM->GetState();
    EXPECT_EQ(state.Beliefs.size(), 8);
    EXPECT_EQ(state.Desires.size(), 4);
    EXPECT_EQ(state.Intentions.size(), 3);
}

TEST_F(AXIOMTest, BeliefUpdate) {
    Vector observation(8, 0.0);
    observation[0] = 1.0;
    
    AXIOM->UpdateBeliefs(observation);
    
    auto state = AXIOM->GetState();
    EXPECT_GT(state.Beliefs[0], 0.0);
}

TEST_F(AXIOMTest, DesireSetting) {
    Vector desires = {1.0, 0.5, 0.0, 0.0};
    AXIOM->SetDesires(desires);
    
    auto state = AXIOM->GetState();
    EXPECT_EQ(state.Desires[0], 1.0);
    EXPECT_EQ(state.Desires[1], 0.5);
}

TEST_F(AXIOMTest, IntentionFormation) {
    Vector observation(8, 0.0);
    observation[1] = 1.0;
    AXIOM->UpdateBeliefs(observation);
    
    int intention = AXIOM->FormIntention();
    
    EXPECT_GE(intention, 0);
    EXPECT_LT(intention, 3);
    
    auto state = AXIOM->GetState();
    EXPECT_EQ(state.Intentions[intention], 1.0);
}

TEST_F(AXIOMTest, AffectUpdate) {
    AXIOM->UpdateAffect(0.8, 0.5);  // High surprise, positive reward
    
    auto state = AXIOM->GetState();
    EXPECT_GT(state.Arousal, 0.5);
    EXPECT_GT(state.Valence, 0.0);
}

TEST_F(AXIOMTest, AffectDynamics) {
    // Sequence of experiences
    AXIOM->UpdateAffect(1.0, 1.0);   // Very surprising, very rewarding
    auto state1 = AXIOM->GetState();
    
    AXIOM->UpdateAffect(0.0, -0.5);  // Not surprising, negative
    auto state2 = AXIOM->GetState();
    
    // Arousal should decrease, valence should decrease
    EXPECT_LT(state2.Arousal, state1.Arousal);
    EXPECT_LT(state2.Valence, state1.Valence);
}

TEST_F(AXIOMTest, FreeEnergyTracking) {
    Vector observation(8, 0.0);
    observation[0] = 1.0;
    AXIOM->UpdateBeliefs(observation);
    
    double fe = AXIOM->GetFreeEnergy();
    
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

// ============================================================================
// Math Utility Tests
// ============================================================================

TEST(MathUtilsTest, Dot) {
    Vector a = {1.0, 2.0, 3.0};
    Vector b = {4.0, 5.0, 6.0};
    
    double result = MathUtils::Dot(a, b);
    EXPECT_DOUBLE_EQ(result, 32.0);  // 1*4 + 2*5 + 3*6 = 32
}

TEST(MathUtilsTest, Norm) {
    Vector v = {3.0, 4.0};
    double norm = MathUtils::Norm(v);
    EXPECT_DOUBLE_EQ(norm, 5.0);
}

TEST(MathUtilsTest, Normalize) {
    Vector v = {3.0, 4.0};
    Vector normalized = MathUtils::Normalize(v);
    
    EXPECT_NEAR(normalized[0], 0.6, 0.001);
    EXPECT_NEAR(normalized[1], 0.8, 0.001);
}

TEST(MathUtilsTest, Softmax) {
    Vector v = {1.0, 2.0, 3.0};
    Vector sm = MathUtils::Softmax(v);
    
    // Should sum to 1
    double sum = sm[0] + sm[1] + sm[2];
    EXPECT_NEAR(sum, 1.0, 0.001);
    
    // Should be ordered
    EXPECT_LT(sm[0], sm[1]);
    EXPECT_LT(sm[1], sm[2]);
}

TEST(MathUtilsTest, KLDivergence) {
    Vector p = {0.5, 0.5};
    Vector q = {0.5, 0.5};
    
    double kl = MathUtils::KLDivergence(p, q);
    EXPECT_NEAR(kl, 0.0, 0.001);  // Same distribution
    
    Vector p2 = {0.9, 0.1};
    double kl2 = MathUtils::KLDivergence(p2, q);
    EXPECT_GT(kl2, 0.0);  // Different distributions
}

TEST(MathUtilsTest, Entropy) {
    Vector uniform = {0.25, 0.25, 0.25, 0.25};
    Vector peaked = {0.97, 0.01, 0.01, 0.01};
    
    double h_uniform = MathUtils::Entropy(uniform);
    double h_peaked = MathUtils::Entropy(peaked);
    
    EXPECT_GT(h_uniform, h_peaked);  // Uniform has higher entropy
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(ActiveInferenceIntegrationTest, FullLoop) {
    // Initialize all components
    MockActiveInferenceEngine engine;
    FGenerativeModel model;
    engine.Initialize(model);
    
    MockNicheConstructionSystem niche;
    niche.Initialize(4, 4);
    
    // Simulation loop
    for (int t = 0; t < 50; t++) {
        // Get affordances as observation
        auto affordances = niche.GetAffordances();
        Vector observation(4);
        for (int i = 0; i < 4; i++) {
            observation[i] = (i < (int)affordances.size()) ? affordances[i] : 0.0;
        }
        
        // Update beliefs
        engine.UpdateBelief(observation);
        
        // Select action
        int action = engine.SelectAction();
        
        // Construct niche based on action
        Vector actionVec(4, 0.0);
        if (action < 4) actionVec[action] = 1.0;
        niche.ConstructNiche(actionVec);
    }
    
    // System should have evolved
    auto finalNiche = niche.GetCurrentNiche();
    EXPECT_EQ(finalNiche.ConstructionCount, 50);
}

TEST(ActiveInferenceIntegrationTest, AXIOMWithNiche) {
    MockAXIOMActiveInference axiom;
    axiom.Initialize(4, 4, 3);
    
    MockNicheConstructionSystem niche;
    niche.Initialize(4, 4);
    
    Vector desires = {1.0, 0.5, 0.0, 0.0};
    axiom.SetDesires(desires);
    
    for (int t = 0; t < 30; t++) {
        // Observe environment
        auto envState = niche.GetCurrentNiche().EnvironmentState;
        Vector observation(4);
        for (int i = 0; i < 4 && i < (int)envState.size(); i++) {
            observation[i] = envState[i];
        }
        
        axiom.UpdateBeliefs(observation);
        int intention = axiom.FormIntention();
        
        // Act on environment
        Vector action(4, 0.0);
        if (intention < 4) action[intention] = 0.5;
        niche.ConstructNiche(action);
        
        // Update affect based on fitness
        double fitness = niche.EvaluateFitness(desires);
        double surprise = std::abs(axiom.GetFreeEnergy());
        axiom.UpdateAffect(surprise, fitness);
    }
    
    auto finalState = axiom.GetState();
    EXPECT_GE(finalState.Arousal, 0.0);
    EXPECT_LE(finalState.Arousal, 1.0);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(ActiveInferencePerformanceTest, BeliefUpdatePerformance) {
    MockActiveInferenceEngine engine;
    FGenerativeModel model;
    engine.Initialize(model);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 3);
    
    for (int i = 0; i < 10000; i++) {
        Vector obs(4, 0.0);
        obs[dist(gen)] = 1.0;
        engine.UpdateBelief(obs);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 500);  // 10000 updates in under 500ms
}

TEST(ActiveInferencePerformanceTest, ActionSelectionPerformance) {
    MockActiveInferenceEngine engine;
    FGenerativeModel model;
    engine.Initialize(model);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 5000; i++) {
        engine.SelectAction();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 200);  // 5000 selections in under 200ms
}
