// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

namespace UE::MediaViewer::Private
{

/**
 * Style declarations for the image widgets.
 */
class FMediaViewerStyle : public FSlateStyleSet
{
public:
	static const FName StyleName;

	static FMediaViewerStyle& Get();

private:
	FMediaViewerStyle();
	virtual ~FMediaViewerStyle() override;
};

} // UE::MediaViewer::Private
