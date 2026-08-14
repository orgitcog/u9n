#include "Avatar9PServer.h"
#include "../Personality/PersonalityTraitSystem.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"
#include "../Narrative/DiaryInsightBlogLoop.h"
#include "../Avatar/Avatar3DComponent.h"

DEFINE_LOG_CATEGORY_STATIC(Log9PServer, Log, All);

UAvatar9PServer::UAvatar9PServer()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60 Hz

    bServerRunning = false;
    ServerPort = 564; // Standard 9P port
    NextFid = 1;
    bEnableCoalescing = true;
    CoalescingInterval = 0.01f; // 10ms coalescing window
    CoalescingTimer = 0.0f;
    NamespaceRoot = TEXT("/mnt/avatar");
    MaxMessageSize = 8192;
    MaxPendingMessages = 100;
    bVerboseLogging = false;
}

void UAvatar9PServer::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UAvatar9PServer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopServer();
    Super::EndPlay(EndPlayReason);
}

void UAvatar9PServer::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bServerRunning && bEnableCoalescing)
    {
        CoalescingTimer += DeltaTime;
        if (CoalescingTimer >= CoalescingInterval)
        {
            ProcessPendingMessages();
            CoalescingTimer = 0.0f;
        }
    }
}

void UAvatar9PServer::Initialize()
{
    // Find sibling components
    if (AActor* Owner = GetOwner())
    {
        PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
        NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
        NarrativeSystem = Owner->FindComponentByClass<UDiaryInsightBlogLoop>();
        AvatarComponent = Owner->FindComponentByClass<UAvatar3DComponent>();
    }

    // Build the namespace
    BuildNamespace();

    UE_LOG(Log9PServer, Log, TEXT("Avatar 9P Server initialized. Namespace root: %s"), *NamespaceRoot);
}

void UAvatar9PServer::StartServer(int32 Port)
{
    ServerPort = Port;
    bServerRunning = true;

    UE_LOG(Log9PServer, Log, TEXT("Avatar 9P Server started on port %d"), ServerPort);
}

void UAvatar9PServer::StopServer()
{
    bServerRunning = false;
    FileHandles.Empty();
    PendingRequests.Empty();

    UE_LOG(Log9PServer, Log, TEXT("Avatar 9P Server stopped"));
}

