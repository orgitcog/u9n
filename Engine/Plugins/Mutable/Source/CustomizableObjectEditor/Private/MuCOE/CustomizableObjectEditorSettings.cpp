// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorSettings.h"

#include "MuCO/CustomizableObjectSystemPrivate.h"

UCustomizableObjectEditorSettings::UCustomizableObjectEditorSettings()
	: EditorDerivedDataCachePolicy(ECustomizableObjectDDCPolicy::Default)
	, CookDerivedDataCachePolicy(ECustomizableObjectDDCPolicy::Default)
{

}
