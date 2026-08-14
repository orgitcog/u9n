// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeSettings.h"
#include "LandscapeModule.h"
#include "Modules/ModuleManager.h"
#include "LandscapeEditorServices.h"

#if WITH_EDITOR

void ULandscapeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ILandscapeModule& LandscapeModule = FModuleManager::GetModuleChecked<ILandscapeModule>("Landscape");
	const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeSettings, BrushSizeUIMax))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeSettings, BrushSizeClampMax)))
	{
		// If landscape mode is active, refresh the detail panel to apply the changes immediately : 
		LandscapeModule.GetLandscapeEditorServices()->RefreshDetailPanel();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeSettings, bDisplayTargetLayerThumbnails))
	{
		LandscapeModule.GetLandscapeEditorServices()->RegenerateLayerThumbnails();
		LandscapeModule.GetLandscapeEditorServices()->RefreshDetailPanel();
	}
}

void ULandscapeSettings::PreEditUndo()
{
	Super::PreEditUndo();

	check(!DisplayTargetLayerThumbnailsBeforeUndo.IsSet());
	DisplayTargetLayerThumbnailsBeforeUndo = bDisplayTargetLayerThumbnails;
}

void ULandscapeSettings::PostEditUndo()
{
	Super::PostEditUndo();

	if (DisplayTargetLayerThumbnailsBeforeUndo.GetValue() != bDisplayTargetLayerThumbnails)
	{
		ILandscapeModule& LandscapeModule = FModuleManager::GetModuleChecked<ILandscapeModule>("Landscape");
		LandscapeModule.GetLandscapeEditorServices()->RegenerateLayerThumbnails();
		LandscapeModule.GetLandscapeEditorServices()->RefreshDetailPanel();
	}
	DisplayTargetLayerThumbnailsBeforeUndo.Reset();
}

#endif // WITH_EDITOR