void UAvatar9PServer::BuildNamespace()
{
    NamespaceEntries.Empty();

    // Root directory
    FAvatarNamespaceEntry RootEntry;
    RootEntry.Name = TEXT("avatar");
    RootEntry.FullPath = NamespaceRoot;
    RootEntry.bIsDirectory = true;
    RootEntry.bReadable = true;
    RootEntry.Description = TEXT("Deep Tree Echo Avatar namespace root");
    NamespaceEntries.Add(RootEntry);

    // Personality namespace
    FAvatarNamespaceEntry PersonalityDir;
    PersonalityDir.Name = TEXT("personality");
    PersonalityDir.FullPath = NamespaceRoot + TEXT("/personality");
    PersonalityDir.bIsDirectory = true;
    PersonalityDir.bReadable = true;
    PersonalityDir.Description = TEXT("Personality traits and state");
    NamespaceEntries.Add(PersonalityDir);

    // Personality traits
    TArray<FString> PersonalityTraits = {
        TEXT("confidence"), TEXT("charm"), TEXT("playfulness"), TEXT("elegance"),
        TEXT("expressiveness"), TEXT("unpredictability"), TEXT("volatility"),
        TEXT("impulsivity"), TEXT("chaos_factor")
    };

    for (const FString& Trait : PersonalityTraits)
    {
        FAvatarNamespaceEntry TraitEntry;
        TraitEntry.Name = Trait;
        TraitEntry.FullPath = NamespaceRoot + TEXT("/personality/traits/") + Trait;
        TraitEntry.bIsDirectory = false;
        TraitEntry.bReadable = true;
        TraitEntry.bWritable = true;
        TraitEntry.Description = FString::Printf(TEXT("Personality trait: %s"), *Trait);
        NamespaceEntries.Add(TraitEntry);
    }

    // Neurochemical namespace
    FAvatarNamespaceEntry NeurochemicalDir;
    NeurochemicalDir.Name = TEXT("neurochemical");
    NeurochemicalDir.FullPath = NamespaceRoot + TEXT("/neurochemical");
    NeurochemicalDir.bIsDirectory = true;
    NeurochemicalDir.bReadable = true;
    NeurochemicalDir.Description = TEXT("Neurochemical levels and emotional chemistry");
    NamespaceEntries.Add(NeurochemicalDir);

    // Neurochemical levels
    TArray<FString> Neurochemicals = {
        TEXT("dopamine"), TEXT("serotonin"), TEXT("norepinephrine"), TEXT("oxytocin"),
        TEXT("cortisol"), TEXT("endorphins"), TEXT("gaba"), TEXT("glutamate"),
        TEXT("acetylcholine")
    };

    for (const FString& Chemical : Neurochemicals)
    {
        FAvatarNamespaceEntry ChemicalEntry;
        ChemicalEntry.Name = Chemical;
        ChemicalEntry.FullPath = NamespaceRoot + TEXT("/neurochemical/levels/") + Chemical;
        ChemicalEntry.bIsDirectory = false;
        ChemicalEntry.bReadable = true;
        ChemicalEntry.bWritable = true;
        ChemicalEntry.Description = FString::Printf(TEXT("Neurochemical level: %s"), *Chemical);
        NamespaceEntries.Add(ChemicalEntry);
    }

    // Emotional state
    FAvatarNamespaceEntry EmotionalStateEntry;
    EmotionalStateEntry.Name = TEXT("emotional_state");
    EmotionalStateEntry.FullPath = NamespaceRoot + TEXT("/neurochemical/emotional_state");
    EmotionalStateEntry.bIsDirectory = false;
    EmotionalStateEntry.bReadable = true;
    EmotionalStateEntry.Description = TEXT("Current emotional state derived from neurochemistry");
    NamespaceEntries.Add(EmotionalStateEntry);

    // Narrative namespace
    FAvatarNamespaceEntry NarrativeDir;
    NarrativeDir.Name = TEXT("narrative");
    NarrativeDir.FullPath = NamespaceRoot + TEXT("/narrative");
    NarrativeDir.bIsDirectory = true;
    NarrativeDir.bReadable = true;
    NarrativeDir.Description = TEXT("Diary entries, insights, and blog posts");
    NamespaceEntries.Add(NarrativeDir);

    // Visual namespace
    FAvatarNamespaceEntry VisualDir;
    VisualDir.Name = TEXT("visual");
    VisualDir.FullPath = NamespaceRoot + TEXT("/visual");
    VisualDir.bIsDirectory = true;
    VisualDir.bReadable = true;
    VisualDir.Description = TEXT("Visual appearance and animation state");
    NamespaceEntries.Add(VisualDir);

    // Cognitive namespace
    FAvatarNamespaceEntry CognitiveDir;
    CognitiveDir.Name = TEXT("cognitive");
    CognitiveDir.FullPath = NamespaceRoot + TEXT("/cognitive");
    CognitiveDir.bIsDirectory = true;
    CognitiveDir.bReadable = true;
    CognitiveDir.Description = TEXT("Cognitive state and processing metrics");
    NamespaceEntries.Add(CognitiveDir);

    TArray<FString> CognitiveParams = {
        TEXT("attention"), TEXT("memory_load"), TEXT("processing_state"),
        TEXT("consciousness_stream"), TEXT("echo_resonance")
    };

    for (const FString& Param : CognitiveParams)
    {
        FAvatarNamespaceEntry CogEntry;
        CogEntry.Name = Param;
        CogEntry.FullPath = NamespaceRoot + TEXT("/cognitive/") + Param;
        CogEntry.bIsDirectory = false;
        CogEntry.bReadable = true;
        CogEntry.Description = FString::Printf(TEXT("Cognitive parameter: %s"), *Param);
        NamespaceEntries.Add(CogEntry);
    }

    // Performance namespace
    FAvatarNamespaceEntry PerformanceDir;
    PerformanceDir.Name = TEXT("performance");
    PerformanceDir.FullPath = NamespaceRoot + TEXT("/performance");
    PerformanceDir.bIsDirectory = true;
    PerformanceDir.bReadable = true;
    PerformanceDir.Description = TEXT("Performance metrics and budgets");
    NamespaceEntries.Add(PerformanceDir);

    // Batch namespace
    FAvatarNamespaceEntry BatchDir;
    BatchDir.Name = TEXT("batch");
    BatchDir.FullPath = NamespaceRoot + TEXT("/batch");
    BatchDir.bIsDirectory = true;
    BatchDir.bReadable = true;
    BatchDir.bWritable = true;
    BatchDir.Description = TEXT("Batch query and write operations");
    NamespaceEntries.Add(BatchDir);

    FAvatarNamespaceEntry BatchQueryEntry;
    BatchQueryEntry.Name = TEXT("query");
    BatchQueryEntry.FullPath = NamespaceRoot + TEXT("/batch/query");
    BatchQueryEntry.bIsDirectory = false;
    BatchQueryEntry.bReadable = true;
    BatchQueryEntry.bWritable = true;
    BatchQueryEntry.Description = TEXT("Batch query input");
    NamespaceEntries.Add(BatchQueryEntry);

    FAvatarNamespaceEntry BatchResultsEntry;
    BatchResultsEntry.Name = TEXT("results");
    BatchResultsEntry.FullPath = NamespaceRoot + TEXT("/batch/results");
    BatchResultsEntry.bIsDirectory = false;
    BatchResultsEntry.bReadable = true;
    BatchResultsEntry.Description = TEXT("Batch query results");
    NamespaceEntries.Add(BatchResultsEntry);

    UE_LOG(Log9PServer, Log, TEXT("Namespace built with %d entries"), NamespaceEntries.Num());
}

