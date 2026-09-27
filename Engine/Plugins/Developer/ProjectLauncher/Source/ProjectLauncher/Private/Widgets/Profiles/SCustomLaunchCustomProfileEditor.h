// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Layout/IScrollableWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/ProjectLauncherModel.h"
#include "Widgets/Views/STableRow.h"
#include "ProfileTree/ILaunchProfileTreeBuilder.h"


class SWidget;
class ITableRow;
class STableViewBase;
template<typename T> class STreeView;

typedef STreeView<ProjectLauncher::FLaunchProfileTreeNodePtr> SLaunchProfileTreeView;
typedef STableRow<ProjectLauncher::FLaunchProfileTreeNodePtr> SLaunchProfileTreeRow;


class SCustomLaunchCustomProfileEditor : public SCompoundWidget, public IScrollableWidget
{
public:
	SLATE_BEGIN_ARGS(SCustomLaunchCustomProfileEditor) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<ProjectLauncher::FModel>& InModel);

	void SetProfile( const ILauncherProfilePtr& Profile );
	ILauncherProfilePtr GetProfile() const { return CurrentProfile; }


	// IScrollableWidget interface
	virtual FVector2D GetScrollDistance() override;
	virtual FVector2D GetScrollDistanceRemaining() override;
	virtual TSharedRef<SWidget> GetScrollWidget() override;

private:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TSharedRef<ITableRow> OnGenerateWidgetForTreeNode( ProjectLauncher::FLaunchProfileTreeNodePtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable );
	void OnGetChildren(ProjectLauncher::FLaunchProfileTreeNodePtr TreeNode, TArray<ProjectLauncher::FLaunchProfileTreeNodePtr>& OutChildren);

	ILauncherProfilePtr CurrentProfile;
	TSharedPtr<ProjectLauncher::FModel> Model;
	TSharedPtr<ProjectLauncher::ILaunchProfileTreeBuilder> TreeBuilder;
	TSharedPtr<SLaunchProfileTreeView> TreeView;

	float SplitterPos = 0.6f;
	

private:

};
