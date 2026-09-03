// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "HAL/Platform.h"
#include "Misc/OptionalFwd.h"

class UObject;
namespace Verse
{
enum class EValueStringFormat;
struct FAllocationContext;
class FNativeString;
struct VObject;

namespace ObjectPrinting
{
struct FHandler
{
	virtual bool TryHandle(UObject* Object, FUtf8StringBuilderBase& Builder, EValueStringFormat Format, uint32 RecursionDepth) = 0;
};

COREUOBJECT_API void RegisterHandler(FHandler* Handler);
COREUOBJECT_API void UnregisterHandler(FHandler* Handler);
} // namespace ObjectPrinting

COREUOBJECT_API void AppendToString(FUtf8StringBuilderBase& Builder, UObject* Object, EValueStringFormat Format, uint32 RecursionDepth);
} // namespace Verse