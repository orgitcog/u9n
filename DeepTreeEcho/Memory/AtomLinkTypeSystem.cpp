// AtomLinkTypeSystem.cpp
// Feature F1.4.1: Atom/Link Type System Implementation for Deep Tree Echo
// Copyright (c) 2025-2026 Deep Tree Echo Project

#include "AtomLinkTypeSystem.h"

// ============================================================================
// TRUTH VALUE IMPLEMENTATION
// ============================================================================

FAtomTruthValue FAtomTruthValue::Revise(const FAtomTruthValue& Other) const
{
    // Simple count-weighted revision
    float TotalCount = static_cast<float>(Count + Other.Count);
    if (TotalCount <= 0.0f)
    {
        return FAtomTruthValue();
    }

    float NewStrength = (Strength * Count + Other.Strength * Other.Count) / TotalCount;
    float NewConfidence = FMath::Min(Confidence + Other.Confidence * (1.0f - Confidence), 1.0f);
    int32 NewCount = Count + Other.Count;

    return FAtomTruthValue(NewStrength, NewConfidence, NewCount);
}

// ============================================================================
// ATTENTION VALUE IMPLEMENTATION
// ============================================================================

void FAtomAttentionValue::Decay(float DecayRate)
{
    STI *= (1.0f - DecayRate);
    LTI *= (1.0f - DecayRate * 0.1f);  // LTI decays slower
    // VLTI doesn't decay
}

void FAtomAttentionValue::Boost(float BoostAmount)
{
    STI = FMath::Clamp(STI + BoostAmount, -1.0f, 1.0f);
    LTI = FMath::Clamp(LTI + BoostAmount * 0.5f, -1.0f, 1.0f);
}

// ============================================================================
// ATOM/LINK TYPE SYSTEM IMPLEMENTATION
// ============================================================================

UAtomLinkTypeSystem::UAtomLinkTypeSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f; // Low-frequency tick for maintenance
}

void UAtomLinkTypeSystem::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UAtomLinkTypeSystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void UAtomLinkTypeSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Periodic statistics update
    UpdateStats();
}

void UAtomLinkTypeSystem::Initialize()
{
    if (bIsInitialized)
    {
        return;
    }

    TypeRegistry.Empty();
    NameToTypeMap.Empty();
    SubtypeCache.Empty();
    AncestorCache.Empty();

    RegisterStandardTypes();
    BuildTypeCaches();

    bIsInitialized = true;

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("AtomLinkTypeSystem: Initialized with %d types"), TypeRegistry.Num());
    }
}

