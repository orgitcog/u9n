// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderNamingTokensData.h"

TArray<FString>& UTakeRecorderNamingTokensData::FindOrAddTokenKeysForField(const FString& InFieldName)
{
	if (FTakeRecorderNamingTokensFieldMapping* Match = FieldToUndefinedKeys.FindByPredicate([&InFieldName](const FTakeRecorderNamingTokensFieldMapping& Fields)
	{
		return Fields.FieldName == InFieldName;
	}))
	{
		return Match->UndefinedKeys;
	}
	FTakeRecorderNamingTokensFieldMapping Data;
	Data.FieldName = InFieldName;
	return FieldToUndefinedKeys.Add_GetRef(Data).UndefinedKeys;
}

bool UTakeRecorderNamingTokensData::IsTokenKeyUndefined(const FString& InTokenKey) const
{
	for (const FTakeRecorderNamingTokensFieldMapping& TokenFieldMapping : FieldToUndefinedKeys)
	{
		if (TokenFieldMapping.UndefinedKeys.Contains(InTokenKey))
		{
			return true;
		}
	}

	return false;
}
