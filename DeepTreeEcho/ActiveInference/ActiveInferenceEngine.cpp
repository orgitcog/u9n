// ActiveInferenceEngine.cpp
// Implementation of Active Inference Engine for Deep Tree Echo

#include "ActiveInferenceEngine.h"
#include "../Learning/PredictiveAdaptationEngine.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Sys6/Sys6LCMClockSynchronizer.h"
#include "NicheConstructionSystem.h"
#include "Math/UnrealMathUtility.h"

UActiveInferenceEngine::UActiveInferenceEngine()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UActiveInferenceEngine::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeGenerativeModel();
    InitializeBeliefStates();
    InitializeMarkovBlanket();
}

void UActiveInferenceEngine::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Inference is driven by sys6 clock and echobeats
    // Continuous updates handled through ProcessSys6Step
}

void UActiveInferenceEngine::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        PredictiveEngine = Owner->FindComponentByClass<UPredictiveAdaptationEngine>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        LCMClock = Owner->FindComponentByClass<USys6LCMClockSynchronizer>();
        NicheSystem = Owner->FindComponentByClass<UNicheConstructionSystem>();
    }
}

void UActiveInferenceEngine::InitializeGenerativeModel()
{
    GenerativeModel.ModelID = TEXT("DeepTreeEcho_GenerativeModel");

    // Initialize hidden states belief
    GenerativeModel.HiddenStates.BeliefID = TEXT("HiddenStates");
    GenerativeModel.HiddenStates.Dimension = NumHiddenFactors * StatesPerFactor;
    GenerativeModel.HiddenStates.Mean.SetNumZeroed(GenerativeModel.HiddenStates.Dimension);
    GenerativeModel.HiddenStates.Precision.Init(1.0f, GenerativeModel.HiddenStates.Dimension);

    // Initialize likelihood mapping (A matrix) - uniform initially
    int32 LikelihoodSize = NumObservationModalities * StatesPerFactor * NumHiddenFactors;
    GenerativeModel.LikelihoodMapping.Init(1.0f / StatesPerFactor, LikelihoodSize);

    // Initialize transition probabilities (B matrix) - identity-like
    int32 TransitionSize = NumActionFactors * StatesPerFactor * StatesPerFactor;
    GenerativeModel.TransitionProbabilities.SetNumZeroed(TransitionSize);
    for (int32 a = 0; a < NumActionFactors; ++a)
    {
        for (int32 s = 0; s < StatesPerFactor; ++s)
        {
            // Self-transition with high probability
            int32 Idx = a * StatesPerFactor * StatesPerFactor + s * StatesPerFactor + s;
            GenerativeModel.TransitionProbabilities[Idx] = 0.8f;

            // Transition to adjacent states
            if (s > 0)
            {
                int32 IdxPrev = a * StatesPerFactor * StatesPerFactor + s * StatesPerFactor + (s - 1);
                GenerativeModel.TransitionProbabilities[IdxPrev] = 0.1f;
            }
            if (s < StatesPerFactor - 1)
            {
                int32 IdxNext = a * StatesPerFactor * StatesPerFactor + s * StatesPerFactor + (s + 1);
                GenerativeModel.TransitionProbabilities[IdxNext] = 0.1f;
            }
        }
    }

    // Initialize preferences (C vector) - neutral initially
    GenerativeModel.Preferences.Init(0.0f, NumObservationModalities * StatesPerFactor);

    // Initialize initial state prior (D vector) - uniform
    GenerativeModel.InitialStatePrior.Init(1.0f / StatesPerFactor, NumHiddenFactors * StatesPerFactor);
}

void UActiveInferenceEngine::InitializeBeliefStates()
{
    BeliefStates.Empty();

    for (int32 f = 0; f < NumHiddenFactors; ++f)
    {
        FBeliefState Belief;
        Belief.BeliefID = FString::Printf(TEXT("Factor_%d"), f);
        Belief.Dimension = StatesPerFactor;
        Belief.Mean.Init(1.0f / StatesPerFactor, StatesPerFactor);  // Uniform prior
        Belief.Precision.Init(1.0f, StatesPerFactor);
        Belief.Entropy = FMath::Loge((float)StatesPerFactor);  // Maximum entropy
        Belief.Confidence = 0.0f;
        Belief.LastUpdate = 0.0f;

        BeliefStates.Add(Belief);
    }
}

void UActiveInferenceEngine::InitializeMarkovBlanket()
{
    // Initialize internal states
    MarkovBlanket.InternalStates.SetNumZeroed(NumHiddenFactors * StatesPerFactor);

    // Initialize sensory states
    MarkovBlanket.SensoryStates.SetNumZeroed(NumObservationModalities * StatesPerFactor);

    // Initialize active states
    MarkovBlanket.ActiveStates.SetNumZeroed(NumActionFactors);

    // Initialize external states (inferred)
    MarkovBlanket.ExternalStates.SetNumZeroed(NumHiddenFactors * StatesPerFactor);

    MarkovBlanket.BlanketIntegrity = 1.0f;
    MarkovBlanket.SteadyStateFlow = 0.0f;
}

