#include "Embodied4ECognition.h"
#include "Components/SkeletalMeshComponent.h"

UEmbodied4ECognition::UEmbodied4ECognition()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz
}

void UEmbodied4ECognition::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
    Initialize4ECognition();
}

void UEmbodied4ECognition::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnable4ECognition)
        return;

    // Update all dimensions
    Update4EDimensions(DeltaTime);

    // Couple dimensions
    CoupleDimensionsThroughReservoir();

    // Check for dominant dimension change
    CheckDominantDimensionChange();
}

void UEmbodied4ECognition::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CognitiveBridge = Owner->FindComponentByClass<UDeepCognitiveBridge>();
        BaseReservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

void UEmbodied4ECognition::Initialize4ECognition()
{
    InitializeDimensionReservoirs();
    InitializeBodySchema();
    InitializeAffordanceDetection();
    InitializeSensorimotorLearning();
    InitializeToolIntegration();
}

void UEmbodied4ECognition::InitializeDimensionReservoirs()
{
    // Initialize Embodied reservoir
    EmbodiedState.Dimension = E4ECognitionDimension::Embodied;
    EmbodiedState.ReservoirState.ReservoirID = TEXT("4E_Embodied");
    EmbodiedState.ReservoirState.Units = UnitsPerDimension;
    EmbodiedState.ReservoirState.SpectralRadius = 0.9f;
    EmbodiedState.ReservoirState.LeakRate = 0.3f;
    EmbodiedState.ReservoirState.ActivationState.SetNum(UnitsPerDimension);

    // Initialize Embedded reservoir
    EmbeddedState.Dimension = E4ECognitionDimension::Embedded;
    EmbeddedState.ReservoirState.ReservoirID = TEXT("4E_Embedded");
    EmbeddedState.ReservoirState.Units = UnitsPerDimension;
    EmbeddedState.ReservoirState.SpectralRadius = 0.85f;
    EmbeddedState.ReservoirState.LeakRate = 0.35f;
    EmbeddedState.ReservoirState.ActivationState.SetNum(UnitsPerDimension);

    // Initialize Enacted reservoir
    EnactedState.Dimension = E4ECognitionDimension::Enacted;
    EnactedState.ReservoirState.ReservoirID = TEXT("4E_Enacted");
    EnactedState.ReservoirState.Units = UnitsPerDimension;
    EnactedState.ReservoirState.SpectralRadius = 0.95f;
    EnactedState.ReservoirState.LeakRate = 0.25f;
    EnactedState.ReservoirState.ActivationState.SetNum(UnitsPerDimension);

    // Initialize Extended reservoir
    ExtendedState.Dimension = E4ECognitionDimension::Extended;
    ExtendedState.ReservoirState.ReservoirID = TEXT("4E_Extended");
    ExtendedState.ReservoirState.Units = UnitsPerDimension;
    ExtendedState.ReservoirState.SpectralRadius = 0.8f;
    ExtendedState.ReservoirState.LeakRate = 0.4f;
    ExtendedState.ReservoirState.ActivationState.SetNum(UnitsPerDimension);
}

void UEmbodied4ECognition::InitializeBodySchema()
{
    BodySchema.Empty();

    // Initialize default body schema elements for avatar
    TArray<FString> ElementNames = {
        TEXT("Head"), TEXT("Neck"), TEXT("Spine"),
        TEXT("LeftShoulder"), TEXT("LeftArm"), TEXT("LeftHand"),
        TEXT("RightShoulder"), TEXT("RightArm"), TEXT("RightHand"),
        TEXT("LeftHip"), TEXT("LeftLeg"), TEXT("LeftFoot"),
        TEXT("RightHip"), TEXT("RightLeg"), TEXT("RightFoot")
    };

    for (const FString& Name : ElementNames)
    {
        FBodySchemaElement Element;
        Element.ElementName = Name;
        Element.ProprioceptiveState = FTransform::Identity;
        Element.KinestheticVelocity = FVector::ZeroVector;
        Element.InteroceptiveSignal = 0.5f;
        Element.ReservoirEncoding.SetNum(UnitsPerDimension / ElementNames.Num());
        BodySchema.Add(Element);
    }
}

void UEmbodied4ECognition::InitializeAffordanceDetection()
{
    Affordances.Empty();
    // Affordances will be populated dynamically through DetectAffordances
}

