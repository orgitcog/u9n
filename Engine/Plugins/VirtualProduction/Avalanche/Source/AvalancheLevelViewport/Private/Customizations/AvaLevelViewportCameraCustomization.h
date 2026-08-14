// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class UToolMenu;

class FAvaLevelViewportCameraCustomization : public TSharedFromThis<FAvaLevelViewportCameraCustomization>
{
public:
	static const TSharedRef<FAvaLevelViewportCameraCustomization>& Get();

	void Register();

	void Unregister();

protected:
	void ExtendLevelViewportToolbar(UToolMenu* InToolMenu);
};