void UActiveInferenceEngine::RunPerceptualInference(const TArray<float>& Observations)
{
    // Update inference mode
    CycleState.Mode = EActiveInferenceMode::Perception;
    OnInferenceModeChanged.Broadcast(CycleState.Mode);

    // Update sensory states in Markov blanket
    if (Observations.Num() > 0)
    {
        MarkovBlanket.SensoryStates = Observations;
    }

    // Run variational inference iterations
    float PreviousVFE = CurrentVFE;
    CycleState.bConverged = false;

    for (int32 i = 0; i < InferenceIterations; ++i)
    {
        // Compute prediction error for each observation modality
        for (int32 m = 0; m < NumObservationModalities && m < Observations.Num(); ++m)
        {
            UpdateBeliefs(Observations, m);
        }

        // Compute new VFE
        FVariationalFreeEnergy NewVFE = ComputeVariationalFreeEnergy();
        CurrentVFE = NewVFE.Total;

        // Check convergence
        if (FMath::Abs(CurrentVFE - PreviousVFE) < ConvergenceThreshold)
        {
            CycleState.bConverged = true;
            break;
        }

        PreviousVFE = CurrentVFE;
        CycleState.InferenceIterations = i + 1;
    }

    // Update cycle state
    CycleState.VFE = ComputeVariationalFreeEnergy();

    // Broadcast events
    for (const FBeliefState& Belief : BeliefStates)
    {
        OnBeliefUpdated.Broadcast(Belief);
    }

    OnFreeEnergyComputed.Broadcast(CurrentVFE, CurrentEFE);
}

int32 UActiveInferenceEngine::RunActiveInference()
{
    // Update inference mode
    CycleState.Mode = EActiveInferenceMode::Action;
    OnInferenceModeChanged.Broadcast(CycleState.Mode);

    // Evaluate policies
    EvaluatedPolicies = EvaluatePolicies();

    // Select policy
    CurrentPolicy = SelectPolicy();
    CycleState.SelectedPolicy = CurrentPolicy;
    OnPolicySelected.Broadcast(CurrentPolicy);

    // Get next action from policy
    int32 ActionIndex = 0;
    if (CurrentPolicy.ActionSequence.Num() > NextActionIndex)
    {
        ActionIndex = CurrentPolicy.ActionSequence[NextActionIndex];
        NextActionIndex++;
    }

    // Execute action
    FActionOutcome Outcome = ExecuteAction(ActionIndex);
    OnActionExecuted.Broadcast(Outcome);

    return ActionIndex;
}

FPolicy UActiveInferenceEngine::RunPlanningInference(int32 Horizon)
{
    // Update inference mode
    CycleState.Mode = EActiveInferenceMode::Planning;
    OnInferenceModeChanged.Broadcast(CycleState.Mode);

    // Generate policies with specified horizon
    TArray<FPolicy> Policies = GeneratePolicies(NumPolicies, Horizon);

    // Evaluate each policy
    float MinEFE = TNumericLimits<float>::Max();
    FPolicy BestPolicy;

    for (FPolicy& Policy : Policies)
    {
        FExpectedFreeEnergy EFE = ComputeExpectedFreeEnergy(Policy);
        Policy.ExpectedFreeEnergy = EFE.Total;
        Policy.EpistemicValue = EFE.EpistemicValue;
        Policy.PragmaticValue = EFE.PragmaticValue;
        Policy.NicheValue = EFE.NicheConstructionValue;

        if (EFE.Total < MinEFE)
        {
            MinEFE = EFE.Total;
            BestPolicy = Policy;
        }
    }

    return BestPolicy;
}

void UActiveInferenceEngine::UpdateBeliefs(const TArray<float>& Observation, int32 Modality)
{
    if (Modality >= NumObservationModalities || Modality >= BeliefStates.Num())
    {
        return;
    }

    FBeliefState& Belief = BeliefStates[Modality];
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // Bayesian belief update
    // posterior ∝ likelihood × prior
    TArray<float> Likelihood;
    Likelihood.SetNumZeroed(StatesPerFactor);

    // Compute likelihood for each state
    for (int32 s = 0; s < StatesPerFactor; ++s)
    {
        // Get likelihood from A matrix
        int32 AIdx = Modality * StatesPerFactor * NumHiddenFactors + s;
        if (AIdx < GenerativeModel.LikelihoodMapping.Num())
        {
            Likelihood[s] = GenerativeModel.LikelihoodMapping[AIdx];
        }
        else
        {
            Likelihood[s] = 1.0f / StatesPerFactor;
        }

        // Weight by observation
        if (s < Observation.Num())
        {
            Likelihood[s] *= FMath::Exp(-FMath::Square(Observation[s] - Belief.Mean[s]));
        }
    }

    // Compute posterior
    TArray<float> Posterior;
    Posterior.SetNumZeroed(StatesPerFactor);
    float Evidence = 0.0f;

    for (int32 s = 0; s < StatesPerFactor; ++s)
    {
        Posterior[s] = Likelihood[s] * Belief.Mean[s];
        Evidence += Posterior[s];
    }

    // Normalize
    if (Evidence > 0.0f)
    {
        for (int32 s = 0; s < StatesPerFactor; ++s)
        {
            Posterior[s] /= Evidence;
        }
    }

    // Update belief
    Belief.Mean = Posterior;
    Belief.Entropy = ComputeEntropy(Posterior);
    Belief.Confidence = 1.0f - Belief.Entropy / FMath::Loge((float)StatesPerFactor);
    Belief.LastUpdate = CurrentTime;

    // Update internal states in Markov blanket
    for (int32 s = 0; s < StatesPerFactor && (Modality * StatesPerFactor + s) < MarkovBlanket.InternalStates.Num(); ++s)
    {
        MarkovBlanket.InternalStates[Modality * StatesPerFactor + s] = Posterior[s];
    }
}

