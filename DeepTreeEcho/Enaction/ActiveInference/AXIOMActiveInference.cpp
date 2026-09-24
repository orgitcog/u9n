/**
 * AXIOM Active Inference Implementation
 * 
 * Implements the AXIOM active inference architecture for Deep-Tree-Echo,
 * integrated with the CognitiveCycleManager's 3-stream, 12-step cognitive cycle.
 */

#include "AXIOMActiveInference.h"

// ========================================
// CONSTRUCTOR
// ========================================

UAXIOMActiveInference::UAXIOMActiveInference()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Tick every frame
}

// ========================================
// LIFECYCLE
// ========================================

void UAXIOMActiveInference::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UAXIOMActiveInference::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Integrate with CognitiveCycleManager if available
    if (CognitiveCycleManager)
    {
        int32 GlobalStep = CognitiveCycleManager->CycleState.GlobalStep;
        
        // Only process on step change
        if (GlobalStep != LastProcessedStep)
        {
            // Determine active stream based on step
            EConsciousnessStream ActiveStream;
            int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
            
            if (StepInCycle >= 1 && StepInCycle <= 4)
                ActiveStream = EConsciousnessStream::Perception;
            else if (StepInCycle >= 5 && StepInCycle <= 8)
                ActiveStream = EConsciousnessStream::Action;
            else
                ActiveStream = EConsciousnessStream::Simulation;
            
            ProcessCognitiveStep(GlobalStep, ActiveStream);
            LastProcessedStep = GlobalStep;
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UAXIOMActiveInference::Initialize()
{
    // Initialize object slots
    CurrentState.Slots.SetNum(Config.MaxSlots);
    for (int32 i = 0; i < Config.MaxSlots; ++i)
    {
        CurrentState.Slots[i].SlotIndex = i;
        CurrentState.Slots[i].State = ESlotState::Empty;
    }
    
    // Initialize default transition prototypes
    InitializeTransitionPrototypes();
    
    // Initialize policies
    Policies.SetNum(Config.NumPolicies);
    
    // Set initial learning phase
    LearningPhase = EAXIOMLearningPhase::Exploration;
    TotalSteps = 0;
    
    UE_LOG(LogTemp, Log, TEXT("AXIOM Active Inference initialized with %d slots, %d prototypes"), 
           Config.MaxSlots, TransitionPrototypes.Num());
}

void UAXIOMActiveInference::Reset()
{
    CurrentState = FWorldState();
    PreviousState = FWorldState();
    ObjectTypes.Empty();
    InteractionClusters.Empty();
    TotalSteps = 0;
    LearningPhase = EAXIOMLearningPhase::Exploration;
    
    Initialize();
}

void UAXIOMActiveInference::SetCognitiveCycleManager(UCognitiveCycleManager* Manager)
{
    CognitiveCycleManager = Manager;
}

void UAXIOMActiveInference::InitializeTransitionPrototypes()
{
    TransitionPrototypes.Empty();
    
    // Prototype 0: Stationary (identity transform)
    FTransitionPrototype Stationary;
    Stationary.PrototypeIndex = 0;
    Stationary.MotionType = EMotionPrototype::Stationary;
    Stationary.TransitionMatrix = {1,0,0,0, 0,1,0,0, 0,0,0,0, 0,0,0,0}; // Position preserved, velocity zeroed
    Stationary.ControlMatrix = {0,0, 0,0, 0,0, 0,0};
    Stationary.ProcessNoise = {0.01f, 0.01f, 0.01f, 0.01f};
    TransitionPrototypes.Add(Stationary);
    
    // Prototype 1: Linear (constant velocity)
    FTransitionPrototype Linear;
    Linear.PrototypeIndex = 1;
    Linear.MotionType = EMotionPrototype::Linear;
    Linear.TransitionMatrix = {1,0,1,0, 0,1,0,1, 0,0,1,0, 0,0,0,1}; // x' = x + vx, y' = y + vy
    Linear.ControlMatrix = {0,0, 0,0, 0,0, 0,0};
    Linear.ProcessNoise = {0.05f, 0.05f, 0.02f, 0.02f};
    TransitionPrototypes.Add(Linear);
    
    // Prototype 2: Falling (gravity-affected)
    FTransitionPrototype Falling;
    Falling.PrototypeIndex = 2;
    Falling.MotionType = EMotionPrototype::Falling;
    Falling.TransitionMatrix = {1,0,1,0, 0,1,0,1, 0,0,1,0, 0,0,0,1}; // Same as linear
    Falling.ControlMatrix = {0,0, 0,0, 0,0, 0,0.1f}; // Gravity in y direction
    Falling.ProcessNoise = {0.05f, 0.05f, 0.02f, 0.05f};
    TransitionPrototypes.Add(Falling);
    
    // Prototype 3: Bouncing (velocity reversal)
    FTransitionPrototype Bouncing;
    Bouncing.PrototypeIndex = 3;
    Bouncing.MotionType = EMotionPrototype::Bouncing;
    Bouncing.TransitionMatrix = {1,0,1,0, 0,1,0,1, 0,0,-0.9f,0, 0,0,0,-0.9f}; // Velocity reversal with damping
    Bouncing.ControlMatrix = {0,0, 0,0, 0,0, 0,0};
    Bouncing.ProcessNoise = {0.1f, 0.1f, 0.1f, 0.1f};
    TransitionPrototypes.Add(Bouncing);
    
    // Prototype 4: Controlled (player-controlled)
    FTransitionPrototype Controlled;
    Controlled.PrototypeIndex = 4;
    Controlled.MotionType = EMotionPrototype::Controlled;
    Controlled.TransitionMatrix = {1,0,0,0, 0,1,0,0, 0,0,0,0, 0,0,0,0}; // Position only
    Controlled.ControlMatrix = {1,0, 0,1, 0,0, 0,0}; // Direct control of position
    Controlled.ProcessNoise = {0.01f, 0.01f, 0.01f, 0.01f};
    TransitionPrototypes.Add(Controlled);
}

// ========================================
// PERCEPTION (sMM + iMM)
// ========================================

void UAXIOMActiveInference::ProcessObservation(const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    // Store previous state
    PreviousState = CurrentState;
    
    // Compute slot assignments from pixels
    ComputeSlotAssignments(Pixels, Width, Height);
    
    // Classify object types
    ClassifyObjectTypes();
    
    // Perform variational inference update
    VariationalUpdate();
    
    TotalSteps++;
}

void UAXIOMActiveInference::UpdateSlots(const TArray<FObjectSlot>& DetectedObjects)
{
    // Simple slot assignment: match by proximity
    for (const FObjectSlot& Detected : DetectedObjects)
    {
        int32 BestSlot = -1;
        float BestDistance = FLT_MAX;
        
        // Find closest existing slot
        for (int32 i = 0; i < CurrentState.Slots.Num(); ++i)
        {
            if (CurrentState.Slots[i].State == ESlotState::Active)
            {
                float Dist = FVector2D::Distance(Detected.Position, CurrentState.Slots[i].Position);
                if (Dist < BestDistance)
                {
                    BestDistance = Dist;
                    BestSlot = i;
                }
            }
        }
        
        // If close enough, update existing slot
        if (BestSlot >= 0 && BestDistance < 50.0f)
        {
            // Compute velocity from position change
            FVector2D OldPos = CurrentState.Slots[BestSlot].Position;
            CurrentState.Slots[BestSlot].Position = Detected.Position;
            CurrentState.Slots[BestSlot].Velocity = Detected.Position - OldPos;
            CurrentState.Slots[BestSlot].Size = Detected.Size;
            CurrentState.Slots[BestSlot].Color = Detected.Color;
            CurrentState.Slots[BestSlot].Activation = 1.0f;
        }
        else
        {
            // Find empty slot for new object
            for (int32 i = 0; i < CurrentState.Slots.Num(); ++i)
            {
                if (CurrentState.Slots[i].State == ESlotState::Empty)
                {
                    CurrentState.Slots[i] = Detected;
                    CurrentState.Slots[i].SlotIndex = i;
                    CurrentState.Slots[i].State = ESlotState::Spawned;
                    CurrentState.Slots[i].Activation = 1.0f;
                    break;
                }
            }
        }
    }
    
    // Mark unmatched slots as occluded
    for (FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State == ESlotState::Active && Slot.Activation < 0.5f)
        {
            Slot.State = ESlotState::Occluded;
        }
        else if (Slot.State == ESlotState::Spawned)
        {
            Slot.State = ESlotState::Active;
        }
    }
}

void UAXIOMActiveInference::ClassifyObjectTypes()
{
    for (FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State != ESlotState::Active && Slot.State != ESlotState::Spawned)
            continue;
        
        // Create feature vector from slot properties
        TArray<float> Features;
        Features.Add(Slot.Size.X);
        Features.Add(Slot.Size.Y);
        Features.Add(Slot.Color.R);
        Features.Add(Slot.Color.G);
        Features.Add(Slot.Color.B);
        
        // Find best matching object type
        int32 BestType = -1;
        float BestLikelihood = -FLT_MAX;
        
        for (int32 i = 0; i < ObjectTypes.Num(); ++i)
        {
            float Likelihood = 0.0f;
            for (int32 j = 0; j < Features.Num() && j < ObjectTypes[i].Mean.Num(); ++j)
            {
                float Diff = Features[j] - ObjectTypes[i].Mean[j];
                float Var = FMath::Max(ObjectTypes[i].Variance[j], 0.001f);
                Likelihood -= 0.5f * (Diff * Diff / Var + FMath::Loge(Var));
            }
            Likelihood += FMath::Loge(ObjectTypes[i].Weight);
            
            if (Likelihood > BestLikelihood)
            {
                BestLikelihood = Likelihood;
                BestType = i;
            }
        }
        
        // If no good match and structure expansion enabled, create new type
        if ((BestType < 0 || BestLikelihood < -10.0f) && Config.bEnableStructureExpansion)
        {
            FGaussianComponent NewType;
            NewType.ComponentIndex = ObjectTypes.Num();
            NewType.Mean = Features;
            NewType.Variance.SetNum(Features.Num());
            for (int32 j = 0; j < Features.Num(); ++j)
                NewType.Variance[j] = 0.1f;
            NewType.Weight = 1.0f / (ObjectTypes.Num() + 1);
            NewType.ObservationCount = 1;
            
            BestType = AddObjectType(NewType);
        }
        
        Slot.ObjectTypeID = BestType;
        Slot.TypeConfidence = BestLikelihood > -5.0f ? 1.0f : FMath::Exp(BestLikelihood + 5.0f);
    }
}

