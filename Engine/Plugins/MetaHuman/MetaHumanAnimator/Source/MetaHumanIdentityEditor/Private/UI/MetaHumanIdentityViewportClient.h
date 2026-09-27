// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "MetaHumanEditorViewportClient.h"

class FMetaHumanIdentityViewportClient
	: public FMetaHumanEditorViewportClient
{
public:
	FMetaHumanIdentityViewportClient(FPreviewScene* InPreviewScene, class UMetaHumanIdentity* InIdentity);

	//~Begin FMetaHumanEditorViewportClient interface
	virtual TArray<class UPrimitiveComponent*> GetHiddenComponentsForView(EABImageViewMode InViewMode) const override;
	virtual void UpdateABVisibility(bool bInSetViewpoint = true) override;
	virtual void Tick(float InDeltaSeconds) override;
	virtual bool CanToggleShowCurves(EABImageViewMode InViewMode) const override;
	virtual bool CanToggleShowControlVertices(EABImageViewMode InViewMode) const override;
	virtual bool CanChangeViewMode(EABImageViewMode InViewMode) const override;
	virtual bool CanChangeEV100(EABImageViewMode InViewMode) const override;
	virtual UMetaHumanFootageComponent* GetActiveFootageComponent(const TArray<UPrimitiveComponent*>& InAllComponents) const override;
	virtual bool GetSetViewpoint() const override;
	virtual bool ShouldShowCurves(EABImageViewMode InViewMode) const override;
	virtual bool ShouldShowControlVertices(EABImageViewMode InViewMode) const override;
	virtual bool IsFootageVisible(EABImageViewMode InViewMode) const override;
	virtual void FocusViewportOnSelection() override;
	//~End FMetaHumanEditorViewportClient interface

	bool IsCurrentPoseVisible(EABImageViewMode InViewMode) const;
	bool IsTemplateMeshVisible(EABImageViewMode InViewMode) const;

	void ToggleCurrentPoseVisibility(EABImageViewMode InViewMode);
	void ToggleConformalMeshVisibility(EABImageViewMode InViewMode);

	DECLARE_DELEGATE_RetVal(class UMetaHumanIdentityPromotedFrame*, FOnGetSelectedPromotedFrame)
	FOnGetSelectedPromotedFrame OnGetSelectedPromotedFrameDelegate;

protected:

	bool HasSelectedPromotedFrame() const;

	TObjectPtr<class UMetaHumanIdentity> Identity;
};