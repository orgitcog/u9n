// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMStrongCellRegistry.h"
#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

FStrongCellRegistry::~FStrongCellRegistry()
{
	// All references must be gone by the time we shut down
	V_DIE_UNLESS(Registry.IsEmpty());
}

void FStrongCellRegistry::Visit(Verse::FMarkStackVisitor& Visitor)
{
	UE::TScopeLock Lock(Mutex);
	for (auto It = Registry.CreateIterator(); It; ++It)
	{
		Visitor.VisitNonNull(It->Key, TEXT("Cell"));
	}
}

void FStrongCellRegistry::AddInternal(FAccessContext Context, VCell* Cell)
{
	UE::TScopeLock Lock(Mutex);
	Registry.Add(TWriteBarrier<VCell>(Context, Cell).Get(), 1);
}

void FStrongCellRegistry::RemoveInternal(FSetElementId Id)
{
	UE::TScopeLock Lock(Mutex);
	Registry.Remove(Id);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