TArray<FObjectSlot> UAXIOMActiveInference::GetObjectSlots() const
{
    TArray<FObjectSlot> ActiveSlots;
    for (const FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State == ESlotState::Active || Slot.State == ESlotState::Spawned)
        {
            ActiveSlots.Add(Slot);
        }
    }
    return ActiveSlots;
}

// ========================================
// DYNAMICS (tMM)
// ========================================

void UAXIOMActiveInference::PredictNextState(int32 Action)
{
    for (FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State != ESlotState::Active)
            continue;
        
        // Get transition prototype for this slot
        int32 ProtoIdx = static_cast<int32>(Slot.MotionPrototype);
        if (ProtoIdx >= TransitionPrototypes.Num())
            ProtoIdx = 0;
        
        const FTransitionPrototype& Proto = TransitionPrototypes[ProtoIdx];
        
        // State vector: [x, y, vx, vy]
        TArray<float> State = {Slot.Position.X, Slot.Position.Y, Slot.Velocity.X, Slot.Velocity.Y};
        TArray<float> NextState;
        NextState.SetNum(4);
        
        // Apply transition: x' = Ax + Bu
        for (int32 i = 0; i < 4; ++i)
        {
            NextState[i] = 0.0f;
            for (int32 j = 0; j < 4; ++j)
            {
                NextState[i] += Proto.TransitionMatrix[i * 4 + j] * State[j];
            }
            // Add control influence (simplified: action maps to velocity)
            if (Slot.MotionPrototype == EMotionPrototype::Controlled)
            {
                // Action encoding: 0=none, 1=left, 2=right, 3=up, 4=down
                float ControlX = (Action == 2) ? 5.0f : (Action == 1) ? -5.0f : 0.0f;
                float ControlY = (Action == 4) ? 5.0f : (Action == 3) ? -5.0f : 0.0f;
                NextState[i] += Proto.ControlMatrix[i * 2] * ControlX + Proto.ControlMatrix[i * 2 + 1] * ControlY;
            }
        }
        
        // Update slot with predicted state
        Slot.Position = FVector2D(NextState[0], NextState[1]);
        Slot.Velocity = FVector2D(NextState[2], NextState[3]);
        
        // Increase uncertainty
        Slot.PositionUncertainty += Proto.ProcessNoise[0];
    }
}