void UAvatar9PServer::ExportNamespace()
{
    // Rebuild namespace with current state
    BuildNamespace();
}

void UAvatar9PServer::RegisterNamespaceEntry(const FAvatarNamespaceEntry& Entry)
{
    // Check if entry already exists
    for (int32 i = 0; i < NamespaceEntries.Num(); i++)
    {
        if (NamespaceEntries[i].FullPath == Entry.FullPath)
        {
            NamespaceEntries[i] = Entry;
            return;
        }
    }

    NamespaceEntries.Add(Entry);
}

F9PResponse UAvatar9PServer::HandleRequest(const F9PRequest& Request)
{
    F9PResponse Response;
    Response.Tag = Request.Tag;

    switch (Request.MessageType)
    {
    case E9PMessageType::Tread:
        return HandleRead(Request.Path, Request.Offset, Request.Count);

    case E9PMessageType::Twrite:
        return HandleWrite(Request.Path, Request.Data);

    case E9PMessageType::Tstat:
        return HandleStat(Request.Path);

    case E9PMessageType::Twalk:
        // Walk to path
        Response.MessageType = E9PMessageType::Rwalk;
        Response.bSuccess = true;
        break;

    case E9PMessageType::Topen:
        // Open file
        Response.MessageType = E9PMessageType::Ropen;
        Response.bSuccess = true;
        break;

    case E9PMessageType::Tclunk:
        // Close fid
        Response.MessageType = E9PMessageType::Rclunk;
        FileHandles.Remove(Request.Fid);
        Response.bSuccess = true;
        break;

    default:
        Response.MessageType = E9PMessageType::Rerror;
        Response.bSuccess = false;
        Response.ErrorMessage = TEXT("Unsupported message type");
        break;
    }

    return Response;
}

F9PResponse UAvatar9PServer::HandleRead(const FString& Path, uint64 Offset, uint32 Count)
{
    F9PResponse Response;
    Response.MessageType = E9PMessageType::Rread;

    FString Content = ReadFromPath(Path);

    if (Content.IsEmpty())
    {
        Response.bSuccess = false;
        Response.ErrorMessage = FString::Printf(TEXT("Path not found: %s"), *Path);
        return Response;
    }

    // Apply offset and count
    if (Offset >= (uint64)Content.Len())
    {
        Response.StringData = TEXT("");
    }
    else
    {
        int32 StartPos = (int32)Offset;
        int32 ReadCount = FMath::Min((int32)Count, Content.Len() - StartPos);
        Response.StringData = Content.Mid(StartPos, ReadCount);
    }

    Response.bSuccess = true;

    if (bVerboseLogging)
    {
        UE_LOG(Log9PServer, Verbose, TEXT("Read %s: %s"), *Path, *Response.StringData);
    }

    return Response;
}

