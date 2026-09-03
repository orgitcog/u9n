#include "AvatarAtomSpaceClient.h"
#include "../9P/Avatar9PServer.h"
#include "../Narrative/DiaryInsightBlogLoop.h"

DEFINE_LOG_CATEGORY_STATIC(LogAtomSpace, Log, All);

UAvatarAtomSpaceClient::UAvatarAtomSpaceClient()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz for sync

    bConnected = false;
    ServerHost = TEXT("localhost");
    ServerPort = 17001;
    bUse9PConnection = true;
    NextHandle = 1;
    bAutoSyncEnabled = true;
    SyncInterval = 5.0f;
    SyncTimer = 0.0f;
    AvatarName = TEXT("deep_tree_echo");
    MaxCacheSize = 10000;
    bVerboseLogging = false;
}

void UAvatarAtomSpaceClient::BeginPlay()
{
    Super::BeginPlay();

    // Find 9P server component if available
    if (AActor* Owner = GetOwner())
    {
        P9Server = Owner->FindComponentByClass<UAvatar9PServer>();
    }

    // Initialize the avatar concept in AtomSpace
    InitializeAvatarConcept();

    UE_LOG(LogAtomSpace, Log, TEXT("AtomSpace client initialized for avatar: %s"), *AvatarName);
}

void UAvatarAtomSpaceClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Sync any pending changes before shutdown
    if (PendingChanges.Num() > 0)
    {
        SyncWithServer();
    }

    Disconnect();
    Super::EndPlay(EndPlayReason);
}

void UAvatarAtomSpaceClient::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bAutoSyncEnabled && bConnected)
    {
        SyncTimer += DeltaTime;
        if (SyncTimer >= SyncInterval)
        {
            ProcessPendingChanges();
            SyncTimer = 0.0f;
        }
    }
}

void UAvatarAtomSpaceClient::InitializeAvatarConcept()
{
    // Create the main avatar concept node
    FTruthValue TV(1.0f, 1.0f, 1);
    AvatarConceptHandle = CreateNode(EAtomType::ConceptNode, AvatarName, TV);

    // Create personality anchor
    FAtomHandle PersonalityAnchor = CreateNode(EAtomType::AnchorNode, AvatarName + TEXT("_personality"), TV);

    // Create memory anchor
    FAtomHandle MemoryAnchor = CreateNode(EAtomType::AnchorNode, AvatarName + TEXT("_memory"), TV);

    // Link personality anchor to avatar
    TArray<FAtomHandle> PersonalityOutgoing;
    PersonalityOutgoing.Add(AvatarConceptHandle);
    PersonalityOutgoing.Add(PersonalityAnchor);
    CreateLink(EAtomType::MemberLink, PersonalityOutgoing, TV);

    // Link memory anchor to avatar
    TArray<FAtomHandle> MemoryOutgoing;
    MemoryOutgoing.Add(AvatarConceptHandle);
    MemoryOutgoing.Add(MemoryAnchor);
    CreateLink(EAtomType::MemberLink, MemoryOutgoing, TV);

    UE_LOG(LogAtomSpace, Log, TEXT("Avatar concept initialized with handle: %llu"), AvatarConceptHandle.Handle);
}

// ===== Connection Management =====

void UAvatarAtomSpaceClient::Connect(const FString& Host, int32 Port)
{
    ServerHost = Host;
    ServerPort = Port;

    // Simulate connection (in real implementation, would establish network connection)
    bConnected = true;

    OnConnectionStateChanged.Broadcast(true);
    UE_LOG(LogAtomSpace, Log, TEXT("Connected to AtomSpace at %s:%d"), *ServerHost, ServerPort);
}

void UAvatarAtomSpaceClient::Disconnect()
{
    bConnected = false;
    OnConnectionStateChanged.Broadcast(false);
    UE_LOG(LogAtomSpace, Log, TEXT("Disconnected from AtomSpace"));
}

void UAvatarAtomSpaceClient::SetConnectionVia9P(bool bUse9P)
{
    bUse9PConnection = bUse9P;
}

// ===== Atom Operations =====

