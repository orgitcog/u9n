// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlendProfileStandaloneAssetDefinition.h"
#include "BlendProfileStandalone.h"
#include "BlendProfileStandaloneEditorToolkit.h"

#define LOCTEXT_NAMESPACE "BlendProfileStandalone"

FText UAssetDefinition_BlendProfileStandalone::GetAssetDisplayName() const
{
	return LOCTEXT("BlendProfileStandalone", "Blend Profile");
}

FLinearColor UAssetDefinition_BlendProfileStandalone::GetAssetColor() const
{
	return FLinearColor(FColor::Yellow);
}

TSoftClassPtr<UObject> UAssetDefinition_BlendProfileStandalone::GetAssetClass() const
{
	return UBlendProfileStandalone::StaticClass();
}

EAssetCommandResult UAssetDefinition_BlendProfileStandalone::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	if (OpenArgs.OpenMethod == EAssetOpenMethod::Edit)
	{
		TArray<UObject*> Assets = OpenArgs.LoadObjects<UObject>();
		MakeShared<FBlendProfileStandaloneEditorToolkit>()->InitEditor(Assets);
	}

	return EAssetCommandResult::Handled;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_BlendProfileStandalone::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE