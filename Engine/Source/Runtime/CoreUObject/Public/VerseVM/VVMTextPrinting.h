// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "HAL/Platform.h"

class FUtf8String;

namespace Verse
{
COREUOBJECT_API void AppendVerseToString(FUtf8StringBuilderBase& Builder, UTF8CHAR Char);
COREUOBJECT_API void AppendVerseToString(FUtf8StringBuilderBase& Builder, UTF32CHAR Char);
COREUOBJECT_API void AppendVerseToString(FUtf8StringBuilderBase& Builder, FUtf8StringView String);

COREUOBJECT_API FUtf8String VerseToString(UTF8CHAR Char);
COREUOBJECT_API FUtf8String VerseToString(UTF32CHAR Char);
COREUOBJECT_API FUtf8String VerseToString(FUtf8StringView String);
} // namespace Verse