// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDIMediaSourceActions.h"
#include "NDIMediaSource.h"

#define LOCTEXT_NAMESPACE "NDIMediaSourceActions"

bool FNDIMediaSourceActions::CanFilter()
{
	return true;
}

FText FNDIMediaSourceActions::GetName() const
{
	return LOCTEXT("NDIMediaSource_Name", "NDI Media Source");
}

UClass* FNDIMediaSourceActions::GetSupportedClass() const
{
	return UNDIMediaSource::StaticClass();
}

FColor FNDIMediaSourceActions::GetTypeColor() const
{
	// From NDI Brand Guidelines: #6257FF
	return FColor(98, 87, 255);
}

#undef LOCTEXT_NAMESPACE