FAtomHandle UAvatarAtomSpaceClient::CreateNode(EAtomType Type, const FString& Name, const FTruthValue& TV)
{
    FAtom NewAtom;
    NewAtom.Handle = FAtomHandle(NextHandle++);
    NewAtom.Type = Type;
    NewAtom.Name = Name;
    NewAtom.TruthValue = TV;
    NewAtom.CreatedAt = FDateTime::Now();
    NewAtom.ModifiedAt = FDateTime::Now();

    AtomCache.Add(NewAtom.Handle.Handle, NewAtom);
    PendingChanges.Add(NewAtom);

    OnAtomCreated.Broadcast(NewAtom.Handle, NewAtom);

    if (bVerboseLogging)
    {
        UE_LOG(LogAtomSpace, Verbose, TEXT("Created node: %s (type: %d, handle: %llu)"),
            *Name, (int32)Type, NewAtom.Handle.Handle);
    }

    return NewAtom.Handle;
}

FAtomHandle UAvatarAtomSpaceClient::CreateLink(EAtomType Type, const TArray<FAtomHandle>& Outgoing, const FTruthValue& TV)
{
    FAtom NewAtom;
    NewAtom.Handle = FAtomHandle(NextHandle++);
    NewAtom.Type = Type;
    NewAtom.Outgoing = Outgoing;
    NewAtom.TruthValue = TV;
    NewAtom.CreatedAt = FDateTime::Now();
    NewAtom.ModifiedAt = FDateTime::Now();

    AtomCache.Add(NewAtom.Handle.Handle, NewAtom);
    PendingChanges.Add(NewAtom);

    OnAtomCreated.Broadcast(NewAtom.Handle, NewAtom);

    if (bVerboseLogging)
    {
        UE_LOG(LogAtomSpace, Verbose, TEXT("Created link: type %d, outgoing count: %d, handle: %llu"),
            (int32)Type, Outgoing.Num(), NewAtom.Handle.Handle);
    }

    return NewAtom.Handle;
}

FAtom UAvatarAtomSpaceClient::GetAtom(const FAtomHandle& Handle)
{
    if (FAtom* Found = AtomCache.Find(Handle.Handle))
    {
        return *Found;
    }

    return FAtom();
}

bool UAvatarAtomSpaceClient::DeleteAtom(const FAtomHandle& Handle)
{
    if (AtomCache.Remove(Handle.Handle) > 0)
    {
        OnAtomDeleted.Broadcast(Handle);
        return true;
    }
    return false;
}

void UAvatarAtomSpaceClient::SetTruthValue(const FAtomHandle& Handle, const FTruthValue& TV)
{
    if (FAtom* Found = AtomCache.Find(Handle.Handle))
    {
        Found->TruthValue = TV;
        Found->ModifiedAt = FDateTime::Now();
        PendingChanges.Add(*Found);
        OnAtomModified.Broadcast(Handle, *Found);
    }
}

void UAvatarAtomSpaceClient::SetAttentionValue(const FAtomHandle& Handle, const FAttentionValue& AV)
{
    if (FAtom* Found = AtomCache.Find(Handle.Handle))
    {
        Found->AttentionValue = AV;
        Found->ModifiedAt = FDateTime::Now();
        PendingChanges.Add(*Found);
        OnAtomModified.Broadcast(Handle, *Found);
    }
}

// ===== Diary Storage =====