FBeliefState UActiveInferenceEngine::GetBeliefState(int32 Factor) const
{
    if (Factor >= 0 && Factor < BeliefStates.Num())
    {
        return BeliefStates[Factor];
    }
    return FBeliefState();
}

TArray<FBeliefState> UActiveInferenceEngine::GetAllBeliefStates() const
{
    return BeliefStates;
}

FVariationalFreeEnergy UActiveInferenceEngine::ComputeVariationalFreeEnergy()
{
    FVariationalFreeEnergy VFE;

    // F = E_q[log q(s)] - E_q[log p(o,s)]
    // F = -H[q(s)] + E_q[-log p(o|s)] + KL[q(s)||p(s)]

    // Compute entropy term
    float TotalEntropy = 0.0f;
    for (const FBeliefState& Belief : BeliefStates)
    {
        TotalEntropy += Belief.Entropy;
    }
    VFE.NegativeEntropy = -TotalEntropy;

    // Compute energy term (expected log joint)
    float Energy = 0.0f;

    // Log likelihood contribution
    for (int32 m = 0; m < NumObservationModalities; ++m)
    {
        for (int32 s = 0; s < StatesPerFactor; ++s)
        {
            if (m < BeliefStates.Num() && s < BeliefStates[m].Mean.Num())
            {
                float Prob = BeliefStates[m].Mean[s];
                if (Prob > 0.0001f)
                {
                    int32 AIdx = m * StatesPerFactor * NumHiddenFactors + s;
                    float Likelihood = (AIdx < GenerativeModel.LikelihoodMapping.Num()) ?
                        GenerativeModel.LikelihoodMapping[AIdx] : 1.0f / StatesPerFactor;
                    Energy += Prob * FMath::Loge(FMath::Max(Likelihood, 0.0001f));
                }
            }
        }
    }
    VFE.Energy = -Energy;

    // Compute KL divergence from prior
    float KL = 0.0f;
    for (int32 f = 0; f < NumHiddenFactors && f < BeliefStates.Num(); ++f)
    {
        TArray<float> Prior;
        Prior.Init(1.0f / StatesPerFactor, StatesPerFactor);
        KL += ComputeKLDivergence(BeliefStates[f].Mean, Prior);
    }
    VFE.KLDivergence = KL;

    // Total VFE
    VFE.Total = VFE.Energy + VFE.NegativeEntropy;
    VFE.Surprise = VFE.Total;  // Approximate

    // Bound tightness (how close VFE is to true log evidence)
    VFE.BoundTightness = FMath::Exp(-VFE.KLDivergence);

    return VFE;
}

FExpectedFreeEnergy UActiveInferenceEngine::ComputeExpectedFreeEnergy(const FPolicy& Policy)
{
    FExpectedFreeEnergy EFE;
    EFE.Horizon = Policy.Horizon;

    // G = E_q[log q(s') - log p(o',s')]
    // G = Ambiguity + Risk
    // G = -EpistemicValue - PragmaticValue

    // Compute epistemic value (information gain)
    EFE.EpistemicValue = ComputeEpistemicValue(Policy);

    // Compute pragmatic value (preference satisfaction)
    EFE.PragmaticValue = ComputePragmaticValue(Policy);

    // Compute niche construction value
    if (bEnableNicheConstruction)
    {
        EFE.NicheConstructionValue = ComputeNicheConstructionValue(Policy);
    }

    // Compute ambiguity (expected entropy of observations)
    EFE.Ambiguity = 0.0f;
    for (int32 t = 0; t < Policy.Horizon; ++t)
    {
        // Simplified: use current belief entropy as proxy
        for (const FBeliefState& Belief : BeliefStates)
        {
            EFE.Ambiguity += Belief.Entropy * FMath::Pow(0.9f, (float)t);  // Discount future
        }
    }

    // Compute risk (expected divergence from preferences)
    EFE.Risk = 0.0f;
    for (int32 s = 0; s < StatesPerFactor && s < GenerativeModel.Preferences.Num(); ++s)
    {
        float Preference = GenerativeModel.Preferences[s];
        float ExpectedProb = (s < BeliefStates[0].Mean.Num()) ? BeliefStates[0].Mean[s] : 0.0f;
        EFE.Risk += ExpectedProb * FMath::Abs(Preference);
    }

    // Total expected free energy
    EFE.Total = EFE.Ambiguity + EFE.Risk
              - EpistemicWeight * EFE.EpistemicValue
              - PragmaticWeight * EFE.PragmaticValue
              - NicheWeight * EFE.NicheConstructionValue;

    return EFE;
}

