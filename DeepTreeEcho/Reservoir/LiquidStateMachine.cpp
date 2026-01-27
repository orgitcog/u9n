// LiquidStateMachine.cpp

#include "LiquidStateMachine.h"
#include "Math/UnrealMathUtility.h"

ULiquidStateMachine::ULiquidStateMachine()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ULiquidStateMachine::BeginPlay()
{
    Super::BeginPlay();
    
    if (bEnableLSM && !State.bIsInitialized)
    {
        InitializeLSM(Config);
    }
}

void ULiquidStateMachine::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bEnableLSM && State.bIsInitialized)
    {
        StepSimulation(DeltaTime);
    }
}

// ========================================
// INITIALIZATION
// ========================================

void ULiquidStateMachine::InitializeLSM(const FLSMConfig& InConfig)
{
    Config = InConfig;
    
    // Validate configuration
    if (Config.OutputDimension > Config.NumNeurons)
    {
        UE_LOG(LogTemp, Error, TEXT("LSM: OutputDimension (%d) exceeds NumNeurons (%d). Clamping to NumNeurons."), 
               Config.OutputDimension, Config.NumNeurons);
        Config.OutputDimension = Config.NumNeurons;
    }
    
    // Initialize neurons
    State.Neurons.Empty();
    State.Neurons.Reserve(Config.NumNeurons);
    
    int32 NumExcitatory = FMath::RoundToInt(Config.NumNeurons * Config.ExcitatoryRatio);
    
    for (int32 i = 0; i < Config.NumNeurons; ++i)
    {
        FLIFNeuronState Neuron;
        Neuron.NeuronID = i;
        Neuron.MembranePotential = Neuron.RestingPotential;
        Neuron.ThresholdPotential = -55.0f + FMath::FRandRange(-2.0f, 2.0f); // Add variability
        Neuron.TauMembrane = 20.0f + FMath::FRandRange(-5.0f, 5.0f); // Add variability
        State.Neurons.Add(Neuron);
    }
    
    // Build network topology
    BuildNetwork();
    
    // Initialize readout layer
    InitializeReadoutLayer();
    
    // Initialize state
    State.CurrentTime = 0.0f;
    State.TotalSpikeCount = 0;
    State.AverageFiringRate = 0.0f;
    State.bIsInitialized = true;
    
    UE_LOG(LogTemp, Log, TEXT("LSM Initialized: %d neurons, %d synapses"), 
           State.Neurons.Num(), State.Synapses.Num());
}

void ULiquidStateMachine::ResetLSM()
{
    // Reset all neuron states
    for (FLIFNeuronState& Neuron : State.Neurons)
    {
        Neuron.MembranePotential = Neuron.RestingPotential;
        Neuron.TimeSinceSpike = 1000.0f;
        Neuron.bIsSpiking = false;
        Neuron.SpikeCount = 0;
        Neuron.LastSpikeTime = -1000.0f;
    }
    
    // Clear spike history
    State.RecentSpikeTrains.Empty();
    SpikeQueue.Empty();
    FiringRateHistory.Empty();
    
    State.CurrentTime = 0.0f;
    State.TotalSpikeCount = 0;
    State.AverageFiringRate = 0.0f;
}

// ========================================
// SPIKE ENCODING/DECODING
// ========================================

TArray<FSpikeTrain> ULiquidStateMachine::EncodeInput(const TArray<float>& Input, float Duration, ESpikeEncodingType EncodingType)
{
    TArray<FSpikeTrain> SpikeTrains;
    
    for (int32 i = 0; i < FMath::Min(Input.Num(), Config.InputDimension); ++i)
    {
        float Value = FMath::Clamp(Input[i], 0.0f, 1.0f);
        
        switch (EncodingType)
        {
            case ESpikeEncodingType::RateCoding:
                SpikeTrains.Add(RateEncode(Value, Duration, i));
                break;
                
            case ESpikeEncodingType::TemporalCoding:
                SpikeTrains.Add(TemporalEncode(Value, Duration, i));
                break;
                
            case ESpikeEncodingType::PopulationCoding:
            {
                int32 PopSize = FMath::Max(10, Config.NumNeurons / Config.InputDimension);
                TArray<FSpikeTrain> PopTrains = PopulationEncode(Value, Duration, i * PopSize, PopSize);
                SpikeTrains.Append(PopTrains);
                break;
            }
                
            case ESpikeEncodingType::PhaseCoding:
                // Simple phase coding - spike time relative to oscillation
                SpikeTrains.Add(TemporalEncode(Value, Duration, i));
                break;
        }
    }
    
    return SpikeTrains;
}