void UEmbodied4ECognition::InitializeSensorimotorLearning()
{
    SensorimotorContingencies.Empty();
    // Contingencies will be learned through LearnSensorimotorContingency
}

void UEmbodied4ECognition::InitializeToolIntegration()
{
    CognitiveTools.Empty();

    // Register default cognitive tools
    RegisterCognitiveTool(TEXT("Language"), TEXT("Symbolic"));
    RegisterCognitiveTool(TEXT("Memory"), TEXT("Storage"));
    RegisterCognitiveTool(TEXT("Interface"), TEXT("Interaction"));
}

void UEmbodied4ECognition::UpdateBodySchema(USkeletalMeshComponent* SkeletalMesh)
{
    if (!SkeletalMesh)
        return;

    for (FBodySchemaElement& Element : BodySchema)
    {
        FName BoneName = FName(*Element.ElementName);
        int32 BoneIndex = SkeletalMesh->GetBoneIndex(BoneName);

        if (BoneIndex != INDEX_NONE)
        {
            // Get bone transform
            FTransform BoneTransform = SkeletalMesh->GetBoneTransform(BoneIndex);
            
            // Compute velocity from previous state
            FVector PreviousLocation = Element.ProprioceptiveState.GetLocation();
            FVector CurrentLocation = BoneTransform.GetLocation();
            Element.KinestheticVelocity = (CurrentLocation - PreviousLocation) * BodySchemaUpdateRate;

            // Update proprioceptive state
            Element.ProprioceptiveState = BoneTransform;

            // Encode to reservoir
            Element.ReservoirEncoding = EncodeBodyElementToReservoir(Element);
        }
    }
}

FBodySchemaElement UEmbodied4ECognition::GetBodySchemaElement(const FString& ElementName) const
{
    for (const FBodySchemaElement& Element : BodySchema)
    {
        if (Element.ElementName == ElementName)
        {
            return Element;
        }
    }
    return FBodySchemaElement();
}

TArray<float> UEmbodied4ECognition::ComputeProprioceptiveState() const
{
    TArray<float> State;
    State.SetNum(UnitsPerDimension);

    int32 Index = 0;
    for (const FBodySchemaElement& Element : BodySchema)
    {
        // Encode position
        FVector Pos = Element.ProprioceptiveState.GetLocation();
        if (Index < State.Num()) State[Index++] = Pos.X / 100.0f;
        if (Index < State.Num()) State[Index++] = Pos.Y / 100.0f;
        if (Index < State.Num()) State[Index++] = Pos.Z / 100.0f;

        // Encode rotation
        FRotator Rot = Element.ProprioceptiveState.Rotator();
        if (Index < State.Num()) State[Index++] = Rot.Pitch / 180.0f;
        if (Index < State.Num()) State[Index++] = Rot.Yaw / 180.0f;
        if (Index < State.Num()) State[Index++] = Rot.Roll / 180.0f;
    }

    return State;
}

float UEmbodied4ECognition::ComputeInteroceptiveState() const
{
    float TotalInteroception = 0.0f;
    for (const FBodySchemaElement& Element : BodySchema)
    {
        TotalInteroception += Element.InteroceptiveSignal;
    }
    return BodySchema.Num() > 0 ? TotalInteroception / BodySchema.Num() : 0.5f;
}

void UEmbodied4ECognition::DetectAffordances(const TArray<AActor*>& EnvironmentActors)
{
    Affordances.Empty();

    for (AActor* Actor : EnvironmentActors)
    {
        if (!Actor)
            continue;

        FEnvironmentalAffordance Affordance;
        Affordance.AffordanceID = Actor->GetName();
        Affordance.Location = Actor->GetActorLocation();

        // Determine affordance type based on actor properties
        // This is a simplified heuristic - real implementation would use more sophisticated detection
        FVector Distance = Affordance.Location - GetOwner()->GetActorLocation();
        float DistanceMagnitude = Distance.Size();

        if (DistanceMagnitude < 100.0f)
        {
            Affordance.ActionType = TEXT("Grasp");
            Affordance.Strength = 1.0f - (DistanceMagnitude / 100.0f);
            Affordance.RequiredCapability = TEXT("Hand");
        }
        else if (DistanceMagnitude < 500.0f)
        {
            Affordance.ActionType = TEXT("Approach");
            Affordance.Strength = 1.0f - (DistanceMagnitude / 500.0f);
            Affordance.RequiredCapability = TEXT("Locomotion");
        }
        else
        {
            Affordance.ActionType = TEXT("Observe");
            Affordance.Strength = FMath::Max(0.0f, 1.0f - (DistanceMagnitude / 1000.0f));
            Affordance.RequiredCapability = TEXT("Vision");
        }

        Affordance.ReservoirEncoding = EncodeAffordanceToReservoir(Affordance);
        Affordances.Add(Affordance);

        // Broadcast event
        OnAffordanceDetected.Broadcast(Affordance);
    }
}

