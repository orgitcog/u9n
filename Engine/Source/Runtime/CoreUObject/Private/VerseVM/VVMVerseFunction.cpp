// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMVerseFunction.h"

UVerseFunction::UVerseFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UVerseFunction::UVerseFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, EFunctionFlags InFunctionFlags, SIZE_T ParamsSize)
	: Super(ObjectInitializer, InSuperFunction, InFunctionFlags, ParamsSize)
{
}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void UVerseFunction::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	UVerseFunction* This = static_cast<UVerseFunction*>(InThis);
	Collector.AddReferencedVerseValue(This->Callee);
}
#endif
