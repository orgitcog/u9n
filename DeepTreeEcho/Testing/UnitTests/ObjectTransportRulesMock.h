/**
 * @file ObjectTransportRulesMock.h
 * @brief Mock implementation for ObjectTransportRules unit testing
 *
 * Provides standalone mock infrastructure for testing P-system membrane
 * computing transport rules without Unreal Engine dependencies.
 *
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0-alpha
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <random>
#include <functional>

// Type aliases for UE-like types
using int32 = int32_t;

struct FName
{
    std::string Value;
    
    FName() : Value() {}
    FName(const std::string& InValue) : Value(InValue) {}
    FName(const char* InValue) : Value(InValue ? InValue : "") {}
    
    bool IsNone() const { return Value.empty(); }
    std::string ToString() const { return Value; }
    
    bool operator==(const FName& Other) const { return Value == Other.Value; }
    bool operator!=(const FName& Other) const { return Value != Other.Value; }
    bool operator<(const FName& Other) const { return Value < Other.Value; }
    
    // Hash function for map
    struct Hash
    {
        size_t operator()(const FName& Name) const
        {
            return std::hash<std::string>()(Name.Value);
        }
    };
};

struct FString
{
    std::string Value;
    
    FString() : Value() {}
    FString(const std::string& InValue) : Value(InValue) {}
    FString(const char* InValue) : Value(InValue ? InValue : "") {}
    
    bool IsEmpty() const { return Value.empty(); }
    bool Contains(const std::string& Sub) const { return Value.find(Sub) != std::string::npos; }
    bool Contains(const char* Sub) const { return Value.find(Sub) != std::string::npos; }
    
    FString& operator+=(const std::string& Other) { Value += Other; return *this; }
    FString operator+(const std::string& Other) const { return FString(Value + Other); }
    
    bool operator==(const FString& Other) const { return Value == Other.Value; }
    bool operator!=(const FString& Other) const { return Value != Other.Value; }
    bool operator<(const FString& Other) const { return Value < Other.Value; }
    
    const char* operator*() const { return Value.c_str(); }
};

// Printf-like formatting for FString
template<typename... Args>
FString FStringPrintf(const char* Format, Args... args)
{
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), Format, args...);
    return FString(buffer);
}

#define TEXT(x) x

template<typename T>
struct TArray
{
    std::vector<T> Data;
    
    int32 Num() const { return static_cast<int32>(Data.size()); }
    bool IsEmpty() const { return Data.empty(); }
    void Add(const T& Item) { Data.push_back(Item); }
    void Empty() { Data.clear(); }
    void RemoveAt(int32 Index) { Data.erase(Data.begin() + Index); }
    bool Remove(const T& Item)
    {
        auto It = std::find(Data.begin(), Data.end(), Item);
        if (It != Data.end())
        {
            Data.erase(It);
            return true;
        }
        return false;
    }
    
    T& operator[](int32 Index) { return Data[Index]; }
    const T& operator[](int32 Index) const { return Data[Index]; }
    
    typename std::vector<T>::iterator begin() { return Data.begin(); }
    typename std::vector<T>::iterator end() { return Data.end(); }
    typename std::vector<T>::const_iterator begin() const { return Data.begin(); }
    typename std::vector<T>::const_iterator end() const { return Data.end(); }
    
    template<typename Pred>
    void Sort(Pred pred) { std::sort(Data.begin(), Data.end(), pred); }
};

template<typename K, typename V>
struct TMap
{
    std::map<K, V> Data;
    
    int32 Num() const { return static_cast<int32>(Data.size()); }
    bool Contains(const K& Key) const { return Data.find(Key) != Data.end(); }
    void Add(const K& Key, const V& Value) { Data[Key] = Value; }
    void Empty() { Data.clear(); }
    bool Remove(const K& Key)
    {
        auto It = Data.find(Key);
        if (It != Data.end())
        {
            Data.erase(It);
            return true;
        }
        return false;
    }
    
    V* Find(const K& Key)
    {
        auto It = Data.find(Key);
        return It != Data.end() ? &It->second : nullptr;
    }
    
    const V* Find(const K& Key) const
    {
        auto It = Data.find(Key);
        return It != Data.end() ? &It->second : nullptr;
    }
    
    void GetKeys(TArray<K>& OutKeys) const
    {
        OutKeys.Empty();
        for (const auto& Pair : Data)
        {
            OutKeys.Add(Pair.first);
        }
    }
    
    typename std::map<K, V>::iterator begin() { return Data.begin(); }
    typename std::map<K, V>::iterator end() { return Data.end(); }
    typename std::map<K, V>::const_iterator begin() const { return Data.begin(); }
    typename std::map<K, V>::const_iterator end() const { return Data.end(); }
};

// Transport direction enumeration
enum class ETransportDirection : uint8_t
{
    Here,
    In,
    Out,
    InAll,
    Dissolve
};

// Rule execution mode
enum class ERuleExecutionMode : uint8_t
{
    Deterministic,
    MaximalParallel,
    Sequential,
    Probabilistic
};

// Symbol representation
struct FMembraneSymbol
{
    FName SymbolName;
    int32 Multiplicity = 1;
    TMap<FString, FString> Payload;
    float CreationTime = 0.0f;
    bool bIsCatalyst = false;
    
    FMembraneSymbol() = default;
    FMembraneSymbol(FName InName, int32 InMultiplicity = 1)
        : SymbolName(InName), Multiplicity(InMultiplicity) {}
    
    bool operator==(const FMembraneSymbol& Other) const
    {
        return SymbolName == Other.SymbolName;
    }
    
    bool IsValid() const { return !SymbolName.IsNone() && Multiplicity > 0; }
};

// Symbol multiset
struct FSymbolMultiset
{
    TArray<FMembraneSymbol> Symbols;
    
    void AddSymbol(const FMembraneSymbol& Symbol)
    {
        if (!Symbol.IsValid()) return;
        
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
    
    bool RemoveSymbol(const FMembraneSymbol& Symbol)
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
    
    bool Contains(const FMembraneSymbol& Symbol) const
    {
        for (const FMembraneSymbol& Existing : Symbols)
        {
            if (Existing.SymbolName == Symbol.SymbolName && 
                Existing.Multiplicity >= Symbol.Multiplicity)
            {
                return true;
            }
        }
        return false;
    }
    
    bool ContainsAll(const FSymbolMultiset& Pattern) const
    {
        for (const FMembraneSymbol& Required : Pattern.Symbols)
        {
            if (!Contains(Required)) return false;
        }
        return true;
    }
    
    int32 GetTotalCount() const
    {
        int32 Total = 0;
        for (const FMembraneSymbol& Symbol : Symbols)
        {
            Total += Symbol.Multiplicity;
        }
        return Total;
    }
    
    int32 GetSymbolCount(FName SymbolName) const
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
    
    void Clear() { Symbols.Empty(); }
    bool IsEmpty() const { return Symbols.Num() == 0 || GetTotalCount() == 0; }
};

// Transport target
struct FTransportTarget
{
    ETransportDirection Direction = ETransportDirection::Here;
    FName TargetMembraneId;
    FName TargetLabel;
    
    FTransportTarget() = default;
    FTransportTarget(ETransportDirection InDirection, FName InTarget = FName())
        : Direction(InDirection), TargetMembraneId(InTarget) {}
};

// Transport rule
struct FTransportRule
{
    FName RuleId;
    FSymbolMultiset LeftHandSide;
    FSymbolMultiset RightHandSide;
    FTransportTarget Target;
    int32 Priority = 0;
    float ProbabilityWeight = 1.0f;
    bool bEnabled = true;
    FSymbolMultiset Catalysts;
    FSymbolMultiset Inhibitors;
    FSymbolMultiset Promoters;
    FString Description;
    
    bool IsValid() const
    {
        return bEnabled && (!LeftHandSide.IsEmpty() || !RightHandSide.IsEmpty());
    }
    
    FString ToString() const
    {
        std::string LHS, RHS;
        for (const FMembraneSymbol& Sym : LeftHandSide.Symbols)
        {
            if (!LHS.empty()) LHS += " ";
            char buf[64];
            snprintf(buf, sizeof(buf), "%s^%d", Sym.SymbolName.Value.c_str(), Sym.Multiplicity);
            LHS += buf;
        }
        if (LHS.empty()) LHS = "ε";
        
        for (const FMembraneSymbol& Sym : RightHandSide.Symbols)
        {
            if (!RHS.empty()) RHS += " ";
            char buf[64];
            snprintf(buf, sizeof(buf), "%s^%d", Sym.SymbolName.Value.c_str(), Sym.Multiplicity);
            RHS += buf;
        }
        if (RHS.empty()) RHS = "ε";
        
        const char* Dir = "here";
        switch (Target.Direction)
        {
            case ETransportDirection::Here: Dir = "here"; break;
            case ETransportDirection::In: Dir = "in"; break;
            case ETransportDirection::Out: Dir = "out"; break;
            case ETransportDirection::InAll: Dir = "in_all"; break;
            case ETransportDirection::Dissolve: Dir = "δ"; break;
        }
        
        char result[256];
        snprintf(result, sizeof(result), "%s: %s -> %s, %s", 
            RuleId.Value.c_str(), LHS.c_str(), RHS.c_str(), Dir);
        return FString(result);
    }
};

// Membrane compartment
struct FMembraneCompartment
{
    FName MembraneId;
    FName Label;
    FName ParentId;
    TArray<FName> ChildIds;
    FSymbolMultiset Contents;
    TArray<FTransportRule> Rules;
    bool bIsActive = true;
    TMap<FName, bool> Permeability;
    float CreationTime = 0.0f;
    
    FMembraneCompartment() = default;
    FMembraneCompartment(FName InId, FName InLabel = FName())
        : MembraneId(InId), Label(InLabel) {}
    
    bool IsSkin() const { return ParentId.IsNone(); }
    bool HasChildren() const { return ChildIds.Num() > 0; }
};

// Rule application result
struct FRuleApplicationResult
{
    bool bSuccess = false;
    FName RuleId;
    FName SourceMembraneId;
    FName TargetMembraneId;
    FSymbolMultiset ConsumedSymbols;
    FSymbolMultiset ProducedSymbols;
    FString ErrorMessage;
    float Timestamp = 0.0f;
};

// Transport statistics
struct FTransportStatistics
{
    mutable int32 TotalRulesEvaluated = 0;
    int32 RulesApplied = 0;
    mutable int32 RulesBlockedBySymbols = 0;
    mutable int32 RulesBlockedByInhibitors = 0;
    int32 TotalSymbolsTransported = 0;
    int32 InTransports = 0;
    int32 OutTransports = 0;
    int32 HereTransforms = 0;
    int32 ComputationSteps = 0;
    float LastStepTime = 0.0f;
    
    void Reset()
    {
        TotalRulesEvaluated = 0;
        RulesApplied = 0;
        RulesBlockedBySymbols = 0;
        RulesBlockedByInhibitors = 0;
        TotalSymbolsTransported = 0;
        InTransports = 0;
        OutTransports = 0;
        HereTransforms = 0;
        ComputationSteps = 0;
        LastStepTime = 0.0f;
    }
};

/**
 * Mock implementation of ObjectTransportRules for unit testing
 */
