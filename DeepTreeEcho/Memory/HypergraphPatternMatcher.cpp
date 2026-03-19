// HypergraphPatternMatcher.cpp
// Efficient pattern matching over hypergraph structures with variable binding
// Feature F1.4.2 - Hypergraph Reasoning Foundation
// Deep Tree Echo Cognitive Architecture

#include "HypergraphPatternMatcher.h"
#include "HypergraphMemorySystem.h"

UHypergraphPatternMatcher::UHypergraphPatternMatcher()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz for index maintenance

    // Initialize default configuration
    DefaultConfig.MatchMode = EPatternMatchMode::AllMatches;
    DefaultConfig.MaxMatches = 1000;
    DefaultConfig.TimeoutMs = 1000.0f;
    DefaultConfig.bEnableParallel = true;
    DefaultConfig.bIncludeOptional = true;
    DefaultConfig.bSortByRelevance = true;
    DefaultConfig.MinConfidence = 0.0f;
    DefaultConfig.bUseIndexes = true;
}

void UHypergraphPatternMatcher::BeginPlay()
{
    Super::BeginPlay();

    // Auto-discover hypergraph if not set
    if (!HypergraphRef)
    {
        AutoDiscoverHypergraph();
    }

    // Build initial indexes
    if (bEnableAutoIndexing && HypergraphRef)
    {
        RebuildIndexes();
    }
}

void UHypergraphPatternMatcher::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update indexes periodically
    if (bEnableAutoIndexing && HypergraphRef)
    {
        IndexUpdateTimer += DeltaTime;
        if (IndexUpdateTimer >= IndexUpdateInterval)
        {
            UpdateIndexes();
            IndexUpdateTimer = 0.0f;
        }
    }
}

// ========================================
// PATTERN REGISTRATION
// ========================================

bool UHypergraphPatternMatcher::RegisterPattern(const FGraphPattern& Pattern)
{
    if (Pattern.PatternID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("HypergraphPatternMatcher: Cannot register pattern without ID"));
        return false;
    }

    RegisteredPatterns.Add(Pattern.PatternID, Pattern);
    return true;
}

bool UHypergraphPatternMatcher::UnregisterPattern(const FString& PatternID)
{
    if (RegisteredPatterns.Contains(PatternID))
    {
        RegisteredPatterns.Remove(PatternID);
        return true;
    }
    return false;
}

FGraphPattern UHypergraphPatternMatcher::GetPattern(const FString& PatternID) const
{
    if (const FGraphPattern* Found = RegisteredPatterns.Find(PatternID))
    {
        return *Found;
    }
    return FGraphPattern();
}

TArray<FString> UHypergraphPatternMatcher::GetRegisteredPatternIDs() const
{
    TArray<FString> IDs;
    RegisteredPatterns.GetKeys(IDs);
    return IDs;
}

void UHypergraphPatternMatcher::ClearAllPatterns()
{
    RegisteredPatterns.Empty();
}

// ========================================
// PATTERN MATCHING
// ========================================

FPatternMatchResult UHypergraphPatternMatcher::MatchPattern(const FGraphPattern& Pattern,
                                                            const FPatternMatchConfig& Config)
{
    FPatternMatchResult Result;
    Result.PatternID = Pattern.PatternID;

    if (!HypergraphRef)
    {
        Result.ErrorMessage = TEXT("No hypergraph reference set");
        return Result;
    }

    double StartTime = FPlatformTime::Seconds();
    TotalMatchAttempts++;

    // Perform matching
    TArray<FBindingEnvironment> Matches = MatchPatternInternal(Pattern, Config);

    // Populate result
    Result.MatchingBindings = Matches;
    Result.TotalMatches = Matches.Num();
    Result.bHasMatches = Matches.Num() > 0;
    Result.ExecutionTimeMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);

    TotalMatchesFound += Matches.Num();
    TotalMatchTimeMs += Result.ExecutionTimeMs;

    // Sort by relevance if requested
    if (Config.bSortByRelevance && Result.MatchingBindings.Num() > 1)
    {
        Result.MatchingBindings.Sort([](const FBindingEnvironment& A, const FBindingEnvironment& B)
        {
            return A.RelevanceScore > B.RelevanceScore;
        });
    }

    // Apply max matches limit
    if (Config.MaxMatches > 0 && Result.MatchingBindings.Num() > Config.MaxMatches)
    {
        Result.MatchingBindings.SetNum(Config.MaxMatches);
    }

    // Fire event
    if (Result.bHasMatches)
    {
        OnPatternMatched.Broadcast(Result.PatternID, Result.TotalMatches);
    }

    return Result;
}

FPatternMatchResult UHypergraphPatternMatcher::MatchPatternByID(const FString& PatternID,
                                                                 const FPatternMatchConfig& Config)
{
    FPatternMatchResult Result;
    Result.PatternID = PatternID;

    const FGraphPattern* Pattern = RegisteredPatterns.Find(PatternID);
    if (!Pattern)
    {
        Result.ErrorMessage = FString::Printf(TEXT("Pattern '%s' not registered"), *PatternID);
        return Result;
    }

    return MatchPattern(*Pattern, Config);
}

FPatternMatchResult UHypergraphPatternMatcher::MatchPatternDefault(const FGraphPattern& Pattern)
{
    return MatchPattern(Pattern, DefaultConfig);
}

bool UHypergraphPatternMatcher::PatternExists(const FGraphPattern& Pattern)
{
    FPatternMatchConfig Config;
    Config.MatchMode = EPatternMatchMode::ExistsCheck;
    Config.MaxMatches = 1;

    FPatternMatchResult Result = MatchPattern(Pattern, Config);
    return Result.bHasMatches;
}

