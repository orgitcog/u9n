// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMContext.h"
#include <atomic>

namespace Verse
{

struct VCppClassInfo;
struct VEmergentType;

struct FGlobalTrivialEmergentTypePtr
{
	FGlobalTrivialEmergentTypePtr() = default;

protected:
	VEmergentType& Get(FAllocationContext Context, VCppClassInfo* ClassInfo, bool bWithShape)
	{
		VEmergentType* Result = EmergentType.load(std::memory_order_relaxed);
		std::atomic_signal_fence(std::memory_order_seq_cst);
		if (Result)
		{
			return *Result;
		}
		else
		{
			return GetSlow(Context, ClassInfo, bWithShape);
		}
	}

	COREUOBJECT_API VEmergentType& GetSlow(FAllocationContext Context, VCppClassInfo* ClassInfo, bool bWithShape);

	std::atomic<VEmergentType*> EmergentType = nullptr;
};

template <VCppClassInfo* ClassInfo>
struct TGlobalTrivialEmergentTypePtr : public FGlobalTrivialEmergentTypePtr
{
	TGlobalTrivialEmergentTypePtr() = default;

	VEmergentType& Get(FAllocationContext Context, bool bWithShape = false)
	{
		return FGlobalTrivialEmergentTypePtr::Get(Context, ClassInfo, bWithShape);
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
