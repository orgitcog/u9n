// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SButton.h"


class STextBlock;

class SImButton : public SButton
{
	SLATE_DECLARE_WIDGET(SImButton, SButton)

public:
	SLATE_BEGIN_ARGS(SImButton) {}
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetText(const FStringView& InText);

private:
	TSharedPtr<STextBlock> TextBox;
};
