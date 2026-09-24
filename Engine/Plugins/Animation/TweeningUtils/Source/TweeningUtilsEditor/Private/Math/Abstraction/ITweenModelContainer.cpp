// Copyright Epic Games, Inc. All Rights Reserved.

#include "Math/Abstraction/ITweenModelContainer.h"

#include "Containers/UnrealString.h"
#include "Misc/CoreMiscDefines.h"

namespace UE::TweeningUtilsEditor
{
int32 ITweenModelContainer::IndexOf(const FTweenModel& InTweenModel) const
{
	for (int32 Index = 0; Index < NumModels(); ++Index)
	{
		if (GetModel(Index) == &InTweenModel)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FTweenModel* ITweenModelContainer::FindModelByIdentifier(const FString& InIdentifier)
{
	for (int32 Index = 0; Index < NumModels(); ++Index)
	{
		FTweenModel* Function = GetModel(Index);
		if (Function && GetModelIdentifier(*Function) == InIdentifier)
		{
			return Function;
		}
	}
	return nullptr;
}
}