FEnvironmentalAffordance UEmbodied4ECognition::GetAffordance(const FString& AffordanceID) const
{
    for (const FEnvironmentalAffordance& Affordance : Affordances)
    {
        if (Affordance.AffordanceID == AffordanceID)
        {
            return Affordance;
        }
    }
    return FEnvironmentalAffordance();
}

FEnvironmentalAffordance UEmbodied4ECognition::GetStrongestAffordance() const
{
    FEnvironmentalAffordance Strongest;
    float MaxStrength = -1.0f;

    for (const FEnvironmentalAffordance& Affordance : Affordances)
    {
        if (Affordance.Strength > MaxStrength)
        {
            MaxStrength = Affordance.Strength;
            Strongest = Affordance;
        }
    }

    return Strongest;
}

TArray<float> UEmbodied4ECognition::ComputeEnvironmentalEmbedding() const
{
    TArray<float> Embedding;
    Embedding.SetNum(UnitsPerDimension);

    // Combine all affordance encodings
    for (const FEnvironmentalAffordance& Affordance : Affordances)
    {
        for (int32 i = 0; i < Embedding.Num() && i < Affordance.ReservoirEncoding.Num(); i++)
        {
            Embedding[i] += Affordance.ReservoirEncoding[i] * Affordance.Strength;
        }
    }

    // Normalize
    float MaxVal = 0.0f;
    for (float Val : Embedding)
    {
        MaxVal = FMath::Max(MaxVal, FMath::Abs(Val));
    }
    if (MaxVal > 0.0f)
    {
        for (float& Val : Embedding)
        {
            Val /= MaxVal;
        }
    }

    return Embedding;
}

void UEmbodied4ECognition::LearnSensorimotorContingency(const TArray<float>& SensoryPattern, const TArray<float>& MotorPattern)
{
    FSensorimotorContingency Contingency;
    Contingency.ContingencyID = FString::Printf(TEXT("SMC_%d"), SensorimotorContingencies.Num());
    Contingency.SensoryPattern = SensoryPattern;
    Contingency.MotorPattern = MotorPattern;
    Contingency.CouplingStrength = 0.5f;
    Contingency.PredictionAccuracy = 0.5f;

    SensorimotorContingencies.Add(Contingency);

    // Broadcast event
    OnContingencyLearned.Broadcast(Contingency);
}

TArray<float> UEmbodied4ECognition::PredictMotorFromSensory(const TArray<float>& SensoryInput) const
{
    TArray<float> PredictedMotor;
    PredictedMotor.SetNum(UnitsPerDimension);

    // Find best matching contingency
    float BestMatch = -1.0f;
    int32 BestIndex = -1;

    for (int32 i = 0; i < SensorimotorContingencies.Num(); i++)
    {
        const FSensorimotorContingency& Contingency = SensorimotorContingencies[i];

        // Compute similarity
        float DotProduct = 0.0f;
        float NormInput = 0.0f;
        float NormPattern = 0.0f;

        int32 MinSize = FMath::Min(SensoryInput.Num(), Contingency.SensoryPattern.Num());
        for (int32 j = 0; j < MinSize; j++)
        {
            DotProduct += SensoryInput[j] * Contingency.SensoryPattern[j];
            NormInput += SensoryInput[j] * SensoryInput[j];
            NormPattern += Contingency.SensoryPattern[j] * Contingency.SensoryPattern[j];
        }

        float Similarity = 0.0f;
        if (NormInput > 0.0f && NormPattern > 0.0f)
        {
            Similarity = DotProduct / (FMath::Sqrt(NormInput) * FMath::Sqrt(NormPattern));
        }

        if (Similarity > BestMatch)
        {
            BestMatch = Similarity;
            BestIndex = i;
        }
    }

    // Return best matching motor pattern
    if (BestIndex >= 0)
    {
        const TArray<float>& MotorPattern = SensorimotorContingencies[BestIndex].MotorPattern;
        for (int32 i = 0; i < PredictedMotor.Num() && i < MotorPattern.Num(); i++)
        {
            PredictedMotor[i] = MotorPattern[i] * BestMatch;
        }
    }

    return PredictedMotor;
}

