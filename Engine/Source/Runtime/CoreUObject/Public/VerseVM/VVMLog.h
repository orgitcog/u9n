// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "VVMUnreachable.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogVerseVM, Log, All);
COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogVerseGC, Log, All);

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#define V_DIE(Format, ...)                                      \
	do                                                          \
	{                                                           \
		UE_LOG(LogVerseVM, Fatal, TEXT(Format), ##__VA_ARGS__); \
		VERSE_UNREACHABLE();                                    \
	}                                                           \
	while (false)

#define V_DIE_IF_MSG(Condition, Format, ...) UE_CLOG(Condition, LogVerseVM, Fatal, TEXT(Format), ##__VA_ARGS__)
#define V_DIE_IF(Condition) V_DIE_IF_MSG(Condition, "Unexpected condition: %s", TEXT(#Condition))
#define V_DIE_UNLESS_MSG(Condition, Format, ...) UE_CLOG(!(Condition), LogVerseVM, Fatal, TEXT(Format), ##__VA_ARGS__)
#define V_DIE_UNLESS(Condition) V_DIE_UNLESS_MSG(Condition, "Assertion failed: %s", TEXT(#Condition))
#endif // WITH_VERSE_VM
