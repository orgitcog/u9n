// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Folder.h"

#include "ActorFolderColumns.generated.h"

/**
 * Column that stores a constructed FFolder
 */
USTRUCT(meta = (DisplayName = "FFolder compatibility"))
struct FFolderCompatibilityColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	FFolder Folder;
};

/**
 * Tag that specifies if a folder is expanded in the UI (child items visible)
 */
USTRUCT(meta = (DisplayName = "Is Folder Expanded"))
struct FFolderExpandedTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};