// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"

namespace UE::AnimNext::Editor
{

class SGraphPinModuleEvent : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinModuleEvent) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

private:
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
};

}