TArray<float> ULiquidStateMachine::DecodeOutput(const TArray<FSpikeTrain>& SpikeTrains, float WindowSize)
{
    TArray<float> Output;
    Output.SetNum(Config.OutputDimension);
    
    // Simple rate decoding
    for (int32 i = 0; i < FMath::Min(SpikeTrains.Num(), Config.OutputDimension); ++i)
    {
        Output[i] = RateDecode(SpikeTrains[i], WindowSize);
    }
    
    return Output;
}

FSpikeTrain ULiquidStateMachine::RateEncode(float Value, float Duration, int32 NeuronID)
{
    FSpikeTrain Train;
    Train.NeuronID = NeuronID;
    Train.EncodingType = ESpikeEncodingType::RateCoding;
    
    // Value (0-1) maps to firing rate (0-100 Hz)
    float TargetRate = Value * 100.0f; // Hz
    float MeanISI = (TargetRate > 0) ? (1000.0f / TargetRate) : 10000.0f; // ms
    
    float CurrentTime = 0.0f;
    while (CurrentTime < Duration)
    {
        // Poisson process - exponential inter-spike intervals
        // Prevent log(0) by clamping random value away from zero
        const float MinRandomValue = 1e-8f; // Small epsilon to prevent log(0)
        float RandValue = FMath::Max(FMath::FRand(), MinRandomValue);
        float ISI = -MeanISI * FMath::Loge(RandValue);
        CurrentTime += ISI;
        
        if (CurrentTime < Duration)
        {
            Train.SpikeTimes.Add(State.CurrentTime + CurrentTime);
        }
    }
    
    return Train;
}

FSpikeTrain ULiquidStateMachine::TemporalEncode(float Value, float Duration, int32 NeuronID)
{
    FSpikeTrain Train;
    Train.NeuronID = NeuronID;
    Train.EncodingType = ESpikeEncodingType::TemporalCoding;
    
    // Value (0-1) maps to spike time within duration
    // Lower values = earlier spikes
    float SpikeTime = State.CurrentTime + (Value * Duration);
    Train.SpikeTimes.Add(SpikeTime);
    
    return Train;
}

TArray<FSpikeTrain> ULiquidStateMachine::PopulationEncode(float Value, float Duration, int32 StartNeuronID, int32 PopulationSize)
{
    TArray<FSpikeTrain> Trains;
    
    // Gaussian tuning curves
    for (int32 i = 0; i < PopulationSize; ++i)
    {
        int32 NeuronID = StartNeuronID + i;
        float PreferredValue = (float)i / (float)(PopulationSize - 1);
        float Sigma = 0.2f;
        
        // Gaussian response
        float Response = FMath::Exp(-FMath::Square(Value - PreferredValue) / (2.0f * Sigma * Sigma));
        
        // Encode response as rate
        FSpikeTrain Train = RateEncode(Response, Duration, NeuronID);
        Trains.Add(Train);
    }
    
    return Trains;
}

float ULiquidStateMachine::RateDecode(const FSpikeTrain& SpikeTrain, float WindowSize) const
{
    if (SpikeTrain.SpikeTimes.Num() == 0)
    {
        return 0.0f;
    }
    
    // Count spikes in recent window
    float WindowStart = State.CurrentTime - WindowSize;
    int32 SpikeCount = 0;
    
    for (float SpikeTime : SpikeTrain.SpikeTimes)
    {
        if (SpikeTime >= WindowStart && SpikeTime <= State.CurrentTime)
        {
            SpikeCount++;
        }
    }
    
    // Convert to rate (Hz) then normalize to 0-1
    float Rate = (SpikeCount / WindowSize) * 1000.0f; // Convert ms to Hz
    return FMath::Clamp(Rate / 100.0f, 0.0f, 1.0f);
}

