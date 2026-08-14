// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMRestValue.h"

#include "VerseVM/VVMPlaceholder.h"

namespace Verse
{

VValue VRestValue::GetSlow(FAllocationContext Context)
{
	checkSlow(Value.Get().IsRoot());
	Value.Set(Context, VValue::Placeholder(VPlaceholder::New(Context, Value.Get().GetSplitDepth())));
	return Value.Get();
}

} // namespace Verse

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
