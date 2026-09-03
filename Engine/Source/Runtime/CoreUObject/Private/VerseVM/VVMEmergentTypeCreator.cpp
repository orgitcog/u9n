// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEmergentTypeCreator.h"

#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMType.h"
#include "VerseVM/VVMUniqueCreator.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{

TGlobalHeapPtr<VEmergentType> VEmergentTypeCreator::EmergentTypeForEmergentType;
TGlobalHeapPtr<VEmergentType> VEmergentTypeCreator::EmergentTypeForTrivialType;
TLazyInitialized<VUniqueCreator<VEmergentType>> VEmergentTypeCreator::UniqueCreator;
bool VEmergentTypeCreator::bIsInitialized;

VEmergentType* VEmergentTypeCreator::GetOrCreate(FAllocationContext Context, VType* Type, VCppClassInfo* CppClassInfo)
{
	return UniqueCreator->GetOrCreate<VEmergentType>(Context, Type, CppClassInfo);
};

VEmergentType* VEmergentTypeCreator::GetOrCreate(FAllocationContext Context, VShape* InShape, VType* Type, VCppClassInfo* CppClassInfo)
{
	return UniqueCreator->GetOrCreate<VEmergentType>(Context, InShape, Type, CppClassInfo);
};

void VEmergentTypeCreator::Initialize(FAllocationContext Context)
{
	/*
	   Need to setup

	   EmergentTypeForEmergentType : VCell(EmergentTypeForEmergentType), Type(TrivialType)
	   EmergentTypeForTrivialType  : VCell(EmergentTypeForEmergentType), Type(TrivialType)
	   TrivialType                 : VCell(EmergentTypeForTrivialType)
	*/
	if (!bIsInitialized)
	{
		EmergentTypeForEmergentType.Set(Context, VEmergentType::NewIncomplete(Context, &VEmergentType::StaticCppClassInfo));
		EmergentTypeForEmergentType->SetEmergentType(Context, EmergentTypeForEmergentType.Get());

		EmergentTypeForTrivialType.Set(Context, VEmergentType::NewIncomplete(Context, &VTrivialType::StaticCppClassInfo));
		EmergentTypeForTrivialType->SetEmergentType(Context, EmergentTypeForEmergentType.Get());

		VTrivialType::Initialize(Context);

		EmergentTypeForEmergentType->Type.Set(Context, VTrivialType::Singleton.Get());
		EmergentTypeForTrivialType->Type.Set(Context, VTrivialType::Singleton.Get());

		UniqueCreator->Add(Context, EmergentTypeForEmergentType.Get());
		UniqueCreator->Add(Context, EmergentTypeForTrivialType.Get());

		bIsInitialized = true;
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