// ========================================
// SIMULATION
// ========================================

TArray<float> ULiquidStateMachine::ProcessInput(const TArray<float>& Input, float Duration)
{
    // Encode input to spikes
    TArray<FSpikeTrain> InputSpikes = EncodeInput(Input, Duration, InputEncodingType);
    
    // Inject spikes into network
    for (const FSpikeTrain& Train : InputSpikes)
    {
        for (float SpikeTime : Train.SpikeTimes)
        {
            if (Train.NeuronID < State.Neurons.Num())
            {
                InjectSpike(Train.NeuronID, SpikeTime);
            }
        }
    }
    
    // Simulate for duration
    float StartTime = State.CurrentTime;
    while (State.CurrentTime < StartTime + Duration)
    {
        StepSimulation(Config.Timestep);
    }
    
    // Extract output spikes from designated output neurons
    TArray<FSpikeTrain> OutputSpikes;
    int32 OutputStartID = State.Neurons.Num() - Config.OutputDimension;
    
    for (int32 i = 0; i < Config.OutputDimension; ++i)
    {
        FSpikeTrain Train;
        Train.NeuronID = OutputStartID + i;
        
        // Collect recent spikes
        for (const FSpikeTrain& StoredTrain : State.RecentSpikeTrains)
        {
            if (StoredTrain.NeuronID == Train.NeuronID)
            {
                Train.SpikeTimes = StoredTrain.SpikeTimes;
                break;
            }
        }
        
        OutputSpikes.Add(Train);
    }
    
    // Decode to continuous values
    return DecodeOutput(OutputSpikes, Duration);
}

void ULiquidStateMachine::StepSimulation(float DeltaTime)
{
    if (!State.bIsInitialized)
    {
        return;
    }
    
    // Update time
    State.CurrentTime += DeltaTime;
    
    // Process delayed spikes
    // NOTE: Using backwards iteration with RemoveAtSwap for O(1) removal per element.
    // For very large spike queues (>1000 spikes), consider using a priority queue or heap
    // to achieve O(log n) scheduling complexity.
    for (int32 i = SpikeQueue.Num() - 1; i >= 0; --i)
    {
        if (SpikeQueue[i].Key <= State.CurrentTime)
        {
            int32 NeuronID = SpikeQueue[i].Value;
            if (State.Neurons.IsValidIndex(NeuronID))
            {
                // Use configurable spike current amplitude
                InjectCurrent(NeuronID, Config.SpikeCurrentAmplitude);
            }
            // Swap and pop for O(1) removal
            SpikeQueue.RemoveAtSwap(i, 1, false);
        }
    }
    
    // Update all neurons
    for (FLIFNeuronState& Neuron : State.Neurons)
    {
        // Compute synaptic input
        float SynapticInput = ComputeSynapticInput(Neuron.NeuronID);
        
        // Update neuron dynamics
        UpdateLIFNeuron(Neuron, DeltaTime, SynapticInput);
        
        // Check for spike
        if (CheckSpikeCondition(Neuron))
        {
            HandleNeuronSpike(Neuron, State.CurrentTime);
        }
    }
    
    // Apply plasticity rules
    if (Config.STDPConfig.bEnableSTDP)
    {
        ApplySTDP();
    }
    
    if (Config.InhibitionConfig.bEnableInhibition)
    {
        ApplyLateralInhibition();
    }
    
    if (Config.HomeostaticConfig.bEnableHomeostasis)
    {
        ApplyHomeostaticPlasticity();
    }
    
    // Update statistics
    UpdateFiringRateStatistics(DeltaTime);
}

void ULiquidStateMachine::InjectSpike(int32 NeuronID, float SpikeTime)
{
    if (State.Neurons.IsValidIndex(NeuronID))
    {
        SpikeQueue.Add(TPair<float, int32>(SpikeTime, NeuronID));
    }
}

