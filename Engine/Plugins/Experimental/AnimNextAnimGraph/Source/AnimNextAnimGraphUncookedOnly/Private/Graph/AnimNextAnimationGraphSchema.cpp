// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextAnimationGraphSchema.h"
#include "AnimGraphUncookedOnlyUtils.h"

bool UAnimNextAnimationGraphSchema::CanSetNodeTitle(URigVMController* InController, const URigVMNode* InNode) const
{
	if (InNode != nullptr)
	{
		if (UE::AnimNext::UncookedOnly::FAnimGraphUtils::IsTraitStackNode(InNode))
		{
			return true;
		}
	}
	return false;
}

bool UAnimNextAnimationGraphSchema::CanUnfoldPin(URigVMController* InController, const URigVMPin* InPinToUnfold) const
{
	if (Super::CanUnfoldPin(InController, InPinToUnfold))
	{
		return true;
	}

	if (InPinToUnfold->IsArray())
	{
		// Required in order to be able to set pin default values to arrays that exist as hidden pins at trait shared data
		// URigVMController::SetPinDefaultValue only allows setting default values to arrays if can be unfolded
		if (InPinToUnfold->IsTraitPin() && InPinToUnfold->GetDirection() == ERigVMPinDirection::Hidden)
		{
			return true;
		}
	}
	return false;
}