class MockObjectTransportRules
{
public:
    MockObjectTransportRules() : bIsInitialized(false), MembraneIdCounter(0), CurrentTime(0.0f) {}
    
    // Initialization
    bool Initialize(FName SkinLabel = FName("Skin"))
    {
        if (bIsInitialized) Reset();
        
        SkinMembraneId = GenerateMembraneId();
        FMembraneCompartment SkinMembrane(SkinMembraneId, SkinLabel);
        SkinMembrane.CreationTime = CurrentTime;
        Membranes.Add(SkinMembraneId, SkinMembrane);
        
        bIsInitialized = true;
        return true;
    }
    
    void Reset()
    {
        Membranes.Empty();
        SkinMembraneId = FName();
        bIsInitialized = false;
        MembraneIdCounter = 0;
        Statistics.Reset();
        CurrentTime = 0.0f;
    }
    
    // Membrane Management
    FName CreateMembrane(FName ParentId, FName Label = FName())
    {
        if (!bIsInitialized) return FName();
        if (!ParentId.IsNone() && !Membranes.Contains(ParentId)) return FName();
        if (GetMembraneDepth(ParentId) >= MaxHierarchyDepth) return FName();
        
        FName NewId = GenerateMembraneId();
        FMembraneCompartment NewMembrane(NewId, Label);
        NewMembrane.ParentId = ParentId;
        NewMembrane.CreationTime = CurrentTime;
        
        Membranes.Add(NewId, NewMembrane);
        
        if (!ParentId.IsNone())
        {
            FMembraneCompartment* Parent = Membranes.Find(ParentId);
            if (Parent) Parent->ChildIds.Add(NewId);
        }
        
        return NewId;
    }
    
