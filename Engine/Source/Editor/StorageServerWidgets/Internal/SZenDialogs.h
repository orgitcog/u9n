// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

class SWidget;

class STORAGESERVERWIDGETS_API SZenStoreStausDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SZenStoreStausDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
};