void ULiquidStateMachine::InjectCurrent(int32 NeuronID, float Current)
{
    if (State.Neurons.IsValidIndex(NeuronID))
    {
        State.Neurons[NeuronID].MembranePotential += Current;
    }
}

// ========================================
// LIF DYNAMICS
// ========================================

void ULiquidStateMachine::UpdateLIFNeuron(FLIFNeuronState& Neuron, float DeltaTime, float InputCurrent)
{
    // Update time since last spike
    Neuron.TimeSinceSpike += DeltaTime;
    
    // Check refractory period
    if (Neuron.TimeSinceSpike < Neuron.RefractoryPeriod)
    {
        // During refractory period, clamp to reset potential
        Neuron.MembranePotential = Neuron.ResetPotential;
        Neuron.bIsSpiking = false;
        return;
    }
    
    // LIF dynamics: dV/dt = (-(V - V_rest) + R*I) / tau
    float DVdt = (-(Neuron.MembranePotential - Neuron.RestingPotential) + InputCurrent) / Neuron.TauMembrane;
    Neuron.MembranePotential += DVdt * DeltaTime;
    
    // Clamp to reasonable range
    Neuron.MembranePotential = FMath::Clamp(Neuron.MembranePotential, -100.0f, -40.0f);
}

bool ULiquidStateMachine::CheckSpikeCondition(const FLIFNeuronState& Neuron) const
{
    return (Neuron.MembranePotential >= Neuron.ThresholdPotential) && 
           (Neuron.TimeSinceSpike >= Neuron.RefractoryPeriod);
}

void ULiquidStateMachine::HandleNeuronSpike(FLIFNeuronState& Neuron, float CurrentTime)
{
    // Set spike flag
    Neuron.bIsSpiking = true;
    Neuron.LastSpikeTime = CurrentTime;
    Neuron.TimeSinceSpike = 0.0f;
    Neuron.SpikeCount++;
    
    // Reset membrane potential
    Neuron.MembranePotential = Neuron.ResetPotential;
    
    // Update global statistics
    State.TotalSpikeCount++;
    
    // Store spike in recent trains
    bool bFoundTrain = false;
    for (FSpikeTrain& Train : State.RecentSpikeTrains)
    {
        if (Train.NeuronID == Neuron.NeuronID)
        {
            Train.SpikeTimes.Add(CurrentTime);
            bFoundTrain = true;
            break;
        }
    }
    
    if (!bFoundTrain)
    {
        FSpikeTrain NewTrain;
        NewTrain.NeuronID = Neuron.NeuronID;
        NewTrain.SpikeTimes.Add(CurrentTime);
        State.RecentSpikeTrains.Add(NewTrain);
    }
    
    // Propagate spike to connected neurons
    PropagateSpikeToSynapses(Neuron.NeuronID, CurrentTime);
}

// ========================================
// SYNAPTIC DYNAMICS
// ========================================

float ULiquidStateMachine::ComputeSynapticInput(int32 NeuronID) const
{
    float TotalInput = 0.0f;
    
    for (const FSynapseState& Synapse : State.Synapses)
    {
        if (Synapse.PostNeuronID == NeuronID)
        {
            // Check if pre-synaptic neuron spiked recently
            if (State.Neurons.IsValidIndex(Synapse.PreNeuronID))
            {
                const FLIFNeuronState& PreNeuron = State.Neurons[Synapse.PreNeuronID];
                float TimeSinceSynapse = State.CurrentTime - PreNeuron.LastSpikeTime - Synapse.Delay;
                
                // Exponential decay kernel with configurable time constants
                if (TimeSinceSynapse > 0 && TimeSinceSynapse < Config.SynapticTimeWindow)
                {
                    float Amplitude = Synapse.bIsExcitatory ? 1.0f : -1.0f;
                    float Input = Amplitude * Synapse.Weight * FMath::Exp(-TimeSinceSynapse / Config.SynapticTimeConstant);
                    TotalInput += Input;
                }
            }
        }
    }
    
    return TotalInput;
}

