// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonBindingAssetDefinition.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"
#include "SkeletonTemplateFramework/SkeletonBindingEditorToolkit.h"

#define LOCTEXT_NAMESPACE "SkeletonTemplateFramework"

FText UAssetDefinition_SkeletonBinding::GetAssetDisplayName() const
{
	return LOCTEXT("SkeletonBinding", "Skeleton Binding");
}

FLinearColor UAssetDefinition_SkeletonBinding::GetAssetColor() const
{
	return FLinearColor(FColor::Purple);
}

TSoftClassPtr<UObject> UAssetDefinition_SkeletonBinding::GetAssetClass() const
{
	return USkeletonBinding::StaticClass();
}

EAssetCommandResult UAssetDefinition_SkeletonBinding::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	if (OpenArgs.OpenMethod == EAssetOpenMethod::Edit)
	{
		const TArray<UObject*> Assets = OpenArgs.LoadObjects<UObject>();
		MakeShared<FSkeletonBindingEditorToolkit>()->InitEditor(Assets);
	}

	return EAssetCommandResult::Handled;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_SkeletonBinding::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, LOCTEXT("UAFSubMenu", "Animation Framework")) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
