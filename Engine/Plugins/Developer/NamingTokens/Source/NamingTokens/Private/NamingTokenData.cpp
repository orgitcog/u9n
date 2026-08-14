// Copyright Epic Games, Inc. All Rights Reserved.

#include "NamingTokenData.h"

FNamingTokenData::FNamingTokenData(const FString& InTokenKey)
{
	TokenKey = InTokenKey;
	DisplayName = FText::FromString(InTokenKey);
}

FNamingTokenData::FNamingTokenData(const FString& InTokenKey, const FText& InTokenDisplayName, const FTokenProcessorDelegateNative& InTokenProcessor)
{
	DisplayName = InTokenDisplayName;
	TokenKey = InTokenKey;
	TokenProcessorNative = InTokenProcessor;
}

FNamingTokenData::FNamingTokenData(const FString& InTokenKey, const FText& InTokenDisplayName, const FText& InTokenDescription,
	const FTokenProcessorDelegateNative& InTokenProcessor): FNamingTokenData(InTokenKey, InTokenDisplayName, InTokenProcessor)
{
	Description = InTokenDescription;
}

bool FNamingTokenData::Equals(const FNamingTokenData& Other, bool bCaseSensitive) const
{
	return TokenKey.Equals(Other.TokenKey, bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
}

bool FNamingTokenData::operator==(const FNamingTokenData& Other) const
{
	return Equals(Other);
}
