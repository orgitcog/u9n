// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "UObject/NameTypes.h"

namespace UE::SequenceNavigator
{

class FNavigationToolItemTypeId
{
public:
	static FNavigationToolItemTypeId Invalid()
	{
		return FNavigationToolItemTypeId(NAME_None);
	}

	explicit FNavigationToolItemTypeId(FName InTypeName)
		: Name(InTypeName)
	{}

	friend uint32 GetTypeHash(FNavigationToolItemTypeId InTypeId)
	{
		return GetTypeHash(InTypeId.Name);
	}

	bool operator==(FNavigationToolItemTypeId InOther) const
	{
		return this->Name == InOther.Name;
	}

	bool IsValid() const
	{
		return !Name.IsNone();
	}

	FName ToName() const
	{
		return Name;
	}

	FString ToString() const
	{
		return Name.ToString();
	}

private:
	FName Name;
};

} // namespace UE::SequenceNavigator