void UAXIOMActiveInference::UpdateTransitionModel()
{
    // Compare predicted vs actual for each slot
    for (int32 i = 0; i < CurrentState.Slots.Num(); ++i)
    {
        const FObjectSlot& Current = CurrentState.Slots[i];
        const FObjectSlot& Previous = PreviousState.Slots[i];
        
        if (Current.State != ESlotState::Active || Previous.State != ESlotState::Active)
            continue;
        
        // Compute prediction error
        FVector2D PredictedPos = Previous.Position + Previous.Velocity;
        FVector2D Error = Current.Position - PredictedPos;
        float ErrorMagnitude = Error.Size();
        
        // If error is large, consider switching motion prototype
        if (ErrorMagnitude > 10.0f)
        {
            // Find best matching prototype
            float BestError = FLT_MAX;
            EMotionPrototype BestProto = Current.MotionPrototype;
            
            for (const FTransitionPrototype& Proto : TransitionPrototypes)
            {
                // Simplified: just check if prototype predicts better
                // In full implementation, would do proper Bayesian inference
                if (Proto.MotionType == EMotionPrototype::Bouncing && 
                    FMath::Sign(Current.Velocity.X) != FMath::Sign(Previous.Velocity.X))
                {
                    BestProto = EMotionPrototype::Bouncing;
                    break;
                }
            }
            
            CurrentState.Slots[i].MotionPrototype = BestProto;
        }
        
        // Update prototype statistics
        int32 ProtoIdx = static_cast<int32>(Current.MotionPrototype);
        if (ProtoIdx < TransitionPrototypes.Num())
        {
            TransitionPrototypes[ProtoIdx].UsageCount++;
        }
    }
}

EMotionPrototype UAXIOMActiveInference::GetMotionPrototype(int32 SlotIndex) const
{
    if (SlotIndex >= 0 && SlotIndex < CurrentState.Slots.Num())
    {
        return CurrentState.Slots[SlotIndex].MotionPrototype;
    }
    return EMotionPrototype::Stationary;
}

// ========================================
// INTERACTIONS (rMM)
// ========================================

float UAXIOMActiveInference::PredictReward(int32 Action)
{
    float TotalReward = 0.0f;
    
    // Check all pairs of active objects for potential interactions
    TArray<FObjectSlot> ActiveSlots = GetObjectSlots();
    
    for (int32 i = 0; i < ActiveSlots.Num(); ++i)
    {
        int32 NearestIdx = FindNearestInteractor(ActiveSlots[i].SlotIndex);
        if (NearestIdx < 0) continue;
        
        // Find matching interaction cluster
        for (const FInteractionCluster& Cluster : InteractionClusters)
        {
            if ((Cluster.ObjectTypePair.X == ActiveSlots[i].ObjectTypeID && 
                 Cluster.ObjectTypePair.Y == ActiveSlots[NearestIdx].ObjectTypeID) ||
                (Cluster.ObjectTypePair.Y == ActiveSlots[i].ObjectTypeID && 
                 Cluster.ObjectTypePair.X == ActiveSlots[NearestIdx].ObjectTypeID))
            {
                float Distance = FVector2D::Distance(ActiveSlots[i].Position, 
                                                     CurrentState.Slots[NearestIdx].Position);
                if (Distance < Cluster.DistanceThreshold)
                {
                    TotalReward += Cluster.ExpectedReward;
                }
            }
        }
    }
    
    return TotalReward;
}

void UAXIOMActiveInference::UpdateInteractionModel(float ObservedReward)
{
    CurrentState.CurrentReward = ObservedReward;
    CurrentState.CumulativeReward += ObservedReward;
    
    if (FMath::Abs(ObservedReward) < 0.001f)
        return; // No significant reward to learn from
    
    // Find which objects are interacting
    TArray<FObjectSlot> ActiveSlots = GetObjectSlots();
    
    for (int32 i = 0; i < ActiveSlots.Num(); ++i)
    {
        int32 NearestIdx = FindNearestInteractor(ActiveSlots[i].SlotIndex);
        if (NearestIdx < 0) continue;
        
        float Distance = FVector2D::Distance(ActiveSlots[i].Position, 
                                             CurrentState.Slots[NearestIdx].Position);
        
        // Check if this interaction is close enough to cause reward
        if (Distance < 30.0f)
        {
            // Find or create interaction cluster
            bool Found = false;
            for (FInteractionCluster& Cluster : InteractionClusters)
            {
                if ((Cluster.ObjectTypePair.X == ActiveSlots[i].ObjectTypeID && 
                     Cluster.ObjectTypePair.Y == CurrentState.Slots[NearestIdx].ObjectTypeID))
                {
                    // Update cluster with new observation (online Bayesian update)
                    float Alpha = 1.0f / (Cluster.ActivationCount + 1);
                    Cluster.ExpectedReward = (1.0f - Alpha) * Cluster.ExpectedReward + Alpha * ObservedReward;
                    Cluster.DistanceThreshold = (1.0f - Alpha) * Cluster.DistanceThreshold + Alpha * Distance;
                    Cluster.ActivationCount++;
                    Found = true;
                    break;
                }
            }
            
            if (!Found && Config.bEnableStructureExpansion)
            {
                // Create new interaction cluster
                FInteractionCluster NewCluster;
                NewCluster.ClusterIndex = InteractionClusters.Num();
                NewCluster.ObjectTypePair = FIntPoint(ActiveSlots[i].ObjectTypeID, 
                                                      CurrentState.Slots[NearestIdx].ObjectTypeID);
                NewCluster.DistanceThreshold = Distance * 1.5f;
                NewCluster.ExpectedReward = ObservedReward;
                NewCluster.RewardVariance = 1.0f;
                NewCluster.ActivationCount = 1;
                
                AddInteractionCluster(NewCluster);
            }
        }
    }
}

int32 UAXIOMActiveInference::FindNearestInteractor(int32 SlotIndex) const
{
    if (SlotIndex < 0 || SlotIndex >= CurrentState.Slots.Num())
        return -1;
    
    const FObjectSlot& Source = CurrentState.Slots[SlotIndex];
    if (Source.State != ESlotState::Active)
        return -1;
    
    int32 NearestIdx = -1;
    float NearestDist = FLT_MAX;
    
    for (int32 i = 0; i < CurrentState.Slots.Num(); ++i)
    {
        if (i == SlotIndex) continue;
        
        const FObjectSlot& Other = CurrentState.Slots[i];
        if (Other.State != ESlotState::Active) continue;
        
        float Dist = FVector2D::Distance(Source.Position, Other.Position);
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestIdx = i;
        }
    }
    
    return NearestIdx;
}

// ========================================
// PLANNING (Active Inference)
// ========================================

void UAXIOMActiveInference::GeneratePolicies()
{
    // Generate random action sequences
    const int32 NumActions = 5; // 0=none, 1=left, 2=right, 3=up, 4=down
    
    for (int32 p = 0; p < Config.NumPolicies; ++p)
    {
        Policies[p].PolicyIndex = p;
        Policies[p].ActionSequence.SetNum(Config.PlanningHorizon);
        
        for (int32 t = 0; t < Config.PlanningHorizon; ++t)
        {
            Policies[p].ActionSequence[t] = FMath::RandRange(0, NumActions - 1);
        }
        
        Policies[p].ExpectedFreeEnergy = 0.0f;
        Policies[p].ExpectedUtility = 0.0f;
        Policies[p].InformationGain = 0.0f;
        Policies[p].Probability = 1.0f / Config.NumPolicies;
    }
}

