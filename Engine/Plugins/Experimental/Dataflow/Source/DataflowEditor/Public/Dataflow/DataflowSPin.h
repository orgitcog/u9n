// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"

/**
* class used to for all dataflow node pins 
*/
class SDataflowPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SDataflowPin)
		: _IsPinInvalid(false)
		, _PinColorOverride(FLinearColor::White)
		, _bIsPinColorOverriden(false)
	{}
		SLATE_ATTRIBUTE(bool, IsPinInvalid)
		SLATE_ATTRIBUTE(FLinearColor, PinColorOverride)
		SLATE_ATTRIBUTE(bool, bIsPinColorOverriden)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

	virtual FSlateColor GetPinColor() const override;

private:
	TAttribute<bool> bIsPinInvalid;
	TAttribute<FLinearColor> PinColorOverride;
	TAttribute<bool> bIsPinColorOverriden;
};

