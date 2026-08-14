// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Profiles/SCustomLaunchCustomProfileEditor.h"

#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/ProjectLauncherStyle.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Shared/SCustomLaunchDeviceCombo.h"
#include "Widgets/Shared/SCustomLaunchDeviceListView.h"
#include "SPositiveActionButton.h"
#include "DesktopPlatformModule.h"
#include "PlatformInfo.h"
#include "ITargetDeviceProxy.h"
#include "ITargetDeviceProxyManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "InstalledPlatformInfo.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/ConfigCacheIni.h"
#include "GameProjectHelper.h"


#define LOCTEXT_NAMESPACE "SCustomLaunchCustomProfileEditor"

namespace ProjectLauncher
{
	extern TSharedRef<ILaunchProfileTreeBuilder> CreateTreeBuilder( const ILauncherProfilePtr& InProfile, TSharedRef<ProjectLauncher::FModel> InModel );
};



class SLaunchProfileCategoryTreeRow : public SLaunchProfileTreeRow
{
public:
	SLATE_BEGIN_ARGS(SLaunchProfileCategoryTreeRow){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, ProjectLauncher::FLaunchProfileTreeNodePtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable )
	{
		auto HasVisibleChildren = [TreeNode]()
		{
			for (ProjectLauncher::FLaunchProfileTreeNodePtr ChildNode : TreeNode->Children)
			{
				if (ChildNode->Callbacks.IsVisible == nullptr || ChildNode->Callbacks.IsVisible())
				{
					return true;
				}
			}

			return false;
		};


		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Background"))
			.Padding(FMargin(0, 0, 0, 1))
			.Visibility_Lambda( [HasVisibleChildren]() { return HasVisibleChildren() ? EVisibility::Visible : EVisibility::Collapsed; } )
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Header"))				
				.Padding(0)
				[
					SNew(SBox)
					.MinDesiredHeight(26.0f)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(2, 0, 0, 0)
						.AutoWidth()
						[
							SNew(SExpanderArrow, SharedThis(this))
							.StyleSet( &FCoreStyle::Get() )
						]

						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(4, 0, 0, 0)
						.FillWidth(1)
						[
							SNew(STextBlock)
							.Text(TreeNode->Name)
							.Font(FCoreStyle::Get().GetFontStyle("SmallFontBold"))
						]
					]
				]
			]
		];

		SLaunchProfileTreeRow::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(false),
			OwnerTable
		);
	}
};

class SLaunchProfilePropertyTreeRow : public SLaunchProfileTreeRow
{
public:
	DECLARE_DELEGATE_OneParam( FOnSplitterResized, float );


	SLATE_BEGIN_ARGS(SLaunchProfilePropertyTreeRow){}
		SLATE_ATTRIBUTE(float, SplitterValue)
		SLATE_EVENT(FOnSplitterResized, OnSplitterResized)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs, ProjectLauncher::FLaunchProfileTreeNodePtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable )
	{
		SplitterValue = InArgs._SplitterValue;
		OnSplitterResized = InArgs._OnSplitterResized;


		TSharedRef<SWidget> OptionalResetToDefaultWidget = SNullWidget::NullWidget;
		if (TreeNode->Callbacks.IsDefault != nullptr && TreeNode->Callbacks.SetToDefault != nullptr)
		{
			auto IsVisible = [TreeNode]()
			{
				if (TreeNode->Callbacks.IsDefault())
				{
					return false;
				}

				if (TreeNode->Callbacks.IsEnabled && !TreeNode->Callbacks.IsEnabled())
				{
					return false;
				}

				return true;
			};

			auto SetToDefault = [TreeNode]()
			{
				TreeNode->Callbacks.SetToDefault();
				TreeNode->GetTreeData()->TreeBuilder->OnPropertyChanged();
				return FReply::Handled();
			};

			OptionalResetToDefaultWidget = SNew(SButton)
				.ButtonStyle(FProjectLauncherStyle::Get(), "HoverHintOnly")
				.ToolTipText( LOCTEXT("ResetToDefaultToolTip", "Reset this property to its default value.") )
				.Visibility_Lambda( [IsVisible]() { return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed; } )
				.OnClicked_Lambda(SetToDefault)
				.ContentPadding(0)
				[
					SNew(SImage)
					.Image( FProjectLauncherStyle::Get().GetBrush("Icons.DiffersFromDefault") )
					.DesiredSizeOverride(FVector2D(16,16))
					.ColorAndOpacity( FSlateColor::UseForeground() )
				]
			;
		}

		auto MakeSplitterSlot = [this]( TSharedRef<SWidget> SlotContent )
		{
			return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(FMargin(0, 0, 0, 1))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew( SBorder )
					.BorderImage_Lambda( [this]() { return IsHovered() ? FAppStyle::Get().GetBrush("Brushes.Header") : FAppStyle::Get().GetBrush("Brushes.Panel"); } )
					.Padding(0)
					[
						SlotContent
					]
				]
			];
		};


		this->ChildSlot
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(1.0)
			.HitDetectionSplitterHandleSize(5.0)
			.MinimumSlotHeight(26.0f)

			// property name
			+SSplitter::Slot()
			.Value_Lambda( [this]() { return 1.0f - SplitterValue.Get(); } )
			.OnSlotResized_Lambda( [this](float NewPos) { OnSplitterResized.ExecuteIfBound(1.0f - FMath::Clamp(NewPos,0.0f, 1.0f)); } )
			.MinSize(8)
			[
				MakeSplitterSlot(
					SNew(SBox)
					.Padding( FMargin(24, 4, 4, 4))
					.VAlign(VAlign_Top)
					[
						SNew(STextBlock)
						.Text(TreeNode->Name)
						.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
					]
				)
			]

			// property value
			+SSplitter::Slot()
			.Value_Lambda( [this](){ return SplitterValue.Get(); } )
			.OnSlotResized_Lambda( [this](float NewPos) { OnSplitterResized.ExecuteIfBound(FMath::Clamp(NewPos,0.0f, 1.0f)); } )
			.MinSize(32)
			[
				MakeSplitterSlot(
					SNew(SBox)
					.Padding( FMargin(8, 1, 1, 1))
					.VAlign(VAlign_Center)
					[
						TreeNode->Widget.ToSharedRef()
					]
				)
			]

			// reset to default button
			+SSplitter::Slot()
			.MinSize(24)
			.Resizable(false)
			.Value(0)
			[
				MakeSplitterSlot(
					SNew(SBox)
					.Padding(0)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						OptionalResetToDefaultWidget
					]
				)
			]
		];


		SLaunchProfileTreeRow::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(false),
			OwnerTable
		);
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


	FOnSplitterResized OnSplitterResized;
	TAttribute<float> SplitterValue;
};



BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCustomLaunchCustomProfileEditor::Construct(const FArguments& InArgs, const TSharedRef<ProjectLauncher::FModel>& InModel)
{
	Model = InModel;

	TreeBuilder = ProjectLauncher::CreateTreeBuilder( nullptr, InModel );


	ChildSlot
	[
		SAssignNew(TreeView, SLaunchProfileTreeView)
		.TreeItemsSource(&TreeBuilder->GetProfileTree()->Nodes)
		.SelectionMode( ESelectionMode::None )
		.OnGenerateRow( this, &SCustomLaunchCustomProfileEditor::OnGenerateWidgetForTreeNode )
		.OnGetChildren( this, &SCustomLaunchCustomProfileEditor::OnGetChildren)
		.HandleDirectionalNavigation(false)
	];

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<ITableRow> SCustomLaunchCustomProfileEditor::OnGenerateWidgetForTreeNode( ProjectLauncher::FLaunchProfileTreeNodePtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable )
{
	if (TreeNode->Widget.IsValid())
	{
		auto IsVisible = [TreeNode]()
		{
			if (TreeNode->Callbacks.IsVisible == nullptr)
			{
				return true;
			}

			return TreeNode->Callbacks.IsVisible();
		};

		return SNew(SLaunchProfilePropertyTreeRow, TreeNode, OwnerTable)
		.SplitterValue_Lambda( [this]() { return SplitterPos; } )
		.OnSplitterResized_Lambda( [this](float NewPos) { SplitterPos = FMath::Clamp(NewPos, 0.0f, 1.0f); } )
		.Visibility_Lambda( [IsVisible]() { return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed; } )
		.IsEnabled_Lambda( [TreeNode]() { return TreeNode->Callbacks.IsEnabled ? TreeNode->Callbacks.IsEnabled() : true; } )
		;
	}
	else
	{
		return SNew(SLaunchProfileCategoryTreeRow, TreeNode, OwnerTable);
	}
}

void SCustomLaunchCustomProfileEditor::OnGetChildren(ProjectLauncher::FLaunchProfileTreeNodePtr TreeNode, TArray<ProjectLauncher::FLaunchProfileTreeNodePtr>& OutChildren)
{
	OutChildren = TreeNode->Children;
}

FVector2D SCustomLaunchCustomProfileEditor::GetScrollDistance()
{
	if (!TreeView.IsValid() || !TreeBuilder.IsValid())
	{
		return FVector2D::ZeroVector;
	}

	return TreeView->GetScrollDistance();
}

FVector2D SCustomLaunchCustomProfileEditor::GetScrollDistanceRemaining()
{
	if (!TreeView.IsValid() || !TreeBuilder.IsValid())
	{
		return FVector2D::ZeroVector;
	}

	return TreeView->GetScrollDistanceRemaining();
}

TSharedRef<SWidget> SCustomLaunchCustomProfileEditor::GetScrollWidget()
{
	return SharedThis(this);
}


void SCustomLaunchCustomProfileEditor::SetProfile( const ILauncherProfilePtr& Profile )
{
	CurrentProfile = Profile;


	TreeBuilder = ProjectLauncher::CreateTreeBuilder( Profile, Model.ToSharedRef() );

	TreeView->SetTreeItemsSource( &TreeBuilder->GetProfileTree()->Nodes );
	for ( ProjectLauncher::FLaunchProfileTreeNodePtr Node : TreeView->GetRootItems())
	{
		TreeView->SetItemExpansion(Node, true);
	}

	TreeView->RequestTreeRefresh();
}


void SCustomLaunchCustomProfileEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (TreeBuilder.IsValid() && TreeBuilder->GetProfileTree()->bRequestTreeRefresh)
	{
		TreeView->RequestTreeRefresh();
		TreeBuilder->GetProfileTree()->bRequestTreeRefresh = false;
	}
}

#undef LOCTEXT_NAMESPACE