    bool RemoveMembrane(FName MembraneId, bool bReleaseContents = true)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return false;
        if (Membrane->IsSkin()) return false;
        
        if (bReleaseContents && !Membrane->ParentId.IsNone())
        {
            FMembraneCompartment* Parent = Membranes.Find(Membrane->ParentId);
            if (Parent)
            {
                for (const FMembraneSymbol& Symbol : Membrane->Contents.Symbols)
                {
                    Parent->Contents.AddSymbol(Symbol);
                }
            }
        }
        
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
        
        if (FMembraneCompartment* Parent = Membranes.Find(Membrane->ParentId))
        {
            Parent->ChildIds.Remove(MembraneId);
        }
        
        Membranes.Remove(MembraneId);
        return true;
    }
    
    const FMembraneCompartment* GetMembrane(FName MembraneId) const
    {
        return Membranes.Find(MembraneId);
    }
    
    TArray<FName> GetAllMembraneIds() const
    {
        TArray<FName> Ids;
        Membranes.GetKeys(Ids);
        return Ids;
    }
    
    TArray<FName> GetChildMembranes(FName MembraneId) const
    {
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        return Membrane ? Membrane->ChildIds : TArray<FName>();
    }
    
    FName GetSkinMembraneId() const { return SkinMembraneId; }
    
    // Symbol Management
    bool AddSymbol(FName MembraneId, const FMembraneSymbol& Symbol)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane || !Symbol.IsValid()) return false;
        Membrane->Contents.AddSymbol(Symbol);
        return true;
    }
    
    bool AddSymbols(FName MembraneId, const FSymbolMultiset& Symbols)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return false;
        for (const FMembraneSymbol& Symbol : Symbols.Symbols)
        {
            Membrane->Contents.AddSymbol(Symbol);
        }
        return true;
    }
    
    bool RemoveSymbol(FName MembraneId, const FMembraneSymbol& Symbol)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return false;
        return Membrane->Contents.RemoveSymbol(Symbol);
    }
    
    FSymbolMultiset GetSymbols(FName MembraneId) const
    {
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        return Membrane ? Membrane->Contents : FSymbolMultiset();
    }
    
    bool MembraneContains(FName MembraneId, const FSymbolMultiset& Symbols) const
    {
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        return Membrane ? Membrane->Contents.ContainsAll(Symbols) : false;
    }
    
    // Rule Management
    bool AddRule(FName MembraneId, const FTransportRule& Rule)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane || !Rule.IsValid()) return false;
        
        for (const FTransportRule& Existing : Membrane->Rules)
        {
            if (Existing.RuleId == Rule.RuleId) return false;
        }
        
        Membrane->Rules.Add(Rule);
        return true;
    }
    
    bool RemoveRule(FName MembraneId, FName RuleId)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return false;
        
        for (int32 i = 0; i < Membrane->Rules.Num(); ++i)
        {
            if (Membrane->Rules[i].RuleId == RuleId)
            {
                Membrane->Rules.RemoveAt(i);
                return true;
            }
        }
        return false;
    }
    
    TArray<FTransportRule> GetRules(FName MembraneId) const
    {
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        return Membrane ? Membrane->Rules : TArray<FTransportRule>();
    }
    
    FTransportRule CreateSimpleRule(
        FName RuleId,
        FName LeftSymbol,
        FName RightSymbol,
        ETransportDirection Direction = ETransportDirection::Here,
        int32 Priority = 0)
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
    
    // Rule Execution
    bool CanApplyRule(FName MembraneId, const FTransportRule& Rule) const
    {
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane || !Rule.IsValid() || !Rule.bEnabled) return false;
        return CheckRuleConditions(*Membrane, Rule);
    }
    
    TArray<FTransportRule> GetApplicableRules(FName MembraneId) const
    {
        TArray<FTransportRule> Applicable;
        const FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return Applicable;
        
        for (const FTransportRule& Rule : Membrane->Rules)
        {
            if (Rule.bEnabled && CheckRuleConditions(*Membrane, Rule))
            {
                Applicable.Add(Rule);
            }
        }
        
        Applicable.Sort([](const FTransportRule& A, const FTransportRule& B) {
            return A.Priority > B.Priority;
        });
        
        return Applicable;
    }
    
    int32 ExecuteStep()
    {
        if (!bIsInitialized) return 0;
        
        int32 TotalApplied = 0;
        TArray<FName> MembraneIds;
        Membranes.GetKeys(MembraneIds);
        
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
        return TotalApplied;
    }
    
    int32 ExecuteSteps(int32 NumSteps)
    {
        int32 TotalSteps = 0;
        for (int32 i = 0; i < NumSteps; ++i)
        {
            int32 Applied = ExecuteStep();
            if (Applied == 0) break;
            TotalSteps++;
        }
        return TotalSteps;
    }
    
    FRuleApplicationResult ApplyRule(FName MembraneId, FName RuleId)
    {
        FRuleApplicationResult Result;
        Result.SourceMembraneId = MembraneId;
        Result.RuleId = RuleId;
        Result.Timestamp = CurrentTime;
        
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane)
        {
            Result.ErrorMessage = FString("Membrane not found");
            return Result;
        }
        
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
            Result.ErrorMessage = FString("Rule not found");
            return Result;
        }
        
        if (!CheckRuleConditions(*Membrane, *RulePtr))
        {
            Result.ErrorMessage = FString("Rule conditions not met");
            return Result;
        }
        
        return ApplyRuleInternal(*Membrane, *RulePtr);
    }
    
    // Transport Operations
    bool TransportIn(FName SourceMembraneId, const FSymbolMultiset& Symbols, FName TargetChildId = FName())
    {
        FMembraneCompartment* Source = Membranes.Find(SourceMembraneId);
        if (!Source) return false;
        if (!Source->Contents.ContainsAll(Symbols)) return false;
        
        FName TargetId = TargetChildId;
        if (TargetId.IsNone())
        {
            if (Source->ChildIds.Num() == 0) return false;
            TargetId = Source->ChildIds[0];
        }
        
        FMembraneCompartment* Target = Membranes.Find(TargetId);
        if (!Target || Target->ParentId != SourceMembraneId) return false;
        
        for (const FMembraneSymbol& Symbol : Symbols.Symbols)
        {
            Source->Contents.RemoveSymbol(Symbol);
            Target->Contents.AddSymbol(Symbol);
            Statistics.TotalSymbolsTransported += Symbol.Multiplicity;
        }
        
        Statistics.InTransports++;
        return true;
    }
    
    bool TransportOut(FName SourceMembraneId, const FSymbolMultiset& Symbols)
    {
        FMembraneCompartment* Source = Membranes.Find(SourceMembraneId);
        if (!Source) return false;
        if (Source->IsSkin()) return false;
        if (!Source->Contents.ContainsAll(Symbols)) return false;
        
        FMembraneCompartment* Parent = Membranes.Find(Source->ParentId);
        if (!Parent) return false;
        
        for (const FMembraneSymbol& Symbol : Symbols.Symbols)
        {
            Source->Contents.RemoveSymbol(Symbol);
            Parent->Contents.AddSymbol(Symbol);
            Statistics.TotalSymbolsTransported += Symbol.Multiplicity;
        }
        
        Statistics.OutTransports++;
        return true;
    }
    
    bool TransformHere(FName MembraneId, const FSymbolMultiset& ConsumedSymbols, const FSymbolMultiset& ProducedSymbols)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return false;
        if (!Membrane->Contents.ContainsAll(ConsumedSymbols)) return false;
        
        for (const FMembraneSymbol& Symbol : ConsumedSymbols.Symbols)
        {
            if (!Symbol.bIsCatalyst)
            {
                Membrane->Contents.RemoveSymbol(Symbol);
            }
        }
        
        for (const FMembraneSymbol& Symbol : ProducedSymbols.Symbols)
        {
            FMembraneSymbol NewSymbol = Symbol;
            NewSymbol.CreationTime = CurrentTime;
            Membrane->Contents.AddSymbol(NewSymbol);
        }
        
        Statistics.HereTransforms++;
        return true;
    }
    
    // Configuration
    ERuleExecutionMode ExecutionMode = ERuleExecutionMode::MaximalParallel;
    int32 MaxHierarchyDepth = 10;
    
    // Statistics
    FTransportStatistics GetStatistics() const { return Statistics; }
    void ResetStatistics() { Statistics.Reset(); }
    
    // Debugging
    FString GetHierarchyString() const
    {
        if (!bIsInitialized) return FString("System not initialized");
        
        std::string Result;
        std::function<void(FName, int32)> PrintMembrane = [&](FName Id, int32 Depth)
        {
            const FMembraneCompartment* Membrane = Membranes.Find(Id);
            if (!Membrane) return;
            
            std::string Indent(Depth * 2, ' ');
            char buf[256];
            snprintf(buf, sizeof(buf), "%s[%s] (%s) - %d symbols, %d rules\n",
                Indent.c_str(), Id.Value.c_str(), Membrane->Label.Value.c_str(),
                Membrane->Contents.GetTotalCount(), Membrane->Rules.Num());
            Result += buf;
            
            for (FName ChildId : Membrane->ChildIds)
            {
                PrintMembrane(ChildId, Depth + 1);
            }
        };
        
        PrintMembrane(SkinMembraneId, 0);
        return FString(Result);
    }
    
    bool ValidateSystem() const
    {
        if (!bIsInitialized) return false;
        if (!Membranes.Contains(SkinMembraneId)) return false;
        
        for (const auto& Pair : Membranes)
        {
            const FMembraneCompartment& Membrane = Pair.second;
            if (!Membrane.IsSkin() && !Membranes.Contains(Membrane.ParentId)) return false;
            for (FName ChildId : Membrane.ChildIds)
            {
                if (!Membranes.Contains(ChildId)) return false;
            }
        }
        
        return true;
    }

