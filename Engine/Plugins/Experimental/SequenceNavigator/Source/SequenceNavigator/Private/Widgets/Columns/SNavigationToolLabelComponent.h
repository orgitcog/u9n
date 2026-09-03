// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNavigationToolLabelItem.h"

struct FInlineEditableTextBlockStyle;

namespace UE::SequenceNavigator
{

class FNavigationToolComponent;
class SNavigationToolTreeRow;

class SNavigationToolLabelComponent : public SNavigationToolLabelItem
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolLabelComponent) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedRef<FNavigationToolComponent>& InItem
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

	//~ Begin SNavigationToolLabelItem
	virtual const FInlineEditableTextBlockStyle* GetTextBlockStyle() const override;
	//~ End SNavigationToolLabelItem
};

} // namespace UE::SequenceNavigator
