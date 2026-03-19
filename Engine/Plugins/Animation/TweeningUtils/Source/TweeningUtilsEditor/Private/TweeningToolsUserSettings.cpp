// Copyright Epic Games, Inc. All Rights Reserved.

#include "TweeningToolsUserSettings.h"

#include "UObject/UObjectGlobals.h"

UTweeningToolsUserSettings* UTweeningToolsUserSettings::Get()
{
	return GetMutableDefault<UTweeningToolsUserSettings>();
}
