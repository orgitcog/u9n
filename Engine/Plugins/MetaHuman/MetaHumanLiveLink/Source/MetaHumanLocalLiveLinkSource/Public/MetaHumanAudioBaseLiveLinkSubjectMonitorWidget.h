// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanAudioBaseLiveLinkSubjectSettings.h"

#include "Pipeline/PipelineData.h"

#include "Widgets/SCompoundWidget.h"



class METAHUMANLOCALLIVELINKSOURCE_API SMetaHumanAudioBaseLiveLinkSubjectMonitorWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanAudioBaseLiveLinkSubjectMonitorWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMetaHumanAudioBaseLiveLinkSubjectSettings* InSettings);

private:

	UMetaHumanAudioBaseLiveLinkSubjectSettings* Settings = nullptr;

	void OnUpdate(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
};
