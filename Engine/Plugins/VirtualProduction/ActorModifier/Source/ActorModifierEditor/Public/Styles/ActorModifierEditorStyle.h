// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Styling/SlateStyle.h"

struct FActorModifierCoreMetadata;

class FActorModifierEditorStyle final : public FSlateStyleSet
{
public:
	static FActorModifierEditorStyle& Get()
	{
		static FActorModifierEditorStyle Instance;
		return Instance;
	}

	FActorModifierEditorStyle();
	virtual ~FActorModifierEditorStyle() override;

	ACTORMODIFIEREDITOR_API static const FSlateColor& GetModifierCategoryColor(FName CategoryName);

private:
	void OnModifierClassRegistered(const FActorModifierCoreMetadata& InMetadata);

	static TMap<FName, FSlateColor> ModifierCategoriesColors;
};
