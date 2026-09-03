// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldConditionSchema.h"
#include "SmartObjectWorldConditionSchema.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

/**
 * World Condition schema describing the data conditions and context data available for Smart Object conditions.
 */
UCLASS(MinimalAPI)
class USmartObjectWorldConditionSchema : public UWorldConditionSchema
{
	GENERATED_BODY()
public:	
	UE_API explicit USmartObjectWorldConditionSchema(const FObjectInitializer& ObjectInitializer);
	
	/** Context data reference for accessing SmartObject request's UserActor. */
	FWorldConditionContextDataRef GetUserActorRef() const { return UserActorRef; };

	/** Context data reference for accessing SmartObject owner Actor. */
	FWorldConditionContextDataRef GetSmartObjectActorRef() const { return SmartObjectActorRef; };
	
	/** Context data reference for accessing SmartObject handle. */
	FWorldConditionContextDataRef GetSmartObjectHandleRef() const { return SmartObjectHandleRef; }
	
	/** Context data reference for accessing SmartObject Slot handle. */
	FWorldConditionContextDataRef GetSlotHandleRef() const { return SlotHandleRef; }

	/** Context data reference for accessing SmartObject subsystem. */
	FWorldConditionContextDataRef GetSubsystemRef() const { return SubsystemRef; }

protected:

	UE_API virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	
private:

	FWorldConditionContextDataRef UserActorRef;
	FWorldConditionContextDataRef SmartObjectActorRef;
	FWorldConditionContextDataRef SmartObjectHandleRef;
	FWorldConditionContextDataRef SlotHandleRef;
	FWorldConditionContextDataRef SubsystemRef;
};

#undef UE_API