void UAXIOMActiveInference::EvaluatePolicies()
{
    TArray<float> EFEs;
    EFEs.SetNum(Config.NumPolicies);
    
    for (int32 p = 0; p < Config.NumPolicies; ++p)
    {
        EFEs[p] = ComputeExpectedFreeEnergy(Policies[p]);
        Policies[p].ExpectedFreeEnergy = EFEs[p];
    }
    
    // Convert to probabilities via softmax of negative EFE
    TArray<float> NegEFEs;
    NegEFEs.SetNum(Config.NumPolicies);
    for (int32 p = 0; p < Config.NumPolicies; ++p)
    {
        NegEFEs[p] = -EFEs[p];
    }
    
    TArray<float> Probs = Softmax(NegEFEs, 1.0f);
    for (int32 p = 0; p < Config.NumPolicies; ++p)
    {
        Policies[p].Probability = Probs[p];
    }
}

int32 UAXIOMActiveInference::SelectAction()
{
    // Sample policy from distribution
    TArray<float> Probs;
    Probs.SetNum(Config.NumPolicies);
    for (int32 p = 0; p < Config.NumPolicies; ++p)
    {
        Probs[p] = Policies[p].Probability;
    }
    
    int32 SelectedPolicyIdx = SampleCategorical(Probs);
    SelectedPolicy = Policies[SelectedPolicyIdx];
    
    // Return first action of selected policy
    int32 Action = SelectedPolicy.ActionSequence.Num() > 0 ? SelectedPolicy.ActionSequence[0] : 0;
    
    // Broadcast event
    OnActionSelected.Broadcast(Action, SelectedPolicy.ExpectedFreeEnergy);
    
    return Action;
}

float UAXIOMActiveInference::ComputeExpectedFreeEnergy(const FPolicy& Policy)
{
    // EFE = -Expected Utility - Information Gain Weight * Information Gain
    float Utility = ComputeExpectedUtility(Policy);
    float InfoGain = ComputeInformationGain(Policy);
    
    // Store components
    const_cast<FPolicy&>(Policy).ExpectedUtility = Utility;
    const_cast<FPolicy&>(Policy).InformationGain = InfoGain;
    
    return -Utility - Config.InformationGainWeight * InfoGain;
}

float UAXIOMActiveInference::ComputeExpectedUtility(const FPolicy& Policy)
{
    // Rollout policy and compute expected cumulative reward
    TArray<FWorldState> Trajectory = RolloutPolicy(Policy, Config.RolloutsPerPolicy);
    
    float TotalReward = 0.0f;
    float Discount = 1.0f;
    const float DiscountFactor = 0.99f;
    
    for (const FWorldState& State : Trajectory)
    {
        TotalReward += Discount * State.CurrentReward;
        Discount *= DiscountFactor;
    }
    
    return TotalReward / FMath::Max(1, Config.RolloutsPerPolicy);
}

float UAXIOMActiveInference::ComputeInformationGain(const FPolicy& Policy)
{
    // Compute expected information gain (epistemic value)
    // This is the expected reduction in uncertainty about the world model
    
    float InfoGain = 0.0f;
    
    // Simplified: information gain is higher for states with high uncertainty
    for (const FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State == ESlotState::Active)
        {
            // Higher uncertainty = more potential information gain
            InfoGain += Slot.PositionUncertainty;
            InfoGain += (1.0f - Slot.TypeConfidence);
        }
    }
    
    // Also add bonus for exploring new interactions
    TArray<FObjectSlot> ActiveSlots = GetObjectSlots();
    for (int32 i = 0; i < ActiveSlots.Num(); ++i)
    {
        for (int32 j = i + 1; j < ActiveSlots.Num(); ++j)
        {
            // Check if this pair has been observed
            bool Observed = false;
            for (const FInteractionCluster& Cluster : InteractionClusters)
            {
                if ((Cluster.ObjectTypePair.X == ActiveSlots[i].ObjectTypeID && 
                     Cluster.ObjectTypePair.Y == ActiveSlots[j].ObjectTypeID) ||
                    (Cluster.ObjectTypePair.Y == ActiveSlots[i].ObjectTypeID && 
                     Cluster.ObjectTypePair.X == ActiveSlots[j].ObjectTypeID))
                {
                    Observed = true;
                    break;
                }
            }
            
            if (!Observed)
            {
                // Bonus for potentially discovering new interaction
                InfoGain += 1.0f;
            }
        }
    }
    
    return InfoGain;
}

// ========================================
// STRUCTURE LEARNING
// ========================================

void UAXIOMActiveInference::ExpandStructure()
{
    if (!Config.bEnableStructureExpansion)
        return;
    
    // Check if we need more object types
    if (NeedsStructureExpansion())
    {
        // Structure expansion is handled in ClassifyObjectTypes and UpdateInteractionModel
        UE_LOG(LogTemp, Log, TEXT("AXIOM: Structure expansion triggered"));
    }
}

void UAXIOMActiveInference::PerformBMR()
{
    if (!Config.bEnableBMR)
        return;
    
    MergeSimilarComponents();
}

