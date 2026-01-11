// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextEventGraphSchema.h"
#include "AnimNextExecuteContext.h"

bool UAnimNextEventGraphSchema::SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const
{
	if(const UScriptStruct* FunctionExecuteContextStruct = InUnitFunction->GetExecuteContextStruct())
	{
		if(FunctionExecuteContextStruct == FAnimNextExecuteContext::StaticStruct())
		{
			// Disallow trait stacks in event graphs
			// TODO: This should probably be using the regular ExecuteContext filtering (i.e. trait stacks should use a different execute context)
			static const UScriptStruct* TraitStackStruct = FindObjectChecked<UScriptStruct>(nullptr, TEXT("/Script/AnimNextAnimGraph.RigUnit_AnimNextTraitStack"));
			if(InUnitFunction->Struct && InUnitFunction->Struct->IsChildOf(TraitStackStruct))
			{
				return false;
			}
		}
	}

	return Super::SupportsUnitFunction(InController, InUnitFunction);
}