TArray<float> UEmbodied4ECognition::PredictSensoryFromMotor(const TArray<float>& MotorCommand) const
{
    TArray<float> PredictedSensory;
    PredictedSensory.SetNum(UnitsPerDimension);

    // Similar to PredictMotorFromSensory but reversed
    float BestMatch = -1.0f;
    int32 BestIndex = -1;

    for (int32 i = 0; i < SensorimotorContingencies.Num(); i++)
    {
        const FSensorimotorContingency& Contingency = SensorimotorContingencies[i];

        float DotProduct = 0.0f;
        float NormInput = 0.0f;
        float NormPattern = 0.0f;

        int32 MinSize = FMath::Min(MotorCommand.Num(), Contingency.MotorPattern.Num());
        for (int32 j = 0; j < MinSize; j++)
        {
            DotProduct += MotorCommand[j] * Contingency.MotorPattern[j];
            NormInput += MotorCommand[j] * MotorCommand[j];
            NormPattern += Contingency.MotorPattern[j] * Contingency.MotorPattern[j];
        }

        float Similarity = 0.0f;
        if (NormInput > 0.0f && NormPattern > 0.0f)
        {
            Similarity = DotProduct / (FMath::Sqrt(NormInput) * FMath::Sqrt(NormPattern));
        }

        if (Similarity > BestMatch)
        {
            BestMatch = Similarity;
            BestIndex = i;
        }
    }

    if (BestIndex >= 0)
    {
        const TArray<float>& SensoryPattern = SensorimotorContingencies[BestIndex].SensoryPattern;
        for (int32 i = 0; i < PredictedSensory.Num() && i < SensoryPattern.Num(); i++)
        {
            PredictedSensory[i] = SensoryPattern[i] * BestMatch;
        }
    }

    return PredictedSensory;
}

TArray<float> UEmbodied4ECognition::ComputeEnactionState() const
{
    TArray<float> State;
    State.SetNum(UnitsPerDimension);

    // Combine all contingency patterns
    for (const FSensorimotorContingency& Contingency : SensorimotorContingencies)
    {
        for (int32 i = 0; i < State.Num(); i++)
        {
            float Sensory = i < Contingency.SensoryPattern.Num() ? Contingency.SensoryPattern[i] : 0.0f;
            float Motor = i < Contingency.MotorPattern.Num() ? Contingency.MotorPattern[i] : 0.0f;
            State[i] += (Sensory + Motor) * Contingency.CouplingStrength * 0.5f;
        }
    }

    return State;
}

void UEmbodied4ECognition::RegisterCognitiveTool(const FString& ToolID, const FString& ToolType)
{
    FExtendedCognitiveTool Tool;
    Tool.ToolID = ToolID;
    Tool.ToolType = ToolType;
    Tool.IntegrationLevel = 0.0f;
    Tool.ToolState.SetNum(UnitsPerDimension / 4);
    Tool.EnhancementFactor = 1.0f;

    CognitiveTools.Add(Tool);
}

void UEmbodied4ECognition::UpdateToolState(const FString& ToolID, const TArray<float>& NewState)
{
    for (FExtendedCognitiveTool& Tool : CognitiveTools)
    {
        if (Tool.ToolID == ToolID)
        {
            Tool.ToolState = NewState;

            // Update integration level based on usage
            float PreviousLevel = Tool.IntegrationLevel;
            Tool.IntegrationLevel = FMath::Min(1.0f, Tool.IntegrationLevel + 0.01f);

            if (FMath::Abs(Tool.IntegrationLevel - PreviousLevel) > 0.05f)
            {
                OnToolIntegrationChanged.Broadcast(ToolID, Tool.IntegrationLevel);
            }

            break;
        }
    }
}

float UEmbodied4ECognition::GetToolIntegrationLevel(const FString& ToolID) const
{
    for (const FExtendedCognitiveTool& Tool : CognitiveTools)
    {
        if (Tool.ToolID == ToolID)
        {
            return Tool.IntegrationLevel;
        }
    }
    return 0.0f;
}

