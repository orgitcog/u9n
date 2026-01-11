// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonTemplateAssetDefinition.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"
#include "SkeletonTemplateFramework/SkeletonTemplateEditorToolkit.h"
#include "Toolkits/SimpleAssetEditor.h"

#define LOCTEXT_NAMESPACE "SkeletonTemplateFramework"

FText UAssetDefinition_SkeletonTemplate::GetAssetDisplayName() const
{
	return LOCTEXT("SkeletonTemplate", "Skeleton Template");
}

FLinearColor UAssetDefinition_SkeletonTemplate::GetAssetColor() const
{
	return FLinearColor(FColor::Purple);
}

TSoftClassPtr<UObject> UAssetDefinition_SkeletonTemplate::GetAssetClass() const
{
	return USkeletonTemplate::StaticClass();
}

EAssetCommandResult UAssetDefinition_SkeletonTemplate::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	if (OpenArgs.OpenMethod == EAssetOpenMethod::Edit)
	{
		TArray<UObject*> Assets = OpenArgs.LoadObjects<UObject>();
		MakeShared<FSkeletonTemplateEditorToolkit>()->InitEditor(Assets);
	}

	return EAssetCommandResult::Handled;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_SkeletonTemplate::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, LOCTEXT("UAFSubMenu", "Animation Framework")) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
