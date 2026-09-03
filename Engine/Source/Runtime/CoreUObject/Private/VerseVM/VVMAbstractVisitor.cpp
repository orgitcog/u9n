// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMRestValue.h"

namespace Verse
{
void FAbstractVisitor::VisitNonNull(VCell* InCell, const TCHAR* ElementName)
{
}

void FAbstractVisitor::VisitNonNull(UObject* InObject, const TCHAR* ElementName)
{
}

void FAbstractVisitor::VisitAuxNonNull(void* InAux, const TCHAR* ElementName)
{
}

void FAbstractVisitor::Visit(VCell* InCell, const TCHAR* ElementName)
{
	if (InCell != nullptr)
	{
		VisitNonNull(InCell, ElementName);
	}
}

void FAbstractVisitor::Visit(UObject* InObject, const TCHAR* ElementName)
{
	if (InObject != nullptr)
	{
		VisitNonNull(InObject, ElementName);
	}
}

void FAbstractVisitor::VisitAux(void* InAux, const TCHAR* ElementName)
{
	if (InAux != nullptr)
	{
		VisitAuxNonNull(InAux, ElementName);
	}
}

void FAbstractVisitor::Visit(VValue Value, const TCHAR* ElementName)
{
	if (VCell* Cell = Value.ExtractCell())
	{
		Visit(Cell, ElementName);
	}
	else if (UObject* Object = Value.ExtractUObject())
	{
		Visit(Object, ElementName);
	}
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
