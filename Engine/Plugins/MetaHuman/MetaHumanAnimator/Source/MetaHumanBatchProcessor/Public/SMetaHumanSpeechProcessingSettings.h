// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

class SMetaHumanSpeechToAnimProcessingSettings
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanSpeechToAnimProcessingSettings) {}
		SLATE_ARGUMENT(UObject*, Settings)
		SLATE_ATTRIBUTE(bool, CanProcessConditional)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModel();

	bool CanProcess() const;
	
	FReply ProcessClicked();
	FReply CancelClicked();

public:
	TObjectPtr<UObject> SettingsObject;
	TAttribute<bool> CanProcessConditional;

private:
	void RequestDestroyWindow();

	TWeakPtr<SWindow> DialogWindow;
	EAppReturnType::Type UserResponse = EAppReturnType::Cancel;
};