void ULiquidStateMachine::UpdateSynapseSTDP(FSynapseState& Synapse, float PreSpikeTime, float PostSpikeTime)
{
    float DeltaT = PostSpikeTime - PreSpikeTime;
    float DeltaW = 0.0f;
    
    if (DeltaT > 0)
    {
        // Post-synaptic spike after pre-synaptic (LTP)
        DeltaW = Config.STDPConfig.APlus * FMath::Exp(-DeltaT / Config.STDPConfig.TauPlus);
    }
    else if (DeltaT < 0)
    {
        // Pre-synaptic spike after post-synaptic (LTD)
        DeltaW = -Config.STDPConfig.AMinus * FMath::Exp(DeltaT / Config.STDPConfig.TauMinus);
    }
    
    // Update weight with learning rate
    Synapse.Weight += Config.STDPConfig.LearningRate * DeltaW;
    
    // Clamp to bounds
    Synapse.Weight = ClampValue(Synapse.Weight, 
                                Config.STDPConfig.WeightMin, 
                                Config.STDPConfig.WeightMax);
}

void ULiquidStateMachine::PropagateSpikeToSynapses(int32 NeuronID, float SpikeTime)
{
    for (FSynapseState& Synapse : State.Synapses)
    {
        if (Synapse.PreNeuronID == NeuronID)
        {
            Synapse.LastPreSpikeTime = SpikeTime;
            
            // Schedule delayed spike delivery
            if (Synapse.Delay > 0)
            {
                SpikeQueue.Add(TPair<float, int32>(SpikeTime + Synapse.Delay, Synapse.PostNeuronID));
            }
        }
        else if (Synapse.PostNeuronID == NeuronID)
        {
            Synapse.LastPostSpikeTime = SpikeTime;
        }
    }
}

// ========================================
// PLASTICITY
// ========================================

void ULiquidStateMachine::ApplySTDP()
{
    for (FSynapseState& Synapse : State.Synapses)
    {
        // Only apply STDP if both neurons have spiked
        if (Synapse.LastPreSpikeTime > 0 && Synapse.LastPostSpikeTime > 0)
        {
            UpdateSynapseSTDP(Synapse, Synapse.LastPreSpikeTime, Synapse.LastPostSpikeTime);
        }
    }
}

void ULiquidStateMachine::ApplyLateralInhibition()
{
    // Find recently active neurons
    TArray<int32> ActiveNeurons;
    for (int32 i = 0; i < State.Neurons.Num(); ++i)
    {
        if (State.Neurons[i].TimeSinceSpike < 10.0f) // Recent activity
        {
            ActiveNeurons.Add(i);
        }
    }
    
    // Apply inhibition
    for (int32 ActiveID : ActiveNeurons)
    {
        for (int32 i = 0; i < State.Neurons.Num(); ++i)
        {
            if (i != ActiveID)
            {
                float Distance = GetNeuronDistance(ActiveID, i);
                if (Distance <= Config.InhibitionConfig.InhibitionRadius)
                {
                    // Inhibit nearby neurons
                    float InhibitionAmount = Config.InhibitionConfig.InhibitionStrength * 
                                           (1.0f - Distance / Config.InhibitionConfig.InhibitionRadius);
                    State.Neurons[i].MembranePotential -= InhibitionAmount;
                }
            }
        }
    }
    
    // Winner-take-all mode
    if (Config.InhibitionConfig.bWinnerTakeAll && ActiveNeurons.Num() > 0)
    {
        // Find most active neuron
        int32 WinnerID = ActiveNeurons[0];
        float MaxActivity = State.Neurons[WinnerID].MembranePotential;
        
        for (int32 ID : ActiveNeurons)
        {
            if (State.Neurons[ID].MembranePotential > MaxActivity)
            {
                WinnerID = ID;
                MaxActivity = State.Neurons[ID].MembranePotential;
            }
        }
        
        // Inhibit all except winner
        for (int32 ID : ActiveNeurons)
        {
            if (ID != WinnerID)
            {
                State.Neurons[ID].MembranePotential = State.Neurons[ID].RestingPotential;
            }
        }
    }
}

