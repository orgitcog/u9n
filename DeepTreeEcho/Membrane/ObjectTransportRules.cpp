/**
 * @file ObjectTransportRules.cpp
 * @brief Feature F1.3.2: Object/Symbol Transport Rules Implementation
 *
 * Implements P-system membrane computing transport rules:
 * - IN: Transport to child membrane
 * - OUT: Transport to parent membrane
 * - HERE: Transform within current membrane
 *
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0-alpha
 */

#include "ObjectTransportRules.h"

// ============================================================================
// FSymbolMultiset Implementation
// ============================================================================

void FSymbolMultiset::AddSymbol(const FMembraneSymbol& Symbol)
{
    if (!Symbol.IsValid())
    {
        return;
    }

    for (FMembraneSymbol& Existing : Symbols)
    {
        if (Existing.SymbolName == Symbol.SymbolName)
        {
            Existing.Multiplicity += Symbol.Multiplicity;
            return;
        }
    }
    Symbols.Add(Symbol);
}

bool FSymbolMultiset::RemoveSymbol(const FMembraneSymbol& Symbol)
{
    for (int32 i = 0; i < Symbols.Num(); ++i)
    {
        if (Symbols[i].SymbolName == Symbol.SymbolName)
        {
            Symbols[i].Multiplicity -= Symbol.Multiplicity;
            if (Symbols[i].Multiplicity <= 0)
            {
                Symbols.RemoveAt(i);
            }
            return true;
        }
    }
    return false;
}

bool FSymbolMultiset::Contains(const FMembraneSymbol& Symbol) const
{
    for (const FMembraneSymbol& Existing : Symbols)
    {
        if (Existing.SymbolName == Symbol.SymbolName && Existing.Multiplicity >= Symbol.Multiplicity)
        {
            return true;
        }
    }
    return false;
}

bool FSymbolMultiset::ContainsAll(const FSymbolMultiset& Pattern) const
{
    for (const FMembraneSymbol& Required : Pattern.Symbols)
    {
        if (!Contains(Required))
        {
            return false;
        }
    }
    return true;
}

int32 FSymbolMultiset::GetTotalCount() const
{
    int32 Total = 0;
    for (const FMembraneSymbol& Symbol : Symbols)
    {
        Total += Symbol.Multiplicity;
    }
    return Total;
}

int32 FSymbolMultiset::GetSymbolCount(FName SymbolName) const
{
    for (const FMembraneSymbol& Symbol : Symbols)
    {
        if (Symbol.SymbolName == SymbolName)
        {
            return Symbol.Multiplicity;
        }
    }
    return 0;
}

// ============================================================================
// FTransportRule Implementation
// ============================================================================

bool FTransportRule::IsValid() const
{
    // A rule needs at least a LHS or RHS and must be enabled
    return bEnabled && (!LeftHandSide.IsEmpty() || !RightHandSide.IsEmpty());
}

FString FTransportRule::ToString() const
{
    FString LHS;
    for (const FMembraneSymbol& Sym : LeftHandSide.Symbols)
    {
        if (!LHS.IsEmpty()) LHS += TEXT(" ");
        LHS += FString::Printf(TEXT("%s^%d"), *Sym.SymbolName.ToString(), Sym.Multiplicity);
    }
    if (LHS.IsEmpty()) LHS = TEXT("ε");

    FString RHS;
    for (const FMembraneSymbol& Sym : RightHandSide.Symbols)
    {
        if (!RHS.IsEmpty()) RHS += TEXT(" ");
        RHS += FString::Printf(TEXT("%s^%d"), *Sym.SymbolName.ToString(), Sym.Multiplicity);
    }
    if (RHS.IsEmpty()) RHS = TEXT("ε");

    FString Dir;
    switch (Target.Direction)
    {
        case ETransportDirection::Here: Dir = TEXT("here"); break;
        case ETransportDirection::In: Dir = TEXT("in"); break;
        case ETransportDirection::Out: Dir = TEXT("out"); break;
        case ETransportDirection::InAll: Dir = TEXT("in_all"); break;
        case ETransportDirection::Dissolve: Dir = TEXT("δ"); break;
    }

    return FString::Printf(TEXT("%s: %s -> %s, %s"), *RuleId.ToString(), *LHS, *RHS, *Dir);
}

