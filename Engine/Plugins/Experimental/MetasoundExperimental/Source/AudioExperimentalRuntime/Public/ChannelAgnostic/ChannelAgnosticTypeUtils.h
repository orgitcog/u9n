// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"

namespace Audio
{
	// Forward decl.
	class FChannelAgnosticType;
	
	class FCatUtils
	{
	public:
		static AUDIOEXPERIMENTALRUNTIME_API void Interleave(const FChannelAgnosticType& In, TArrayView<float> Out);
	};
}
