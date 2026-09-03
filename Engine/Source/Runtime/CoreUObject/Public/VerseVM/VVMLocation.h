// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

#include "VerseVM/VVMAbstractVisitor.h"

namespace Verse
{
struct FAllocationContext;
struct VUniqueString;

struct FLocation
{
	FLocation() = default;

	explicit FLocation(uint32 Line)
		: Line(Line)
	{
	}

	friend bool operator==(FLocation Left, FLocation Right)
	{
		return Left.Line == Right.Line;
	}

	friend bool operator!=(FLocation Left, FLocation Right)
	{
		return Left.Line != Right.Line;
	}

	friend uint32 GetTypeHash(const FLocation& Location)
	{
		return ::GetTypeHash(Location.Line);
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FLocation& Value)
	{
		Visitor.Visit(Value.Line, TEXT("Line"));
	}

	uint32 Line{};
};

inline FLocation EmptyLocation()
{
	return FLocation{0};
}
} // namespace Verse

#endif