// ============================================================================
// UObjectTransportRules Implementation
// ============================================================================

UObjectTransportRules::UObjectTransportRules()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UObjectTransportRules::BeginPlay()
{
    Super::BeginPlay();

    if (RandomSeed != 0)
    {
        RandomStream.Initialize(RandomSeed);
    }
    else
    {
        RandomStream.GenerateNewSeed();
    }
}

void UObjectTransportRules::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    CurrentTime += DeltaTime;

    if (bAutoStep && bIsInitialized)
    {
        ExecuteSteps(StepsPerTick);
    }
}

// ==================== Initialization ====================

bool UObjectTransportRules::Initialize(FName SkinLabel)
{
    if (bIsInitialized)
    {
        Reset();
    }

    // Create skin membrane
    SkinMembraneId = GenerateMembraneId();
    FMembraneCompartment SkinMembrane(SkinMembraneId, SkinLabel);
    SkinMembrane.CreationTime = CurrentTime;
    Membranes.Add(SkinMembraneId, SkinMembrane);

    bIsInitialized = true;
    OnMembraneCreated.Broadcast(SkinMembraneId);

    LogMessage(FString::Printf(TEXT("Initialized membrane system with skin: %s"), *SkinMembraneId.ToString()));
    return true;
}

void UObjectTransportRules::Reset()
{
    Membranes.Empty();
    SkinMembraneId = NAME_None;
    bIsInitialized = false;
    MembraneIdCounter = 0;
    Statistics.Reset();
    CurrentTime = 0.0f;

    LogMessage(TEXT("Membrane system reset"));
}

// ==================== Membrane Management ====================

FName UObjectTransportRules::CreateMembrane(FName ParentId, FName Label)
{
    if (!bIsInitialized)
    {
        LogMessage(TEXT("Cannot create membrane: system not initialized"));
        return NAME_None;
    }

    // Verify parent exists (except for skin)
    if (!ParentId.IsNone() && !Membranes.Contains(ParentId))
    {
        LogMessage(FString::Printf(TEXT("Cannot create membrane: parent %s not found"), *ParentId.ToString()));
        return NAME_None;
    }

    // Check hierarchy depth
    if (GetMembraneDepth(ParentId) >= MaxHierarchyDepth)
    {
        LogMessage(TEXT("Cannot create membrane: max hierarchy depth exceeded"));
        return NAME_None;
    }

    // Create new membrane
    FName NewId = GenerateMembraneId();
    FMembraneCompartment NewMembrane(NewId, Label);
    NewMembrane.ParentId = ParentId;
    NewMembrane.CreationTime = CurrentTime;

    Membranes.Add(NewId, NewMembrane);

    // Update parent's children list
    if (!ParentId.IsNone())
    {
        FMembraneCompartment* Parent = Membranes.Find(ParentId);
        if (Parent)
        {
            Parent->ChildIds.Add(NewId);
        }
    }

    OnMembraneCreated.Broadcast(NewId);
    LogMessage(FString::Printf(TEXT("Created membrane %s (parent: %s, label: %s)"), 
        *NewId.ToString(), *ParentId.ToString(), *Label.ToString()));

    return NewId;
}

