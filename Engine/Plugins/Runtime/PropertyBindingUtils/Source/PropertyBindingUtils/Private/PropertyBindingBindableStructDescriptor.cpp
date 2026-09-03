// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBindingBindableStructDescriptor.h"

FPropertyBindingBindableStructDescriptor::~FPropertyBindingBindableStructDescriptor()
{
}

FString FPropertyBindingBindableStructDescriptor::ToString() const
{
	FStringBuilderBase Result;

	Result += TEXT("'");
	Result += Name.ToString();
	Result += TEXT("'");

	return Result.ToString();
}