int32 UAXIOMActiveInference::AddObjectType(const FGaussianComponent& Component)
{
    if (ObjectTypes.Num() >= Config.MaxObjectTypes)
        return -1;
    
    FGaussianComponent NewComponent = Component;
    NewComponent.ComponentIndex = ObjectTypes.Num();
    ObjectTypes.Add(NewComponent);
    
    // Renormalize weights
    float TotalWeight = 0.0f;
    for (const FGaussianComponent& C : ObjectTypes)
        TotalWeight += C.Weight;
    for (FGaussianComponent& C : ObjectTypes)
        C.Weight /= TotalWeight;
    
    OnObjectTypeDiscovered.Broadcast(NewComponent.ComponentIndex);
    
    return NewComponent.ComponentIndex;
}

int32 UAXIOMActiveInference::AddTransitionPrototype(const FTransitionPrototype& Prototype)
{
    if (TransitionPrototypes.Num() >= Config.MaxTransitionPrototypes)
        return -1;
    
    FTransitionPrototype NewPrototype = Prototype;
    NewPrototype.PrototypeIndex = TransitionPrototypes.Num();
    TransitionPrototypes.Add(NewPrototype);
    
    return NewPrototype.PrototypeIndex;
}

int32 UAXIOMActiveInference::AddInteractionCluster(const FInteractionCluster& Cluster)
{
    if (InteractionClusters.Num() >= Config.MaxInteractionClusters)
        return -1;
    
    FInteractionCluster NewCluster = Cluster;
    NewCluster.ClusterIndex = InteractionClusters.Num();
    InteractionClusters.Add(NewCluster);
    
    OnInteractionLearned.Broadcast(NewCluster.ClusterIndex, NewCluster.ExpectedReward);
    
    return NewCluster.ClusterIndex;
}

// ========================================
// COGNITIVE CYCLE INTEGRATION
// ========================================

void UAXIOMActiveInference::ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream)
{
    int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
    
    switch (ActiveStream)
    {
    case EConsciousnessStream::Perception:
        // Steps 1-4: Perception processing
        if (StepInCycle == 1)
        {
            // Step 1: Relevance Realization - sMM inference
            // (ProcessObservation should be called externally with pixel data)
        }
        else if (StepInCycle == 4)
        {
            // Step 4: iMM classification
            ClassifyObjectTypes();
        }
        break;
        
    case EConsciousnessStream::Action:
        // Steps 5-8: Action processing
        if (StepInCycle == 5)
        {
            // Step 5: Generate policies
            GeneratePolicies();
        }
        else if (StepInCycle == 6)
        {
            // Step 6: Evaluate policies (EFE computation)
            EvaluatePolicies();
        }
        else if (StepInCycle == 8)
        {
            // Step 8: Select action
            CurrentState.CurrentAction = SelectAction();
        }
        break;
        
    case EConsciousnessStream::Simulation:
        // Steps 9-12: Simulation processing
        if (StepInCycle == 9)
        {
            // Step 9: Predict interactions (rMM)
            PredictReward(CurrentState.CurrentAction);
        }
        else if (StepInCycle == 10)
        {
            // Step 10: Compare prediction vs observation
            UpdateTransitionModel();
        }
        else if (StepInCycle == 12)
        {
            // Step 12: Bayesian Model Reduction
            PerformBMR();
            ExpandStructure();
        }
        break;
    }
    
    // Update salience and affordance values for CognitiveCycleManager
    if (CognitiveCycleManager)
    {
        CognitiveCycleManager->SetStreamSalience(ActiveStream, GetSalienceValue());
        CognitiveCycleManager->SetStreamAffordance(ActiveStream, GetAffordanceValue(CurrentState.CurrentAction));
    }
}

float UAXIOMActiveInference::GetSalienceValue() const
{
    // Salience is based on prediction error and uncertainty
    float Salience = 0.0f;
    
    // Higher prediction error = higher salience
    Salience += CurrentState.VariationalFreeEnergy;
    
    // Unexpected rewards are salient
    Salience += FMath::Abs(CurrentState.CurrentReward);
    
    // Normalize to 0-1
    return FMath::Clamp(Salience / 10.0f, 0.0f, 1.0f);
}

float UAXIOMActiveInference::GetAffordanceValue(int32 Action) const
{
    // Affordance is based on expected utility of action
    float Affordance = 0.0f;
    
    // Find policies starting with this action
    for (const FPolicy& Policy : Policies)
    {
        if (Policy.ActionSequence.Num() > 0 && Policy.ActionSequence[0] == Action)
        {
            Affordance = FMath::Max(Affordance, Policy.ExpectedUtility);
        }
    }
    
    // Normalize to 0-1
    return FMath::Clamp((Affordance + 10.0f) / 20.0f, 0.0f, 1.0f);
}

// ========================================
// INTERNAL METHODS
// ========================================

