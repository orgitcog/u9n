// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCalibrationGeneratorOptions.h"

#include "Widgets/SWindow.h"
#include "IDetailsView.h"

#include "CaptureData.h"

class SMetaHumanCalibrationGeneratorWindow : public SWindow
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanCalibrationGeneratorWindow)
		: _CaptureData(nullptr)
		{
		}

		SLATE_ARGUMENT(UFootageCaptureData*, CaptureData)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<TStrongObjectPtr<UMetaHumanCalibrationGeneratorOptions>> ShowModal();

private:

	FString GetDefaultPackagePath();

	bool UserResponse = false;

	TSharedPtr<IDetailsView> DetailsView;
	UFootageCaptureData* CaptureData;
};