bool UObjectTransportRules::RemoveMembrane(FName MembraneId, bool bReleaseContents)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return false;
    }

    // Cannot remove skin membrane
    if (Membrane->IsSkin())
    {
        LogMessage(TEXT("Cannot remove skin membrane"));
        return false;
    }

    // Handle contents
    if (bReleaseContents && !Membrane->ParentId.IsNone())
    {
        FMembraneCompartment* Parent = Membranes.Find(Membrane->ParentId);
        if (Parent)
        {
            // Transfer contents to parent
            for (const FMembraneSymbol& Symbol : Membrane->Contents.Symbols)
            {
                Parent->Contents.AddSymbol(Symbol);
            }
        }
    }

    // Handle children - re-parent them to this membrane's parent
    for (FName ChildId : Membrane->ChildIds)
    {
        FMembraneCompartment* Child = Membranes.Find(ChildId);
        if (Child)
        {
            Child->ParentId = Membrane->ParentId;
            if (FMembraneCompartment* Parent = Membranes.Find(Membrane->ParentId))
            {
                Parent->ChildIds.Add(ChildId);
            }
        }
    }

    // Remove from parent's children list
    if (FMembraneCompartment* Parent = Membranes.Find(Membrane->ParentId))
    {
        Parent->ChildIds.Remove(MembraneId);
    }

    Membranes.Remove(MembraneId);
    OnMembraneDissolved.Broadcast(MembraneId);
    LogMessage(FString::Printf(TEXT("Removed membrane %s"), *MembraneId.ToString()));

    return true;
}

const FMembraneCompartment* UObjectTransportRules::GetMembrane(FName MembraneId) const
{
    return Membranes.Find(MembraneId);
}

TArray<FName> UObjectTransportRules::GetAllMembraneIds() const
{
    TArray<FName> Ids;
    Membranes.GetKeys(Ids);
    return Ids;
}

TArray<FName> UObjectTransportRules::GetChildMembranes(FName MembraneId) const
{
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (Membrane)
    {
        return Membrane->ChildIds;
    }
    return TArray<FName>();
}

// ==================== Symbol Management ====================

bool UObjectTransportRules::AddSymbol(FName MembraneId, const FMembraneSymbol& Symbol)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane || !Symbol.IsValid())
    {
        return false;
    }

    Membrane->Contents.AddSymbol(Symbol);
    LogMessage(FString::Printf(TEXT("Added %s^%d to %s"), 
        *Symbol.SymbolName.ToString(), Symbol.Multiplicity, *MembraneId.ToString()));
    return true;
}

bool UObjectTransportRules::AddSymbols(FName MembraneId, const FSymbolMultiset& Symbols)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return false;
    }

    for (const FMembraneSymbol& Symbol : Symbols.Symbols)
    {
        Membrane->Contents.AddSymbol(Symbol);
    }
    return true;
}

bool UObjectTransportRules::RemoveSymbol(FName MembraneId, const FMembraneSymbol& Symbol)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return false;
    }

    return Membrane->Contents.RemoveSymbol(Symbol);
}

FSymbolMultiset UObjectTransportRules::GetSymbols(FName MembraneId) const
{
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (Membrane)
    {
        return Membrane->Contents;
    }
    return FSymbolMultiset();
}

bool UObjectTransportRules::MembraneContains(FName MembraneId, const FSymbolMultiset& Symbols) const
{
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return false;
    }

    return Membrane->Contents.ContainsAll(Symbols);
}

// ==================== Rule Management ====================

bool UObjectTransportRules::AddRule(FName MembraneId, const FTransportRule& Rule)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane || !Rule.IsValid())
    {
        return false;
    }

    // Check for duplicate rule ID
    for (const FTransportRule& Existing : Membrane->Rules)
    {
        if (Existing.RuleId == Rule.RuleId)
        {
            LogMessage(FString::Printf(TEXT("Rule %s already exists in membrane %s"), 
                *Rule.RuleId.ToString(), *MembraneId.ToString()));
            return false;
        }
    }

    Membrane->Rules.Add(Rule);
    LogMessage(FString::Printf(TEXT("Added rule %s to %s"), *Rule.ToString(), *MembraneId.ToString()));
    return true;
}

bool UObjectTransportRules::RemoveRule(FName MembraneId, FName RuleId)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return false;
    }

    for (int32 i = 0; i < Membrane->Rules.Num(); ++i)
    {
        if (Membrane->Rules[i].RuleId == RuleId)
        {
            Membrane->Rules.RemoveAt(i);
            LogMessage(FString::Printf(TEXT("Removed rule %s from %s"), *RuleId.ToString(), *MembraneId.ToString()));
            return true;
        }
    }
    return false;
}

