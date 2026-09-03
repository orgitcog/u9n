// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMUniqueString.h"
#include "Async/UniqueLock.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMUniqueStringInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{
UE::FMutex VStringInternPool::Mutex;

DEFINE_DERIVED_VCPPCLASSINFO(VUniqueString);
DEFINE_TRIVIAL_VISIT_REFERENCES(VUniqueString);
TGlobalTrivialEmergentTypePtr<&VUniqueString::StaticCppClassInfo> VUniqueString::GlobalTrivialEmergentType;

TLazyInitialized<VStringInternPool> VUniqueString::StringPool;

void VUniqueString::SerializeLayout(FAllocationContext Context, VUniqueString*& This, FStructuredArchiveVisitor& Visitor)
{
	FUtf8String String;
	if (!Visitor.IsLoading())
	{
		String = This->AsStringView();
	}

	Visitor.Visit(String, TEXT("Value"));
	if (Visitor.IsLoading())
	{
		This = &VUniqueString::New(Context, String);
	}
}

void VUniqueString::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
}

VUniqueString& VStringInternPool::Intern(FAllocationContext Context, FUtf8StringView String)
{
	UE::TUniqueLock Lock(Mutex);
	if (TWeakBarrier<VUniqueString>* UniqueStringEntry = UniqueStrings.Find(String))
	{
		// If we found an entry, but GC clears the weak reference before we can use it, fall through
		// to add a new entry for the string.
		if (VUniqueString* UniqueString = UniqueStringEntry->Get(Context))
		{
			return *UniqueString;
		}
	}

	VUniqueString& UniqueString = VUniqueString::Make(Context, String);
	UniqueStrings.Add(TWeakBarrier<VUniqueString>(UniqueString));
	return UniqueString;
}

void VStringInternPool::ConductCensus()
{
	UE::TUniqueLock Lock(Mutex);
	for (auto It = UniqueStrings.CreateIterator(); It; ++It)
	{
		// If the cell that the string is allocated in is not marked (i.e. non-live) during GC marking
		// the weak reference will be removed and thus we can remove the map entry from the pool as well.
		if (It->ClearWeakDuringCensus())
		{
			It.RemoveCurrent();
		}
	}
}

bool VUniqueStringSet::Equals(const VUniqueStringSet& Other) const
{
	if (Num() != Other.Num())
	{
		return false;
	}
	for (const TWriteBarrier<VUniqueString>& String : *this)
	{
		if (!Other.IsValidId(Other.FindId(String->AsStringView())))
		{
			return false;
		}
	}
	return true;
}

VUniqueStringSet& VUniqueStringSetInternPool::Intern(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	UE::TUniqueLock Lock(Mutex);
	if (TWeakBarrier<VUniqueStringSet>* UniqueSet = Sets.Find({InSet}))
	{
		// If we found an entry, but GC clears the weak reference before we can use it, fall through
		// to add a new entry for the set.
		if (VUniqueStringSet* CurrentSet = UniqueSet->Get(Context))
		{
			return *CurrentSet;
		}
	}

	VUniqueStringSet& UniqueStringSet = VUniqueStringSet::Make(Context, InSet);
	Sets.Add({UniqueStringSet});
	return UniqueStringSet;
}

void VUniqueStringSetInternPool::ConductCensus()
{
	UE::TUniqueLock Lock(Mutex);
	for (auto It = Sets.CreateIterator(); It; ++It)
	{
		// If the cell that the string is allocated in is not marked (i.e. non-live) during GC marking
		// the weak reference will be removed and thus we can remove the map entry from the pool as well.
		if (It->ClearWeakDuringCensus())
		{
			It.RemoveCurrent();
		}
	}
}

UE::FMutex VUniqueStringSetInternPool::Mutex;

DEFINE_DERIVED_VCPPCLASSINFO(VUniqueStringSet);
TGlobalTrivialEmergentTypePtr<&VUniqueStringSet::StaticCppClassInfo> VUniqueStringSet::GlobalTrivialEmergentType;

TLazyInitialized<VUniqueStringSetInternPool> VUniqueStringSet::Pool;

template <typename TVisitor>
void VUniqueStringSet::VisitReferencesImpl(TVisitor& Visitor)
{
	// We still have to mark each of the strings in the set as being used.
	Visitor.Visit(Strings, TEXT("Strings"));
}

void VUniqueStringSet::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	int Index = 0;
	for (auto& CurrentString : *this)
	{
		if (Index++ != 0)
		{
			Builder.Append(UTF8TEXT(", "));
		}
		Builder.Append(UTF8TEXT("("));
		CurrentString->AppendToString(Builder, Context, Format, RecursionDepth + 1);
		Builder.Append(UTF8TEXT(")"));
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
