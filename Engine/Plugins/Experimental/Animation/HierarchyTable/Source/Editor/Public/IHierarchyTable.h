// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IHierarchyTable : public SCompoundWidget
{
public:
	virtual int32 GetSelectedEntryIndex() const = 0;
};