TArray<FTransportRule> UObjectTransportRules::GetRules(FName MembraneId) const
{
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (Membrane)
    {
        return Membrane->Rules;
    }
    return TArray<FTransportRule>();
}

FTransportRule UObjectTransportRules::CreateSimpleRule(
    FName RuleId,
    FName LeftSymbol,
    FName RightSymbol,
    ETransportDirection Direction,
    int32 Priority)
{
    FTransportRule Rule;
    Rule.RuleId = RuleId;
    Rule.Priority = Priority;
    Rule.Target.Direction = Direction;

    if (!LeftSymbol.IsNone())
    {
        Rule.LeftHandSide.AddSymbol(FMembraneSymbol(LeftSymbol, 1));
    }

    if (!RightSymbol.IsNone())
    {
        Rule.RightHandSide.AddSymbol(FMembraneSymbol(RightSymbol, 1));
    }

    return Rule;
}

// ==================== Rule Execution ====================

int32 UObjectTransportRules::ExecuteStep()
{
    if (!bIsInitialized)
    {
        return 0;
    }

    double StartTime = FPlatformTime::Seconds();
    int32 TotalApplied = 0;

    // Execute rules for all membranes (bottom-up for proper transport)
    TArray<FName> MembraneIds;
    Membranes.GetKeys(MembraneIds);

    // Sort by depth (deepest first)
    MembraneIds.Sort([this](const FName& A, const FName& B) {
        return GetMembraneDepth(A) > GetMembraneDepth(B);
    });

    for (FName MembraneId : MembraneIds)
    {
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (Membrane && Membrane->bIsActive)
        {
            TotalApplied += ExecuteMembraneStep(MembraneId);
        }
    }

    Statistics.ComputationSteps++;
    Statistics.LastStepTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);

    // Check for halting condition
    if (TotalApplied == 0)
    {
        OnSystemHalted.Broadcast();
    }

    return TotalApplied;
}

int32 UObjectTransportRules::ExecuteSteps(int32 NumSteps)
{
    int32 TotalSteps = 0;
    for (int32 i = 0; i < NumSteps; ++i)
    {
        int32 Applied = ExecuteStep();
        if (Applied == 0)
        {
            break; // Halted
        }
        TotalSteps++;
    }
    return TotalSteps;
}

bool UObjectTransportRules::CanApplyRule(FName MembraneId, const FTransportRule& Rule) const
{
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane || !Rule.IsValid() || !Rule.bEnabled)
    {
        return false;
    }

    return CheckRuleConditions(*Membrane, Rule);
}

TArray<FTransportRule> UObjectTransportRules::GetApplicableRules(FName MembraneId) const
{
    TArray<FTransportRule> Applicable;
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return Applicable;
    }

    for (const FTransportRule& Rule : Membrane->Rules)
    {
        if (Rule.bEnabled && CheckRuleConditions(*Membrane, Rule))
        {
            Applicable.Add(Rule);
        }
    }

    // Sort by priority
    Applicable.Sort([](const FTransportRule& A, const FTransportRule& B) {
        return A.Priority > B.Priority;
    });

    return Applicable;
}

FRuleApplicationResult UObjectTransportRules::ApplyRule(FName MembraneId, FName RuleId)
{
    FRuleApplicationResult Result;
    Result.SourceMembraneId = MembraneId;
    Result.RuleId = RuleId;
    Result.Timestamp = CurrentTime;

    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        Result.ErrorMessage = TEXT("Membrane not found");
        return Result;
    }

    // Find the rule
    const FTransportRule* RulePtr = nullptr;
    for (const FTransportRule& Rule : Membrane->Rules)
    {
        if (Rule.RuleId == RuleId)
        {
            RulePtr = &Rule;
            break;
        }
    }

    if (!RulePtr)
    {
        Result.ErrorMessage = TEXT("Rule not found");
        return Result;
    }

    if (!CheckRuleConditions(*Membrane, *RulePtr))
    {
        Result.ErrorMessage = TEXT("Rule conditions not met");
        return Result;
    }

    return ApplyRuleInternal(*Membrane, *RulePtr);
}