void UAXIOMActiveInference::ComputeSlotAssignments(const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    // Simplified object detection: find colored regions
    // In full implementation, would use proper sMM with variational inference
    
    TArray<FObjectSlot> DetectedObjects;
    
    // Simple blob detection by scanning for non-black pixels
    TSet<int32> Visited;
    
    for (int32 y = 0; y < Height; y += 4)
    {
        for (int32 x = 0; x < Width; x += 4)
        {
            int32 Idx = y * Width + x;
            if (Visited.Contains(Idx)) continue;
            
            const FColor& Pixel = Pixels[Idx];
            if (Pixel.R > 20 || Pixel.G > 20 || Pixel.B > 20)
            {
                // Found a colored pixel, flood fill to find blob
                FVector2D MinPos(x, y);
                FVector2D MaxPos(x, y);
                FLinearColor AvgColor = FLinearColor(Pixel);
                int32 PixelCount = 1;
                
                // Simple region growing
                TArray<FIntPoint> Stack;
                Stack.Add(FIntPoint(x, y));
                Visited.Add(Idx);
                
                while (Stack.Num() > 0 && PixelCount < 1000)
                {
                    FIntPoint Pos = Stack.Pop();
                    
                    // Check neighbors
                    for (int32 dy = -2; dy <= 2; dy += 2)
                    {
                        for (int32 dx = -2; dx <= 2; dx += 2)
                        {
                            int32 nx = Pos.X + dx;
                            int32 ny = Pos.Y + dy;
                            if (nx < 0 || nx >= Width || ny < 0 || ny >= Height) continue;
                            
                            int32 nIdx = ny * Width + nx;
                            if (Visited.Contains(nIdx)) continue;
                            
                            const FColor& NPixel = Pixels[nIdx];
                            // Check if similar color
                            if (FMath::Abs((int32)NPixel.R - (int32)Pixel.R) < 50 &&
                                FMath::Abs((int32)NPixel.G - (int32)Pixel.G) < 50 &&
                                FMath::Abs((int32)NPixel.B - (int32)Pixel.B) < 50)
                            {
                                Stack.Add(FIntPoint(nx, ny));
                                Visited.Add(nIdx);
                                
                                MinPos.X = FMath::Min(MinPos.X, (float)nx);
                                MinPos.Y = FMath::Min(MinPos.Y, (float)ny);
                                MaxPos.X = FMath::Max(MaxPos.X, (float)nx);
                                MaxPos.Y = FMath::Max(MaxPos.Y, (float)ny);
                                AvgColor += FLinearColor(NPixel);
                                PixelCount++;
                            }
                        }
                    }
                }
                
                // Create object slot if blob is large enough
                if (PixelCount > 10)
                {
                    FObjectSlot NewSlot;
                    NewSlot.Position = (MinPos + MaxPos) * 0.5f;
                    NewSlot.Size = MaxPos - MinPos;
                    NewSlot.Color = AvgColor / PixelCount;
                    NewSlot.State = ESlotState::Active;
                    NewSlot.Activation = 1.0f;
                    DetectedObjects.Add(NewSlot);
                }
            }
        }
    }
    
    UpdateSlots(DetectedObjects);
}

void UAXIOMActiveInference::VariationalUpdate()
{
    // Compute variational free energy (negative log evidence bound)
    float FreeEnergy = 0.0f;
    
    // Reconstruction error term
    for (const FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State == ESlotState::Active)
        {
            FreeEnergy += Slot.PositionUncertainty;
            FreeEnergy += (1.0f - Slot.TypeConfidence);
        }
    }
    
    // KL divergence from prior (simplified)
    FreeEnergy += 0.01f * ObjectTypes.Num();
    FreeEnergy += 0.01f * InteractionClusters.Num();
    
    CurrentState.VariationalFreeEnergy = FreeEnergy;
}

TArray<FWorldState> UAXIOMActiveInference::RolloutPolicy(const FPolicy& Policy, int32 NumSamples)
{
    TArray<FWorldState> Trajectory;
    
    // Create copy of current state for rollout
    FWorldState RolloutState = CurrentState;
    
    for (int32 t = 0; t < Policy.ActionSequence.Num(); ++t)
    {
        int32 Action = Policy.ActionSequence[t];
        
        // Predict next state
        for (FObjectSlot& Slot : RolloutState.Slots)
        {
            if (Slot.State != ESlotState::Active) continue;
            
            int32 ProtoIdx = static_cast<int32>(Slot.MotionPrototype);
            if (ProtoIdx >= TransitionPrototypes.Num()) ProtoIdx = 0;
            
            const FTransitionPrototype& Proto = TransitionPrototypes[ProtoIdx];
            
            // Simple linear prediction
            Slot.Position += Slot.Velocity;
            
            // Apply control for player-controlled objects
            if (Slot.MotionPrototype == EMotionPrototype::Controlled)
            {
                float ControlX = (Action == 2) ? 5.0f : (Action == 1) ? -5.0f : 0.0f;
                float ControlY = (Action == 4) ? 5.0f : (Action == 3) ? -5.0f : 0.0f;
                Slot.Position += FVector2D(ControlX, ControlY);
            }
        }
        
        // Predict reward
        RolloutState.CurrentReward = PredictReward(Action);
        RolloutState.Timestep = t;
        
        Trajectory.Add(RolloutState);
    }
    
    return Trajectory;
}