void UAtomLinkTypeSystem::RegisterStandardTypes()
{
    // === Base Types ===
    RegisterType(EAtomTypeID::Atom, TEXT("Atom"), EAtomTypeID::Atom,
        true, true, -1, true, TEXT("Abstract base type for all atoms"));

    // === Node Types ===
    RegisterType(EAtomTypeID::Node, TEXT("Node"), EAtomTypeID::Atom,
        true, true, -1, true, TEXT("Abstract base type for all nodes"));

    RegisterType(EAtomTypeID::ConceptNode, TEXT("ConceptNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents abstract concepts"));

    RegisterType(EAtomTypeID::PredicateNode, TEXT("PredicateNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents relations/properties"));

    RegisterType(EAtomTypeID::NumberNode, TEXT("NumberNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents numeric values"));

    RegisterType(EAtomTypeID::TypeNode, TEXT("TypeNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents types (meta-level)"));

    RegisterType(EAtomTypeID::VariableNode, TEXT("VariableNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents unbound variables for pattern matching"));

    RegisterType(EAtomTypeID::AnchorNode, TEXT("AnchorNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents grounding points to external systems"));

    RegisterType(EAtomTypeID::TimeNode, TEXT("TimeNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents temporal points"));

    RegisterType(EAtomTypeID::SchemaNode, TEXT("SchemaNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Represents procedural schemas"));

    RegisterType(EAtomTypeID::GroundedSchemaNode, TEXT("GroundedSchemaNode"), EAtomTypeID::SchemaNode,
        true, false, -1, true, TEXT("Executable procedure"));

    RegisterType(EAtomTypeID::GroundedPredicateNode, TEXT("GroundedPredicateNode"), EAtomTypeID::PredicateNode,
        true, false, -1, true, TEXT("Executable predicate"));

    // === Link Types ===
    RegisterType(EAtomTypeID::Link, TEXT("Link"), EAtomTypeID::Atom,
        false, true, 0, true, TEXT("Abstract base type for all links"));

    RegisterType(EAtomTypeID::UnorderedLink, TEXT("UnorderedLink"), EAtomTypeID::Link,
        false, true, 0, false, TEXT("Base for unordered collections"));

    RegisterType(EAtomTypeID::OrderedLink, TEXT("OrderedLink"), EAtomTypeID::Link,
        false, true, 0, true, TEXT("Base for ordered collections"));

    RegisterType(EAtomTypeID::ListLink, TEXT("ListLink"), EAtomTypeID::OrderedLink,
        false, false, 0, true, TEXT("Ordered list of atoms"));

    RegisterType(EAtomTypeID::SetLink, TEXT("SetLink"), EAtomTypeID::UnorderedLink,
        false, false, 0, false, TEXT("Unordered set of atoms"));

    // === Logical Links ===
    RegisterType(EAtomTypeID::AndLink, TEXT("AndLink"), EAtomTypeID::UnorderedLink,
        false, false, 0, false, TEXT("Logical conjunction"));

    RegisterType(EAtomTypeID::OrLink, TEXT("OrLink"), EAtomTypeID::UnorderedLink,
        false, false, 0, false, TEXT("Logical disjunction"));

    RegisterType(EAtomTypeID::NotLink, TEXT("NotLink"), EAtomTypeID::Link,
        false, false, 1, true, TEXT("Logical negation"));

    // === Inheritance Links ===
    RegisterType(EAtomTypeID::InheritanceLink, TEXT("InheritanceLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("IS-A relation (subset/subclass)"));

    RegisterType(EAtomTypeID::SimilarityLink, TEXT("SimilarityLink"), EAtomTypeID::UnorderedLink,
        false, false, 2, false, TEXT("Bidirectional similarity"));

    // === Evaluation Links ===
    RegisterType(EAtomTypeID::EvaluationLink, TEXT("EvaluationLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Predicate application"));

    RegisterType(EAtomTypeID::ExecutionLink, TEXT("ExecutionLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Schema execution"));

    // === Member Links ===
    RegisterType(EAtomTypeID::MemberLink, TEXT("MemberLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Set membership"));

    // === Context Links ===
    RegisterType(EAtomTypeID::ContextLink, TEXT("ContextLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Contextual wrapper"));

    // === Implication Links ===
    RegisterType(EAtomTypeID::ImplicationLink, TEXT("ImplicationLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Logical implication"));

    RegisterType(EAtomTypeID::EquivalenceLink, TEXT("EquivalenceLink"), EAtomTypeID::UnorderedLink,
        false, false, 2, false, TEXT("Logical equivalence"));

    // === State Links ===
    RegisterType(EAtomTypeID::StateLink, TEXT("StateLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Mutable state holder"));

    RegisterType(EAtomTypeID::DefineLink, TEXT("DefineLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Definition binding"));

    // === Temporal Links ===
    RegisterType(EAtomTypeID::AtTimeLink, TEXT("AtTimeLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Temporal annotation"));

    RegisterType(EAtomTypeID::BeforeLink, TEXT("BeforeLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Temporal ordering"));

    RegisterType(EAtomTypeID::DuringLink, TEXT("DuringLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Temporal containment"));

    RegisterType(EAtomTypeID::SequentialLink, TEXT("SequentialLink"), EAtomTypeID::OrderedLink,
        false, false, 0, true, TEXT("Sequential composition"));

    // === Deep Tree Echo Specific Types ===
    RegisterType(EAtomTypeID::EchoResonanceLink, TEXT("EchoResonanceLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Memory resonance in ESN"));

    RegisterType(EAtomTypeID::CognitiveStateNode, TEXT("CognitiveStateNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Cognitive state marker"));

    RegisterType(EAtomTypeID::EmbodiedSchemaNode, TEXT("EmbodiedSchemaNode"), EAtomTypeID::SchemaNode,
        true, false, -1, true, TEXT("4E embodied action schema"));

    RegisterType(EAtomTypeID::AffordanceLink, TEXT("AffordanceLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Environmental affordance"));

    RegisterType(EAtomTypeID::RelevanceLink, TEXT("RelevanceLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Relevance realization connection"));

    RegisterType(EAtomTypeID::GestaltLink, TEXT("GestaltLink"), EAtomTypeID::UnorderedLink,
        false, false, 0, false, TEXT("Holistic pattern grouping"));

    RegisterType(EAtomTypeID::ReservoirActivationNode, TEXT("ReservoirActivationNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("ESN reservoir state"));

    RegisterType(EAtomTypeID::MembraneBoundaryLink, TEXT("MembraneBoundaryLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("P-system membrane boundary"));

    // === Memory Types ===
    RegisterType(EAtomTypeID::EpisodicMemoryNode, TEXT("EpisodicMemoryNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Experiential memory"));

    RegisterType(EAtomTypeID::SemanticMemoryNode, TEXT("SemanticMemoryNode"), EAtomTypeID::ConceptNode,
        true, false, -1, true, TEXT("Factual knowledge"));

    RegisterType(EAtomTypeID::ProceduralMemoryNode, TEXT("ProceduralMemoryNode"), EAtomTypeID::SchemaNode,
        true, false, -1, true, TEXT("Skill memory"));

    RegisterType(EAtomTypeID::IntentionalMemoryNode, TEXT("IntentionalMemoryNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("BDI state (beliefs, desires, intentions)"));

    // === Emotional Types ===
    RegisterType(EAtomTypeID::EmotionalStateNode, TEXT("EmotionalStateNode"), EAtomTypeID::Node,
        true, false, -1, true, TEXT("Emotional marker"));

    RegisterType(EAtomTypeID::SomaticMarkerLink, TEXT("SomaticMarkerLink"), EAtomTypeID::OrderedLink,
        false, false, 2, true, TEXT("Embodied emotion link"));
}

void UAtomLinkTypeSystem::Reset()
{
    TypeRegistry.Empty();
    NameToTypeMap.Empty();
    SubtypeCache.Empty();
    AncestorCache.Empty();
    NextHandleID = 1;
    bIsInitialized = false;
    Stats = FTypeSystemStats();
}

bool UAtomLinkTypeSystem::RegisterType(
    EAtomTypeID TypeID,
    const FString& TypeName,
    EAtomTypeID ParentType,
    bool bIsNodeType,
    bool bIsAbstract,
    int32 Arity,
    bool bIsOrdered,
    const FString& Description)
{
    // Validate parent exists (except for Atom which is root)
    if (TypeID != EAtomTypeID::Atom && !TypeRegistry.Contains(ParentType))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot register %s - parent type not found"), *TypeName);
        return false;
    }

    FAtomTypeInfo TypeInfo(TypeID, TypeName, ParentType, bIsNodeType, bIsAbstract, Arity, bIsOrdered, Description);
    TypeRegistry.Add(TypeID, TypeInfo);
    NameToTypeMap.Add(TypeName.ToLower(), TypeID);

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("AtomLinkTypeSystem: Registered type %s (parent: %s)"), 
            *TypeName, *TypeIDToString(ParentType));
    }

    return true;
}

bool UAtomLinkTypeSystem::IsTypeRegistered(EAtomTypeID TypeID) const
{
    return TypeRegistry.Contains(TypeID);
}

FAtomTypeInfo UAtomLinkTypeSystem::GetTypeInfo(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return *Info;
    }
    return FAtomTypeInfo();
}

TArray<EAtomTypeID> UAtomLinkTypeSystem::GetAllRegisteredTypes() const
{
    TArray<EAtomTypeID> Types;
    TypeRegistry.GetKeys(Types);
    return Types;
}

bool UAtomLinkTypeSystem::IsSubtypeOf(EAtomTypeID TypeA, EAtomTypeID TypeB) const
{
    if (TypeA == TypeB)
    {
        return true; // Every type is a subtype of itself
    }

    // Check cached ancestors
    if (bEnableTypeCaching)
    {
        if (const TArray<EAtomTypeID>* Ancestors = AncestorCache.Find(TypeA))
        {
            return Ancestors->Contains(TypeB);
        }
    }

    // Walk up the hierarchy
    EAtomTypeID Current = TypeA;
    while (Current != EAtomTypeID::Atom)
    {
        if (const FAtomTypeInfo* Info = TypeRegistry.Find(Current))
        {
            if (Info->ParentType == TypeB)
            {
                return true;
            }
            Current = Info->ParentType;
        }
        else
        {
            break;
        }
    }

    return TypeB == EAtomTypeID::Atom && IsTypeRegistered(TypeA);
}

bool UAtomLinkTypeSystem::IsSupertypeOf(EAtomTypeID TypeA, EAtomTypeID TypeB) const
{
    return IsSubtypeOf(TypeB, TypeA);
}

EAtomTypeID UAtomLinkTypeSystem::GetParentType(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return Info->ParentType;
    }
    return EAtomTypeID::Atom;
}

TArray<EAtomTypeID> UAtomLinkTypeSystem::GetAncestorTypes(EAtomTypeID TypeID) const
{
    // Check cache first
    if (bEnableTypeCaching)
    {
        if (const TArray<EAtomTypeID>* CachedAncestors = AncestorCache.Find(TypeID))
        {
            return *CachedAncestors;
        }
    }

    TArray<EAtomTypeID> Ancestors;
    EAtomTypeID Current = TypeID;

    while (Current != EAtomTypeID::Atom)
    {
        if (const FAtomTypeInfo* Info = TypeRegistry.Find(Current))
        {
            if (Info->ParentType != Current)
            {
                Ancestors.Add(Info->ParentType);
                Current = Info->ParentType;
            }
            else
            {
                break; // Reached root
            }
        }
        else
        {
            break;
        }
    }

    return Ancestors;
}

TArray<EAtomTypeID> UAtomLinkTypeSystem::GetDirectSubtypes(EAtomTypeID TypeID) const
{
    // Check cache first
    if (bEnableTypeCaching)
    {
        if (const TArray<EAtomTypeID>* CachedSubtypes = SubtypeCache.Find(TypeID))
        {
            return *CachedSubtypes;
        }
    }

    TArray<EAtomTypeID> Subtypes;
    for (const auto& Pair : TypeRegistry)
    {
        if (Pair.Value.ParentType == TypeID && Pair.Key != TypeID)
        {
            Subtypes.Add(Pair.Key);
        }
    }
    return Subtypes;
}

TArray<EAtomTypeID> UAtomLinkTypeSystem::GetDescendantTypes(EAtomTypeID TypeID) const
{
    TArray<EAtomTypeID> Descendants;
    TArray<EAtomTypeID> ToProcess = GetDirectSubtypes(TypeID);

    while (ToProcess.Num() > 0)
    {
        EAtomTypeID Current = ToProcess.Pop();
        Descendants.Add(Current);
        ToProcess.Append(GetDirectSubtypes(Current));
    }

    return Descendants;
}

int32 UAtomLinkTypeSystem::GetInheritanceDepth(EAtomTypeID TypeID) const
{
    return CalculateDepth(TypeID);
}

EAtomTypeID UAtomLinkTypeSystem::FindCommonSupertype(EAtomTypeID TypeA, EAtomTypeID TypeB) const
{
    if (TypeA == TypeB)
    {
        return TypeA;
    }

    TArray<EAtomTypeID> AncestorsA = GetAncestorTypes(TypeA);
    AncestorsA.Insert(TypeA, 0);

    TArray<EAtomTypeID> AncestorsB = GetAncestorTypes(TypeB);
    AncestorsB.Insert(TypeB, 0);

    // Find first common ancestor
    for (EAtomTypeID AncA : AncestorsA)
    {
        if (AncestorsB.Contains(AncA))
        {
            return AncA;
        }
    }

    return EAtomTypeID::Atom; // Ultimate common supertype
}

bool UAtomLinkTypeSystem::IsNodeType(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return Info->bIsNodeType;
    }
    return false;
}

bool UAtomLinkTypeSystem::IsLinkType(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return !Info->bIsNodeType;
    }
    return false;
}

bool UAtomLinkTypeSystem::IsAbstractType(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return Info->bIsAbstract;
    }
    return false;
}

