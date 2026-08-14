// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "StateTreeTest.generated.h"

UCLASS(HideDropdown)
class UStateTreeTestSchema : public UStateTreeSchema
{
	GENERATED_BODY()

	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override
	{
		return true;
	}
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override
	{
		return true;
	}
	virtual bool IsScheduledTickAllowed() const
	{
		return true;
	}
};

UCLASS(HideDropdown)
class UStateTreeTestSchema2 : public UStateTreeSchema
{
	GENERATED_BODY()
};

