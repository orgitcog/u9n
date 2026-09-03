// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Widgets/MediaViewerDelegates.h"

class FStructOnScope;
class IStructureDetailsView;

namespace UE::MediaViewer
{
	class FMediaImageViewer;
	enum class EMediaImageViewerPosition : uint8;
}

namespace UE::MediaViewer::Private
{

struct FMediaViewerDelegates;

class SMediaImageViewerDetails : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SMediaImageViewerDetails, SCompoundWidget)

	SLATE_BEGIN_ARGS(SMediaImageViewerDetails)
		{}
	SLATE_END_ARGS()

public:
	SMediaImageViewerDetails();
	virtual ~SMediaImageViewerDetails() = default;

	void Construct(const FArguments& InArgs, EMediaImageViewerPosition InPosition, const TSharedRef<FMediaViewerDelegates>& InDelegates);

protected:
	EMediaImageViewerPosition Position;
	TSharedPtr<FMediaViewerDelegates> Delegates;

	TSharedPtr<IStructureDetailsView> PanelDetailsView;
	TSharedPtr<IStructureDetailsView> PaintDetailsView;
	TSharedPtr<IStructureDetailsView> CustomDetailsView;

	TSharedRef<SWidget> CreatePanelSettings();

	TSharedRef<SWidget> CreatePaintSettings();

	TSharedPtr<SWidget> CreateCustomSettings();
};

} // UE::MediaViewer::Private
