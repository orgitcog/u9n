// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "MetaHumanEditorViewportClient.h"

class FMetaHumanPerformanceViewportClient
	: public FMetaHumanEditorViewportClient
{
public:
	FMetaHumanPerformanceViewportClient(FPreviewScene* InPreviewScene, class UMetaHumanPerformance* InPerformance);

	//~Begin FMetaHumanEditorViewportClient interface
	virtual TArray<class UPrimitiveComponent*> GetHiddenComponentsForView(EABImageViewMode InViewMode) const override;
	virtual void UpdateABVisibility(bool bInSetViewpoint = true) override;
	virtual bool ShouldShowCurves(EABImageViewMode InViewMode) const override;
	virtual bool ShouldShowControlVertices(EABImageViewMode InViewMode) const override;
	virtual void FocusViewportOnSelection() override;
	//~End FMetaHumanEditorViewportClient interface

	bool IsControlRigVisible(EABImageViewMode InViewMode) const;
	void ToggleControlRigVisibility(EABImageViewMode InViewMode);

	void SetRigComponent(TAttribute<class USkeletalMeshComponent*> InRigComponent);
	void SetFootageComponent(TAttribute<class UMetaHumanFootageComponent*> InFootageComponent);
	void SetControlRigComponent(TAttribute<class UMetaHumanPerformanceControlRigComponent*> InControlRigComponent);

protected:

	TObjectPtr<class UMetaHumanPerformance> Performance;

private:

	TAttribute<class UMetaHumanPerformanceControlRigComponent*> ControlRigComponent;
	TAttribute<class USkeletalMeshComponent*> RigComponent;
	TAttribute<class UMetaHumanFootageComponent*> FootageComponent;
};