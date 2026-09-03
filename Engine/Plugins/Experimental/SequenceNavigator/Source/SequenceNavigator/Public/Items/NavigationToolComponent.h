// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolBinding.h"

class UActorComponent;

namespace UE::SequenceNavigator
{

/**
 * Navigation Tool Item representing an Actor Component binding
 */
class SEQUENCENAVIGATOR_API FNavigationToolComponent
	: public FNavigationToolBinding
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolComponent
		, FNavigationToolBinding);

	FNavigationToolComponent(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, const TSharedPtr<FNavigationToolSequence>& InParentSequenceItem
		, const FMovieSceneBinding& InBinding);

	//~ Begin INavigationToolItem

	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;
	virtual void GetItemProxies(TArray<TSharedPtr<FNavigationToolItemProxy>>& OutItemProxies) override;

	virtual bool IsAllowedInTool() const override;
	virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;
	virtual bool CanReceiveParentVisibilityPropagation() const override { return false; }
	virtual TSharedRef<SWidget> GenerateLabelWidget(const TSharedRef<SNavigationToolTreeRow>& InRow) override;

	virtual FLinearColor GetItemTintColor() const override;
	virtual TArray<FName> GetTags() const override;

	virtual bool ShowVisibility() const override { return true; }
	virtual bool GetVisibility() const override;
	virtual void OnVisibilityChanged(const bool bInNewVisibility) override;

	//~ End INavigationToolItem

	//~ Begin IRenameableExtension
	virtual bool CanRename() const override;
	virtual bool Rename(const FString& InName) override;
	//~ End IRenameableExtension

	UActorComponent* GetComponent() const;
};

} // namespace UE::SequenceNavigator