TArray<EAtomTypeID> UAtomLinkTypeSystem::GetAllNodeTypes() const
{
    TArray<EAtomTypeID> NodeTypes;
    for (const auto& Pair : TypeRegistry)
    {
        if (Pair.Value.bIsNodeType && !Pair.Value.bIsAbstract)
        {
            NodeTypes.Add(Pair.Key);
        }
    }
    return NodeTypes;
}

TArray<EAtomTypeID> UAtomLinkTypeSystem::GetAllLinkTypes() const
{
    TArray<EAtomTypeID> LinkTypes;
    for (const auto& Pair : TypeRegistry)
    {
        if (!Pair.Value.bIsNodeType && !Pair.Value.bIsAbstract)
        {
            LinkTypes.Add(Pair.Key);
        }
    }
    return LinkTypes;
}

int32 UAtomLinkTypeSystem::GetExpectedArity(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return Info->Arity;
    }
    return -1;
}

FTypedAtom UAtomLinkTypeSystem::CreateTypedNode(
    EAtomTypeID TypeID,
    const FString& Name,
    const FAtomTruthValue& TruthValue)
{
    FTypedAtom Atom;

    // Validate type
    if (!IsTypeRegistered(TypeID))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create node - type not registered"));
        return Atom;
    }

    if (!IsNodeType(TypeID))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create node - %s is not a node type"),
            *TypeIDToString(TypeID));
        return Atom;
    }

    if (bEnableStrictTypeChecking && IsAbstractType(TypeID))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create node - %s is abstract"),
            *TypeIDToString(TypeID));
        return Atom;
    }

    Atom.Handle = FTypedAtomHandle(GenerateHandleID(), TypeID);
    Atom.TypeID = TypeID;
    Atom.Name = Name;
    Atom.TruthValue = TruthValue;
    Atom.CreatedAt = FDateTime::Now();
    Atom.ModifiedAt = Atom.CreatedAt;

    Stats.TotalAtomsCreated++;

    return Atom;
}

