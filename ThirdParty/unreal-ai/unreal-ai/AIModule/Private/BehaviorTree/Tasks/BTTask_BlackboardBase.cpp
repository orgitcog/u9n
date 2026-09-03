// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTTask_BlackboardBase)

UBTTask_BlackboardBase::UBTTask_BlackboardBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "BlackboardBase";

	// empty KeySelector = allow everything
}

void UBTTask_BlackboardBase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		BlackboardKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		BlackboardKey.InvalidateResolvedKey();
	}
}

#if WITH_EDITOR
FString UBTTask_BlackboardBase::GetErrorMessage() const
{
	if (GetBlackboardAsset() == nullptr)
	{
		return UE::BehaviorTree::Messages::BlackboardNotSet.ToString();
	}
	return Super::GetErrorMessage();
}
#endif // WITH_EDITOR

