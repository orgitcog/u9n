// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

#include "TypedElementVisibilityColumns.generated.h"

/**
 * VisibleInEditor column that signifies whether or not this row's object should be visible in view ports
 */
USTRUCT(meta = (DisplayName = "Visibility"))
struct FVisibleInEditorColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

public:

	bool bIsVisibleInEditor = true;
};