F9PResponse UAvatar9PServer::HandleWrite(const FString& Path, const TArray<uint8>& Data)
{
    F9PResponse Response;
    Response.MessageType = E9PMessageType::Rwrite;

    // Convert data to string
    FString Value = FString(ANSI_TO_TCHAR((const char*)Data.GetData()));

    bool bSuccess = WriteToPath(Path, Value);

    Response.bSuccess = bSuccess;
    if (!bSuccess)
    {
        Response.ErrorMessage = FString::Printf(TEXT("Failed to write to path: %s"), *Path);
    }
    else
    {
        // Broadcast state change event
        On9PStateChanged.Broadcast(Path, Value);

        if (bVerboseLogging)
        {
            UE_LOG(Log9PServer, Verbose, TEXT("Write %s: %s"), *Path, *Value);
        }
    }

    return Response;
}

F9PResponse UAvatar9PServer::HandleStat(const FString& Path)
{
    F9PResponse Response;
    Response.MessageType = E9PMessageType::Rstat;

    // Find the namespace entry
    for (const FAvatarNamespaceEntry& Entry : NamespaceEntries)
    {
        if (Entry.FullPath == Path)
        {
            Response.bSuccess = true;
            // Return stat info as JSON
            Response.StringData = FString::Printf(
                TEXT("{\"name\":\"%s\",\"path\":\"%s\",\"isDir\":%s,\"readable\":%s,\"writable\":%s,\"desc\":\"%s\"}"),
                *Entry.Name,
                *Entry.FullPath,
                Entry.bIsDirectory ? TEXT("true") : TEXT("false"),
                Entry.bReadable ? TEXT("true") : TEXT("false"),
                Entry.bWritable ? TEXT("true") : TEXT("false"),
                *Entry.Description
            );
            return Response;
        }
    }

    Response.bSuccess = false;
    Response.ErrorMessage = FString::Printf(TEXT("Path not found: %s"), *Path);
    return Response;
}

FString UAvatar9PServer::ReadFromPath(const FString& Path)
{
    // Parse the path and route to appropriate handler
    TArray<FString> PathParts = ParsePath(Path);

    if (PathParts.Num() < 2) return TEXT("");

    FString Namespace = PathParts[1]; // First part after /mnt

    if (Namespace == TEXT("avatar"))
    {
        if (PathParts.Num() < 3) return TEXT("");
        FString SubNamespace = PathParts[2];

        if (SubNamespace == TEXT("personality"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("traits"))
            {
                return ReadPersonalityTrait(PathParts[4]);
            }
            else if (PathParts.Num() >= 4 && PathParts[3] == TEXT("state"))
            {
                return ReadPersonalityState();
            }
        }
        else if (SubNamespace == TEXT("neurochemical"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("levels"))
            {
                return ReadNeurochemicalLevel(PathParts[4]);
            }
            else if (PathParts.Num() >= 4 && PathParts[3] == TEXT("emotional_state"))
            {
                return ReadEmotionalState();
            }
        }
        else if (SubNamespace == TEXT("cognitive"))
        {
            if (PathParts.Num() >= 4)
            {
                return ReadCognitiveState(PathParts[3]);
            }
        }
        else if (SubNamespace == TEXT("performance"))
        {
            if (PathParts.Num() >= 4)
            {
                if (PathParts[3] == TEXT("metrics"))
                {
                    return ReadPerformanceMetrics();
                }
                else if (PathParts[3] == TEXT("budgets"))
                {
                    return ReadPerformanceBudgets();
                }
            }
        }
        else if (SubNamespace == TEXT("visual"))
        {
            if (PathParts.Num() >= 4)
            {
                if (PathParts[3] == TEXT("animation"))
                {
                    return ReadAnimationState();
                }
                else if (PathParts[3] == TEXT("appearance") && PathParts.Num() >= 5)
                {
                    return ReadAppearance(PathParts[4]);
                }
            }
        }
        else if (SubNamespace == TEXT("narrative"))
        {
            if (PathParts.Num() >= 5)
            {
                if (PathParts[3] == TEXT("diary"))
                {
                    return ReadDiaryEntry(PathParts[4]);
                }
                else if (PathParts[3] == TEXT("insights"))
                {
                    return ReadInsight(PathParts[4]);
                }
            }
        }
    }

    return TEXT("");
}

