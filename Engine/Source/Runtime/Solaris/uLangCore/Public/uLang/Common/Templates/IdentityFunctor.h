// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "uLang/Common/Common.h"

// nb: this was copied over from Core until uLang can properly depend on Core.

namespace uLang
{
/**
 * A functor which returns whatever is passed to it.  Mainly used for generic composition.
 */
struct FIdentityFunctor
{
	template <typename T>
	ULANG_FORCEINLINE T&& operator()(T&& Val) const
	{
		return (T&&)Val;
	}
};
}
