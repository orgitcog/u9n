// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

class ANIMNEXTEDITOR_API FAnimNextGraphPanelPinFactory : public FGraphPanelPinFactory
{
public:
	// FGraphPanelPinFactory interface
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* InPin) const override;
};
