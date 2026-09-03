// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSubjectSettings.h"

#include "Pipeline/PipelineData.h"

#include "Widgets/SCompoundWidget.h"



class METAHUMANLOCALLIVELINKSOURCE_API SMetaHumanLocalLiveLinkSubjectMonitorWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanLocalLiveLinkSubjectMonitorWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMetaHumanLocalLiveLinkSubjectSettings* InSettings);

private:

	UMetaHumanLocalLiveLinkSubjectSettings* Settings = nullptr;

	void OnUpdate(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);

	float FPS = -1;
	int32 FPSCount = 0;
	double FPSStart = 0;
};