void UAvatarAtomSpaceClient::StoreDiaryEntry(const FDiaryEntry& Entry)
{
    FTruthValue TV(1.0f, 0.9f, 1);

    // Create diary entry node
    FAtomHandle EntryNode = CreateNode(EAtomType::DiaryEntryNode, Entry.EntryId, TV);

    // Create emotional state node
    FString EmotionalStateId = FString::Printf(TEXT("emotional_state_%s"), *Entry.EntryId);
    FAtomHandle EmotionalNode = CreateNode(EAtomType::EmotionalStateNode, EmotionalStateId, TV);

    // Create timestamp node
    FAtomHandle TimestampNode = CreateNode(EAtomType::NumberNode,
        FString::Printf(TEXT("%lld"), Entry.Timestamp.ToUnixTimestamp()), TV);

    // Create EvaluationLink: (diary_entry avatar entry_id emotional_state)
    FAtomHandle PredicateNode = CreateNode(EAtomType::PredicateNode, TEXT("diary_entry"), TV);

    TArray<FAtomHandle> ListOutgoing;
    ListOutgoing.Add(AvatarConceptHandle);
    ListOutgoing.Add(EntryNode);
    ListOutgoing.Add(EmotionalNode);
    FAtomHandle ListLink = CreateLink(EAtomType::ListLink, ListOutgoing, TV);

    TArray<FAtomHandle> EvalOutgoing;
    EvalOutgoing.Add(PredicateNode);
    EvalOutgoing.Add(ListLink);
    CreateLink(EAtomType::EvaluationLink, EvalOutgoing, TV);

    // Store content as state
    // In a full implementation, this would store the actual content

    UE_LOG(LogAtomSpace, Log, TEXT("Stored diary entry: %s"), *Entry.EntryId);
}

FDiaryEntry UAvatarAtomSpaceClient::GetDiaryEntry(const FString& EntryId)
{
    FDiaryEntry Entry;
    Entry.EntryId = EntryId;

    // Query AtomSpace for entry
    TArray<FAtomHandle> Handles = FindByName(EntryId);
    if (Handles.Num() > 0)
    {
        FAtom Atom = GetAtom(Handles[0]);
        Entry.Timestamp = Atom.CreatedAt;
        // In full implementation, would reconstruct content from links
    }

    return Entry;
}

TArray<FDiaryEntry> UAvatarAtomSpaceClient::GetRecentDiaryEntries(int32 Count)
{
    TArray<FDiaryEntry> Entries;

    // Find all diary entry nodes, sort by timestamp
    TArray<FAtomHandle> DiaryHandles = FindByType(EAtomType::DiaryEntryNode);

    // Sort by creation time (most recent first)
    TArray<FAtom> DiaryAtoms;
    for (const FAtomHandle& Handle : DiaryHandles)
    {
        DiaryAtoms.Add(GetAtom(Handle));
    }

    DiaryAtoms.Sort([](const FAtom& A, const FAtom& B) {
        return A.CreatedAt > B.CreatedAt;
    });

    // Return top Count entries
    int32 ResultCount = FMath::Min(Count, DiaryAtoms.Num());
    for (int32 i = 0; i < ResultCount; i++)
    {
        FDiaryEntry Entry;
        Entry.EntryId = DiaryAtoms[i].Name;
        Entry.Timestamp = DiaryAtoms[i].CreatedAt;
        Entries.Add(Entry);
    }

    return Entries;
}

TArray<FDiaryEntry> UAvatarAtomSpaceClient::SearchDiaryEntries(const FString& Query, int32 MaxResults)
{
    TArray<FDiaryEntry> Results;

    // Simple pattern matching search
    TArray<FAtomHandle> Handles = FindByName(TEXT("*") + Query + TEXT("*"));

    int32 Count = FMath::Min(MaxResults, Handles.Num());
    for (int32 i = 0; i < Count; i++)
    {
        FAtom Atom = GetAtom(Handles[i]);
        if (Atom.Type == EAtomType::DiaryEntryNode)
        {
            FDiaryEntry Entry;
            Entry.EntryId = Atom.Name;
            Entry.Timestamp = Atom.CreatedAt;
            Results.Add(Entry);
        }
    }

    return Results;
}

// ===== Insight Storage =====