int32 UHypergraphPatternMatcher::CountPatternMatches(const FGraphPattern& Pattern)
{
    FPatternMatchConfig Config;
    Config.MatchMode = EPatternMatchMode::CountOnly;
    Config.MaxMatches = 0; // No limit for counting

    FPatternMatchResult Result = MatchPattern(Pattern, Config);
    return Result.TotalMatches;
}

// ========================================
// SIMPLE PATTERN BUILDERS
// ========================================

FEdgePattern UHypergraphPatternMatcher::CreateEdgePattern(
    const FString& SourceVar,
    const FString& TargetVar,
    ESemanticRelation Relation)
{
    FEdgePattern Pattern;
    Pattern.Source = FPatternElement::Variable(SourceVar);
    Pattern.Target = FPatternElement::Variable(TargetVar);
    Pattern.AllowedRelations.Add(Relation);
    Pattern.bDirected = true;
    return Pattern;
}

FPatternElement UHypergraphPatternMatcher::CreateTypedNodePattern(
    const FString& VariableName,
    EMemoryNodeType NodeType)
{
    return FPatternElement::TypedVariable(VariableName, NodeType);
}

FGraphPattern UHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
    const FString& SourceVar,
    const FString& TargetVar,
    ESemanticRelation Relation,
    const FString& PatternID)
{
    FGraphPattern Pattern;
    Pattern.PatternID = PatternID.IsEmpty()
        ? FString::Printf(TEXT("edge_%s_%s"), *SourceVar, *TargetVar)
        : PatternID;
    Pattern.EdgePatterns.Add(CreateEdgePattern(SourceVar, TargetVar, Relation));
    return Pattern;
}

FGraphPattern UHypergraphPatternMatcher::CreateTrianglePattern(
    const FString& VarA,
    const FString& VarB,
    const FString& VarC,
    ESemanticRelation RelationAB,
    ESemanticRelation RelationBC,
    ESemanticRelation RelationAC)
{
    FGraphPattern Pattern;
    Pattern.PatternID = FString::Printf(TEXT("triangle_%s_%s_%s"), *VarA, *VarB, *VarC);
    Pattern.Description = TEXT("Triangle pattern with three connected nodes");

    Pattern.EdgePatterns.Add(CreateEdgePattern(VarA, VarB, RelationAB));
    Pattern.EdgePatterns.Add(CreateEdgePattern(VarB, VarC, RelationBC));
    Pattern.EdgePatterns.Add(CreateEdgePattern(VarA, VarC, RelationAC));

    return Pattern;
}

FGraphPattern UHypergraphPatternMatcher::CreatePathPattern(
    const TArray<FString>& VariableNames,
    const TArray<ESemanticRelation>& Relations)
{
    FGraphPattern Pattern;
    Pattern.PatternID = TEXT("path_pattern");
    Pattern.Description = TEXT("Path pattern through multiple nodes");

    // Create edges for consecutive pairs
    int32 NumEdges = FMath::Min(VariableNames.Num() - 1, Relations.Num());
    for (int32 i = 0; i < NumEdges; i++)
    {
        Pattern.EdgePatterns.Add(CreateEdgePattern(
            VariableNames[i],
            VariableNames[i + 1],
            Relations[i]));
    }

    return Pattern;
}

// ========================================
// BINDING OPERATIONS
// ========================================

FBindingEnvironment UHypergraphPatternMatcher::ExtendBinding(
    const FBindingEnvironment& Env,
    const FString& VariableName,
    int64 NodeID)
{
    FBindingEnvironment Extended = Env;

    // Check if already bound
    for (const FVariableBinding& Binding : Extended.Bindings)
    {
        if (Binding.VariableName == VariableName)
        {
            // Already bound - check if consistent
            if (Binding.BoundNodeID == NodeID)
            {
                return Extended; // Same binding, OK
            }
            else
            {
                // Conflict - return invalid environment
                Extended.bIsComplete = false;
                Extended.MatchConfidence = 0.0f;
                return Extended;
            }
        }
    }

    // Add new binding
    FVariableBinding NewBinding;
    NewBinding.VariableName = VariableName;
    NewBinding.BoundNodeID = NodeID;
    NewBinding.bIsNodeBinding = true;
    NewBinding.Confidence = 1.0f;
    Extended.Bindings.Add(NewBinding);

    return Extended;
}

FBindingEnvironment UHypergraphPatternMatcher::MergeBindings(
    const FBindingEnvironment& Env1,
    const FBindingEnvironment& Env2)
{
    if (!BindingsAreCompatible(Env1, Env2))
    {
        FBindingEnvironment Invalid;
        Invalid.bIsComplete = false;
        Invalid.MatchConfidence = 0.0f;
        return Invalid;
    }

    FBindingEnvironment Merged = Env1;

    // Add bindings from Env2 that aren't in Env1
    for (const FVariableBinding& Binding : Env2.Bindings)
    {
        bool bFound = false;
        for (const FVariableBinding& Existing : Merged.Bindings)
        {
            if (Existing.VariableName == Binding.VariableName)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            Merged.Bindings.Add(Binding);
        }
    }

    // Combine confidence scores
    Merged.MatchConfidence = Env1.MatchConfidence * Env2.MatchConfidence;

    return Merged;
}

