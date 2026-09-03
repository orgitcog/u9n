// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanGenerateDepthWindowOptions.h"

#include "Widgets/SWindow.h"

#include "MetaHumanCaptureSource.h"
#include "CaptureData.h"

class SMetaHumanGenerateDepthWindow : public SWindow
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanGenerateDepthWindow)
		: _CaptureData(nullptr)
		{
		}

		SLATE_ARGUMENT(UFootageCaptureData*, CaptureData)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<TStrongObjectPtr<UMetaHumanGenerateDepthWindowOptions>> ShowModal();

private:

	FString GetDefaultPackagePath();
	FDirectoryPath GetDefaultStoragePath();
	FString GetDirectoryName();
	TObjectPtr<UCameraCalibration> GetDefaultCameraCalibration();

	TSharedRef<SWidget> GenerateWarningMessageIfNeeded() const;

	bool UserResponse = false;

	TSharedPtr<IDetailsView> DetailsView;
	UFootageCaptureData* CaptureData;
};