void UAXIOMActiveInference::MergeSimilarComponents()
{
    // Merge similar object types
    for (int32 i = 0; i < ObjectTypes.Num(); ++i)
    {
        for (int32 j = i + 1; j < ObjectTypes.Num(); ++j)
        {
            float KL = ComputeKLDivergence(ObjectTypes[i], ObjectTypes[j]);
            if (KL < Config.BMRThreshold)
            {
                // Merge j into i
                float TotalObs = ObjectTypes[i].ObservationCount + ObjectTypes[j].ObservationCount;
                float Wi = ObjectTypes[i].ObservationCount / TotalObs;
                float Wj = ObjectTypes[j].ObservationCount / TotalObs;
                
                for (int32 k = 0; k < ObjectTypes[i].Mean.Num(); ++k)
                {
                    ObjectTypes[i].Mean[k] = Wi * ObjectTypes[i].Mean[k] + Wj * ObjectTypes[j].Mean[k];
                    ObjectTypes[i].Variance[k] = Wi * ObjectTypes[i].Variance[k] + Wj * ObjectTypes[j].Variance[k];
                }
                ObjectTypes[i].ObservationCount = TotalObs;
                ObjectTypes[i].Weight += ObjectTypes[j].Weight;
                
                // Remove merged component
                ObjectTypes.RemoveAt(j);
                j--;
                
                UE_LOG(LogTemp, Log, TEXT("AXIOM BMR: Merged object types"));
            }
        }
    }
    
    // Similar merging for interaction clusters
    for (int32 i = 0; i < InteractionClusters.Num(); ++i)
    {
        for (int32 j = i + 1; j < InteractionClusters.Num(); ++j)
        {
            // Merge if same object type pair and similar reward
            if (InteractionClusters[i].ObjectTypePair == InteractionClusters[j].ObjectTypePair &&
                FMath::Abs(InteractionClusters[i].ExpectedReward - InteractionClusters[j].ExpectedReward) < 0.5f)
            {
                float TotalCount = InteractionClusters[i].ActivationCount + InteractionClusters[j].ActivationCount;
                float Wi = InteractionClusters[i].ActivationCount / TotalCount;
                float Wj = InteractionClusters[j].ActivationCount / TotalCount;
                
                InteractionClusters[i].ExpectedReward = Wi * InteractionClusters[i].ExpectedReward + 
                                                        Wj * InteractionClusters[j].ExpectedReward;
                InteractionClusters[i].DistanceThreshold = Wi * InteractionClusters[i].DistanceThreshold + 
                                                          Wj * InteractionClusters[j].DistanceThreshold;
                InteractionClusters[i].ActivationCount = TotalCount;
                
                InteractionClusters.RemoveAt(j);
                j--;
                
                UE_LOG(LogTemp, Log, TEXT("AXIOM BMR: Merged interaction clusters"));
            }
        }
    }
}

bool UAXIOMActiveInference::NeedsStructureExpansion() const
{
    // Check if current model is insufficient
    
    // High free energy suggests model doesn't fit well
    if (CurrentState.VariationalFreeEnergy > 5.0f)
        return true;
    
    // Many unclassified objects
    int32 Unclassified = 0;
    for (const FObjectSlot& Slot : CurrentState.Slots)
    {
        if (Slot.State == ESlotState::Active && Slot.ObjectTypeID < 0)
            Unclassified++;
    }
    if (Unclassified > 2)
        return true;
    
    return false;
}

float UAXIOMActiveInference::ComputeKLDivergence(const FGaussianComponent& P, const FGaussianComponent& Q) const
{
    // KL(P||Q) for diagonal Gaussians
    float KL = 0.0f;
    
    int32 D = FMath::Min(P.Mean.Num(), Q.Mean.Num());
    for (int32 i = 0; i < D; ++i)
    {
        float VarP = FMath::Max(P.Variance[i], 0.001f);
        float VarQ = FMath::Max(Q.Variance[i], 0.001f);
        float MeanDiff = P.Mean[i] - Q.Mean[i];
        
        KL += FMath::Loge(VarQ / VarP) + (VarP + MeanDiff * MeanDiff) / VarQ - 1.0f;
    }
    
    return 0.5f * KL;
}

int32 UAXIOMActiveInference::SampleCategorical(const TArray<float>& Probabilities) const
{
    float U = FMath::FRand();
    float CumSum = 0.0f;
    
    for (int32 i = 0; i < Probabilities.Num(); ++i)
    {
        CumSum += Probabilities[i];
        if (U < CumSum)
            return i;
    }
    
    return Probabilities.Num() - 1;
}

TArray<float> UAXIOMActiveInference::Softmax(const TArray<float>& Values, float Temperature) const
{
    TArray<float> Result;
    Result.SetNum(Values.Num());
    
    // Find max for numerical stability
    float MaxVal = -FLT_MAX;
    for (float V : Values)
        MaxVal = FMath::Max(MaxVal, V);
    
    // Compute exp and sum
    float Sum = 0.0f;
    for (int32 i = 0; i < Values.Num(); ++i)
    {
        Result[i] = FMath::Exp((Values[i] - MaxVal) / Temperature);
        Sum += Result[i];
    }
    
    // Normalize
    for (int32 i = 0; i < Result.Num(); ++i)
    {
        Result[i] /= Sum;
    }
    
    return Result;
}
