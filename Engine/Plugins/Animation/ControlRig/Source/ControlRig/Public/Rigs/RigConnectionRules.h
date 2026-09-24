// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "UObject/StructOnScope.h"
#include "RigConnectionRules.generated.h"

#define UE_API CONTROLRIG_API

struct FRigBaseElement;
struct FRigConnectionRule;
class FRigElementKeyRedirector;
class URigHierarchy;
struct FRigModuleInstance;
struct FRigBaseElement;
struct FRigTransformElement;
struct FRigConnectorElement;

USTRUCT(BlueprintType)
struct FRigConnectionRuleStash
{
	GENERATED_BODY()

	UE_API FRigConnectionRuleStash();
	UE_API FRigConnectionRuleStash(const FRigConnectionRule* InRule);

	UE_API void Save(FArchive& Ar);
	UE_API void Load(FArchive& Ar);
	
	friend uint32 GetTypeHash(const FRigConnectionRuleStash& InRuleStash);

	UE_API bool IsValid() const;
	UE_API UScriptStruct* GetScriptStruct() const;
	UE_API TSharedPtr<FStructOnScope> Get() const;
	UE_API const FRigConnectionRule* Get(TSharedPtr<FStructOnScope>& InOutStorage) const;

	UE_API bool operator == (const FRigConnectionRuleStash& InOther) const;

	bool operator != (const FRigConnectionRuleStash& InOther) const
	{
		return !(*this == InOther);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Rule)
	FString ScriptStructPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Rule)
	FString ExportedText;
};

struct FRigConnectionRuleInput
{
public:
	
	FRigConnectionRuleInput()
	: Hierarchy(nullptr)
	, Module(nullptr)
	, Redirector(nullptr)
	{
	}

	const URigHierarchy* GetHierarchy() const
	{
		return Hierarchy;
	}
	
	const FRigModuleInstance* GetModule() const
	{
		return Module;
	}
	
	const FRigElementKeyRedirector* GetRedirector() const
	{
		return Redirector;
	}

	UE_API const FRigConnectorElement* FindPrimaryConnector(FText* OutErrorMessage = nullptr) const;
   	UE_API TArray<const FRigConnectorElement*> FindSecondaryConnectors(bool bOptional, FText* OutErrorMessage = nullptr) const;

	UE_API const FRigTransformElement* ResolveConnector(const FRigConnectorElement* InConnector, FText* OutErrorMessage) const;
	UE_API const FRigTransformElement* ResolvePrimaryConnector(FText* OutErrorMessage = nullptr) const;

private:

	const URigHierarchy* Hierarchy;
	const FRigModuleInstance* Module;
	const FRigElementKeyRedirector* Redirector;

	friend class UModularRigRuleManager;
};

USTRUCT(meta=(Hidden))
struct FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigConnectionRule() {}
	virtual ~FRigConnectionRule() {}

	virtual UScriptStruct* GetScriptStruct() const { return FRigConnectionRule::StaticStruct(); }
	UE_API virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const;
};

USTRUCT(BlueprintType, DisplayName="And Rule")
struct FRigAndConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigAndConnectionRule()
	{}

	template<typename TypeA, typename TypeB>
	FRigAndConnectionRule(const TypeA& InA, const TypeB& InB)
	{
		ChildRules.Emplace(&InA);
		ChildRules.Emplace(&InB);
	}

	virtual ~FRigAndConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigAndConnectionRule::StaticStruct(); }
	UE_API virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	TArray<FRigConnectionRuleStash> ChildRules;
};

USTRUCT(BlueprintType, DisplayName="Or Rule")
struct FRigOrConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigOrConnectionRule()
	{}

	template<typename TypeA, typename TypeB>
	FRigOrConnectionRule(const TypeA& InA, const TypeB& InB)
	{
		ChildRules.Emplace(&InA);
		ChildRules.Emplace(&InB);
	}

	virtual ~FRigOrConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigOrConnectionRule::StaticStruct(); }
	UE_API virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	TArray<FRigConnectionRuleStash> ChildRules;
};

USTRUCT(BlueprintType, DisplayName="Type Rule")
struct FRigTypeConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigTypeConnectionRule()
		: ElementType(ERigElementType::Socket)
	{}

	FRigTypeConnectionRule(ERigElementType InElementType)
	: ElementType(InElementType)
	{}

	virtual ~FRigTypeConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigTypeConnectionRule::StaticStruct(); }
	UE_API virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	ERigElementType ElementType;
};

USTRUCT(BlueprintType, DisplayName="Tag Rule")
struct FRigTagConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigTagConnectionRule()
		: Tag(NAME_None)
	{}

	FRigTagConnectionRule(const FName& InTag)
	: Tag(InTag)
	{}

	virtual ~FRigTagConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigTagConnectionRule::StaticStruct(); }
	UE_API virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	FName Tag;
};

USTRUCT(BlueprintType, DisplayName="Child of Primary")
struct FRigChildOfPrimaryConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigChildOfPrimaryConnectionRule()
	{}

	virtual ~FRigChildOfPrimaryConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigChildOfPrimaryConnectionRule::StaticStruct(); }
	UE_API virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;
};
/*
USTRUCT(BlueprintType, DisplayName="On Chain Rule")
struct CONTROLRIG_API FRigOnChainRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigOnChainRule()
	: MinNumBones(2)
	, MaxNumBones(0)
	{}

	FRigOnChainRule(int32 InMinNumBones = 2, int32 InMaxNumBones = 0)
	: MinNumBones(InMinNumBones)
	, MaxNumBones(InMaxNumBones)
	{}

	virtual ~FRigOnChainRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigOnChainRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MinNumBones;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MaxNumBones;
};
*/

#undef UE_API