TArray<float> UEmbodied4ECognition::ComputeExtendedCognitionState() const
{
    TArray<float> State;
    State.SetNum(UnitsPerDimension);

    // Combine all tool states weighted by integration level
    for (const FExtendedCognitiveTool& Tool : CognitiveTools)
    {
        for (int32 i = 0; i < State.Num() && i < Tool.ToolState.Num(); i++)
        {
            State[i] += Tool.ToolState[i] * Tool.IntegrationLevel * Tool.EnhancementFactor;
        }
    }

    return State;
}

void UEmbodied4ECognition::Update4EDimensions(float DeltaTime)
{
    UpdateEmbodiedDimension(DeltaTime);
    UpdateEmbeddedDimension(DeltaTime);
    UpdateEnactedDimension(DeltaTime);
    UpdateExtendedDimension(DeltaTime);
}

F4EDimensionState UEmbodied4ECognition::GetDimensionState(E4ECognitionDimension Dimension) const
{
    switch (Dimension)
    {
        case E4ECognitionDimension::Embodied: return EmbodiedState;
        case E4ECognitionDimension::Embedded: return EmbeddedState;
        case E4ECognitionDimension::Enacted: return EnactedState;
        case E4ECognitionDimension::Extended: return ExtendedState;
        default: return F4EDimensionState();
    }
}

float UEmbodied4ECognition::ComputeCrossDimensionalCoherence() const
{
    // Compute pairwise coherence between all dimensions
    TArray<const TArray<float>*> States = {
        &EmbodiedState.ReservoirState.ActivationState,
        &EmbeddedState.ReservoirState.ActivationState,
        &EnactedState.ReservoirState.ActivationState,
        &ExtendedState.ReservoirState.ActivationState
    };

    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (int32 i = 0; i < 4; i++)
    {
        for (int32 j = i + 1; j < 4; j++)
        {
            const TArray<float>& A = *States[i];
            const TArray<float>& B = *States[j];

            float DotProduct = 0.0f;
            float NormA = 0.0f;
            float NormB = 0.0f;

            int32 MinSize = FMath::Min(A.Num(), B.Num());
            for (int32 k = 0; k < MinSize; k++)
            {
                DotProduct += A[k] * B[k];
                NormA += A[k] * A[k];
                NormB += B[k] * B[k];
            }

            if (NormA > 0.0f && NormB > 0.0f)
            {
                TotalCoherence += DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
            }
            PairCount++;
        }
    }

    return PairCount > 0 ? TotalCoherence / PairCount : 0.0f;
}

E4ECognitionDimension UEmbodied4ECognition::GetDominantDimension() const
{
    float MaxActivation = EmbodiedState.ActivationLevel;
    E4ECognitionDimension Dominant = E4ECognitionDimension::Embodied;

    if (EmbeddedState.ActivationLevel > MaxActivation)
    {
        MaxActivation = EmbeddedState.ActivationLevel;
        Dominant = E4ECognitionDimension::Embedded;
    }
    if (EnactedState.ActivationLevel > MaxActivation)
    {
        MaxActivation = EnactedState.ActivationLevel;
        Dominant = E4ECognitionDimension::Enacted;
    }
    if (ExtendedState.ActivationLevel > MaxActivation)
    {
        Dominant = E4ECognitionDimension::Extended;
    }

    return Dominant;
}

TArray<float> UEmbodied4ECognition::ComputeIntegrated4EState() const
{
    TArray<float> IntegratedState;
    IntegratedState.SetNum(UnitsPerDimension);

    // Weighted combination of all dimensions
    for (int32 i = 0; i < IntegratedState.Num(); i++)
    {
        float Embodied = i < EmbodiedState.ReservoirState.ActivationState.Num() ?
            EmbodiedState.ReservoirState.ActivationState[i] * EmbodiedState.CognitiveContribution : 0.0f;
        float Embedded = i < EmbeddedState.ReservoirState.ActivationState.Num() ?
            EmbeddedState.ReservoirState.ActivationState[i] * EmbeddedState.CognitiveContribution : 0.0f;
        float Enacted = i < EnactedState.ReservoirState.ActivationState.Num() ?
            EnactedState.ReservoirState.ActivationState[i] * EnactedState.CognitiveContribution : 0.0f;
        float Extended = i < ExtendedState.ReservoirState.ActivationState.Num() ?
            ExtendedState.ReservoirState.ActivationState[i] * ExtendedState.CognitiveContribution : 0.0f;

        IntegratedState[i] = Embodied + Embedded + Enacted + Extended;
    }

    return IntegratedState;
}