void UAvatarAtomSpaceClient::StoreInsight(const FInsight& Insight)
{
    FTruthValue TV(Insight.Confidence, 0.8f, 1);

    // Create insight node
    FAtomHandle InsightNode = CreateNode(EAtomType::InsightNode, Insight.InsightId, TV);

    // Create category node
    FAtomHandle CategoryNode = CreateNode(EAtomType::ConceptNode, Insight.Category, TV);

    // Create InheritanceLink: insight -> category
    TArray<FAtomHandle> InheritOutgoing;
    InheritOutgoing.Add(InsightNode);
    InheritOutgoing.Add(CategoryNode);
    CreateLink(EAtomType::InheritanceLink, InheritOutgoing, TV);

    // Link source diary entries
    for (const FString& SourceId : Insight.SourceDiaryEntryIds)
    {
        TArray<FAtomHandle> SourceHandles = FindByName(SourceId);
        if (SourceHandles.Num() > 0)
        {
            TArray<FAtomHandle> ContextOutgoing;
            ContextOutgoing.Add(SourceHandles[0]);
            ContextOutgoing.Add(InsightNode);
            CreateLink(EAtomType::ContextLink, ContextOutgoing, TV);
        }
    }

    UE_LOG(LogAtomSpace, Log, TEXT("Stored insight: %s (category: %s, confidence: %.2f)"),
        *Insight.InsightId, *Insight.Category, Insight.Confidence);
}

FInsight UAvatarAtomSpaceClient::GetInsight(const FString& InsightId)
{
    FInsight Insight;
    Insight.InsightId = InsightId;

    TArray<FAtomHandle> Handles = FindByName(InsightId);
    if (Handles.Num() > 0)
    {
        FAtom Atom = GetAtom(Handles[0]);
        Insight.Confidence = Atom.TruthValue.Strength;
        Insight.DiscoveredAt = Atom.CreatedAt;
    }

    return Insight;
}

TArray<FInsight> UAvatarAtomSpaceClient::GetRelatedInsights(const FString& Context, int32 MaxResults)
{
    TArray<FInsight> Results;

    // Find insights related to context
    TArray<FAtomHandle> InsightHandles = FindByType(EAtomType::InsightNode);

    int32 Count = FMath::Min(MaxResults, InsightHandles.Num());
    for (int32 i = 0; i < Count; i++)
    {
        FAtom Atom = GetAtom(InsightHandles[i]);
        FInsight Insight;
        Insight.InsightId = Atom.Name;
        Insight.Confidence = Atom.TruthValue.Strength;
        Insight.DiscoveredAt = Atom.CreatedAt;
        Results.Add(Insight);
    }

    return Results;
}

void UAvatarAtomSpaceClient::ValidateInsight(const FString& InsightId, bool bValid)
{
    TArray<FAtomHandle> Handles = FindByName(InsightId);
    if (Handles.Num() > 0)
    {
        FTruthValue TV = GetAtom(Handles[0]).TruthValue;
        TV.Strength = bValid ? FMath::Min(1.0f, TV.Strength + 0.1f) : FMath::Max(0.0f, TV.Strength - 0.1f);
        TV.Confidence = FMath::Min(1.0f, TV.Confidence + 0.05f);
        TV.Count++;
        SetTruthValue(Handles[0], TV);
    }
}

// ===== Personality Trait Storage =====

void UAvatarAtomSpaceClient::StorePersonalityTrait(const FString& TraitName, float Value, float Confidence)
{
    FTruthValue TV(Value, Confidence, 1);

    // Create trait node if it doesn't exist
    FAtomHandle TraitNode = CreateNode(EAtomType::PersonalityTraitNode, TraitName, TV);

    // Create value node
    FAtomHandle ValueNode = CreateNode(EAtomType::NumberNode, FString::Printf(TEXT("%.4f"), Value), TV);

    // Create has_trait predicate
    FAtomHandle PredicateNode = CreateNode(EAtomType::PredicateNode, TEXT("has_trait"), TV);

    // Create EvaluationLink: (has_trait (avatar trait value))
    TArray<FAtomHandle> ListOutgoing;
    ListOutgoing.Add(AvatarConceptHandle);
    ListOutgoing.Add(TraitNode);
    ListOutgoing.Add(ValueNode);
    FAtomHandle ListLink = CreateLink(EAtomType::ListLink, ListOutgoing, TV);

    TArray<FAtomHandle> EvalOutgoing;
    EvalOutgoing.Add(PredicateNode);
    EvalOutgoing.Add(ListLink);
    CreateLink(EAtomType::EvaluationLink, EvalOutgoing, TV);
}

