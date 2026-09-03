// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanStringUtils.h"

bool MetaHumanStringContainsWhitespace(const FString& InString)
{
	for (const FString::ElementType& Character : InString)
	{
		if (FChar::IsWhitespace(Character))
		{
			return true;
		}
	}
	return false;
}