private:
    TMap<FName, FMembraneCompartment> Membranes;
    FName SkinMembraneId;
    bool bIsInitialized;
    FTransportStatistics Statistics;
    int32 MembraneIdCounter;
    float CurrentTime;
    std::mt19937 RandomEngine{std::random_device{}()};
    
    FName GenerateMembraneId()
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Membrane_%d", MembraneIdCounter++);
        return FName(buf);
    }
    
    int32 GetMembraneDepth(FName MembraneId) const
    {
        int32 Depth = 0;
        FName CurrentId = MembraneId;
        
        while (!CurrentId.IsNone())
        {
            const FMembraneCompartment* Membrane = Membranes.Find(CurrentId);
            if (!Membrane) break;
            CurrentId = Membrane->ParentId;
            Depth++;
        }
        
        return Depth;
    }
    
    int32 ExecuteMembraneStep(FName MembraneId)
    {
        FMembraneCompartment* Membrane = Membranes.Find(MembraneId);
        if (!Membrane) return 0;
        
        int32 Applied = 0;
        bool bAnyApplied = true;
        
        while (bAnyApplied)
        {
            bAnyApplied = false;
            TArray<FTransportRule> Applicable = GetApplicableRules(MembraneId);
            
            for (const FTransportRule& Rule : Applicable)
            {
                if (CheckRuleConditions(*Membrane, Rule))
                {
                    FRuleApplicationResult Result = ApplyRuleInternal(*Membrane, Rule);
                    if (Result.bSuccess)
                    {
                        Applied++;
                        bAnyApplied = true;
                    }
                }
            }
        }
        
        return Applied;
    }
    
    bool CheckRuleConditions(const FMembraneCompartment& Membrane, const FTransportRule& Rule) const
    {
        Statistics.TotalRulesEvaluated++;
        
        if (!Membrane.Contents.ContainsAll(Rule.LeftHandSide))
        {
            Statistics.RulesBlockedBySymbols++;
            return false;
        }
        
        if (!Membrane.Contents.ContainsAll(Rule.Catalysts))
        {
            Statistics.RulesBlockedBySymbols++;
            return false;
        }
        
        if (!Rule.Promoters.IsEmpty() && !Membrane.Contents.ContainsAll(Rule.Promoters))
        {
            Statistics.RulesBlockedBySymbols++;
            return false;
        }
        
        for (const FMembraneSymbol& Inhibitor : Rule.Inhibitors.Symbols)
        {
            if (Membrane.Contents.Contains(Inhibitor))
            {
                Statistics.RulesBlockedByInhibitors++;
                return false;
            }
        }
        
        if (Rule.Target.Direction == ETransportDirection::In || 
            Rule.Target.Direction == ETransportDirection::InAll)
        {
            if (Membrane.ChildIds.Num() == 0) return false;
        }
        else if (Rule.Target.Direction == ETransportDirection::Out)
        {
            if (Membrane.IsSkin()) return false;
        }
        
        return true;
    }
    
    FName FindTransportTarget(FName SourceId, const FTransportTarget& Target) const
    {
        const FMembraneCompartment* Source = Membranes.Find(SourceId);
        if (!Source) return FName();
        
        switch (Target.Direction)
        {
            case ETransportDirection::Here:
                return SourceId;
            case ETransportDirection::Out:
                return Source->ParentId;
            case ETransportDirection::In:
            case ETransportDirection::InAll:
                if (!Target.TargetMembraneId.IsNone())
                    return Target.TargetMembraneId;
                if (!Target.TargetLabel.IsNone())
                {
                    for (FName ChildId : Source->ChildIds)
                    {
                        const FMembraneCompartment* Child = Membranes.Find(ChildId);
                        if (Child && Child->Label == Target.TargetLabel)
                            return ChildId;
                    }
                }
                return Source->ChildIds.Num() > 0 ? Source->ChildIds[0] : FName();
            case ETransportDirection::Dissolve:
                return Source->ParentId;
            default:
                return FName();
        }
    }
    
    FRuleApplicationResult ApplyRuleInternal(FMembraneCompartment& Membrane, const FTransportRule& Rule)
    {
        FRuleApplicationResult Result;
        Result.RuleId = Rule.RuleId;
        Result.SourceMembraneId = Membrane.MembraneId;
        Result.Timestamp = CurrentTime;
        
        FName TargetId = FindTransportTarget(Membrane.MembraneId, Rule.Target);
        Result.TargetMembraneId = TargetId;
        
        for (const FMembraneSymbol& Symbol : Rule.LeftHandSide.Symbols)
        {
            if (!Symbol.bIsCatalyst)
            {
                Membrane.Contents.RemoveSymbol(Symbol);
                Result.ConsumedSymbols.AddSymbol(Symbol);
            }
        }
        
        FMembraneCompartment* TargetMembrane = Membranes.Find(TargetId);
        if (!TargetMembrane) TargetMembrane = &Membrane;
        
        for (const FMembraneSymbol& Symbol : Rule.RightHandSide.Symbols)
        {
            FMembraneSymbol NewSymbol = Symbol;
            NewSymbol.CreationTime = CurrentTime;
            TargetMembrane->Contents.AddSymbol(NewSymbol);
            Result.ProducedSymbols.AddSymbol(Symbol);
            Statistics.TotalSymbolsTransported += Symbol.Multiplicity;
        }
        
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
                Membrane.bIsActive = false;
                break;
        }
        
        Statistics.RulesApplied++;
        Result.bSuccess = true;
        
        return Result;
    }
};
