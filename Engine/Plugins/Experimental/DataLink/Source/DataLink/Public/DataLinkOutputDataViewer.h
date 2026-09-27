// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/StructView.h"

struct FDataLinkPin;

struct FDataLinkOutputDataEntry
{
	bool operator==(FName InOutputName) const
	{
		return Name == InOutputName;
	}

	/** Name of the Output Data */
	FName Name;

	FStructView GetDataView(const UScriptStruct* InDesiredStruct) const;

private:
	/** Output Data instantiated */
	mutable FInstancedStruct OutputData;
};

class FDataLinkOutputDataViewer
{
public:
	explicit FDataLinkOutputDataViewer(TConstArrayView<FDataLinkPin> InOutputPins);

	DATALINK_API FStructView Find(FName InOutputName, const UScriptStruct* InDesiredStruct) const;

	int32 Num() const
	{
		return DataEntries.Num();
	}

	template<typename T>
	T& Get(FName InOutputName) const
	{
		return this->Find(InOutputName, T::StaticStruct()).template Get<T>();
	}

private:
	TArray<FDataLinkOutputDataEntry> DataEntries;
};
