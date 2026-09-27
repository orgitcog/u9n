// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"

namespace GuiToRawControlsUtils
{
	METAHUMANCORETECH_API TMap<FString, float> ConvertGuiToRawControls(const TMap<FString, float>& InGuiControls);
}
