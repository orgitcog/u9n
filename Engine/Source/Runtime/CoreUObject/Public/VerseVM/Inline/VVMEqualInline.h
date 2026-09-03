// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMOption.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
inline bool IsEmptyContainer(VValue Value)
{
	if (VArrayBase* Array = Value.DynamicCast<VArrayBase>())
	{
		return Array->Num() == 0;
	}
	if (VMapBase* Map = Value.DynamicCast<VMapBase>())
	{
		return Map->Num() == 0;
	}
	return false;
}

template <typename HandlePlaceholderFunction>
inline ECompares VValue::Equal(FAllocationContext Context, VValue Left, VValue Right, HandlePlaceholderFunction HandlePlaceholder)
{
	if (Left.IsPlaceholder() || Right.IsPlaceholder())
	{
		HandlePlaceholder(Left, Right);
		return ECompares::Eq;
	}
	if (Left == Right)
	{
		return ECompares::Eq;
	}
	if (Left.IsFloat() && Right.IsFloat())
	{
		return Left.AsFloat() == Right.AsFloat() ? ECompares::Eq : ECompares::Neq;
	}
	if (Left.IsInt())
	{
		return VInt::Eq(Context, Left.AsInt(), Right) ? ECompares::Eq : ECompares::Neq;
	}
	if (Right.IsInt())
	{
		return VInt::Eq(Context, Right.AsInt(), Left) ? ECompares::Eq : ECompares::Neq;
	}
	if (IsEmptyContainer(Left))
	{
		return (Right.IsLogic() && !Right.AsBool()) || IsEmptyContainer(Right) ? ECompares::Eq : ECompares::Neq;
	}
	if (IsEmptyContainer(Right))
	{
		return (Left.IsLogic() && !Left.AsBool()) || IsEmptyContainer(Left) ? ECompares::Eq : ECompares::Neq;
	}
	if (Left.IsLogic() || Right.IsLogic())
	{
		return (Left.IsLogic() && Right.IsLogic() && Left.AsBool() == Right.AsBool()) ? ECompares::Eq : ECompares::Neq;
	}
	if (Left.IsEnumerator() || Right.IsEnumerator())
	{
		checkSlow(Left != Right);
		return ECompares::Neq;
	}
	if (Left.IsCell() && Right.IsCell())
	{
		VCell* LeftCell = &Left.AsCell();
		VCell* RightCell = &Right.AsCell();

		if (LeftCell->IsA<VOption>())
		{
			if (!RightCell->IsA<VOption>())
			{
				return ECompares::Neq;
			}
			return Equal(Context, LeftCell->StaticCast<VOption>().GetValue(), RightCell->StaticCast<VOption>().GetValue(), HandlePlaceholder);
		}
		return LeftCell->Equal(Context, RightCell, HandlePlaceholder);
	}

	return ECompares::Neq;
}

} // namespace Verse
#endif // WITH_VERSE_VM