bool UHypergraphPatternMatcher::BindingsAreCompatible(
    const FBindingEnvironment& Env1,
    const FBindingEnvironment& Env2)
{
    // Check for conflicting bindings
    for (const FVariableBinding& Binding1 : Env1.Bindings)
    {
        for (const FVariableBinding& Binding2 : Env2.Bindings)
        {
            if (Binding1.VariableName == Binding2.VariableName)
            {
                // Same variable - check if same value
                if (Binding1.bIsNodeBinding && Binding2.bIsNodeBinding)
                {
                    if (Binding1.BoundNodeID != Binding2.BoundNodeID)
                    {
                        return false; // Conflict
                    }
                }
                else if (Binding1.bIsEdgeBinding && Binding2.bIsEdgeBinding)
                {
                    if (Binding1.BoundEdgeID != Binding2.BoundEdgeID)
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

int64 UHypergraphPatternMatcher::ResolvePatternElement(
    const FPatternElement& Element,
    const FBindingEnvironment& Env) const
{
    switch (Element.ElementType)
    {
    case EPatternElementType::ConcreteNode:
        return Element.ConcreteNodeID;

    case EPatternElementType::Variable:
    case EPatternElementType::TypeConstraint:
    case EPatternElementType::PropertyConstraint:
        return Env.GetBoundNodeID(Element.VariableName);

    case EPatternElementType::Wildcard:
    default:
        return 0;
    }
}

// ========================================
// INDEX MANAGEMENT
// ========================================

void UHypergraphPatternMatcher::RebuildIndexes()
{
    if (!HypergraphRef)
    {
        return;
    }

    ClearIndexes();

    // Get all nodes and index them
    FHypergraphStats Stats = HypergraphRef->GetStats();

    // This would need to iterate over all nodes in the hypergraph
    // For now, we track that we've updated
    PatternIndex.LastUpdateTime = FPlatformTime::Seconds();
}

void UHypergraphPatternMatcher::UpdateIndexes()
{
    if (!HypergraphRef)
    {
        return;
    }

    // Incremental index update
    PatternIndex.LastUpdateTime = FPlatformTime::Seconds();
}

void UHypergraphPatternMatcher::ClearIndexes()
{
    PatternIndex.NodesByType.Empty();
    PatternIndex.EdgesByRelation.Empty();
    PatternIndex.NodesByLabel.Empty();
    PatternIndex.NodesByProperty.Empty();
    PatternIndex.LastUpdateTime = 0.0;
}

FString UHypergraphPatternMatcher::GetIndexStats() const
{
    return FString::Printf(
        TEXT("Index Stats:\n")
        TEXT("  Node Type Entries: %d\n")
        TEXT("  Edge Relation Entries: %d\n")
        TEXT("  Label Entries: %d\n")
        TEXT("  Property Entries: %d\n")
        TEXT("  Last Update: %.2f"),
        PatternIndex.NodesByType.Num(),
        PatternIndex.EdgesByRelation.Num(),
        PatternIndex.NodesByLabel.Num(),
        PatternIndex.NodesByProperty.Num(),
        PatternIndex.LastUpdateTime);
}

// ========================================
// HYPERGRAPH REFERENCE
// ========================================

void UHypergraphPatternMatcher::SetHypergraph(UHypergraphMemorySystem* Hypergraph)
{
    HypergraphRef = Hypergraph;

    if (bEnableAutoIndexing && HypergraphRef)
    {
        RebuildIndexes();
    }
}

UHypergraphMemorySystem* UHypergraphPatternMatcher::GetHypergraph() const
{
    return HypergraphRef;
}

bool UHypergraphPatternMatcher::AutoDiscoverHypergraph()
{
    if (AActor* Owner = GetOwner())
    {
        HypergraphRef = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        if (HypergraphRef)
        {
            if (bEnableAutoIndexing)
            {
                RebuildIndexes();
            }
            return true;
        }
    }
    return false;
}

// ========================================
// DIAGNOSTICS
// ========================================

FString UHypergraphPatternMatcher::GetMatchingStats() const
{
    float AvgTime = TotalMatchAttempts > 0
        ? static_cast<float>(TotalMatchTimeMs / TotalMatchAttempts)
        : 0.0f;
    float AvgMatches = TotalMatchAttempts > 0
        ? static_cast<float>(TotalMatchesFound) / TotalMatchAttempts
        : 0.0f;

    return FString::Printf(
        TEXT("Pattern Matching Statistics:\n")
        TEXT("  Total Attempts: %lld\n")
        TEXT("  Total Matches Found: %lld\n")
        TEXT("  Average Time: %.2f ms\n")
        TEXT("  Average Matches: %.2f\n")
        TEXT("  Registered Patterns: %d\n")
        TEXT("  Cache Hits: %d\n")
        TEXT("  Cache Misses: %d"),
        TotalMatchAttempts,
        TotalMatchesFound,
        AvgTime,
        AvgMatches,
        RegisteredPatterns.Num(),
        CacheHits,
        CacheMisses);
}

void UHypergraphPatternMatcher::ResetStats()
{
    TotalMatchAttempts = 0;
    TotalMatchesFound = 0;
    TotalMatchTimeMs = 0.0;
    CacheHits = 0;
    CacheMisses = 0;
}

// ========================================
// INTERNAL MATCHING ENGINE
// ========================================

TArray<FBindingEnvironment> UHypergraphPatternMatcher::MatchPatternInternal(
    const FGraphPattern& Pattern,
    const FPatternMatchConfig& Config)
{
    TArray<FBindingEnvironment> Results;

    if (!HypergraphRef)
    {
        return Results;
    }

    // Start with empty binding environment
    FBindingEnvironment InitialEnv;
    InitialEnv.bIsComplete = true;
    InitialEnv.MatchConfidence = 1.0f;
    InitialEnv.RelevanceScore = 1.0f;

    TArray<FBindingEnvironment> CurrentBindings;
    CurrentBindings.Add(InitialEnv);

    // Match standalone node patterns first (most selective)
    for (const FPatternElement& NodePattern : Pattern.NodePatterns)
    {
        TArray<FBindingEnvironment> NewBindings;

        for (const FBindingEnvironment& Env : CurrentBindings)
        {
            TArray<FBindingEnvironment> NodeMatches = MatchNodePattern(NodePattern, Env, Config);
            NewBindings.Append(NodeMatches);

            // Early exit if we have enough matches
            if (Config.MatchMode == EPatternMatchMode::FirstMatch && NewBindings.Num() > 0)
            {
                Results.Append(NewBindings);
                return Results;
            }

            if (Config.MatchMode == EPatternMatchMode::ExistsCheck && NewBindings.Num() > 0)
            {
                Results.Append(NewBindings);
                return Results;
            }
        }

        CurrentBindings = NewBindings;

        if (CurrentBindings.Num() == 0)
        {
            return Results; // No matches possible
        }
    }

    // Match edge patterns
    for (const FEdgePattern& EdgePattern : Pattern.EdgePatterns)
    {
        TArray<FBindingEnvironment> NewBindings;

        for (const FBindingEnvironment& Env : CurrentBindings)
        {
            TArray<FBindingEnvironment> EdgeMatches;

            if (EdgePattern.bTransitive)
            {
                EdgeMatches = ComputeTransitiveClosure(EdgePattern, Env, Config);
            }
            else
            {
                EdgeMatches = MatchEdgePattern(EdgePattern, Env, Config);
            }

            NewBindings.Append(EdgeMatches);

            // Early exit checks
            if (Config.MatchMode == EPatternMatchMode::FirstMatch && NewBindings.Num() > 0)
            {
                Results.Append(NewBindings);
                return Results;
            }

            if (Config.MatchMode == EPatternMatchMode::ExistsCheck && NewBindings.Num() > 0)
            {
                Results.Append(NewBindings);
                return Results;
            }
        }

        CurrentBindings = NewBindings;

        if (CurrentBindings.Num() == 0)
        {
            return Results;
        }
    }

    // Match hyperedge patterns
    for (const FHyperedgePattern& HyperedgePattern : Pattern.HyperedgePatterns)
    {
        TArray<FBindingEnvironment> NewBindings;

        for (const FBindingEnvironment& Env : CurrentBindings)
        {
            TArray<FBindingEnvironment> HyperedgeMatches = MatchHyperedgePattern(HyperedgePattern, Env, Config);
            NewBindings.Append(HyperedgeMatches);
        }

        CurrentBindings = NewBindings;

        if (CurrentBindings.Num() == 0)
        {
            return Results;
        }
    }

    // Check negated patterns (filter out matches where negated pattern exists)
    for (const FEdgePattern& NegatedPattern : Pattern.NegatedEdgePatterns)
    {
        TArray<FBindingEnvironment> FilteredBindings;

        for (const FBindingEnvironment& Env : CurrentBindings)
        {
            TArray<FBindingEnvironment> NegatedMatches = MatchEdgePattern(NegatedPattern, Env, Config);

            // Keep this binding only if the negated pattern has NO matches
            if (NegatedMatches.Num() == 0)
            {
                FilteredBindings.Add(Env);
            }
        }

        CurrentBindings = FilteredBindings;
    }

    // Check cross-variable constraints
    if (Pattern.CrossConstraints.Num() > 0)
    {
        TArray<FBindingEnvironment> FilteredBindings;

        for (const FBindingEnvironment& Env : CurrentBindings)
        {
            bool bSatisfiesAll = true;

            for (const FString& Constraint : Pattern.CrossConstraints)
            {
                if (!EvaluateCrossConstraint(Constraint, Env))
                {
                    bSatisfiesAll = false;
                    break;
                }
            }

            if (bSatisfiesAll)
            {
                FilteredBindings.Add(Env);
            }
        }

        CurrentBindings = FilteredBindings;
    }

    // Handle optional patterns (extend bindings with optional matches)
    if (Config.bIncludeOptional)
    {
        for (const FEdgePattern& OptionalPattern : Pattern.OptionalEdgePatterns)
        {
            TArray<FBindingEnvironment> ExtendedBindings;

            for (const FBindingEnvironment& Env : CurrentBindings)
            {
                TArray<FBindingEnvironment> OptionalMatches = MatchEdgePattern(OptionalPattern, Env, Config);

                if (OptionalMatches.Num() > 0)
                {
                    // Use the matched bindings
                    ExtendedBindings.Append(OptionalMatches);
                }
                else
                {
                    // Keep original binding (optional not matched)
                    ExtendedBindings.Add(Env);
                }
            }

            CurrentBindings = ExtendedBindings;
        }
    }

    // Filter by minimum confidence
    if (Config.MinConfidence > 0.0f)
    {
        CurrentBindings.RemoveAll([&Config](const FBindingEnvironment& Env)
        {
            return Env.MatchConfidence < Config.MinConfidence;
        });
    }

    // Mark all as complete
    for (FBindingEnvironment& Env : CurrentBindings)
    {
        Env.bIsComplete = true;
    }

    Results = CurrentBindings;
    return Results;
}

TArray<FBindingEnvironment> UHypergraphPatternMatcher::MatchEdgePattern(
    const FEdgePattern& EdgePattern,
    const FBindingEnvironment& CurrentBindings,
    const FPatternMatchConfig& Config)
{
    TArray<FBindingEnvironment> Results;

    if (!HypergraphRef)
    {
        return Results;
    }

    // Resolve source and target if already bound
    int64 SourceID = ResolvePatternElement(EdgePattern.Source, CurrentBindings);
    int64 TargetID = ResolvePatternElement(EdgePattern.Target, CurrentBindings);

    // Get candidate edges
    TArray<int64> CandidateEdges = GetCandidateEdges(EdgePattern, CurrentBindings);

    // Try each candidate edge
    for (int64 EdgeID : CandidateEdges)
    {
        FMemoryEdge Edge = HypergraphRef->GetEdge(EdgeID);

        // Check relation type constraint
        if (EdgePattern.AllowedRelations.Num() > 0)
        {
            if (!EdgePattern.AllowedRelations.Contains(Edge.RelationType))
            {
                continue;
            }
        }

        // Check weight constraint
        if (Edge.Weight < EdgePattern.MinWeight)
        {
            continue;
        }

        // Check source binding
        if (SourceID != 0 && Edge.SourceNodeID != SourceID)
        {
            // If directed, must match exactly
            if (EdgePattern.bDirected)
            {
                continue;
            }
            // If undirected, check reverse
            else if (Edge.TargetNodeID != SourceID)
            {
                continue;
            }
        }

        // Check target binding
        if (TargetID != 0 && Edge.TargetNodeID != TargetID)
        {
            if (EdgePattern.bDirected)
            {
                continue;
            }
            else if (Edge.SourceNodeID != TargetID)
            {
                continue;
            }
        }

        // Check source node constraints
        FMemoryNode SourceNode = HypergraphRef->GetNode(Edge.SourceNodeID);
        if (!NodeSatisfiesElement(SourceNode, EdgePattern.Source))
        {
            continue;
        }

        // Check target node constraints
        FMemoryNode TargetNode = HypergraphRef->GetNode(Edge.TargetNodeID);
        if (!NodeSatisfiesElement(TargetNode, EdgePattern.Target))
        {
            continue;
        }

        // Create extended bindings
        FBindingEnvironment NewEnv = CurrentBindings;

        // Bind source variable
        if (EdgePattern.Source.ElementType == EPatternElementType::Variable ||
            EdgePattern.Source.ElementType == EPatternElementType::TypeConstraint)
        {
            NewEnv = ExtendBinding(NewEnv, EdgePattern.Source.VariableName, Edge.SourceNodeID);
            if (NewEnv.MatchConfidence == 0.0f)
            {
                continue; // Binding conflict
            }
        }

        // Bind target variable
        if (EdgePattern.Target.ElementType == EPatternElementType::Variable ||
            EdgePattern.Target.ElementType == EPatternElementType::TypeConstraint)
        {
            NewEnv = ExtendBinding(NewEnv, EdgePattern.Target.VariableName, Edge.TargetNodeID);
            if (NewEnv.MatchConfidence == 0.0f)
            {
                continue; // Binding conflict
            }
        }

        // Bind edge variable if specified
        if (!EdgePattern.EdgeVariableName.IsEmpty())
        {
            FVariableBinding EdgeBinding;
            EdgeBinding.VariableName = EdgePattern.EdgeVariableName;
            EdgeBinding.BoundEdgeID = EdgeID;
            EdgeBinding.bIsNodeBinding = false;
            EdgeBinding.bIsEdgeBinding = true;
            EdgeBinding.Confidence = Edge.Confidence;
            NewEnv.Bindings.Add(EdgeBinding);
        }

        // Update confidence
        NewEnv.MatchConfidence *= Edge.Confidence;
        NewEnv.RelevanceScore *= Edge.Weight;

        Results.Add(NewEnv);

        // Early exit for FirstMatch mode
        if (Config.MatchMode == EPatternMatchMode::FirstMatch)
        {
            return Results;
        }
    }

    return Results;
}

TArray<FBindingEnvironment> UHypergraphPatternMatcher::MatchHyperedgePattern(
    const FHyperedgePattern& HyperedgePattern,
    const FBindingEnvironment& CurrentBindings,
    const FPatternMatchConfig& Config)
{
    TArray<FBindingEnvironment> Results;

    if (!HypergraphRef)
    {
        return Results;
    }

    // Get all hyperedges (filtered by type if specified)
    // Note: This would need a GetAllHyperedges method in HypergraphMemorySystem
    // For now, we'll iterate over member nodes

    // Find a bound member to start from
    int64 StartNodeID = 0;
    int32 StartIndex = -1;

    for (int32 i = 0; i < HyperedgePattern.Members.Num(); i++)
    {
        int64 BoundID = ResolvePatternElement(HyperedgePattern.Members[i], CurrentBindings);
        if (BoundID != 0)
        {
            StartNodeID = BoundID;
            StartIndex = i;
            break;
        }
    }

    // If we have a starting point, get its hyperedges
    if (StartNodeID != 0)
    {
        TArray<FMemoryHyperedge> CandidateHyperedges = HypergraphRef->GetNodeHyperedges(StartNodeID);

        for (const FMemoryHyperedge& Hyperedge : CandidateHyperedges)
        {
            // Check type constraint
            if (!HyperedgePattern.HyperedgeTypeConstraint.IsEmpty())
            {
                if (Hyperedge.HyperedgeType != HyperedgePattern.HyperedgeTypeConstraint)
                {
                    continue;
                }
            }

            // Check weight constraint
            if (Hyperedge.Weight < HyperedgePattern.MinWeight)
            {
                continue;
            }

            // Check member count
            int32 RequiredMembers = HyperedgePattern.bAllowPartialMatch
                ? HyperedgePattern.MinMembersForPartial
                : HyperedgePattern.Members.Num();

            if (Hyperedge.MemberNodes.Num() < RequiredMembers)
            {
                continue;
            }

            // Try to match all members
            FBindingEnvironment NewEnv = CurrentBindings;
            bool bAllMatched = true;

            for (int32 i = 0; i < HyperedgePattern.Members.Num() && i < Hyperedge.MemberNodes.Num(); i++)
            {
                const FPatternElement& MemberPattern = HyperedgePattern.Members[i];
                int64 MemberNodeID = Hyperedge.MemberNodes[i];

                // Check role constraint
                if (i < HyperedgePattern.ExpectedRoles.Num() && !HyperedgePattern.ExpectedRoles[i].IsEmpty())
                {
                    if (i >= Hyperedge.MemberRoles.Num() ||
                        Hyperedge.MemberRoles[i] != HyperedgePattern.ExpectedRoles[i])
                    {
                        bAllMatched = false;
                        break;
                    }
                }

                // Check node constraints
                FMemoryNode MemberNode = HypergraphRef->GetNode(MemberNodeID);
                if (!NodeSatisfiesElement(MemberNode, MemberPattern))
                {
                    bAllMatched = false;
                    break;
                }

                // Add binding
                if (MemberPattern.ElementType == EPatternElementType::Variable ||
                    MemberPattern.ElementType == EPatternElementType::TypeConstraint)
                {
                    NewEnv = ExtendBinding(NewEnv, MemberPattern.VariableName, MemberNodeID);
                    if (NewEnv.MatchConfidence == 0.0f)
                    {
                        bAllMatched = false;
                        break;
                    }
                }
            }

            if (!bAllMatched && !HyperedgePattern.bAllowPartialMatch)
            {
                continue;
            }

            // Bind hyperedge variable if specified
            if (!HyperedgePattern.HyperedgeVariableName.IsEmpty())
            {
                FVariableBinding HyperedgeBinding;
                HyperedgeBinding.VariableName = HyperedgePattern.HyperedgeVariableName;
                HyperedgeBinding.BoundHyperedgeID = Hyperedge.HyperedgeID;
                HyperedgeBinding.bIsNodeBinding = false;
                HyperedgeBinding.bIsHyperedgeBinding = true;
                HyperedgeBinding.Confidence = Hyperedge.Confidence;
                NewEnv.Bindings.Add(HyperedgeBinding);
            }

            NewEnv.MatchConfidence *= Hyperedge.Confidence;
            NewEnv.RelevanceScore *= Hyperedge.Weight;

            Results.Add(NewEnv);
        }
    }

    return Results;
}

TArray<FBindingEnvironment> UHypergraphPatternMatcher::MatchNodePattern(
    const FPatternElement& NodePattern,
    const FBindingEnvironment& CurrentBindings,
    const FPatternMatchConfig& Config)
{
    TArray<FBindingEnvironment> Results;

    if (!HypergraphRef)
    {
        return Results;
    }

    // Get candidate nodes
    TArray<int64> CandidateNodes = GetCandidateNodes(NodePattern, CurrentBindings);

    for (int64 NodeID : CandidateNodes)
    {
        FMemoryNode Node = HypergraphRef->GetNode(NodeID);

        if (!NodeSatisfiesElement(Node, NodePattern))
        {
            continue;
        }

        // Create binding
        FBindingEnvironment NewEnv = CurrentBindings;

        if (NodePattern.ElementType == EPatternElementType::Variable ||
            NodePattern.ElementType == EPatternElementType::TypeConstraint)
        {
            NewEnv = ExtendBinding(NewEnv, NodePattern.VariableName, NodeID);
            if (NewEnv.MatchConfidence == 0.0f)
            {
                continue;
            }
        }

        // Update scores based on node
        NewEnv.RelevanceScore *= Node.Strength;

        Results.Add(NewEnv);

        if (Config.MatchMode == EPatternMatchMode::FirstMatch)
        {
            return Results;
        }
    }

    return Results;
}

bool UHypergraphPatternMatcher::NodeSatisfiesElement(
    const FMemoryNode& Node,
    const FPatternElement& Element) const
{
    // Check type constraint
    if (Element.ElementType == EPatternElementType::TypeConstraint)
    {
        if (Node.NodeType != Element.NodeTypeConstraint)
        {
            return false;
        }
    }

    // Check activation threshold
    if (Element.MinActivation > 0.0f && Node.Activation < Element.MinActivation)
    {
        return false;
    }

    // Check strength threshold
    if (Element.MinStrength > 0.0f && Node.Strength < Element.MinStrength)
    {
        return false;
    }

    // Check property constraints
    for (const FPropertyConstraint& Constraint : Element.PropertyConstraints)
    {
        if (!CheckPropertyConstraint(Node, Constraint))
        {
            return false;
        }
    }

    return true;
}

bool UHypergraphPatternMatcher::EdgeSatisfiesPattern(
    const FMemoryEdge& Edge,
    const FEdgePattern& Pattern) const
{
    // Check relation type
    if (Pattern.AllowedRelations.Num() > 0)
    {
        if (!Pattern.AllowedRelations.Contains(Edge.RelationType))
        {
            return false;
        }
    }

    // Check custom relation label
    if (Edge.RelationType == ESemanticRelation::CUSTOM)
    {
        if (!Pattern.CustomRelationLabel.IsEmpty() &&
            Edge.CustomRelationLabel != Pattern.CustomRelationLabel)
        {
            return false;
        }
    }

    // Check weight
    if (Edge.Weight < Pattern.MinWeight)
    {
        return false;
    }

    return true;
}

bool UHypergraphPatternMatcher::CheckPropertyConstraint(
    const FMemoryNode& Node,
    const FPropertyConstraint& Constraint) const
{
    const FString* PropValue = Node.Properties.Find(Constraint.PropertyKey);

    // Handle IsNull/IsNotNull
    if (Constraint.Operator == EConstraintOperator::IsNull)
    {
        return PropValue == nullptr || PropValue->IsEmpty();
    }

    if (Constraint.Operator == EConstraintOperator::IsNotNull)
    {
        return PropValue != nullptr && !PropValue->IsEmpty();
    }

    // Other operators require the property to exist
    if (PropValue == nullptr)
    {
        return false;
    }

    // Numeric comparison
    if (Constraint.bNumericComparison)
    {
        float NodeValue = FCString::Atof(**PropValue);
        float ConstraintValue = FCString::Atof(*Constraint.Value);

        switch (Constraint.Operator)
        {
        case EConstraintOperator::Equals:
            return FMath::IsNearlyEqual(NodeValue, ConstraintValue);
        case EConstraintOperator::NotEquals:
            return !FMath::IsNearlyEqual(NodeValue, ConstraintValue);
        case EConstraintOperator::GreaterThan:
            return NodeValue > ConstraintValue;
        case EConstraintOperator::LessThan:
            return NodeValue < ConstraintValue;
        case EConstraintOperator::GreaterOrEqual:
            return NodeValue >= ConstraintValue;
        case EConstraintOperator::LessOrEqual:
            return NodeValue <= ConstraintValue;
        case EConstraintOperator::InRange:
            {
                float SecondValue = FCString::Atof(*Constraint.SecondaryValue);
                return NodeValue >= ConstraintValue && NodeValue <= SecondValue;
            }
        default:
            return false;
        }
    }

    // String comparison
    switch (Constraint.Operator)
    {
    case EConstraintOperator::Equals:
        return *PropValue == Constraint.Value;
    case EConstraintOperator::NotEquals:
        return *PropValue != Constraint.Value;
    case EConstraintOperator::Contains:
        return PropValue->Contains(Constraint.Value);
    case EConstraintOperator::StartsWith:
        return PropValue->StartsWith(Constraint.Value);
    case EConstraintOperator::EndsWith:
        return PropValue->EndsWith(Constraint.Value);
    case EConstraintOperator::Regex:
        {
            FRegexPattern Pattern(Constraint.Value);
            FRegexMatcher Matcher(Pattern, *PropValue);
            return Matcher.FindNext();
        }
    default:
        return false;
    }
}

bool UHypergraphPatternMatcher::EvaluateCrossConstraint(
    const FString& Constraint,
    const FBindingEnvironment& Env) const
{
    // Parse simple cross-constraints of form: "?x.property == ?y.property"
    // or "?x == ?y" for node equality

    // Simple equality check: "?x == ?y"
    if (Constraint.Contains(TEXT("==")))
    {
        FString Left, Right;
        if (Constraint.Split(TEXT("=="), &Left, &Right))
        {
            Left = Left.TrimStartAndEnd();
            Right = Right.TrimStartAndEnd();

            // Check for variable references
            if (Left.StartsWith(TEXT("?")) && Right.StartsWith(TEXT("?")))
            {
                FString LeftVar = Left.Mid(1); // Remove ?
                FString RightVar = Right.Mid(1);

                // Property access?
                if (LeftVar.Contains(TEXT(".")))
                {
                    // Property comparison - not implemented yet
                    return true;
                }
                else
                {
                    // Simple variable equality
                    int64 LeftID = Env.GetBoundNodeID(LeftVar);
                    int64 RightID = Env.GetBoundNodeID(RightVar);

                    return LeftID == RightID;
                }
            }
        }
    }

    // Not equals: "?x != ?y"
    if (Constraint.Contains(TEXT("!=")))
    {
        FString Left, Right;
        if (Constraint.Split(TEXT("!="), &Left, &Right))
        {
            Left = Left.TrimStartAndEnd();
            Right = Right.TrimStartAndEnd();

            if (Left.StartsWith(TEXT("?")) && Right.StartsWith(TEXT("?")))
            {
                FString LeftVar = Left.Mid(1);
                FString RightVar = Right.Mid(1);

                int64 LeftID = Env.GetBoundNodeID(LeftVar);
                int64 RightID = Env.GetBoundNodeID(RightVar);

                return LeftID != RightID;
            }
        }
    }

    // Default: pass if we can't parse
    return true;
}

TArray<int64> UHypergraphPatternMatcher::GetCandidateNodes(
    const FPatternElement& Element,
    const FBindingEnvironment& CurrentBindings) const
{
    TArray<int64> Candidates;

    if (!HypergraphRef)
    {
        return Candidates;
    }

    // If concrete node, return just that
    if (Element.ElementType == EPatternElementType::ConcreteNode)
    {
        if (HypergraphRef->NodeExists(Element.ConcreteNodeID))
        {
            Candidates.Add(Element.ConcreteNodeID);
        }
        return Candidates;
    }

    // If variable already bound, return bound value
    if (!Element.VariableName.IsEmpty())
    {
        int64 BoundID = CurrentBindings.GetBoundNodeID(Element.VariableName);
        if (BoundID != 0)
        {
            Candidates.Add(BoundID);
            return Candidates;
        }
    }

    // Use index if available and type constrained
    if (Element.ElementType == EPatternElementType::TypeConstraint)
    {
        const TArray<int64>* IndexedNodes = PatternIndex.NodesByType.Find(Element.NodeTypeConstraint);
        if (IndexedNodes)
        {
            CacheHits++;
            return *IndexedNodes;
        }
        CacheMisses++;
    }

    // Fall back to getting all nodes from hypergraph stats
    // This would need a proper iteration method in HypergraphMemorySystem
    FHypergraphStats Stats = HypergraphRef->GetStats();

    // For now, return empty - actual implementation would iterate nodes
    return Candidates;
}

TArray<int64> UHypergraphPatternMatcher::GetCandidateEdges(
    const FEdgePattern& Pattern,
    const FBindingEnvironment& CurrentBindings) const
{
    TArray<int64> Candidates;

    if (!HypergraphRef)
    {
        return Candidates;
    }

    // Try to use bound nodes to get edges
    int64 SourceID = ResolvePatternElement(Pattern.Source, CurrentBindings);
    int64 TargetID = ResolvePatternElement(Pattern.Target, CurrentBindings);

    if (SourceID != 0)
    {
        // Get outgoing edges from source
        TArray<FMemoryEdge> OutgoingEdges = HypergraphRef->GetOutgoingEdges(SourceID);
        for (const FMemoryEdge& Edge : OutgoingEdges)
        {
            if (TargetID == 0 || Edge.TargetNodeID == TargetID)
            {
                if (EdgeSatisfiesPattern(Edge, Pattern))
                {
                    Candidates.Add(Edge.EdgeID);
                }
            }
        }
    }
    else if (TargetID != 0)
    {
        // Get incoming edges to target
        TArray<FMemoryEdge> IncomingEdges = HypergraphRef->GetIncomingEdges(TargetID);
        for (const FMemoryEdge& Edge : IncomingEdges)
        {
            if (EdgeSatisfiesPattern(Edge, Pattern))
            {
                Candidates.Add(Edge.EdgeID);
            }
        }
    }
    else
    {
        // Neither bound - use index if available
        if (Pattern.AllowedRelations.Num() == 1)
        {
            const TArray<int64>* IndexedEdges = PatternIndex.EdgesByRelation.Find(Pattern.AllowedRelations[0]);
            if (IndexedEdges)
            {
                CacheHits++;
                return *IndexedEdges;
            }
            CacheMisses++;
        }
    }

    return Candidates;
}

TArray<FBindingEnvironment> UHypergraphPatternMatcher::ComputeTransitiveClosure(
    const FEdgePattern& EdgePattern,
    const FBindingEnvironment& CurrentBindings,
    const FPatternMatchConfig& Config)
{
    TArray<FBindingEnvironment> Results;

    if (!HypergraphRef)
    {
        return Results;
    }

    int64 SourceID = ResolvePatternElement(EdgePattern.Source, CurrentBindings);
    int64 TargetID = ResolvePatternElement(EdgePattern.Target, CurrentBindings);

    // BFS for transitive closure
    TSet<int64> Visited;
    TQueue<TPair<int64, int32>> Queue; // <NodeID, Depth>

    if (SourceID != 0)
    {
        Queue.Enqueue(TPair<int64, int32>(SourceID, 0));
        Visited.Add(SourceID);
    }

    while (!Queue.IsEmpty())
    {
        TPair<int64, int32> Current;
        Queue.Dequeue(Current);

        int64 CurrentNode = Current.Key;
        int32 CurrentDepth = Current.Value;

        if (CurrentDepth >= EdgePattern.MaxTransitiveDepth)
        {
            continue;
        }

        // Get outgoing edges
        TArray<FMemoryEdge> OutgoingEdges = HypergraphRef->GetOutgoingEdges(CurrentNode);

        for (const FMemoryEdge& Edge : OutgoingEdges)
        {
            if (!EdgeSatisfiesPattern(Edge, EdgePattern))
            {
                continue;
            }

            int64 NextNode = Edge.TargetNodeID;

            // Check if we've reached target
            if (TargetID != 0 && NextNode == TargetID)
            {
                // Found path - create binding
                FBindingEnvironment NewEnv = CurrentBindings;

                if (!EdgePattern.Source.VariableName.IsEmpty())
                {
                    NewEnv = ExtendBinding(NewEnv, EdgePattern.Source.VariableName, SourceID);
                }

                if (!EdgePattern.Target.VariableName.IsEmpty())
                {
                    NewEnv = ExtendBinding(NewEnv, EdgePattern.Target.VariableName, TargetID);
                }

                Results.Add(NewEnv);

                if (Config.MatchMode == EPatternMatchMode::FirstMatch)
                {
                    return Results;
                }
            }
            else if (TargetID == 0)
            {
                // No specific target - all reachable nodes are valid
                FBindingEnvironment NewEnv = CurrentBindings;

                if (!EdgePattern.Source.VariableName.IsEmpty())
                {
                    NewEnv = ExtendBinding(NewEnv, EdgePattern.Source.VariableName, SourceID);
                }

                if (!EdgePattern.Target.VariableName.IsEmpty())
                {
                    NewEnv = ExtendBinding(NewEnv, EdgePattern.Target.VariableName, NextNode);
                }

                Results.Add(NewEnv);
            }

            // Continue BFS
            if (!Visited.Contains(NextNode))
            {
                Visited.Add(NextNode);
                Queue.Enqueue(TPair<int64, int32>(NextNode, CurrentDepth + 1));
            }
        }
    }

    return Results;
}

void UHypergraphPatternMatcher::OptimizePatternOrder(FGraphPattern& Pattern)
{
    // Sort edge patterns by selectivity (estimated)
    // Patterns with type constraints first, then those with bound variables

    Pattern.EdgePatterns.Sort([](const FEdgePattern& A, const FEdgePattern& B)
    {
        // Score based on selectivity (lower is more selective = first)
        auto ScoreElement = [](const FPatternElement& Elem) -> int32
        {
            switch (Elem.ElementType)
            {
            case EPatternElementType::ConcreteNode:
                return 0; // Most selective
            case EPatternElementType::TypeConstraint:
                return 1;
            case EPatternElementType::PropertyConstraint:
                return 2;
            case EPatternElementType::Variable:
                return 3;
            case EPatternElementType::Wildcard:
                return 4; // Least selective
            default:
                return 5;
            }
        };

        int32 ScoreA = ScoreElement(A.Source) + ScoreElement(A.Target);
        int32 ScoreB = ScoreElement(B.Source) + ScoreElement(B.Target);

        // Also consider relation type specificity
        if (A.AllowedRelations.Num() > 0)
        {
            ScoreA -= 1;
        }
        if (B.AllowedRelations.Num() > 0)
        {
            ScoreB -= 1;
        }

        return ScoreA < ScoreB;
    });
}
