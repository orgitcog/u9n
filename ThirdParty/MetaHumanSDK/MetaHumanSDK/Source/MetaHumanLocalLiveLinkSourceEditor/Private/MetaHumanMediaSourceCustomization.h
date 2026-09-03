// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"



class FMetaHumanMediaSourceCustomization
{

protected:

	void Setup(IDetailLayoutBuilder& InDetailBuilder, bool bInIsVideo);

	TSharedPtr<class SMetaHumanMediaSourceWidget> MediaSource;
	TSharedPtr<class SEditableTextBox> SubjectName;

	FTextBlockStyle ButtonTextStyle;

	void AddRow(IDetailCategoryBuilder& InCategoryBuilder, FText InText, TSharedPtr<SWidget> InWidget, bool bInIsAdvanced = false, FText InToolTip = FText());
};
