// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"

namespace UE::SceneState::Editor
{

class SStateMachineEntryPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SStateMachineEntryPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

private:
	const FSlateBrush* GetEntryPinBorder() const;
};

} // UE::SceneState::Editor