FTypedAtom UAtomLinkTypeSystem::CreateTypedLink(
    EAtomTypeID TypeID,
    const TArray<FTypedAtomHandle>& OutgoingSet,
    const FAtomTruthValue& TruthValue)
{
    FTypedAtom Atom;

    // Validate type
    if (!IsTypeRegistered(TypeID))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create link - type not registered"));
        return Atom;
    }

    if (!IsLinkType(TypeID))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create link - %s is not a link type"),
            *TypeIDToString(TypeID));
        return Atom;
    }

    if (bEnableStrictTypeChecking && IsAbstractType(TypeID))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create link - %s is abstract"),
            *TypeIDToString(TypeID));
        return Atom;
    }

    // Validate arity
    FString Error;
    if (bEnableStrictTypeChecking && !ValidateLinkOutgoing(TypeID, OutgoingSet, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("AtomLinkTypeSystem: Cannot create link - %s"), *Error);
        return Atom;
    }

    Atom.Handle = FTypedAtomHandle(GenerateHandleID(), TypeID);
    Atom.TypeID = TypeID;
    Atom.OutgoingSet = OutgoingSet;
    Atom.TruthValue = TruthValue;
    Atom.CreatedAt = FDateTime::Now();
    Atom.ModifiedAt = Atom.CreatedAt;

    Stats.TotalAtomsCreated++;

    return Atom;
}