void ULiquidStateMachine::ApplyHomeostaticPlasticity()
{
    float DeltaTime = Config.Timestep / 1000.0f; // Convert to seconds
    
    for (FLIFNeuronState& Neuron : State.Neurons)
    {
        // Compute recent firing rate
        float ActualRate = GetNeuronFiringRate(Neuron.NeuronID, 1000.0f);
        
        // Adjust threshold to maintain target rate
        float RateError = ActualRate - Config.HomeostaticConfig.TargetFiringRate;
        float ThresholdAdjustment = Config.HomeostaticConfig.ThresholdAdjustmentRate * 
                                    RateError * 
                                    (DeltaTime / Config.HomeostaticConfig.TauHomeostasis);
        
        Neuron.ThresholdPotential += ThresholdAdjustment;
        
        // Clamp threshold
        Neuron.ThresholdPotential = ClampValue(Neuron.ThresholdPotential,
                                              Config.HomeostaticConfig.ThresholdMin,
                                              Config.HomeostaticConfig.ThresholdMax);
    }
}

// ========================================
// ANALYSIS
// ========================================

float ULiquidStateMachine::GetNeuronFiringRate(int32 NeuronID, float WindowSize) const
{
    if (!State.Neurons.IsValidIndex(NeuronID))
    {
        return 0.0f;
    }
    
    // Find spike train for this neuron
    for (const FSpikeTrain& Train : State.RecentSpikeTrains)
    {
        if (Train.NeuronID == NeuronID)
        {
            // Count spikes in window
            float WindowStart = State.CurrentTime - WindowSize;
            int32 SpikeCount = 0;
            
            for (float SpikeTime : Train.SpikeTimes)
            {
                if (SpikeTime >= WindowStart && SpikeTime <= State.CurrentTime)
                {
                    SpikeCount++;
                }
            }
            
            // Convert to Hz
            return (SpikeCount / WindowSize) * 1000.0f;
        }
    }
    
    return 0.0f;
}

TArray<float> ULiquidStateMachine::GetLiquidState() const
{
    TArray<float> StateVector;
    StateVector.Reserve(State.Neurons.Num());
    
    for (const FLIFNeuronState& Neuron : State.Neurons)
    {
        // Normalize membrane potential to 0-1
        float Normalized = (Neuron.MembranePotential - Neuron.RestingPotential) / 
                          (Neuron.ThresholdPotential - Neuron.RestingPotential);
        StateVector.Add(FMath::Clamp(Normalized, 0.0f, 1.0f));
    }
    
    return StateVector;
}

int32 ULiquidStateMachine::GetSpikeCount(float StartTime, float EndTime) const
{
    int32 Count = 0;
    
    for (const FSpikeTrain& Train : State.RecentSpikeTrains)
    {
        for (float SpikeTime : Train.SpikeTimes)
        {
            if (SpikeTime >= StartTime && SpikeTime <= EndTime)
            {
                Count++;
            }
        }
    }
    
    return Count;
}

float ULiquidStateMachine::GetActivityLevel() const
{
    if (State.Neurons.Num() == 0)
    {
        return 0.0f;
    }
    
    // Compute average absolute deviation from resting potential
    float TotalDeviation = 0.0f;
    for (const FLIFNeuronState& Neuron : State.Neurons)
    {
        TotalDeviation += FMath::Abs(Neuron.MembranePotential - Neuron.RestingPotential);
    }
    
    return TotalDeviation / State.Neurons.Num();
}

// ========================================
// NETWORK CONSTRUCTION
// ========================================

