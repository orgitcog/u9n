// Copyright Epic Games, Inc. All Rights Reserved.


#include "RigUnit_OwningObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_OwningObject)

FRigUnit_OwningObject_Execute()
{
	Result = const_cast<UObject*>(ExecuteContext.GetOwningObject());
}