float UActiveInferenceEngine::GetVariationalFreeEnergy() const
{
    return CurrentVFE;
}

float UActiveInferenceEngine::GetExpectedFreeEnergy() const
{
    return CurrentEFE;
}

float UActiveInferenceEngine::MinimizeVariationalFreeEnergy(int32 Iterations)
{
    float InitialVFE = CurrentVFE;

    for (int32 i = 0; i < Iterations; ++i)
    {
        // Gradient descent on belief parameters
        for (FBeliefState& Belief : BeliefStates)
        {
            // Compute gradient of VFE w.r.t. belief mean
            TArray<float> Gradient;
            Gradient.SetNumZeroed(Belief.Dimension);

            for (int32 s = 0; s < Belief.Dimension; ++s)
            {
                // Approximate gradient
                float Epsilon = 0.01f;
                TArray<float> PerturbedMean = Belief.Mean;
                PerturbedMean[s] += Epsilon;
                PerturbedMean = NormalizeProbabilities(PerturbedMean);

                // Compute VFE with perturbed belief
                float PerturbedEntropy = ComputeEntropy(PerturbedMean);
                float CurrentEntropy = Belief.Entropy;

                Gradient[s] = (PerturbedEntropy - CurrentEntropy) / Epsilon;
            }

            // Update belief mean (gradient descent)
            float LearningRate = 0.1f;
            for (int32 s = 0; s < Belief.Dimension; ++s)
            {
                Belief.Mean[s] -= LearningRate * Gradient[s];
            }

            // Normalize
            Belief.Mean = NormalizeProbabilities(Belief.Mean);
            Belief.Entropy = ComputeEntropy(Belief.Mean);
        }

        // Recompute VFE
        FVariationalFreeEnergy NewVFE = ComputeVariationalFreeEnergy();
        CurrentVFE = NewVFE.Total;

        // Check convergence
        if (FMath::Abs(CurrentVFE - InitialVFE) < ConvergenceThreshold)
        {
            break;
        }
    }

    return CurrentVFE;
}

TArray<FPolicy> UActiveInferenceEngine::EvaluatePolicies()
{
    // Generate policies if not already done
    if (EvaluatedPolicies.Num() == 0)
    {
        EvaluatedPolicies = GeneratePolicies(NumPolicies, PlanningHorizon);
    }

    // Evaluate each policy
    TArray<float> ExpectedFreeEnergies;

    for (FPolicy& Policy : EvaluatedPolicies)
    {
        FExpectedFreeEnergy EFE = ComputeExpectedFreeEnergy(Policy);
        Policy.ExpectedFreeEnergy = EFE.Total;
        Policy.EpistemicValue = EFE.EpistemicValue;
        Policy.PragmaticValue = EFE.PragmaticValue;
        Policy.NicheValue = EFE.NicheConstructionValue;

        ExpectedFreeEnergies.Add(EFE.Total);
    }

    // Compute policy probabilities (softmax)
    TArray<float> Probabilities = SoftmaxPolicies(ExpectedFreeEnergies);

    for (int32 i = 0; i < EvaluatedPolicies.Num(); ++i)
    {
        EvaluatedPolicies[i].Probability = Probabilities[i];
    }

    return EvaluatedPolicies;
}

FPolicy UActiveInferenceEngine::SelectPolicy()
{
    if (EvaluatedPolicies.Num() == 0)
    {
        EvaluatePolicies();
    }

    // Sample from policy distribution
    float RandomValue = FMath::FRand();
    float CumulativeProb = 0.0f;

    for (const FPolicy& Policy : EvaluatedPolicies)
    {
        CumulativeProb += Policy.Probability;
        if (RandomValue <= CumulativeProb)
        {
            CurrentPolicy = Policy;
            CurrentEFE = Policy.ExpectedFreeEnergy;
            NextActionIndex = 0;
            return Policy;
        }
    }

    // Fallback to first policy
    if (EvaluatedPolicies.Num() > 0)
    {
        CurrentPolicy = EvaluatedPolicies[0];
        CurrentEFE = CurrentPolicy.ExpectedFreeEnergy;
        NextActionIndex = 0;
    }

    return CurrentPolicy;
}

float UActiveInferenceEngine::GetPolicyProbability(const FPolicy& Policy) const
{
    for (const FPolicy& P : EvaluatedPolicies)
    {
        if (P.PolicyID == Policy.PolicyID)
        {
            return P.Probability;
        }
    }
    return 0.0f;
}

