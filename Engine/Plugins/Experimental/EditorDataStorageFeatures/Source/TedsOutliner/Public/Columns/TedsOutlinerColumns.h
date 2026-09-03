// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/SharedPointer.h"

#include "TedsOutlinerColumns.generated.h"

class ISceneOutliner;
class UToolMenu;
class SSceneOutliner;

// Column used to store a reference to the Teds Outliner owning a specific row
// Currently only added to widget rows in the Teds Outliner
USTRUCT(meta = (DisplayName = "Owning Table Viewer"))
struct FTedsOutlinerColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	TWeakPtr<ISceneOutliner> Outliner;
};

DECLARE_DELEGATE_TwoParams(FTedsOutlinerContextMenuDelegate, UToolMenu* Menu, SSceneOutliner& SceneOutliner)

/** Column used to allow context menu to be extended for an item in the outliner */
USTRUCT()
struct FTedsOutlinerContextMenuColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	FTedsOutlinerContextMenuDelegate OnCreateContextMenu;
};