TMap<FString, float> UEmbodied4ECognition::Map4EToExpressionHints() const
{
    TMap<FString, float> Hints;

    // Map embodied state to physical expression hints
    Hints.Add(TEXT("BodyTension"), ComputeInteroceptiveState());
    Hints.Add(TEXT("Groundedness"), EmbodiedState.ActivationLevel);

    // Map embedded state to environmental awareness hints
    FEnvironmentalAffordance StrongestAffordance = GetStrongestAffordance();
    Hints.Add(TEXT("EnvironmentalEngagement"), EmbeddedState.ActivationLevel);
    Hints.Add(TEXT("AffordanceAttention"), StrongestAffordance.Strength);

    // Map enacted state to action readiness hints
    Hints.Add(TEXT("ActionReadiness"), EnactedState.ActivationLevel);
    Hints.Add(TEXT("SensorimotorCoupling"), SensorimotorContingencies.Num() > 0 ?
        SensorimotorContingencies[0].CouplingStrength : 0.0f);

    // Map extended state to tool integration hints
    float TotalToolIntegration = 0.0f;
    for (const FExtendedCognitiveTool& Tool : CognitiveTools)
    {
        TotalToolIntegration += Tool.IntegrationLevel;
    }
    Hints.Add(TEXT("ToolIntegration"), CognitiveTools.Num() > 0 ?
        TotalToolIntegration / CognitiveTools.Num() : 0.0f);
    Hints.Add(TEXT("CognitiveExtension"), ExtendedState.ActivationLevel);

    // Cross-dimensional coherence
    Hints.Add(TEXT("4ECoherence"), ComputeCrossDimensionalCoherence());

    return Hints;
}

float UEmbodied4ECognition::GetEmbodiedExpressionWeight() const
{
    return EmbodiedState.ActivationLevel * EmbodiedState.CognitiveContribution;
}

float UEmbodied4ECognition::GetEnvironmentalExpressionModulation() const
{
    return EmbeddedState.ActivationLevel * (1.0f + GetStrongestAffordance().Strength);
}

void UEmbodied4ECognition::UpdateEmbodiedDimension(float DeltaTime)
{
    // Update body schema timer
    BodySchemaTimer += DeltaTime;
    if (BodySchemaTimer >= 1.0f / BodySchemaUpdateRate)
    {
        BodySchemaTimer = 0.0f;

        // Update body schema from owner's skeletal mesh
        AActor* Owner = GetOwner();
        if (Owner)
        {
            USkeletalMeshComponent* SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
            if (SkeletalMesh)
            {
                UpdateBodySchema(SkeletalMesh);
            }
        }
    }

    // Update reservoir state
    TArray<float> ProprioceptiveState = ComputeProprioceptiveState();
    FReservoirState& Reservoir = EmbodiedState.ReservoirState;

    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < ProprioceptiveState.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(ProprioceptiveState[i]);
    }

    // Update activation level
    float TotalActivation = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        TotalActivation += FMath::Abs(Val);
    }
    EmbodiedState.ActivationLevel = TotalActivation / Reservoir.ActivationState.Num();
}

void UEmbodied4ECognition::UpdateEmbeddedDimension(float DeltaTime)
{
    // Update reservoir state from environmental embedding
    TArray<float> EnvironmentalEmbedding = ComputeEnvironmentalEmbedding();
    FReservoirState& Reservoir = EmbeddedState.ReservoirState;

    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < EnvironmentalEmbedding.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(EnvironmentalEmbedding[i]);
    }

    // Update activation level
    float TotalActivation = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        TotalActivation += FMath::Abs(Val);
    }
    EmbeddedState.ActivationLevel = TotalActivation / Reservoir.ActivationState.Num();
}

void UEmbodied4ECognition::UpdateEnactedDimension(float DeltaTime)
{
    // Update reservoir state from enaction state
    TArray<float> EnactionState = ComputeEnactionState();
    FReservoirState& Reservoir = EnactedState.ReservoirState;

    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < EnactionState.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(EnactionState[i]);
    }

    // Update activation level
    float TotalActivation = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        TotalActivation += FMath::Abs(Val);
    }
    EnactedState.ActivationLevel = TotalActivation / Reservoir.ActivationState.Num();
}

