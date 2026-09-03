// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

/** Interface to the morph target viewer in the skeletal mesh editor */
class IMorphTargetViewer
{
public:
	
	virtual TArray<FName> GetSelectedMorphTargetNames() const = 0;
	virtual TSharedRef<SWidget> AsWidget() = 0;

	virtual ~IMorphTargetViewer() = default;
};