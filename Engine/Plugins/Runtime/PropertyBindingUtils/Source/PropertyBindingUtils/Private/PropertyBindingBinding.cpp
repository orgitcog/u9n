// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBindingBinding.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingBindableStructDescriptor.h"

FString UE::PropertyBinding::GetDescriptorAndPathAsString(const FPropertyBindingBindableStructDescriptor& InDescriptor
	, const FPropertyBindingPath& InPath)
{
	FStringBuilderBase Result;

	Result += InDescriptor.ToString();

	if (!InPath.IsPathEmpty())
	{
		Result += TEXT(" ");
		Result += InPath.ToString();
	}

	return Result.ToString();
}

FString FPropertyBindingBinding::ToString() const
{
	FStringBuilderBase Result;
	Result += SourcePropertyPath.ToString();
	Result += TEXT(" --> ");
	Result += TargetPropertyPath.ToString();

	return *Result;
}
