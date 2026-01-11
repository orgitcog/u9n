// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGDataVisualization.h"

#include "SAssetEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class FPCGEditorViewportClient;

class SPCGEditorViewport : public SAssetEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorViewport) {}
	SLATE_END_ARGS()

	virtual ~SPCGEditorViewport();

	void Construct(const FArguments& InArgs);

	//~ Begin ICommonEditorViewportToolbarInfoProvider Interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override {}
	//~ End ICommonEditorViewportToolbarInfoProvider Interface

	/** Rebuilds the scene using the provided resources and setup callback. */
	void SetupScene(const TArray<UObject*>& InResources, const FPCGSetupSceneFunc& SetupFunc);
	void ResetScene();

	/** Add UObject references for GC */
	void AddReferencedObjects(FReferenceCollector& Collector);
protected:
	//~ Begin SAssetEditorViewport Interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;
	//~ End SAssetEditorViewport Interface

	void ReleaseManagedResources();

private:
	TSharedPtr<class FPCGEditorViewportClient> EditorViewportClient = nullptr;

	/** Objects used by the scene. Must be managed for GC. */
	TArray<TObjectPtr<UObject>> ManagedResources;
};
