// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "VVMNativeString.h"

#include "VVMProfilingLibrary.generated.h"

#define UE_API COREUOBJECT_API

USTRUCT()
struct FProfileLocus
{
	GENERATED_BODY()
public:
	uint64_t BeginRow;
	uint64_t BeginColumn;
	uint64_t EndRow;
	uint64_t EndColumn;

	::Verse::FNativeString SnippetPath;
};

USTRUCT()
struct FSolarisProfilingData
{
	GENERATED_BODY()

public:
	uint64_t WallTimeStart;
	FProfileLocus Locus;
};

DECLARE_MULTICAST_DELEGATE(FVerseBeginProfilingEventHandler);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FVerseEndProfilingEventHandler, const char* /*UserTag*/, double /*TimeInMs*/, const FProfileLocus& /*Locus*/);

class FVerseProfilingDelegates
{
public:
	static UE_API FVerseBeginProfilingEventHandler OnBeginProfilingEvent;
	static UE_API FVerseEndProfilingEventHandler OnEndProfilingEvent;

	static UE_API void RaiseBeginProfilingEvent();
	static UE_API void RaiseEndProfilingEvent(const char* UserTag, double TimeInMs, const FProfileLocus& /*Locus*/);
};

#undef UE_API
