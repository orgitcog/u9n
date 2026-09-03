// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"

namespace Verse
{
struct FInstantiationContext
{
	UObject* Outer = nullptr;
	EObjectFlags Flags = RF_NoFlags;
};

struct FInstantiationScope
{
	COREUOBJECT_API static FInstantiationContext Context;
	FInstantiationContext OldContext;

	FInstantiationScope(UObject* Outer, EObjectFlags Flags)
		: OldContext(Context)
	{
		Context = FInstantiationContext{Outer, Flags};
	}

	FInstantiationScope(const FInstantiationScope&) = delete;
	FInstantiationScope& operator=(const FInstantiationScope&) = delete;
	FInstantiationScope(FInstantiationScope&&) = delete;
	FInstantiationScope& operator=(FInstantiationScope&&) = delete;

	~FInstantiationScope()
	{
		Context = OldContext;
	}
};
} // namespace Verse