void UEmbodied4ECognition::UpdateExtendedDimension(float DeltaTime)
{
    // Update reservoir state from extended cognition state
    TArray<float> ExtendedCognitionState = ComputeExtendedCognitionState();
    FReservoirState& Reservoir = ExtendedState.ReservoirState;

    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < ExtendedCognitionState.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(ExtendedCognitionState[i]);
    }

    // Update activation level
    float TotalActivation = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        TotalActivation += FMath::Abs(Val);
    }
    ExtendedState.ActivationLevel = TotalActivation / Reservoir.ActivationState.Num();
}

void UEmbodied4ECognition::CoupleDimensionsThroughReservoir()
{
    // Apply cross-dimensional coupling
    TArray<FReservoirState*> Reservoirs = {
        &EmbodiedState.ReservoirState,
        &EmbeddedState.ReservoirState,
        &EnactedState.ReservoirState,
        &ExtendedState.ReservoirState
    };

    // Compute average state
    TArray<float> AverageState;
    AverageState.SetNum(UnitsPerDimension);

    for (FReservoirState* Reservoir : Reservoirs)
    {
        for (int32 i = 0; i < AverageState.Num() && i < Reservoir->ActivationState.Num(); i++)
        {
            AverageState[i] += Reservoir->ActivationState[i] / 4.0f;
        }
    }

    // Apply coupling
    for (FReservoirState* Reservoir : Reservoirs)
    {
        for (int32 i = 0; i < Reservoir->ActivationState.Num() && i < AverageState.Num(); i++)
        {
            Reservoir->ActivationState[i] = FMath::Lerp(
                Reservoir->ActivationState[i],
                AverageState[i],
                CrossDimensionalCoupling * 0.1f
            );
        }
    }

    // Update cross-dimensional coherence
    float Coherence = ComputeCrossDimensionalCoherence();
    EmbodiedState.CrossDimensionalCoherence = Coherence;
    EmbeddedState.CrossDimensionalCoherence = Coherence;
    EnactedState.CrossDimensionalCoherence = Coherence;
    ExtendedState.CrossDimensionalCoherence = Coherence;
}

void UEmbodied4ECognition::CheckDominantDimensionChange()
{
    E4ECognitionDimension CurrentDominant = GetDominantDimension();
    if (CurrentDominant != PreviousDominantDimension)
    {
        OnDominantDimensionChanged.Broadcast(PreviousDominantDimension, CurrentDominant);
        PreviousDominantDimension = CurrentDominant;
    }
}

TArray<float> UEmbodied4ECognition::EncodeBodyElementToReservoir(const FBodySchemaElement& Element) const
{
    TArray<float> Encoding;
    int32 EncodingSize = UnitsPerDimension / FMath::Max(1, BodySchema.Num());
    Encoding.SetNum(EncodingSize);

    // Encode position
    FVector Pos = Element.ProprioceptiveState.GetLocation();
    if (Encoding.Num() > 0) Encoding[0] = FMath::Tanh(Pos.X / 100.0f);
    if (Encoding.Num() > 1) Encoding[1] = FMath::Tanh(Pos.Y / 100.0f);
    if (Encoding.Num() > 2) Encoding[2] = FMath::Tanh(Pos.Z / 100.0f);

    // Encode velocity
    if (Encoding.Num() > 3) Encoding[3] = FMath::Tanh(Element.KinestheticVelocity.X / 100.0f);
    if (Encoding.Num() > 4) Encoding[4] = FMath::Tanh(Element.KinestheticVelocity.Y / 100.0f);
    if (Encoding.Num() > 5) Encoding[5] = FMath::Tanh(Element.KinestheticVelocity.Z / 100.0f);

    // Encode interoception
    if (Encoding.Num() > 6) Encoding[6] = Element.InteroceptiveSignal;

    return Encoding;
}

TArray<float> UEmbodied4ECognition::EncodeAffordanceToReservoir(const FEnvironmentalAffordance& Affordance) const
{
    TArray<float> Encoding;
    int32 EncodingSize = UnitsPerDimension / FMath::Max(1, Affordances.Num());
    Encoding.SetNum(EncodingSize);

    // Encode location
    if (Encoding.Num() > 0) Encoding[0] = FMath::Tanh(Affordance.Location.X / 1000.0f);
    if (Encoding.Num() > 1) Encoding[1] = FMath::Tanh(Affordance.Location.Y / 1000.0f);
    if (Encoding.Num() > 2) Encoding[2] = FMath::Tanh(Affordance.Location.Z / 1000.0f);

    // Encode strength
    if (Encoding.Num() > 3) Encoding[3] = Affordance.Strength;

    return Encoding;
}