bool UAvatar9PServer::WriteToPath(const FString& Path, const FString& Value)
{
    TArray<FString> PathParts = ParsePath(Path);

    if (PathParts.Num() < 2) return false;

    FString Namespace = PathParts[1];

    if (Namespace == TEXT("avatar"))
    {
        if (PathParts.Num() < 3) return false;
        FString SubNamespace = PathParts[2];

        if (SubNamespace == TEXT("personality"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("traits"))
            {
                float FloatValue = FCString::Atof(*Value);
                return WritePersonalityTrait(PathParts[4], FloatValue);
            }
        }
        else if (SubNamespace == TEXT("neurochemical"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("levels"))
            {
                float FloatValue = FCString::Atof(*Value);
                return WriteNeurochemicalLevel(PathParts[4], FloatValue);
            }
        }
        else if (SubNamespace == TEXT("visual"))
        {
            if (PathParts.Num() >= 5 && PathParts[3] == TEXT("appearance"))
            {
                return WriteAppearance(PathParts[4], Value);
            }
        }
        else if (SubNamespace == TEXT("batch"))
        {
            if (PathParts.Num() >= 4 && PathParts[3] == TEXT("query"))
            {
                // Process batch query - store results for later read
                ProcessBatchQuery(Value);
                return true;
            }
        }
    }

    return false;
}

// ===== Personality Namespace Handlers =====

