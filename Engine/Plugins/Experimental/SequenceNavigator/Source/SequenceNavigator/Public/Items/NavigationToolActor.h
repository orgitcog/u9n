// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolBinding.h"
#include "NavigationToolDefines.h"

class AActor;

namespace UE::SequenceNavigator
{

class INavigationToolView;

/**
 * Navigation Tool Item representing an AActor binding
 */
class SEQUENCENAVIGATOR_API FNavigationToolActor
	: public FNavigationToolBinding
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolActor
		, FNavigationToolBinding);

	FNavigationToolActor(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, const TSharedPtr<FNavigationToolSequence>& InParentSequenceItem
		, const FMovieSceneBinding& InBinding);

	//~ Begin INavigationToolItem

	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;

	virtual bool IsAllowedInTool() const override;
	virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;
	virtual bool CanReceiveParentVisibilityPropagation() const override { return true; }

	virtual TArray<FName> GetTags() const override;

	virtual bool ShowVisibility() const override { return true; }
	virtual bool GetVisibility() const override;
	virtual void OnVisibilityChanged(const bool bInNewVisibility) override;

	//~ End INavigationToolItem

	//~ Begin IRenameableExtension
	virtual bool CanRename() const override;
	virtual bool Rename(const FString& InName) override;
	//~ End IRenameableExtension

	AActor* GetActor() const;
};

} // namespace UE::SequenceNavigator
