// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PCGEdge.generated.h"

#define UE_API PCG_API

class UPCGNode;
class UPCGPin;

UCLASS(MinimalAPI, ClassGroup = (Procedural))
class UPCGEdge : public UObject
{
	GENERATED_BODY()
public:
	UE_API UPCGEdge(const FObjectInitializer& ObjectInitializer);

	// ~Begin UObject interface
	UE_API virtual void PostLoad() override;
	// ~End UObject interface

	UPROPERTY()
	FName InboundLabel_DEPRECATED = NAME_None;

	UPROPERTY()
	TObjectPtr<UPCGNode> InboundNode_DEPRECATED;

	UPROPERTY()
	FName OutboundLabel_DEPRECATED = NAME_None;

	UPROPERTY()
	TObjectPtr<UPCGNode> OutboundNode_DEPRECATED;

	/** Pin at upstream end of edge. */
	UPROPERTY()
	TObjectPtr<UPCGPin> InputPin;

	/** Pin at downstream end of edge. */
	UPROPERTY()
	TObjectPtr<UPCGPin> OutputPin;

	UE_API bool IsValid() const;
	UE_API UPCGPin* GetOtherPin(const UPCGPin* Pin);
	UE_API const UPCGPin* GetOtherPin(const UPCGPin* Pin) const;
};

#undef UE_API
