// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Async/TransactionallySafeMutex.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "VVMCell.h"
#include "VVMGlobalHeapRoot.h"

namespace Verse
{

// A global registry that prevents cells from getting GC'ed
struct FStrongCellRegistry : FGlobalHeapRoot
{
	COREUOBJECT_API ~FStrongCellRegistry();

	// Adds a VCell reference to the set and returns reference count for the VCell after adding
	uint32 Add(FAccessContext Context, VCell* Cell)
	{
		V_DIE_UNLESS(Cell);

		// We are the mutator thread so we can read the map without locking
		uint32* ExistingRefCounter = Registry.Find(Cell);
		if (ExistingRefCounter)
		{
			// We can also mutate the counter without lock because GC does not look at that
			return ++*ExistingRefCounter;
		}

		AddInternal(Context, Cell);
		return 1;
	}

	// Removes a VCell reference from the set and returns reference count for the VCell after removing
	uint32 Remove(VCell* Cell)
	{
		// We are the mutator thread so we can read the map without locking
		FSetElementId ExistingId = Registry.FindId(Cell);
		V_DIE_UNLESS(ExistingId.IsValidId());
		TPair<VCell*, uint32>& Entry = Registry.Get(ExistingId);
		// We can also mutate the counter without lock because GC does not look at that
		uint32 NewRefCount = --Entry.Value;
		if (NewRefCount == 0)
		{
			RemoveInternal(ExistingId);
		}
		return NewRefCount;
	}

private:
	COREUOBJECT_API void Visit(Verse::FMarkStackVisitor& Visitor) override;

	COREUOBJECT_API void AddInternal(FAccessContext Context, VCell* Cell);
	COREUOBJECT_API void RemoveInternal(FSetElementId Id);

	UE::FTransactionallySafeMutex Mutex;
	TMap<VCell*, uint32> Registry;
};

} // namespace Verse
#endif // WITH_VERSE_VM