float UAvatarAtomSpaceClient::GetPersonalityTrait(const FString& TraitName)
{
    TArray<FAtomHandle> Handles = FindByName(TraitName);
    for (const FAtomHandle& Handle : Handles)
    {
        FAtom Atom = GetAtom(Handle);
        if (Atom.Type == EAtomType::PersonalityTraitNode)
        {
            return Atom.TruthValue.Strength;
        }
    }
    return 0.5f; // Default value
}

TMap<FString, float> UAvatarAtomSpaceClient::GetAllPersonalityTraits()
{
    TMap<FString, float> Traits;

    TArray<FAtomHandle> TraitHandles = FindByType(EAtomType::PersonalityTraitNode);
    for (const FAtomHandle& Handle : TraitHandles)
    {
        FAtom Atom = GetAtom(Handle);
        Traits.Add(Atom.Name, Atom.TruthValue.Strength);
    }

    return Traits;
}

// ===== Memory Queries =====

TArray<FMemory> UAvatarAtomSpaceClient::QueryRelatedMemories(const FString& Context, int32 MaxResults)
{
    TArray<FMemory> Results;

    // Find memories related to context
    TArray<FAtomHandle> MemoryHandles = FindByType(EAtomType::MemoryNode);

    // Calculate relevance based on echo resonance
    TArray<TPair<FAtomHandle, float>> ScoredMemories;
    for (const FAtomHandle& Handle : MemoryHandles)
    {
        FAtom Atom = GetAtom(Handle);
        float Resonance = GetEchoResonance(Context, Atom.Name);
        ScoredMemories.Add(TPair<FAtomHandle, float>(Handle, Resonance));
    }

    // Sort by resonance
    ScoredMemories.Sort([](const TPair<FAtomHandle, float>& A, const TPair<FAtomHandle, float>& B) {
        return A.Value > B.Value;
    });

    // Return top results
    int32 Count = FMath::Min(MaxResults, ScoredMemories.Num());
    for (int32 i = 0; i < Count; i++)
    {
        FAtom Atom = GetAtom(ScoredMemories[i].Key);
        FMemory Memory;
        Memory.MemoryId = Atom.Name;
        Memory.Timestamp = Atom.CreatedAt;
        Memory.Relevance = ScoredMemories[i].Value;
        Memory.Strength = Atom.TruthValue.Strength;
        Results.Add(Memory);
    }

    return Results;
}

TArray<FAtom> UAvatarAtomSpaceClient::QueryAtoms(const FMemoryQuery& Query)
{
    TArray<FAtom> Results;

    for (auto& Pair : AtomCache)
    {
        FAtom& Atom = Pair.Value;

        // Check type filter
        if (Query.AtomTypes.Num() > 0 && !Query.AtomTypes.Contains(Atom.Type))
        {
            continue;
        }

        // Check name pattern
        if (!Query.NamePattern.IsEmpty() && !Atom.Name.Contains(Query.NamePattern))
        {
            continue;
        }

        // Check truth value filters
        if (Atom.TruthValue.Strength < Query.MinStrength)
        {
            continue;
        }
        if (Atom.TruthValue.Confidence < Query.MinConfidence)
        {
            continue;
        }

        // Check attention value filter
        if (Atom.AttentionValue.STI < Query.MinSTI)
        {
            continue;
        }

        // Check time range
        if (Atom.CreatedAt < Query.FromTime || Atom.CreatedAt > Query.ToTime)
        {
            continue;
        }

        Results.Add(Atom);

        if (Results.Num() >= Query.MaxResults)
        {
            break;
        }
    }

    return Results;
}

TArray<FAtomHandle> UAvatarAtomSpaceClient::FindByName(const FString& NamePattern)
{
    TArray<FAtomHandle> Results;

    for (auto& Pair : AtomCache)
    {
        if (Pair.Value.Name.Contains(NamePattern) || Pair.Value.Name.MatchesWildcard(NamePattern))
        {
            Results.Add(Pair.Value.Handle);
        }
    }

    return Results;
}

