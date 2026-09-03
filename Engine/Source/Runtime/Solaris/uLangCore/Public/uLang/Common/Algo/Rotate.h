// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "uLang/Common/Templates/Storage.h" // for Swap()
#include "uLang/Common/Common.h"

// nb: this was copied over from Core until uLang can properly depend on Core.

namespace uLang
{
namespace AlgoImpl
{
	template <typename T>
	size_t RotateInternal(T* First, size_t Num, size_t Count)
	{
		if (Count == 0)
		{
			return Num;
		}

		if (Count >= Num)
		{
			return 0;
		}

		T* Iter = First;
		T* Mid  = First + Count;
		T* End  = First + Num;

		T* OldMid = Mid;
		for (;;)
		{
			Swap(*Iter++, *Mid++);
			if (Mid == End)
			{
				if (Iter == OldMid)
				{
					return Num - Count;
				}

				Mid = OldMid;
			}
			else if (Iter == OldMid)
			{
				OldMid = Mid;
			}
		}
	}
}

namespace Algo
{
	/**
	 * Rotates a given amount of elements from the front of the range to the end of the range.
	 *
	 * @param  Range  The range to rotate.
	 * @param  Num    The number of elements to rotate from the front of the range.
	 *
	 * @return The new index of the element that was previously at the start of the range.
	 */
	template <typename RangeType>
	ULANG_FORCEINLINE size_t Rotate(RangeType&& Range, size_t Count)
	{
		return AlgoImpl::RotateInternal(ULangGetData(Range), ULangGetNum(Range), Count);
	}
}
}