bool UAtomLinkTypeSystem::ValidateAtom(const FTypedAtom& Atom, FString& OutError) const
{
    if (!Atom.Handle.bIsValid)
    {
        OutError = TEXT("Invalid atom handle");
        return false;
    }

    if (!IsTypeRegistered(Atom.TypeID))
    {
        OutError = FString::Printf(TEXT("Unregistered type: %s"), *TypeIDToString(Atom.TypeID));
        return false;
    }

    bool bIsNode = IsNodeType(Atom.TypeID);

    if (bIsNode && Atom.OutgoingSet.Num() > 0)
    {
        OutError = TEXT("Node type cannot have outgoing set");
        return false;
    }

    if (!bIsNode && !ValidateLinkOutgoing(Atom.TypeID, Atom.OutgoingSet, OutError))
    {
        return false;
    }

    return true;
}

bool UAtomLinkTypeSystem::ValidateLinkOutgoing(EAtomTypeID LinkType, const TArray<FTypedAtomHandle>& OutgoingSet, FString& OutError) const
{
    int32 ExpectedArity = GetExpectedArity(LinkType);

    // Arity 0 means variable arity (any number allowed)
    if (ExpectedArity > 0 && OutgoingSet.Num() != ExpectedArity)
    {
        OutError = FString::Printf(TEXT("Expected arity %d, got %d"), ExpectedArity, OutgoingSet.Num());
        return false;
    }

    // Validate all handles
    for (const FTypedAtomHandle& Handle : OutgoingSet)
    {
        if (!Handle.bIsValid)
        {
            OutError = TEXT("Invalid handle in outgoing set");
            return false;
        }
    }

    return true;
}