FPolicy UActiveInferenceEngine::GetSelectedPolicy() const
{
    return CurrentPolicy;
}

TArray<FPolicy> UActiveInferenceEngine::GeneratePolicies(int32 Count, int32 Horizon)
{
    TArray<FPolicy> Policies;

    // Get number of available actions
    int32 NumActions = NumActionFactors * StatesPerFactor;

    for (int32 p = 0; p < Count; ++p)
    {
        FPolicy Policy;
        Policy.PolicyID = FString::Printf(TEXT("Policy_%d"), p);
        Policy.Horizon = Horizon;

        // Determine policy type
        if (p < Count / 4)
        {
            Policy.Type = EPolicyType::Habitual;
        }
        else if (p < Count / 2)
        {
            Policy.Type = EPolicyType::GoalDirected;
        }
        else if (p < 3 * Count / 4)
        {
            Policy.Type = EPolicyType::Exploratory;
        }
        else
        {
            Policy.Type = bEnableNicheConstruction ? EPolicyType::NicheShaping : EPolicyType::Epistemic;
        }

        // Generate action sequence
        for (int32 t = 0; t < Horizon; ++t)
        {
            int32 Action = 0;

            switch (Policy.Type)
            {
            case EPolicyType::Habitual:
                // Repeat most likely action
                Action = p % NumActions;
                break;

            case EPolicyType::GoalDirected:
                // Actions toward preferred states
                Action = (p + t) % NumActions;
                break;

            case EPolicyType::Exploratory:
                // Random exploration
                Action = FMath::RandRange(0, NumActions - 1);
                break;

            case EPolicyType::Epistemic:
                // Information-seeking actions
                Action = (p * 3 + t * 7) % NumActions;
                break;

            case EPolicyType::NicheShaping:
                // Environment-modifying actions
                Action = (p * 5 + t * 11) % NumActions;
                break;

            default:
                Action = t % NumActions;
                break;
            }

            Policy.ActionSequence.Add(Action);
        }

        Policies.Add(Policy);
    }

    return Policies;
}

FActionOutcome UActiveInferenceEngine::ExecuteAction(int32 ActionIndex)
{
    FActionOutcome Outcome;
    Outcome.ActionID = ActionIndex;
    Outcome.ActionType = FString::Printf(TEXT("Action_%d"), ActionIndex);
    Outcome.Timestamp = GetWorld()->GetTimeSeconds();

    // Predict outcome
    Outcome.PredictedOutcome = PredictActionOutcome(ActionIndex);

    // Update active states in Markov blanket
    if (ActionIndex < MarkovBlanket.ActiveStates.Num())
    {
        MarkovBlanket.ActiveStates[ActionIndex] = 1.0f;
    }

    // Simulate actual outcome (in real system, this would come from environment)
    Outcome.ActualOutcome = Outcome.PredictedOutcome;
    for (float& Val : Outcome.ActualOutcome)
    {
        Val += FMath::FRandRange(-0.1f, 0.1f);  // Add noise
    }

    // Compute prediction error
    float ErrorSum = 0.0f;
    for (int32 i = 0; i < FMath::Min(Outcome.PredictedOutcome.Num(), Outcome.ActualOutcome.Num()); ++i)
    {
        ErrorSum += FMath::Square(Outcome.PredictedOutcome[i] - Outcome.ActualOutcome[i]);
    }
    Outcome.PredictionError = FMath::Sqrt(ErrorSum);

    // Compute free energy change
    float OldVFE = CurrentVFE;
    RunPerceptualInference(Outcome.ActualOutcome);
    Outcome.FreeEnergyChange = CurrentVFE - OldVFE;

    // Compute niche modification
    if (bEnableNicheConstruction)
    {
        Outcome.NicheModification = GetNicheModificationPotential(ActionIndex);
    }

    // Store in history
    ActionHistory.Add(Outcome);

    return Outcome;
}

TArray<int32> UActiveInferenceEngine::GetAvailableActions() const
{
    TArray<int32> Actions;
    for (int32 i = 0; i < NumActionFactors * StatesPerFactor; ++i)
    {
        Actions.Add(i);
    }
    return Actions;
}

TArray<float> UActiveInferenceEngine::PredictActionOutcome(int32 ActionIndex) const
{
    TArray<float> PredictedOutcome;
    PredictedOutcome.SetNumZeroed(NumObservationModalities * StatesPerFactor);

    // Use transition probabilities to predict next state
    int32 ActionFactor = ActionIndex / StatesPerFactor;
    int32 ActionState = ActionIndex % StatesPerFactor;

    for (int32 s = 0; s < StatesPerFactor; ++s)
    {
        // Get transition probability
        int32 BIdx = ActionFactor * StatesPerFactor * StatesPerFactor + ActionState * StatesPerFactor + s;
        float TransProb = (BIdx < GenerativeModel.TransitionProbabilities.Num()) ?
            GenerativeModel.TransitionProbabilities[BIdx] : 1.0f / StatesPerFactor;

        // Weight by current belief
        for (int32 f = 0; f < NumHiddenFactors && f < BeliefStates.Num(); ++f)
        {
            if (s < BeliefStates[f].Mean.Num())
            {
                float BeliefProb = BeliefStates[f].Mean[s];
                int32 OutIdx = f * StatesPerFactor + s;
                if (OutIdx < PredictedOutcome.Num())
                {
                    PredictedOutcome[OutIdx] = TransProb * BeliefProb;
                }
            }
        }
    }

    return PredictedOutcome;
}

