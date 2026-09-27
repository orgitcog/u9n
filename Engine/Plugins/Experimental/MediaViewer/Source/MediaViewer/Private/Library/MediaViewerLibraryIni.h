// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "Library/MediaViewerLibraryGroup.h"
#include "Library/MediaViewerLibraryItem.h"
#include "ImageViewer/MediaImageViewer.h"
#include "Misc/Guid.h"
#include "Widgets/MediaViewerSettings.h"

#include "MediaViewerLibraryIni.generated.h"

enum class EMediaImageViewerActivePosition : uint8;

namespace UE::MediaViewer::Private
{
	class FMediaViewerLibrary;
}

USTRUCT()
struct FMediaViewerLibraryItemData
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemType;

	UPROPERTY()
	FMediaViewerLibraryItem Item;
};

USTRUCT()
struct FMediaViewerImageState
{
	GENERATED_BODY()

	UPROPERTY(Config)
	FName ImageType;

	UPROPERTY(Config)
	FString StringValue;

	UPROPERTY(Config)
	FMediaImageViewerPanelSettings PanelSettings;

	UPROPERTY(Config)
	FMediaImagePaintSettings PaintSettings;
};

USTRUCT()
struct FMediaViewerState
{
	GENERATED_BODY()

	UPROPERTY(Config)
	FMediaViewerSettings ViewerSettings;

	UPROPERTY(Config)
	EMediaImageViewerActivePosition ActiveView;

	UPROPERTY(Config)
	TArray<FMediaViewerImageState> Images;
};

UCLASS(Config = EditorPerProjectUserSettings)
class UMediaViewerLibraryIni : public UObject
{
	GENERATED_BODY()

public:
	static UMediaViewerLibraryIni& Get();

	void SaveLibrary(const TSharedRef<UE::MediaViewer::Private::FMediaViewerLibrary>& InLibrary);

	void LoadLibrary(const TSharedRef<UE::MediaViewer::Private::FMediaViewerLibrary>& InLibrary) const;

	bool HasGroup(const FGuid& InGroupId) const;

	bool HasItem(const FGuid& InItemId) const;

	TConstArrayView<FMediaViewerState> GetSavedStates() const;

	void SetSavedStates(const TArray<FMediaViewerState>& InStates);

protected:
	UPROPERTY(Config)
	TArray<FMediaViewerLibraryGroup> Groups;

	UPROPERTY(Config)
	TArray<FMediaViewerLibraryItemData> Items;

	UPROPERTY(Config)
	TArray<FMediaViewerState> SavedStates;
};
