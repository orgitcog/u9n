// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Library/MediaViewerLibraryDynamicGroup.h"

namespace UE::MediaViewer::Private
{

/**
 * A group that generates entries based on available UMediaTextures.
*/
struct FMediaTextureGroup : public FMediaViewerLibraryDynamicGroup
{
	FMediaTextureGroup(const TSharedRef<IMediaViewerLibrary>& InLibrary);
	FMediaTextureGroup(const TSharedRef<IMediaViewerLibrary>& InLibrary, const FGuid& InId);

	virtual ~FMediaTextureGroup() override = default;

protected:
	static TArray<TSharedRef<FMediaViewerLibraryItem>> GetMediaTextureItems();
};

} // UE::MediaViewer::Private
