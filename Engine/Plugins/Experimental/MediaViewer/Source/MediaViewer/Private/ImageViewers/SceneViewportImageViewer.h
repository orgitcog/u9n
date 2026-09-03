// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImageViewers/ViewportImageViewer.h"

class FSceneViewport;

namespace UE::MediaViewer::Private
{

class FSceneViewportImageViewer : public FViewportImageViewer
{
public:
	FSceneViewportImageViewer(const TSharedPtr<FSceneViewport>& InViewport, const FText& InName);
	FSceneViewportImageViewer(const FGuid& InId, const TSharedPtr<FSceneViewport>& InViewport, const FText& InDisplayName);

protected:
	TWeakPtr<FSceneViewport> ViewportWeak;

	//~ Begin FViewportImageViewer
	virtual FViewport* GetViewport() const override;
	//~ End FViewportImageViewer
};

} // UE::MediaViewer::Private
