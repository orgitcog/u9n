// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"

namespace Verse
{

struct FOpResult;

struct FDebuggerVisitor
{
	UE_NONCOPYABLE(FDebuggerVisitor);

	virtual ~FDebuggerVisitor() = default;

	virtual void VisitArray(TFunctionRef<void()> VisitElements) = 0;

	virtual void VisitMap(TFunctionRef<void()> VisitElements) = 0;

	virtual void VisitOption(TFunctionRef<void()> VisitElement) = 0;

	virtual void VisitObject(TFunctionRef<void()> VisitFields) = 0;

	virtual void Visit(VValue Value, FUtf8StringView ElementName) = 0;

	virtual void Visit(const FOpResult& Value, FUtf8StringView ElementName) = 0;

protected:
	FDebuggerVisitor() = default;
};

} // namespace Verse
#endif // WITH_VERSE_VM
