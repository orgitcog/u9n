// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskMaterialReference.h"

FString FAvaMaskMaterialReference::ToString() const
{
	const UObject* const Object = GetObject();
	if (!Object)
	{
		return TEXT("(Invalid Reference)");
	}

	if (Index == INDEX_NONE)
	{
		return Object->GetName();
	}

	return FString::Printf(TEXT("%s [%d]"), *Object->GetName(), Index);
}

UObject* FAvaMaskMaterialReference::GetObject() const
{
	return ObjectWeak.Get();
}
