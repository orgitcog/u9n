// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"



class METAHUMANLIVELINKSOURCEEDITOR_API FMetaHumanLiveLinkSourceStyle : public FSlateStyleSet
{
public:

	static FMetaHumanLiveLinkSourceStyle& Get();

	static void Register();
	static void Unregister();

private:

	FMetaHumanLiveLinkSourceStyle();
};