FString UAtomLinkTypeSystem::TypeIDToString(EAtomTypeID TypeID) const
{
    if (const FAtomTypeInfo* Info = TypeRegistry.Find(TypeID))
    {
        return Info->TypeName;
    }

    // Fallback to enum name
    const UEnum* EnumPtr = StaticEnum<EAtomTypeID>();
    if (EnumPtr)
    {
        return EnumPtr->GetNameStringByValue(static_cast<int64>(TypeID));
    }

    return TEXT("Unknown");
}

EAtomTypeID UAtomLinkTypeSystem::StringToTypeID(const FString& TypeName) const
{
    FString LowerName = TypeName.ToLower();
    if (const EAtomTypeID* TypeID = NameToTypeMap.Find(LowerName))
    {
        return *TypeID;
    }
    return EAtomTypeID::Atom;
}

FTypeSystemStats UAtomLinkTypeSystem::GetStats() const
{
    return Stats;
}

TArray<FString> UAtomLinkTypeSystem::GenerateDiagnosticReport() const
{
    TArray<FString> Report;

    Report.Add(TEXT("=== Atom/Link Type System Diagnostic Report ==="));
    Report.Add(FString::Printf(TEXT("Total Types: %d"), Stats.TotalTypes));
    Report.Add(FString::Printf(TEXT("Node Types: %d"), Stats.NodeTypes));
    Report.Add(FString::Printf(TEXT("Link Types: %d"), Stats.LinkTypes));
    Report.Add(FString::Printf(TEXT("Max Inheritance Depth: %d"), Stats.MaxInheritanceDepth));
    Report.Add(FString::Printf(TEXT("Total Atoms Created: %lld"), Stats.TotalAtomsCreated));
    Report.Add(FString::Printf(TEXT("Strict Type Checking: %s"), bEnableStrictTypeChecking ? TEXT("Enabled") : TEXT("Disabled")));
    Report.Add(FString::Printf(TEXT("Type Caching: %s"), bEnableTypeCaching ? TEXT("Enabled") : TEXT("Disabled")));

    Report.Add(TEXT("\n--- Type Hierarchy ---"));
    for (const auto& Pair : TypeRegistry)
    {
        const FAtomTypeInfo& Info = Pair.Value;
        Report.Add(FString::Printf(TEXT("  %s : %s [%s%s]"),
            *Info.TypeName,
            *TypeIDToString(Info.ParentType),
            Info.bIsNodeType ? TEXT("Node") : TEXT("Link"),
            Info.bIsAbstract ? TEXT(", Abstract") : TEXT("")));
    }

    return Report;
}