void ULiquidStateMachine::BuildNetwork()
{
    State.Synapses.Empty();
    
    int32 NumExcitatory = FMath::RoundToInt(Config.NumNeurons * Config.ExcitatoryRatio);
    
    // Create random connections
    for (int32 PreID = 0; PreID < Config.NumNeurons; ++PreID)
    {
        for (int32 PostID = 0; PostID < Config.NumNeurons; ++PostID)
        {
            if (PreID != PostID && ShouldConnect(PreID, PostID))
            {
                bool bExcitatory = PreID < NumExcitatory;
                float Weight = Config.InitialWeightScale * FMath::FRandRange(0.5f, 1.5f);
                float Delay = FMath::FRandRange(0.5f, 2.0f);
                
                CreateSynapse(PreID, PostID, Weight, Delay, bExcitatory);
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Built LSM network: %d synapses"), State.Synapses.Num());
}

void ULiquidStateMachine::CreateSynapse(int32 PreID, int32 PostID, float Weight, float Delay, bool bExcitatory)
{
    FSynapseState Synapse;
    Synapse.SynapseID = State.Synapses.Num();
    Synapse.PreNeuronID = PreID;
    Synapse.PostNeuronID = PostID;
    Synapse.Weight = Weight;
    Synapse.Delay = Delay;
    Synapse.bIsExcitatory = bExcitatory;
    Synapse.LastPreSpikeTime = -1000.0f;
    Synapse.LastPostSpikeTime = -1000.0f;
    Synapse.EligibilityTrace = 0.0f;
    
    State.Synapses.Add(Synapse);
}

void ULiquidStateMachine::InitializeReadoutLayer()
{
    ReadoutWeights.Empty();
    ReadoutWeights.SetNum(Config.OutputDimension);
    
    for (int32 i = 0; i < Config.OutputDimension; ++i)
    {
        ReadoutWeights[i].SetNum(Config.NumNeurons);
        
        for (int32 j = 0; j < Config.NumNeurons; ++j)
        {
            ReadoutWeights[i][j] = FMath::FRandRange(-0.1f, 0.1f);
        }
    }
}

// ========================================
// UTILITIES
// ========================================

bool ULiquidStateMachine::ShouldConnect(int32 PreID, int32 PostID) const
{
    // Distance-dependent connection probability
    float Distance = GetNeuronDistance(PreID, PostID);
    float DistanceScale = 10.0f; // Spatial scale
    float Probability = Config.ConnectionProbability * FMath::Exp(-Distance / DistanceScale);
    
    return FMath::FRand() < Probability;
}

float ULiquidStateMachine::GetNeuronDistance(int32 NeuronID1, int32 NeuronID2) const
{
    // Neurons arranged in 3D grid for spatial locality
    // NOTE: This assumes cubic spatial arrangement. For other topologies:
    // - 1D: Use abs(NeuronID1 - NeuronID2)
    // - 2D: Use 2D grid coordinates
    // - Custom: Store explicit position arrays in FLSMConfig
    // TODO: Make spatial topology configurable via FLSMConfig
    int32 GridSize = FMath::CeilToInt(FMath::Pow(Config.NumNeurons, 1.0f / 3.0f));
    
    int32 X1 = NeuronID1 % GridSize;
    int32 Y1 = (NeuronID1 / GridSize) % GridSize;
    int32 Z1 = NeuronID1 / (GridSize * GridSize);
    
    int32 X2 = NeuronID2 % GridSize;
    int32 Y2 = (NeuronID2 / GridSize) % GridSize;
    int32 Z2 = NeuronID2 / (GridSize * GridSize);
    
    return FMath::Sqrt(FMath::Square(X2 - X1) + FMath::Square(Y2 - Y1) + FMath::Square(Z2 - Z1));
}

void ULiquidStateMachine::UpdateFiringRateStatistics(float DeltaTime)
{
    // Update average firing rate
    float WindowSize = 1000.0f; // 1 second window
    int32 RecentSpikes = GetSpikeCount(State.CurrentTime - WindowSize, State.CurrentTime);
    
    if (State.Neurons.Num() > 0)
    {
        State.AverageFiringRate = (RecentSpikes / WindowSize) * 1000.0f / State.Neurons.Num();
    }
}

float ULiquidStateMachine::ClampValue(float Value, float Min, float Max) const
{
    return FMath::Clamp(Value, Min, Max);
}