TArray<FActionOutcome> UActiveInferenceEngine::GetActionHistory() const
{
    return ActionHistory;
}

FMarkovBlanket UActiveInferenceEngine::GetMarkovBlanket() const
{
    return MarkovBlanket;
}

void UActiveInferenceEngine::UpdateMarkovBlanket(const TArray<float>& Sensory, const TArray<float>& Active)
{
    if (Sensory.Num() > 0)
    {
        MarkovBlanket.SensoryStates = Sensory;
    }

    if (Active.Num() > 0)
    {
        MarkovBlanket.ActiveStates = Active;
    }

    // Update blanket integrity
    MarkovBlanket.BlanketIntegrity = ComputeBlanketIntegrity();

    // Compute steady state flow
    MarkovBlanket.SteadyStateFlow = GetSteadyStateFlow();

    OnMarkovBlanketUpdated.Broadcast(MarkovBlanket);
}

float UActiveInferenceEngine::ComputeBlanketIntegrity() const
{
    // Measure coupling between internal and external states through blanket
    float Coupling = 0.0f;
    int32 Count = 0;

    for (int32 i = 0; i < MarkovBlanket.InternalStates.Num() && i < MarkovBlanket.SensoryStates.Num(); ++i)
    {
        Coupling += FMath::Abs(MarkovBlanket.InternalStates[i] - MarkovBlanket.SensoryStates[i]);
        Count++;
    }

    if (Count > 0)
    {
        Coupling /= Count;
    }

    // Integrity is inverse of coupling mismatch
    return FMath::Exp(-Coupling);
}

float UActiveInferenceEngine::GetSteadyStateFlow() const
{
    // Compute flow towards steady state (free energy gradient)
    float Flow = 0.0f;

    for (const FBeliefState& Belief : BeliefStates)
    {
        // Flow is proportional to entropy reduction potential
        float MaxEntropy = FMath::Loge((float)Belief.Dimension);
        Flow += (MaxEntropy - Belief.Entropy) / MaxEntropy;
    }

    if (BeliefStates.Num() > 0)
    {
        Flow /= BeliefStates.Num();
    }

    return Flow;
}

FGenerativeModel UActiveInferenceEngine::GetGenerativeModel() const
{
    return GenerativeModel;
}

void UActiveInferenceEngine::UpdateLikelihoodMapping(const TArray<float>& NewMapping)
{
    if (NewMapping.Num() > 0)
    {
        GenerativeModel.LikelihoodMapping = NewMapping;
    }
}

void UActiveInferenceEngine::UpdateTransitionProbabilities(const TArray<float>& NewTransitions)
{
    if (NewTransitions.Num() > 0)
    {
        GenerativeModel.TransitionProbabilities = NewTransitions;
    }
}

void UActiveInferenceEngine::SetPreferences(const TArray<float>& NewPreferences)
{
    if (NewPreferences.Num() > 0)
    {
        GenerativeModel.Preferences = NewPreferences;
    }
}

void UActiveInferenceEngine::LearnModelStructure(const TArray<FActionOutcome>& Experiences)
{
    CycleState.Mode = EActiveInferenceMode::Learning;
    OnInferenceModeChanged.Broadcast(CycleState.Mode);

    // Update likelihood mapping based on experiences
    for (const FActionOutcome& Exp : Experiences)
    {
        // Update A matrix based on prediction errors
        for (int32 i = 0; i < Exp.ActualOutcome.Num() && i < GenerativeModel.LikelihoodMapping.Num(); ++i)
        {
            float LearningRate = 0.1f * FMath::Exp(-Exp.PredictionError);
            GenerativeModel.LikelihoodMapping[i] += LearningRate * (Exp.ActualOutcome[i] - GenerativeModel.LikelihoodMapping[i]);
        }

        // Update B matrix based on state transitions
        int32 ActionIdx = Exp.ActionID;
        if (ActionIdx < NumActionFactors * StatesPerFactor)
        {
            int32 ActionFactor = ActionIdx / StatesPerFactor;
            for (int32 s = 0; s < StatesPerFactor; ++s)
            {
                int32 BIdx = ActionFactor * StatesPerFactor * StatesPerFactor + s;
                if (BIdx < GenerativeModel.TransitionProbabilities.Num())
                {
                    float Target = (s < Exp.ActualOutcome.Num()) ? Exp.ActualOutcome[s] : 0.0f;
                    GenerativeModel.TransitionProbabilities[BIdx] += 0.05f * (Target - GenerativeModel.TransitionProbabilities[BIdx]);
                }
            }
        }
    }

    // Normalize probabilities
    GenerativeModel.LikelihoodMapping = NormalizeProbabilities(GenerativeModel.LikelihoodMapping);
}

