// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownRCFieldItem.h"

struct FRemoteControlFunction;
class FReply;

class FAvaRundownRCFunctionItem : public FAvaRundownRCFieldItem
{
public:
	static TSharedPtr<FAvaRundownRCFunctionItem> CreateItem(const TSharedRef<SAvaRundownPageRemoteControlProps>& InPropertyPanel
		, const TSharedRef<FRemoteControlFunction>& InFunctionEntity
		, bool bInControlled);

private:
	void Initialize();

	FText GetLabel() const;

	FReply OnFunctionButtonClicked() const;
};
