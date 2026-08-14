// Copyright Epic Games, Inc. All Rights Reserved.

#include "StalledIterationCounter.h"

namespace UE::Cook
{

	FStalledIterationCounter::FStalledIterationCounter()
		: Value(0)
		, StalledIterationCount(0)
	{}

	void FStalledIterationCounter::Update(int32 NewValue)
	{
		if (Value != NewValue)
		{
			Value = NewValue;
			StalledIterationCount = 0;
		}

		++StalledIterationCount;
	}
}
