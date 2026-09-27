// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Library/MediaViewerLibraryGroup.h"

namespace UE::MediaViewer
{

class IMediaViewerLibrary;

/**
 * Contains all data for a Library group which has dynamic elements based on an external source.
*/
struct FMediaViewerLibraryDynamicGroup : public FMediaViewerLibraryGroup
{
	DECLARE_DELEGATE_RetVal(TArray<TSharedRef<FMediaViewerLibraryItem>>, FGenerateItems)

	MEDIAVIEWER_API FMediaViewerLibraryDynamicGroup(const TSharedRef<IMediaViewerLibrary>& InLibrary, const FText& InName,
		const FText& InToolTip, FGenerateItems InItemGenerator);

	MEDIAVIEWER_API FMediaViewerLibraryDynamicGroup(const TSharedRef<IMediaViewerLibrary>& InLibrary, const FGuid& InId,
		const FText& InName, const FText& InToolTip, FGenerateItems InItemGenerator);

	virtual ~FMediaViewerLibraryDynamicGroup() override = default;

	MEDIAVIEWER_API virtual void UpdateItems();

protected:
	TWeakPtr<IMediaViewerLibrary> LibraryWeak;
	FGenerateItems GenerateItemsDelegate;

	TArray<FGuid> GetUpdatedIs(const TArray<FGuid>& InCurrentIds);
};

} // UE::MediaViewer
