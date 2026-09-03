// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/StructView.h"

struct FDataLinkInputDataEntry
{
	bool operator==(FName InInputName) const
	{
		return Name == InInputName;
	}

	/** Name of the Input Data */
	FName Name;

	/** View to the Input Data */
	FConstStructView DataView;
};

struct FDataLinkPin;

class FDataLinkInputDataViewer
{
	friend class FDataLinkExecutor;

public:
	explicit FDataLinkInputDataViewer(TConstArrayView<FDataLinkPin> InInputPins);

	DATALINK_API FConstStructView Find(FName InInputName) const;

	int32 Num() const
	{
		return DataEntries.Num();
	}

	template<typename T>
	const T& Get(FName InInputName) const
	{
		return Find(InInputName).Get<const T>();
	}

	TConstArrayView<FDataLinkInputDataEntry> GetDataEntries() const;

private:
	bool HasInvalidDataEntry() const;

	void SetEntryData(const FDataLinkPin& InPin, FConstStructView InInputDataView);

	TArray<FDataLinkInputDataEntry> DataEntries;
};
