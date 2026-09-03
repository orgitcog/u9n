// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/Blackboard/BlackboardKeyEnums.h"
#include "SmartObjectRuntime.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_SOClaimHandle.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

class UBlackboardComponent;

/**
 * Blackboard key type that holds a SmartObject claim handle 
 */
UCLASS(MinimalAPI, EditInlineNew, meta=(DisplayName="SO Claim Handle"))
class UBlackboardKeyType_SOClaimHandle : public UBlackboardKeyType
{
	GENERATED_BODY()
public:
	UE_API explicit UBlackboardKeyType_SOClaimHandle(const FObjectInitializer& ObjectInitializer);
	
	typedef FSmartObjectClaimHandle FDataType;
	static UE_API const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly)
	FSmartObjectClaimHandle Handle;
	
	static UE_API FSmartObjectClaimHandle GetValue(const UBlackboardKeyType_SOClaimHandle* KeyOb, const uint8* MemoryBlock);
	static UE_API bool SetValue(UBlackboardKeyType_SOClaimHandle* KeyOb, uint8* MemoryBlock, const FSmartObjectClaimHandle& Value);

protected:
	UE_API virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp,
												   const uint8* MemoryBlock,
												   const UBlackboardKeyType* OtherKeyOb,
												   const uint8* OtherMemoryBlock) const override;

	UE_API virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	UE_API virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	UE_API virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const override;
	UE_API virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};

#undef UE_API