float UActiveInferenceEngine::ComputeNicheConstructionValue(const FPolicy& Policy) const
{
    if (!bEnableNicheConstruction)
    {
        return 0.0f;
    }

    float NicheValue = 0.0f;

    // Compute expected niche modification from policy
    for (int32 t = 0; t < Policy.ActionSequence.Num(); ++t)
    {
        int32 ActionIdx = Policy.ActionSequence[t];
        float ModPotential = GetNicheModificationPotential(ActionIdx);

        // Discount future modifications
        float Discount = FMath::Pow(0.9f, (float)t);
        NicheValue += Discount * ModPotential;
    }

    return NicheValue;
}

float UActiveInferenceEngine::GetNicheModificationPotential(int32 ActionIndex) const
{
    // Actions that modify the environment have higher niche construction potential
    // This is a simplified model - in reality, would depend on environment dynamics

    float BasePotential = 0.1f;

    // Some actions are more niche-modifying than others
    int32 ActionFactor = ActionIndex / StatesPerFactor;
    int32 ActionState = ActionIndex % StatesPerFactor;

    // Higher action states have more modification potential
    float StateFactor = (float)ActionState / (float)StatesPerFactor;

    // Certain action factors are more environment-modifying
    float FactorWeight = (ActionFactor == 0) ? 1.0f : 0.5f;

    return BasePotential * StateFactor * FactorWeight;
}

FPolicy UActiveInferenceEngine::PlanNicheConstruction(int32 Horizon)
{
    CycleState.Mode = EActiveInferenceMode::NicheConstruct;
    OnInferenceModeChanged.Broadcast(CycleState.Mode);

    // Generate niche-focused policies
    TArray<FPolicy> NichePolicies;

    for (int32 p = 0; p < NumPolicies; ++p)
    {
        FPolicy Policy;
        Policy.PolicyID = FString::Printf(TEXT("NichePolicy_%d"), p);
        Policy.Type = EPolicyType::NicheShaping;
        Policy.Horizon = Horizon;

        // Generate action sequence prioritizing niche modification
        for (int32 t = 0; t < Horizon; ++t)
        {
            // Find action with highest niche modification potential
            int32 BestAction = 0;
            float BestPotential = 0.0f;

            for (int32 a = 0; a < NumActionFactors * StatesPerFactor; ++a)
            {
                float Potential = GetNicheModificationPotential(a);

                // Add exploration noise
                Potential += FMath::FRandRange(0.0f, 0.2f);

                if (Potential > BestPotential)
                {
                    BestPotential = Potential;
                    BestAction = a;
                }
            }

            Policy.ActionSequence.Add(BestAction);
        }

        // Evaluate policy
        FExpectedFreeEnergy EFE = ComputeExpectedFreeEnergy(Policy);
        Policy.ExpectedFreeEnergy = EFE.Total;
        Policy.NicheValue = EFE.NicheConstructionValue;

        NichePolicies.Add(Policy);
    }

    // Select best niche construction policy
    FPolicy BestPolicy;
    float BestNicheValue = -TNumericLimits<float>::Max();

    for (const FPolicy& Policy : NichePolicies)
    {
        if (Policy.NicheValue > BestNicheValue)
        {
            BestNicheValue = Policy.NicheValue;
            BestPolicy = Policy;
        }
    }

    return BestPolicy;
}

void UActiveInferenceEngine::ProcessSys6Step(int32 LCMStep)
{
    CycleState.LCMStep = LCMStep;

    // Map LCM step to echobeat step
    int32 EchobeatStep = ((LCMStep * 12) / 30) + 1;
    EchobeatStep = FMath::Clamp(EchobeatStep, 1, 12);
    CycleState.EchobeatStep = EchobeatStep;

    // Update inference mode based on echobeat step
    UpdateInferenceMode(EchobeatStep);
}

EActiveInferenceMode UActiveInferenceEngine::GetInferenceModeForStep(int32 EchobeatStep) const
{
    // Map echobeat steps to inference modes
    // Steps 1,5,9: Pivotal (relevance realization) -> Perception
    // Steps 2,6,10: Affordance -> Action
    // Steps 3,7,11: Salience -> Planning
    // Steps 4,8,12: Integration -> Learning/Niche

    int32 Phase = ((EchobeatStep - 1) % 4);

    switch (Phase)
    {
    case 0:
        return EActiveInferenceMode::Perception;
    case 1:
        return EActiveInferenceMode::Action;
    case 2:
        return EActiveInferenceMode::Planning;
    case 3:
        return bEnableNicheConstruction ? EActiveInferenceMode::NicheConstruct : EActiveInferenceMode::Learning;
    default:
        return EActiveInferenceMode::Perception;
    }
}