TArray<FAtomHandle> UAvatarAtomSpaceClient::FindByType(EAtomType Type)
{
    TArray<FAtomHandle> Results;

    for (auto& Pair : AtomCache)
    {
        if (Pair.Value.Type == Type)
        {
            Results.Add(Pair.Value.Handle);
        }
    }

    return Results;
}

// ===== Pattern Mining =====

TArray<FInsight> UAvatarAtomSpaceClient::MinePatterns(int32 MinSupport)
{
    TArray<FInsight> DiscoveredInsights;

    // Simple frequency-based pattern mining
    TMap<FString, int32> ConceptFrequency;

    for (auto& Pair : AtomCache)
    {
        if (Pair.Value.Type == EAtomType::ConceptNode)
        {
            int32& Count = ConceptFrequency.FindOrAdd(Pair.Value.Name);
            Count++;
        }
    }

    // Find frequent patterns
    for (auto& FreqPair : ConceptFrequency)
    {
        if (FreqPair.Value >= MinSupport)
        {
            FInsight Insight;
            Insight.InsightId = GenerateUniqueId();
            Insight.Content = FString::Printf(TEXT("Frequent pattern: %s (count: %d)"), *FreqPair.Key, FreqPair.Value);
            Insight.Category = TEXT("frequency_pattern");
            Insight.Confidence = FMath::Min(1.0f, FreqPair.Value / 10.0f);
            Insight.DiscoveredAt = FDateTime::Now();
            DiscoveredInsights.Add(Insight);
        }
    }

    return DiscoveredInsights;
}

void UAvatarAtomSpaceClient::DiscoverCorrelations(const FString& ConceptA, const FString& ConceptB)
{
    // Find links that connect both concepts
    TArray<FAtomHandle> HandlesA = FindByName(ConceptA);
    TArray<FAtomHandle> HandlesB = FindByName(ConceptB);

    if (HandlesA.Num() > 0 && HandlesB.Num() > 0)
    {
        // Check for existing correlation link
        // In full implementation, would analyze co-occurrence patterns
        float Correlation = 0.5f; // Placeholder

        // Store correlation as an insight
        FInsight CorrelationInsight;
        CorrelationInsight.InsightId = GenerateUniqueId();
        CorrelationInsight.Content = FString::Printf(TEXT("Correlation discovered: %s <-> %s (%.2f)"),
            *ConceptA, *ConceptB, Correlation);
        CorrelationInsight.Category = TEXT("correlation");
        CorrelationInsight.Confidence = Correlation;
        StoreInsight(CorrelationInsight);
    }
}

// ===== Echo Resonance Integration =====

void UAvatarAtomSpaceClient::StoreEchoResonance(const FString& SourceId, const FString& TargetId, float Resonance)
{
    FTruthValue TV(Resonance, 0.9f, 1);

    TArray<FAtomHandle> SourceHandles = FindByName(SourceId);
    TArray<FAtomHandle> TargetHandles = FindByName(TargetId);

    if (SourceHandles.Num() > 0 && TargetHandles.Num() > 0)
    {
        TArray<FAtomHandle> Outgoing;
        Outgoing.Add(SourceHandles[0]);
        Outgoing.Add(TargetHandles[0]);
        CreateLink(EAtomType::EchoResonanceLink, Outgoing, TV);
    }
}

float UAvatarAtomSpaceClient::GetEchoResonance(const FString& SourceId, const FString& TargetId)
{
    TArray<FAtomHandle> ResonanceLinks = FindByType(EAtomType::EchoResonanceLink);

    for (const FAtomHandle& Handle : ResonanceLinks)
    {
        FAtom Atom = GetAtom(Handle);
        if (Atom.Outgoing.Num() >= 2)
        {
            FAtom Source = GetAtom(Atom.Outgoing[0]);
            FAtom Target = GetAtom(Atom.Outgoing[1]);
            if (Source.Name == SourceId && Target.Name == TargetId)
            {
                return Atom.TruthValue.Strength;
            }
        }
    }

    return 0.0f;
}