// ==================== Transport Operations ====================

bool UObjectTransportRules::TransportIn(FName SourceMembraneId, const FSymbolMultiset& Symbols, FName TargetChildId)
{
    FMembraneCompartment* Source = Membranes.Find(SourceMembraneId);
    if (!Source)
    {
        return false;
    }

    // Verify source has the symbols
    if (!Source->Contents.ContainsAll(Symbols))
    {
        return false;
    }

    // Find target child
    FName TargetId = TargetChildId;
    if (TargetId.IsNone())
    {
        if (Source->ChildIds.Num() == 0)
        {
            return false;
        }
        TargetId = Source->ChildIds[0];
    }

    FMembraneCompartment* Target = Membranes.Find(TargetId);
    if (!Target || Target->ParentId != SourceMembraneId)
    {
        return false;
    }

    // Perform transport
    for (const FMembraneSymbol& Symbol : Symbols.Symbols)
    {
        Source->Contents.RemoveSymbol(Symbol);
        Target->Contents.AddSymbol(Symbol);
        Statistics.TotalSymbolsTransported += Symbol.Multiplicity;
    }

    Statistics.InTransports++;
    LogMessage(FString::Printf(TEXT("Transported IN: %s -> %s"), 
        *SourceMembraneId.ToString(), *TargetId.ToString()));

    return true;
}

bool UObjectTransportRules::TransportOut(FName SourceMembraneId, const FSymbolMultiset& Symbols)
{
    FMembraneCompartment* Source = Membranes.Find(SourceMembraneId);
    if (!Source)
    {
        return false;
    }

    // Cannot transport out from skin
    if (Source->IsSkin())
    {
        LogMessage(TEXT("Cannot transport OUT from skin membrane"));
        return false;
    }

    // Verify source has the symbols
    if (!Source->Contents.ContainsAll(Symbols))
    {
        return false;
    }

    FMembraneCompartment* Parent = Membranes.Find(Source->ParentId);
    if (!Parent)
    {
        return false;
    }

    // Perform transport
    for (const FMembraneSymbol& Symbol : Symbols.Symbols)
    {
        Source->Contents.RemoveSymbol(Symbol);
        Parent->Contents.AddSymbol(Symbol);
        Statistics.TotalSymbolsTransported += Symbol.Multiplicity;
    }

    Statistics.OutTransports++;
    LogMessage(FString::Printf(TEXT("Transported OUT: %s -> %s"), 
        *SourceMembraneId.ToString(), *Source->ParentId.ToString()));

    return true;
}

bool UObjectTransportRules::TransformHere(FName MembraneId, const FSymbolMultiset& ConsumedSymbols, const FSymbolMultiset& ProducedSymbols)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return false;
    }

    // Verify membrane has symbols to consume
    if (!Membrane->Contents.ContainsAll(ConsumedSymbols))
    {
        return false;
    }

    // Remove consumed symbols
    for (const FMembraneSymbol& Symbol : ConsumedSymbols.Symbols)
    {
        if (!Symbol.bIsCatalyst)
        {
            Membrane->Contents.RemoveSymbol(Symbol);
        }
    }

    // Add produced symbols
    for (const FMembraneSymbol& Symbol : ProducedSymbols.Symbols)
    {
        FMembraneSymbol NewSymbol = Symbol;
        NewSymbol.CreationTime = CurrentTime;
        Membrane->Contents.AddSymbol(NewSymbol);
    }

    Statistics.HereTransforms++;
    LogMessage(FString::Printf(TEXT("Transform HERE in %s"), *MembraneId.ToString()));

    return true;
}

// ==================== Debugging ====================

