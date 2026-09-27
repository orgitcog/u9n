// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanVideoBaseLiveLinkSubjectSettings.h"
#include "MetaHumanLocalLiveLinkSubjectMonitorWidget.h"
#include "SMetaHumanImageViewer.h"

#include "Pipeline/PipelineData.h"

#include "Widgets/SCompoundWidget.h"
#include "Engine/TimerHandle.h"



class METAHUMANLOCALLIVELINKSOURCE_API SMetaHumanVideoBaseLiveLinkSubjectMonitorWidget : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanVideoBaseLiveLinkSubjectMonitorWidget) {}
	SLATE_END_ARGS()

	virtual ~SMetaHumanVideoBaseLiveLinkSubjectMonitorWidget();

	void Construct(const FArguments& InArgs, UMetaHumanVideoBaseLiveLinkSubjectSettings* InSettings, TSharedPtr<SMetaHumanLocalLiveLinkSubjectMonitorWidget> InLocalLiveLinkSubjectMonitorWidget);

	//~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override;
	//~End FGCObject interface

private:

	UMetaHumanVideoBaseLiveLinkSubjectSettings* Settings = nullptr;

	void OnUpdate(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);

	TSharedPtr<SMetaHumanLocalLiveLinkSubjectMonitorWidget> LocalLiveLinkSubjectMonitorWidget;

	// 2D Image review window
	TSharedPtr<SMetaHumanImageViewer> ImageViewer;
	FSlateBrush ImageViewerBrush;
	TObjectPtr<UTexture2D> ImageTexture;

	bool bIsDropping = false;
	double DropStart = 0;

	void FillTexture(const UE::MetaHuman::Pipeline::FUEImageDataType& InImage);
	void ClearTexture();

	FTimerHandle EditorTimerHandle;
};