TArray<FString> UAvatarAtomSpaceClient::GetResonantMemories(const FString& CurrentContext, float MinResonance)
{
    TArray<FString> ResonantIds;

    TArray<FAtomHandle> ResonanceLinks = FindByType(EAtomType::EchoResonanceLink);

    for (const FAtomHandle& Handle : ResonanceLinks)
    {
        FAtom Atom = GetAtom(Handle);
        if (Atom.TruthValue.Strength >= MinResonance && Atom.Outgoing.Num() >= 2)
        {
            FAtom Source = GetAtom(Atom.Outgoing[0]);
            if (Source.Name.Contains(CurrentContext))
            {
                FAtom Target = GetAtom(Atom.Outgoing[1]);
                ResonantIds.Add(Target.Name);
            }
        }
    }

    return ResonantIds;
}

// ===== Synchronization =====

void UAvatarAtomSpaceClient::SyncWithServer()
{
    ProcessPendingChanges();
}

void UAvatarAtomSpaceClient::EnableAutoSync(bool bEnable, float IntervalSeconds)
{
    bAutoSyncEnabled = bEnable;
    SyncInterval = FMath::Max(0.5f, IntervalSeconds);
}

void UAvatarAtomSpaceClient::ProcessPendingChanges()
{
    if (PendingChanges.Num() == 0)
    {
        return;
    }

    if (bVerboseLogging)
    {
        UE_LOG(LogAtomSpace, Verbose, TEXT("Syncing %d pending changes to AtomSpace"), PendingChanges.Num());
    }

    // In full implementation, would serialize and send to AtomSpace server
    // For now, just clear pending changes (local cache only)
    PendingChanges.Empty();
}

// ===== Helper Methods =====

FString UAvatarAtomSpaceClient::GenerateUniqueId()
{
    return FGuid::NewGuid().ToString();
}

FString UAvatarAtomSpaceClient::AtomToScheme(const FAtom& Atom)
{
    // Convert atom to Scheme representation (OpenCog format)
    FString TypeName;
    switch (Atom.Type)
    {
    case EAtomType::ConceptNode:
        TypeName = TEXT("ConceptNode");
        break;
    case EAtomType::PredicateNode:
        TypeName = TEXT("PredicateNode");
        break;
    case EAtomType::NumberNode:
        TypeName = TEXT("NumberNode");
        break;
    case EAtomType::InheritanceLink:
        TypeName = TEXT("InheritanceLink");
        break;
    case EAtomType::EvaluationLink:
        TypeName = TEXT("EvaluationLink");
        break;
    case EAtomType::ListLink:
        TypeName = TEXT("ListLink");
        break;
    default:
        TypeName = TEXT("Atom");
        break;
    }

    if (Atom.Outgoing.Num() == 0)
    {
        // Node
        return FString::Printf(TEXT("(%s \"%s\")"), *TypeName, *Atom.Name);
    }
    else
    {
        // Link
        FString OutgoingStr;
        for (const FAtomHandle& Handle : Atom.Outgoing)
        {
            FAtom OutAtom = GetAtom(Handle);
            OutgoingStr += TEXT("\n    ") + AtomToScheme(OutAtom);
        }
        return FString::Printf(TEXT("(%s%s)"), *TypeName, *OutgoingStr);
    }
}

FAtom UAvatarAtomSpaceClient::SchemeToAtom(const FString& Scheme)
{
    // Parse Scheme representation (simplified)
    FAtom Atom;
    // In full implementation, would parse the Scheme string
    return Atom;
}

FString UAvatarAtomSpaceClient::Read9PPath(const FString& Path)
{
    if (P9Server)
    {
        F9PResponse Response = P9Server->HandleRead(Path, 0, 8192);
        return Response.StringData;
    }
    return TEXT("");
}

bool UAvatarAtomSpaceClient::Write9PPath(const FString& Path, const FString& Data)
{
    if (P9Server)
    {
        TArray<uint8> ByteData;
        ByteData.SetNumUninitialized(Data.Len());
        FMemory::Memcpy(ByteData.GetData(), TCHAR_TO_ANSI(*Data), Data.Len());

        F9PResponse Response = P9Server->HandleWrite(Path, ByteData);
        return Response.bSuccess;
    }
    return false;
}