FString UObjectTransportRules::GetHierarchyString() const
{
    if (!bIsInitialized)
    {
        return TEXT("System not initialized");
    }

    FString Result;
    TFunction<void(FName, int32)> PrintMembrane = [&](FName Id, int32 Depth)
    {
        const FMembraneCompartment* Membrane = Membranes.Find(Id);
        if (!Membrane) return;

        FString Indent = FString::ChrN(Depth * 2, ' ');
        Result += FString::Printf(TEXT("%s[%s] (%s) - %d symbols, %d rules\n"),
            *Indent, *Id.ToString(), *Membrane->Label.ToString(),
            Membrane->Contents.GetTotalCount(), Membrane->Rules.Num());

        for (FName ChildId : Membrane->ChildIds)
        {
            PrintMembrane(ChildId, Depth + 1);
        }
    };

    PrintMembrane(SkinMembraneId, 0);
    return Result;
}

FString UObjectTransportRules::GetMembraneInfo(FName MembraneId) const
{
    const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return FString::Printf(TEXT("Membrane %s not found"), *MembraneId.ToString());
    }

    FString SymbolStr;
    for (const FMembraneSymbol& Symbol : Membrane->Contents.Symbols)
    {
        if (!SymbolStr.IsEmpty()) SymbolStr += TEXT(", ");
        SymbolStr += FString::Printf(TEXT("%s^%d"), *Symbol.SymbolName.ToString(), Symbol.Multiplicity);
    }
    if (SymbolStr.IsEmpty()) SymbolStr = TEXT("empty");

    FString RuleStr;
    for (const FTransportRule& Rule : Membrane->Rules)
    {
        if (!RuleStr.IsEmpty()) RuleStr += TEXT("\n  ");
        RuleStr += Rule.ToString();
    }
    if (RuleStr.IsEmpty()) RuleStr = TEXT("none");

    return FString::Printf(TEXT(
        "Membrane: %s\n"
        "Label: %s\n"
        "Parent: %s\n"
        "Children: %d\n"
        "Active: %s\n"
        "Symbols: %s\n"
        "Rules:\n  %s"),
        *MembraneId.ToString(),
        *Membrane->Label.ToString(),
        *Membrane->ParentId.ToString(),
        Membrane->ChildIds.Num(),
        Membrane->bIsActive ? TEXT("Yes") : TEXT("No"),
        *SymbolStr,
        *RuleStr);
}

bool UObjectTransportRules::ValidateSystem() const
{
    if (!bIsInitialized)
    {
        return false;
    }

    // Check skin membrane exists
    if (!Membranes.Contains(SkinMembraneId))
    {
        return false;
    }

    // Validate all membranes
    for (const auto& Pair : Membranes)
    {
        const FMembraneCompartment& Membrane = Pair.Value;

        // Check parent exists (except skin)
        if (!Membrane.IsSkin() && !Membranes.Contains(Membrane.ParentId))
        {
            return false;
        }

        // Check all children exist
        for (FName ChildId : Membrane.ChildIds)
        {
            if (!Membranes.Contains(ChildId))
            {
                return false;
            }
        }
    }

    return true;
}

// ==================== Internal Methods ====================

FName UObjectTransportRules::GenerateMembraneId()
{
    return FName(*FString::Printf(TEXT("Membrane_%d"), MembraneIdCounter++));
}

int32 UObjectTransportRules::ExecuteMembraneStep(FName MembraneId)
{
    switch (ExecutionMode)
    {
        case ERuleExecutionMode::MaximalParallel:
            return ExecuteMaximalParallel(MembraneId);
        case ERuleExecutionMode::Sequential:
            return ExecuteSequential(MembraneId);
        case ERuleExecutionMode::Probabilistic:
            return ExecuteProbabilistic(MembraneId);
        case ERuleExecutionMode::Deterministic:
        default:
            return ExecuteSequential(MembraneId);
    }
}