void UAtomLinkTypeSystem::BuildTypeCaches()
{
    if (!bEnableTypeCaching)
    {
        return;
    }

    SubtypeCache.Empty();
    AncestorCache.Empty();

    // Build subtype cache
    for (const auto& Pair : TypeRegistry)
    {
        EAtomTypeID TypeID = Pair.Key;
        TArray<EAtomTypeID> Subtypes;

        for (const auto& OtherPair : TypeRegistry)
        {
            if (OtherPair.Value.ParentType == TypeID && OtherPair.Key != TypeID)
            {
                Subtypes.Add(OtherPair.Key);
            }
        }

        SubtypeCache.Add(TypeID, Subtypes);
    }

    // Build ancestor cache
    for (const auto& Pair : TypeRegistry)
    {
        EAtomTypeID TypeID = Pair.Key;
        TArray<EAtomTypeID> Ancestors;
        EAtomTypeID Current = TypeID;

        while (Current != EAtomTypeID::Atom)
        {
            if (const FAtomTypeInfo* Info = TypeRegistry.Find(Current))
            {
                if (Info->ParentType != Current)
                {
                    Ancestors.Add(Info->ParentType);
                    Current = Info->ParentType;
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

        AncestorCache.Add(TypeID, Ancestors);
    }

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("AtomLinkTypeSystem: Built caches for %d types"), TypeRegistry.Num());
    }
}

void UAtomLinkTypeSystem::RebuildSubtypeCache(EAtomTypeID TypeID)
{
    TArray<EAtomTypeID> Subtypes;
    for (const auto& Pair : TypeRegistry)
    {
        if (Pair.Value.ParentType == TypeID && Pair.Key != TypeID)
        {
            Subtypes.Add(Pair.Key);
        }
    }
    SubtypeCache.Add(TypeID, Subtypes);
}

int32 UAtomLinkTypeSystem::CalculateDepth(EAtomTypeID TypeID) const
{
    int32 Depth = 0;
    EAtomTypeID Current = TypeID;

    while (Current != EAtomTypeID::Atom)
    {
        if (const FAtomTypeInfo* Info = TypeRegistry.Find(Current))
        {
            if (Info->ParentType != Current)
            {
                Depth++;
                Current = Info->ParentType;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    return Depth;
}

int64 UAtomLinkTypeSystem::GenerateHandleID()
{
    return NextHandleID++;
}

void UAtomLinkTypeSystem::UpdateStats()
{
    Stats.TotalTypes = TypeRegistry.Num();
    Stats.NodeTypes = 0;
    Stats.LinkTypes = 0;
    Stats.MaxInheritanceDepth = 0;

    for (const auto& Pair : TypeRegistry)
    {
        if (Pair.Value.bIsNodeType)
        {
            Stats.NodeTypes++;
        }
        else
        {
            Stats.LinkTypes++;
        }

        int32 Depth = CalculateDepth(Pair.Key);
        Stats.MaxInheritanceDepth = FMath::Max(Stats.MaxInheritanceDepth, Depth);
    }
}
