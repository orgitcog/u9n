// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlateIMRoot.h"

class SImWrapper;
class SWindow;

class FSlateIMWindowRoot : public ISlateIMRoot
{
	SLATE_IM_TYPE_DATA(FSlateIMWindowRoot, ISlateIMRoot)

public:
	FSlateIMWindowRoot(TSharedRef<SWindow> Window);
	virtual ~FSlateIMWindowRoot() override;

	virtual void UpdateChild(TSharedRef<SWidget> Child, const FSlateIMSlotData& AlignmentData) override;
	virtual bool IsVisible() const override;
	virtual FSlateIMInputState& GetInputState() override;

	void UpdateWindow(const FStringView& Title);
	
private:
	TWeakPtr<SWindow> RootWindow;
	TSharedPtr<SImWrapper> WindowRootWidget;

};