int32 UObjectTransportRules::ExecuteMaximalParallel(FName MembraneId)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return 0;
    }

    int32 Applied = 0;
    bool bAnyApplied = true;

    // Keep applying rules until no more can be applied
    while (bAnyApplied)
    {
        bAnyApplied = false;

        // Get all applicable rules
        TArray<FTransportRule> Applicable = GetApplicableRules(MembraneId);

        for (const FTransportRule& Rule : Applicable)
        {
            // Re-check if rule is still applicable
            if (CheckRuleConditions(*Membrane, Rule))
            {
                FRuleApplicationResult Result = ApplyRuleInternal(*Membrane, Rule);
                if (Result.bSuccess)
                {
                    Applied++;
                    bAnyApplied = true;
                    OnRuleApplied.Broadcast(Result);
                }
            }
        }
    }

    return Applied;
}

int32 UObjectTransportRules::ExecuteSequential(FName MembraneId)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return 0;
    }

    // Get applicable rules sorted by priority
    TArray<FTransportRule> Applicable = GetApplicableRules(MembraneId);

    int32 Applied = 0;
    for (const FTransportRule& Rule : Applicable)
    {
        // Re-check if rule is still applicable
        if (CheckRuleConditions(*Membrane, Rule))
        {
            FRuleApplicationResult Result = ApplyRuleInternal(*Membrane, Rule);
            if (Result.bSuccess)
            {
                Applied++;
                OnRuleApplied.Broadcast(Result);
            }
        }
    }

    return Applied;
}

int32 UObjectTransportRules::ExecuteProbabilistic(FName MembraneId)
{
    FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
    if (!Membrane)
    {
        return 0;
    }

    TArray<FTransportRule> Applicable = GetApplicableRules(MembraneId);
    if (Applicable.Num() == 0)
    {
        return 0;
    }

    // Calculate total weight
    float TotalWeight = 0.0f;
    for (const FTransportRule& Rule : Applicable)
    {
        TotalWeight += Rule.ProbabilityWeight;
    }

    if (TotalWeight <= 0.0f)
    {
        return 0;
    }

    // Select rule probabilistically
    float Selection = RandomStream.FRandRange(0.0f, TotalWeight);
    float Cumulative = 0.0f;

    for (const FTransportRule& Rule : Applicable)
    {
        Cumulative += Rule.ProbabilityWeight;
        if (Selection <= Cumulative)
        {
            if (CheckRuleConditions(*Membrane, Rule))
            {
                FRuleApplicationResult Result = ApplyRuleInternal(*Membrane, Rule);
                if (Result.bSuccess)
                {
                    OnRuleApplied.Broadcast(Result);
                    return 1;
                }
            }
            break;
        }
    }

    return 0;
}

FName UObjectTransportRules::FindTransportTarget(FName SourceId, const FTransportTarget& Target) const
{
    const FMembraneCompartment* Source = Membranes.Find(SourceId);
    if (!Source)
    {
        return NAME_None;
    }

    switch (Target.Direction)
    {
        case ETransportDirection::Here:
            return SourceId;

        case ETransportDirection::Out:
            return Source->ParentId;

        case ETransportDirection::In:
        case ETransportDirection::InAll:
            // Find specific child or first child
            if (!Target.TargetMembraneId.IsNone())
            {
                return Target.TargetMembraneId;
            }
            if (!Target.TargetLabel.IsNone())
            {
                // Find child by label
                for (FName ChildId : Source->ChildIds)
                {
                    const FMembraneCompartment* Child = Membranes.Find(ChildId);
                    if (Child && Child->Label == Target.TargetLabel)
                    {
                        return ChildId;
                    }
                }
            }
            // Default to first child
            return Source->ChildIds.Num() > 0 ? Source->ChildIds[0] : NAME_None;

        case ETransportDirection::Dissolve:
            return Source->ParentId;

        default:
            return NAME_None;
    }
}

