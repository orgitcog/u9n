// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoveLibrary/MoverBlackboard.h"



void UMoverBlackboard::Invalidate(FName ObjName)
{
	FRWScopeLock Lock(ObjectsMapLock, SLT_Write);
	ObjectsByName.Remove(ObjName);
}

void UMoverBlackboard::Invalidate(EInvalidationReason Reason)
{
	switch (Reason)
	{
		default:
		case EInvalidationReason::FullReset:
		{
			FRWScopeLock Lock(ObjectsMapLock, SLT_Write);
			ObjectsByName.Empty();
		}
		break;

		// TODO: Support other reasons
	}
}

void UMoverBlackboard::BeginDestroy()
{
	InvalidateAll();
	Super::BeginDestroy();
}