FString UAvatar9PServer::ReadPersonalityTrait(const FString& TraitName)
{
    if (!PersonalitySystem)
    {
        return TEXT("0.0");
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();

    if (TraitName == TEXT("confidence"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Confidence);
    }
    else if (TraitName == TEXT("charm"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Charm);
    }
    else if (TraitName == TEXT("playfulness"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Playfulness);
    }
    else if (TraitName == TEXT("elegance"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Elegance);
    }
    else if (TraitName == TEXT("expressiveness"))
    {
        return FString::Printf(TEXT("%.4f"), SHGTrait.Expressiveness);
    }
    else if (TraitName == TEXT("unpredictability"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.UnpredictabilityFactor);
    }
    else if (TraitName == TEXT("volatility"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.EmotionalVolatility);
    }
    else if (TraitName == TEXT("impulsivity"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.ImpulsivityLevel);
    }
    else if (TraitName == TEXT("chaos_factor"))
    {
        return FString::Printf(TEXT("%.4f"), HCTrait.GlitchEffectProbability);
    }

    return TEXT("0.0");
}

bool UAvatar9PServer::WritePersonalityTrait(const FString& TraitName, float Value)
{
    if (!PersonalitySystem)
    {
        return false;
    }

    Value = FMath::Clamp(Value, 0.0f, 1.0f);

    if (TraitName == TEXT("confidence") || TraitName == TEXT("charm") || TraitName == TEXT("playfulness"))
    {
        FSuperHotGirlTrait CurrentTrait = PersonalitySystem->GetSuperHotGirlTrait();
        float Confidence = TraitName == TEXT("confidence") ? Value : CurrentTrait.Confidence;
        float Charm = TraitName == TEXT("charm") ? Value : CurrentTrait.Charm;
        float Playfulness = TraitName == TEXT("playfulness") ? Value : CurrentTrait.Playfulness;
        PersonalitySystem->SetSuperHotGirlParameters(Confidence, Charm, Playfulness);
        return true;
    }
    else if (TraitName == TEXT("unpredictability") || TraitName == TEXT("volatility") || TraitName == TEXT("impulsivity"))
    {
        FHyperChaoticTrait CurrentTrait = PersonalitySystem->GetHyperChaoticTrait();
        float Unpredictability = TraitName == TEXT("unpredictability") ? Value : CurrentTrait.UnpredictabilityFactor;
        float Volatility = TraitName == TEXT("volatility") ? Value : CurrentTrait.EmotionalVolatility;
        float Impulsivity = TraitName == TEXT("impulsivity") ? Value : CurrentTrait.ImpulsivityLevel;
        PersonalitySystem->SetHyperChaoticParameters(Unpredictability, Volatility, Impulsivity);
        return true;
    }

    return false;
}

FString UAvatar9PServer::ReadPersonalityState()
{
    if (!PersonalitySystem)
    {
        return TEXT("{}");
    }

    FSuperHotGirlTrait SHGTrait = PersonalitySystem->GetSuperHotGirlTrait();
    FHyperChaoticTrait HCTrait = PersonalitySystem->GetHyperChaoticTrait();
    FVector2D EmotionalTendency = PersonalitySystem->GetEmotionalTendency();

    return FString::Printf(
        TEXT("{\"confidence\":%.4f,\"charm\":%.4f,\"playfulness\":%.4f,")
        TEXT("\"elegance\":%.4f,\"expressiveness\":%.4f,")
        TEXT("\"unpredictability\":%.4f,\"volatility\":%.4f,")
        TEXT("\"valence\":%.4f,\"arousal\":%.4f,")
        TEXT("\"predictability\":%.4f,\"social_engagement\":%.4f}"),
        SHGTrait.Confidence, SHGTrait.Charm, SHGTrait.Playfulness,
        SHGTrait.Elegance, SHGTrait.Expressiveness,
        HCTrait.UnpredictabilityFactor, HCTrait.EmotionalVolatility,
        EmotionalTendency.X, EmotionalTendency.Y,
        PersonalitySystem->GetBehavioralPredictability(),
        PersonalitySystem->GetSocialEngagementLevel()
    );
}

// ===== Neurochemical Namespace Handlers =====

FString UAvatar9PServer::ReadNeurochemicalLevel(const FString& ChemicalName)
{
    if (!NeurochemicalSystem)
    {
        return TEXT("0.5");
    }

    FNeurochemicalState State = NeurochemicalSystem->GetCurrentState();

    if (ChemicalName == TEXT("dopamine"))
    {
        return FString::Printf(TEXT("%.4f"), State.DopamineLevel);
    }
    else if (ChemicalName == TEXT("serotonin"))
    {
        return FString::Printf(TEXT("%.4f"), State.SerotoninLevel);
    }
    else if (ChemicalName == TEXT("norepinephrine"))
    {
        return FString::Printf(TEXT("%.4f"), State.NorepinephrineLevel);
    }
    else if (ChemicalName == TEXT("oxytocin"))
    {
        return FString::Printf(TEXT("%.4f"), State.OxytocinLevel);
    }
    else if (ChemicalName == TEXT("cortisol"))
    {
        return FString::Printf(TEXT("%.4f"), State.CortisolLevel);
    }
    else if (ChemicalName == TEXT("endorphins"))
    {
        return FString::Printf(TEXT("%.4f"), State.EndorphinsLevel);
    }
    else if (ChemicalName == TEXT("gaba"))
    {
        return FString::Printf(TEXT("%.4f"), State.GABALevel);
    }
    else if (ChemicalName == TEXT("glutamate"))
    {
        return FString::Printf(TEXT("%.4f"), State.GlutamateLevel);
    }
    else if (ChemicalName == TEXT("acetylcholine"))
    {
        return FString::Printf(TEXT("%.4f"), State.AcetylcholineLevel);
    }

    return TEXT("0.5");
}

bool UAvatar9PServer::WriteNeurochemicalLevel(const FString& ChemicalName, float Value)
{
    if (!NeurochemicalSystem)
    {
        return false;
    }

    Value = FMath::Clamp(Value, 0.0f, 1.0f);

    ENeurochemicalType Type;
    if (ChemicalName == TEXT("dopamine"))
    {
        Type = ENeurochemicalType::Dopamine;
    }
    else if (ChemicalName == TEXT("serotonin"))
    {
        Type = ENeurochemicalType::Serotonin;
    }
    else if (ChemicalName == TEXT("norepinephrine"))
    {
        Type = ENeurochemicalType::Norepinephrine;
    }
    else if (ChemicalName == TEXT("oxytocin"))
    {
        Type = ENeurochemicalType::Oxytocin;
    }
    else if (ChemicalName == TEXT("cortisol"))
    {
        Type = ENeurochemicalType::Cortisol;
    }
    else if (ChemicalName == TEXT("endorphins"))
    {
        Type = ENeurochemicalType::Endorphins;
    }
    else if (ChemicalName == TEXT("gaba"))
    {
        Type = ENeurochemicalType::GABA;
    }
    else if (ChemicalName == TEXT("glutamate"))
    {
        Type = ENeurochemicalType::Glutamate;
    }
    else if (ChemicalName == TEXT("acetylcholine"))
    {
        Type = ENeurochemicalType::Acetylcholine;
    }
    else
    {
        return false;
    }

    NeurochemicalSystem->SetNeurochemicalLevel(Type, Value);
    return true;
}

FString UAvatar9PServer::ReadEmotionalState()
{
    if (!NeurochemicalSystem)
    {
        return TEXT("{}");
    }

    FEmotionalChemistry Emotions = NeurochemicalSystem->GetEmotionalChemistry();

    return FString::Printf(
        TEXT("{\"happiness\":%.4f,\"excitement\":%.4f,\"calmness\":%.4f,")
        TEXT("\"anxiety\":%.4f,\"affection\":%.4f,\"focus\":%.4f,")
        TEXT("\"motivation\":%.4f,\"energy\":%.4f,\"social_desire\":%.4f,")
        TEXT("\"clarity\":%.4f,\"stability\":%.4f}"),
        Emotions.Happiness, Emotions.Excitement, Emotions.Calmness,
        Emotions.Anxiety, Emotions.Affection, Emotions.Focus,
        NeurochemicalSystem->GetMotivationLevel(),
        NeurochemicalSystem->GetEnergyLevel(),
        NeurochemicalSystem->GetSocialDesire(),
        NeurochemicalSystem->GetCognitiveClarity(),
        NeurochemicalSystem->GetEmotionalStability()
    );
}

// ===== Narrative Namespace Handlers =====

FString UAvatar9PServer::ReadDiaryEntry(const FString& EntryId)
{
    // Return diary entry as JSON
    // This would query the NarrativeSystem if available
    return FString::Printf(TEXT("{\"id\":\"%s\",\"content\":\"Diary entry placeholder\",\"timestamp\":\"%s\"}"),
        *EntryId, *FDateTime::Now().ToString());
}

TArray<FString> UAvatar9PServer::ListDiaryEntries()
{
    TArray<FString> Entries;
    // List diary entries from NarrativeSystem
    return Entries;
}

FString UAvatar9PServer::ReadInsight(const FString& InsightId)
{
    return FString::Printf(TEXT("{\"id\":\"%s\",\"insight\":\"Insight placeholder\",\"confidence\":0.8}"),
        *InsightId);
}

// ===== Visual Namespace Handlers =====

FString UAvatar9PServer::ReadAppearance(const FString& Parameter)
{
    // Return visual parameter
    return TEXT("1.0");
}

bool UAvatar9PServer::WriteAppearance(const FString& Parameter, const FString& Value)
{
    // Set visual parameter
    return true;
}

FString UAvatar9PServer::ReadAnimationState()
{
    return TEXT("{\"current_state\":\"idle\",\"blend_alpha\":1.0,\"playing_montage\":false}");
}

// ===== Cognitive Namespace Handlers =====

FString UAvatar9PServer::ReadCognitiveState(const FString& Parameter)
{
    if (Parameter == TEXT("attention"))
    {
        return FString::Printf(TEXT("%.4f"), GetAttentionLevel());
    }
    else if (Parameter == TEXT("memory_load"))
    {
        return FString::Printf(TEXT("%.4f"), GetMemoryLoad());
    }
    else if (Parameter == TEXT("processing_state"))
    {
        return TEXT("active");
    }
    else if (Parameter == TEXT("consciousness_stream"))
    {
        return TEXT("{\"stream_id\":1,\"phase\":0.0,\"resonance\":0.8}");
    }
    else if (Parameter == TEXT("echo_resonance"))
    {
        return TEXT("0.75");
    }

    return TEXT("");
}

float UAvatar9PServer::GetAttentionLevel() const
{
    return 0.8f; // Placeholder - would query cognitive system
}

float UAvatar9PServer::GetMemoryLoad() const
{
    return 0.5f; // Placeholder - would query memory system
}

// ===== Performance Namespace Handlers =====

FString UAvatar9PServer::ReadPerformanceMetrics()
{
    return TEXT("{\"fps\":60.0,\"frame_time_ms\":16.67,\"9p_latency_ms\":2.0,\"message_throughput\":500}");
}

FString UAvatar9PServer::ReadPerformanceBudgets()
{
    return TEXT("{\"cpu_budget_ms\":10.0,\"gpu_budget_ms\":6.0,\"memory_mb\":512,\"network_kbps\":1000}");
}

// ===== Batch Operations =====

FString UAvatar9PServer::ProcessBatchQuery(const FString& QueryList)
{
    // Parse newline-separated list of paths
    TArray<FString> Paths;
    QueryList.ParseIntoArray(Paths, TEXT("\n"));

    TArray<FString> Results;
    for (const FString& Path : Paths)
    {
        FString TrimmedPath = Path.TrimStartAndEnd();
        if (!TrimmedPath.IsEmpty())
        {
            FString Value = ReadFromPath(TrimmedPath);
            Results.Add(FString::Printf(TEXT("%s=%s"), *TrimmedPath, *Value));
        }
    }

    return FString::Join(Results, TEXT("\n"));
}

FString UAvatar9PServer::ProcessBatchWrite(const FString& WriteList)
{
    // Parse lines of "path=value"
    TArray<FString> Lines;
    WriteList.ParseIntoArray(Lines, TEXT("\n"));

    TArray<FString> Results;
    for (const FString& Line : Lines)
    {
        FString TrimmedLine = Line.TrimStartAndEnd();
        int32 EqualsPos;
        if (TrimmedLine.FindChar('=', EqualsPos))
        {
            FString Path = TrimmedLine.Left(EqualsPos);
            FString Value = TrimmedLine.RightChop(EqualsPos + 1);
            bool bSuccess = WriteToPath(Path, Value);
            Results.Add(FString::Printf(TEXT("%s:%s"), *Path, bSuccess ? TEXT("ok") : TEXT("error")));
        }
    }

    return FString::Join(Results, TEXT("\n"));
}

// ===== Message Coalescing =====

void UAvatar9PServer::EnableMessageCoalescing(bool bEnable)
{
    bEnableCoalescing = bEnable;
}

void UAvatar9PServer::SetCoalescingInterval(float IntervalSeconds)
{
    CoalescingInterval = FMath::Max(0.001f, IntervalSeconds);
}

void UAvatar9PServer::ProcessPendingMessages()
{
    if (PendingRequests.Num() == 0)
    {
        return;
    }

    // Process all pending requests
    for (const F9PRequest& Request : PendingRequests)
    {
        HandleRequest(Request);
    }

    PendingRequests.Empty();
}

// ===== Path Parsing Helpers =====

TArray<FString> UAvatar9PServer::ParsePath(const FString& Path)
{
    TArray<FString> Parts;
    Path.ParseIntoArray(Parts, TEXT("/"));
    return Parts;
}

FString UAvatar9PServer::GetNamespaceRoot(const FString& Path)
{
    TArray<FString> Parts = ParsePath(Path);
    if (Parts.Num() >= 2)
    {
        return Parts[1];
    }
    return TEXT("");
}

FString UAvatar9PServer::GetRelativePath(const FString& Path)
{
    if (Path.StartsWith(NamespaceRoot))
    {
        return Path.RightChop(NamespaceRoot.Len());
    }
    return Path;
}

FString UAvatar9PServer::SerializeToJson(const FString& Path)
{
    return ReadFromPath(Path);
}

bool UAvatar9PServer::DeserializeFromJson(const FString& Path, const FString& JsonValue)
{
    return WriteToPath(Path, JsonValue);
}