bool UObjectTransportRules::CheckRuleConditions(const FMembraneCompartment& Membrane, const FTransportRule& Rule) const
{
    Statistics.TotalRulesEvaluated++;

    // Check LHS symbols are present
    if (!Membrane.Contents.ContainsAll(Rule.LeftHandSide))
    {
        Statistics.RulesBlockedBySymbols++;
        return false;
    }

    // Check catalysts are present
    if (!Membrane.Contents.ContainsAll(Rule.Catalysts))
    {
        Statistics.RulesBlockedBySymbols++;
        return false;
    }

    // Check promoters are present (if any)
    if (!Rule.Promoters.IsEmpty() && !Membrane.Contents.ContainsAll(Rule.Promoters))
    {
        Statistics.RulesBlockedBySymbols++;
        return false;
    }

    // Check inhibitors are NOT present
    for (const FMembraneSymbol& Inhibitor : Rule.Inhibitors.Symbols)
    {
        if (Membrane.Contents.Contains(Inhibitor))
        {
            Statistics.RulesBlockedByInhibitors++;
            return false;
        }
    }

    // Check transport target exists (for In/Out directions)
    if (Rule.Target.Direction == ETransportDirection::In || Rule.Target.Direction == ETransportDirection::InAll)
    {
        if (Membrane.ChildIds.Num() == 0)
        {
            return false;
        }
    }
    else if (Rule.Target.Direction == ETransportDirection::Out)
    {
        if (Membrane.IsSkin())
        {
            return false;
        }
    }

    return true;
}

FRuleApplicationResult UObjectTransportRules::ApplyRuleInternal(FMembraneCompartment& Membrane, const FTransportRule& Rule)
{
    FRuleApplicationResult Result;
    Result.RuleId = Rule.RuleId;
    Result.SourceMembraneId = Membrane.MembraneId;
    Result.Timestamp = CurrentTime;

    // Find target membrane
    FName TargetId = FindTransportTarget(Membrane.MembraneId, Rule.Target);
    Result.TargetMembraneId = TargetId;

    // Consume LHS symbols (except catalysts)
    for (const FMembraneSymbol& Symbol : Rule.LeftHandSide.Symbols)
    {
        if (!Symbol.bIsCatalyst)
        {
            Membrane.Contents.RemoveSymbol(Symbol);
            Result.ConsumedSymbols.AddSymbol(Symbol);
        }
    }

    // Produce RHS symbols
    FMembraneCompartment* TargetMembrane = Membranes.Find(TargetId);
    if (!TargetMembrane)
    {
        TargetMembrane = &Membrane; // Default to source
    }

    for (const FMembraneSymbol& Symbol : Rule.RightHandSide.Symbols)
    {
        FMembraneSymbol NewSymbol = Symbol;
        NewSymbol.CreationTime = CurrentTime;
        TargetMembrane->Contents.AddSymbol(NewSymbol);
        Result.ProducedSymbols.AddSymbol(Symbol);
        Statistics.TotalSymbolsTransported += Symbol.Multiplicity;
    }

    // Handle special directions
    switch (Rule.Target.Direction)
    {
        case ETransportDirection::Here:
            Statistics.HereTransforms++;
            break;
        case ETransportDirection::In:
        case ETransportDirection::InAll:
            Statistics.InTransports++;
            break;
        case ETransportDirection::Out:
            Statistics.OutTransports++;
            break;
        case ETransportDirection::Dissolve:
            Statistics.OutTransports++;
            // Mark membrane for dissolution
            Membrane.bIsActive = false;
            break;
    }

    Statistics.RulesApplied++;
    Result.bSuccess = true;

    LogMessage(FString::Printf(TEXT("Applied rule %s: %s"), *Rule.RuleId.ToString(), *Rule.ToString()));

    return Result;
}

int32 UObjectTransportRules::GetMembraneDepth(FName MembraneId) const
{
    int32 Depth = 0;
    FName CurrentId = MembraneId;

    while (!CurrentId.IsNone())
    {
        const FMembraneCompartment* Membrane = Membranes.Find(CurrentId);
        if (!Membrane)
        {
            break;
        }
        CurrentId = Membrane->ParentId;
        Depth++;
    }

    return Depth;
}

void UObjectTransportRules::LogMessage(const FString& Message) const
{
    if (bEnableLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ObjectTransportRules] %s"), *Message);
    }
}
