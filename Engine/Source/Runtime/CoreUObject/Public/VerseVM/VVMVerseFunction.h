// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMWriteBarrier.h"

#include "VVMVerseFunction.generated.h"

UENUM(Flags)
enum class EVerseFunctionFlags : uint32
{
	None = 0x00000000u,
	UHTNative = 0x00000001u,
};
ENUM_CLASS_FLAGS(EVerseFunctionFlags)

// A UFunction wrapper for a VerseVM callee (VFunction or VNativeFunction)
UCLASS(MinimalAPI)
class UVerseFunction : public UFunction
{
	GENERATED_BODY()

public:
	COREUOBJECT_API explicit UVerseFunction(const FObjectInitializer& ObjectInitializer);
	COREUOBJECT_API explicit UVerseFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);

	// The alternate name is used between the native function declaration and the expected verse name
	FName AlternateName;

	EVerseFunctionFlags VerseFunctionFlags = EVerseFunctionFlags::None;

	bool IsUHTNative()
	{
		return EnumHasAnyFlags(VerseFunctionFlags, EVerseFunctionFlags::UHTNative);
	}

	static bool IsVerseGeneratedFunction(UField* Field)
	{
		if (UVerseFunction* VerseFunction = Cast<UVerseFunction>(Field))
		{
			return !VerseFunction->IsUHTNative();
		}
		return false;
	}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	Verse::TWriteBarrier<Verse::VValue> Callee;
#endif
};