FInferenceCycleState UActiveInferenceEngine::GetInferenceCycleState() const
{
    return CycleState;
}

void UActiveInferenceEngine::UpdateInferenceMode(int32 EchobeatStep)
{
    EActiveInferenceMode NewMode = GetInferenceModeForStep(EchobeatStep);

    if (NewMode != CycleState.Mode)
    {
        CycleState.Mode = NewMode;
        OnInferenceModeChanged.Broadcast(NewMode);
    }
}

TArray<float> UActiveInferenceEngine::SoftmaxPolicies(const TArray<float>& ExpectedFreeEnergies) const
{
    TArray<float> Probabilities;
    Probabilities.SetNumZeroed(ExpectedFreeEnergies.Num());

    // Compute softmax with action precision (inverse temperature)
    float MaxEFE = -TNumericLimits<float>::Max();
    for (float EFE : ExpectedFreeEnergies)
    {
        MaxEFE = FMath::Max(MaxEFE, -EFE);  // Negate because lower EFE is better
    }

    float SumExp = 0.0f;
    for (int32 i = 0; i < ExpectedFreeEnergies.Num(); ++i)
    {
        Probabilities[i] = FMath::Exp(ActionPrecision * (-ExpectedFreeEnergies[i] - MaxEFE));
        SumExp += Probabilities[i];
    }

    // Normalize
    if (SumExp > 0.0f)
    {
        for (float& Prob : Probabilities)
        {
            Prob /= SumExp;
        }
    }

    return Probabilities;
}

float UActiveInferenceEngine::ComputeKLDivergence(const TArray<float>& Q, const TArray<float>& P) const
{
    float KL = 0.0f;

    for (int32 i = 0; i < FMath::Min(Q.Num(), P.Num()); ++i)
    {
        if (Q[i] > 0.0001f && P[i] > 0.0001f)
        {
            KL += Q[i] * FMath::Loge(Q[i] / P[i]);
        }
    }

    return FMath::Max(0.0f, KL);
}

float UActiveInferenceEngine::ComputeEntropy(const TArray<float>& Distribution) const
{
    float Entropy = 0.0f;

    for (float P : Distribution)
    {
        if (P > 0.0001f)
        {
            Entropy -= P * FMath::Loge(P);
        }
    }

    return FMath::Max(0.0f, Entropy);
}

TArray<float> UActiveInferenceEngine::NormalizeProbabilities(const TArray<float>& Probs) const
{
    TArray<float> Normalized = Probs;

    float Sum = 0.0f;
    for (float P : Normalized)
    {
        Sum += FMath::Max(0.0f, P);
    }

    if (Sum > 0.0f)
    {
        for (float& P : Normalized)
        {
            P = FMath::Max(0.0f, P) / Sum;
        }
    }
    else if (Normalized.Num() > 0)
    {
        // Uniform distribution if all zeros
        float Uniform = 1.0f / Normalized.Num();
        for (float& P : Normalized)
        {
            P = Uniform;
        }
    }

    return Normalized;
}

float UActiveInferenceEngine::ComputeEpistemicValue(const FPolicy& Policy) const
{
    // Epistemic value = expected information gain
    // Approximated as expected reduction in belief entropy

    float EpistemicValue = 0.0f;

    for (int32 t = 0; t < Policy.ActionSequence.Num(); ++t)
    {
        int32 ActionIdx = Policy.ActionSequence[t];

        // Predict state after action
        TArray<float> PredictedOutcome = PredictActionOutcome(ActionIdx);

        // Compute expected entropy reduction
        float PredictedEntropy = ComputeEntropy(PredictedOutcome);
        float CurrentEntropy = 0.0f;
        for (const FBeliefState& Belief : BeliefStates)
        {
            CurrentEntropy += Belief.Entropy;
        }

        float EntropyReduction = CurrentEntropy - PredictedEntropy;

        // Discount future information gain
        float Discount = FMath::Pow(0.9f, (float)t);
        EpistemicValue += Discount * FMath::Max(0.0f, EntropyReduction);
    }

    return EpistemicValue;
}

float UActiveInferenceEngine::ComputePragmaticValue(const FPolicy& Policy) const
{
    // Pragmatic value = expected preference satisfaction

    float PragmaticValue = 0.0f;

    for (int32 t = 0; t < Policy.ActionSequence.Num(); ++t)
    {
        int32 ActionIdx = Policy.ActionSequence[t];

        // Predict outcome
        TArray<float> PredictedOutcome = PredictActionOutcome(ActionIdx);

        // Compute expected preference satisfaction
        float Satisfaction = 0.0f;
        for (int32 s = 0; s < FMath::Min(PredictedOutcome.Num(), GenerativeModel.Preferences.Num()); ++s)
        {
            Satisfaction += PredictedOutcome[s] * GenerativeModel.Preferences[s];
        }

        // Discount future satisfaction
        float Discount = FMath::Pow(0.9f, (float)t);
        PragmaticValue += Discount * Satisfaction;
    }

    return PragmaticValue;
}
