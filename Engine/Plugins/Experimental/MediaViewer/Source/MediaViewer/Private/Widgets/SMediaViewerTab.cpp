// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaViewerTab.h"

#include "ImageViewers/ColorImageViewer.h"
#include "ImageViewers/NullImageViewer.h"
#include "Library/MediaViewerLibrary.h"
#include "Widgets/SMediaViewer.h"
#include "Widgets/SMediaViewerLibrary.h"

#define LOCTEXT_NAMESPACE "MediaViewerTab"

namespace UE::MediaViewer
{

SMediaViewerTab::SMediaViewerTab()
{
}

void SMediaViewerTab::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SMediaViewerTab::Construct(const FArguments& InArgs, const FMediaViewerArgs& InMediaViewerArgs)
{
	TSharedPtr<FMediaImageViewer> ImageViewerLeft = InArgs._ImageViewerLeft;
	TSharedPtr<FMediaImageViewer> ImageViewerRight = InArgs._ImageViewerRight;

	if (!ImageViewerLeft.IsValid())
	{
		ImageViewerLeft = Private::FNullImageViewer::GetNullImageViewer();
	}

	if (!ImageViewerRight.IsValid())
	{
		ImageViewerRight = Private::FNullImageViewer::GetNullImageViewer();
	}

	ChildSlot
	[
		SAssignNew(Viewer, Private::SMediaViewer, SharedThis(this), InMediaViewerArgs, ImageViewerLeft.ToSharedRef(), ImageViewerRight.ToSharedRef())
	];
}

const FMediaViewerArgs& SMediaViewerTab::GetArgs() const
{
	return Viewer->GetArgs();
}

TSharedRef<IMediaViewerLibrary> SMediaViewerTab::GetLibrary() const
{
	return Viewer->GetLibrary();
}

TSharedPtr<FMediaImageViewer> SMediaViewerTab::GetImageViewer(EMediaImageViewerPosition InPosition) const
{
	return Viewer->GetImageViewer(InPosition);
}

void SMediaViewerTab::SetImageViewer(EMediaImageViewerPosition InPosition, TSharedPtr<FMediaImageViewer> InImageViewer)
{
	if (!InImageViewer.IsValid())
	{
		InImageViewer = Private::FNullImageViewer::GetNullImageViewer();
	}

	Viewer->SetImageViewer(InPosition, InImageViewer.ToSharedRef());
}

TSharedRef<Private::SMediaViewer> SMediaViewerTab::GetViewer() const
{
	return Viewer.ToSharedRef();
}

} // UE::MediaViewer

#undef LOCTEXT_NAMESPACE
