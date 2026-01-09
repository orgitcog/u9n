// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfileTree/LaunchProfileTreeData.h"
#include "ProfileTree/ILaunchProfileTreeBuilder.h"
#include "Extension/LaunchExtension.h"
#include "Styling/ProjectLauncherStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/Visibility.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "DesktopPlatformModule.h"
#include "Misc/ConfigCacheIni.h"


#define LOCTEXT_NAMESPACE "SCustomLaunchCustomProfileEditor"

namespace ProjectLauncher
{
	FLaunchProfileTreeData::FLaunchProfileTreeData(ILauncherProfilePtr InProfile, TSharedRef<ProjectLauncher::FModel> InModel, ILaunchProfileTreeBuilder* InTreeBuilder) 
		: Profile(InProfile)
		, Model(InModel)
		, TreeBuilder(InTreeBuilder)
	{
		check(TreeBuilder);

		if (InProfile.IsValid())
		{
			ExtensionInstances = FLaunchExtension::CreateExtensionInstancesForProfile(Profile.ToSharedRef(), InModel, InTreeBuilder);

			bHasAnyMenuExtensions = false;
			for (TSharedPtr<FLaunchExtensionInstance> ExtensionInstance : ExtensionInstances)
			{
				if (ExtensionInstance->HasCustomExtensionMenu())
				{
					bHasAnyMenuExtensions = true;
				}
			}
		}
	}

	FLaunchProfileTreeNode& FLaunchProfileTreeData::AddHeading( FText InName )
	{
		FLaunchProfileTreeNodePtr TreeNode = MakeShared<FLaunchProfileTreeNode>(this);
		TreeNode->Name = InName;
		Nodes.Add(TreeNode);

		return *TreeNode.Get();
	}

	void FLaunchProfileTreeData::CreateExtensionsUI()
	{
		for (TSharedPtr<FLaunchExtensionInstance> ExtensionInstance : ExtensionInstances)
		{
			ExtensionInstance->CustomizeTree(*this);
		}
	}
	
	void FLaunchProfileTreeData::RequestTreeRefresh()
	{
		bRequestTreeRefresh = true;
	}



	FLaunchProfileTreeNode::FLaunchProfileTreeNode( FLaunchProfileTreeData* InTreeData ) 
		: TreeData(InTreeData)
	{
	}

	FLaunchProfileTreeNode& FLaunchProfileTreeNode::AddWidget( FText InName, FCallbacks&& InWidgetCallbacks, TSharedRef<SWidget> InValueWidget )
	{
		FLaunchProfileTreeNodePtr TreeNode = MakeShared<FLaunchProfileTreeNode>(TreeData);
		TreeNode->Name = InName;
		TreeNode->Widget = InValueWidget;
		TreeNode->Callbacks = InWidgetCallbacks;
		Children.Add(TreeNode);

		TreeData->RequestTreeRefresh();
		return *this;
	}

	FLaunchProfileTreeNode& FLaunchProfileTreeNode::AddWidget( FText InName, TSharedRef<SWidget> InValueWidget )
	{
		return AddWidget( InName, {}, InValueWidget );
	}


	FLaunchProfileTreeNode& FLaunchProfileTreeNode::AddBoolean( FText InName, FBooleanCallbacks&& BooleanCallbacks )
	{
		check(BooleanCallbacks.GetValue);
		check(BooleanCallbacks.SetValue);

		FCallbacks WidgetCallbacks
		{
			.IsVisible = BooleanCallbacks.IsVisible,
			.IsEnabled = BooleanCallbacks.IsEnabled,
		};
		if (BooleanCallbacks.GetDefaultValue != nullptr)
		{
			WidgetCallbacks.IsDefault = [BooleanCallbacks]()
			{
				return BooleanCallbacks.GetValue() == BooleanCallbacks.GetDefaultValue();
			};

			WidgetCallbacks.SetToDefault = [BooleanCallbacks]()
			{
				BooleanCallbacks.SetValue(BooleanCallbacks.GetDefaultValue());
				// nb. OnPropertyChanged is called via SCustomLaunchWidgetTableRow's set to default
			};
		};

		auto SetCheckState = [this, BooleanCallbacks]( ECheckBoxState CheckState )
		{
			BooleanCallbacks.SetValue( (CheckState == ECheckBoxState::Checked) );
			TreeData->TreeBuilder->OnPropertyChanged();
		};

		auto GetCheckState = [BooleanCallbacks]()
		{
			return BooleanCallbacks.GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		return AddWidget( InName,
			MoveTemp(WidgetCallbacks),
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(SetCheckState)
			.IsChecked_Lambda(GetCheckState)
		);
	}

	FLaunchProfileTreeNode& FLaunchProfileTreeNode::AddString( FText InName, FStringCallbacks&& StringCallbacks )
	{
		check(StringCallbacks.GetValue);
		check(StringCallbacks.SetValue);

		FCallbacks WidgetCallbacks
		{
			.IsVisible = StringCallbacks.IsVisible,
			.IsEnabled = StringCallbacks.IsEnabled,
		};
		if (StringCallbacks.GetDefaultValue != nullptr)
		{
			WidgetCallbacks.IsDefault = [StringCallbacks]()
			{
				return StringCallbacks.GetValue() == StringCallbacks.GetDefaultValue();
			};

			WidgetCallbacks.SetToDefault = [StringCallbacks]()
			{
				StringCallbacks.SetValue(StringCallbacks.GetDefaultValue());
				// nb. OnPropertyChanged is called via SCustomLaunchWidgetTableRow's set to default
			};
		};

		auto SetString = [this, StringCallbacks]( const FText& InText, ETextCommit::Type InTextCommit )
		{
			StringCallbacks.SetValue( InText.ToString() );
			TreeData->TreeBuilder->OnPropertyChanged();
		};

		auto GetString = [StringCallbacks]()
		{
			return FText::FromString(StringCallbacks.GetValue());
		};

		return AddWidget( InName,
			MoveTemp(WidgetCallbacks),
			SNew(SMultiLineEditableTextBox)
			.AllowMultiLine(false)
			.AutoWrapText(true)
			.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			.Text_Lambda( GetString )
			.OnTextCommitted_Lambda( SetString )
		);

	}

	FLaunchProfileTreeNode& FLaunchProfileTreeNode::AddDirectoryString( FText InName, FStringCallbacks&& StringCallbacks )
	{
		check(StringCallbacks.GetValue);
		check(StringCallbacks.SetValue);

		FCallbacks WidgetCallbacks
		{
			.IsVisible = StringCallbacks.IsVisible,
			.IsEnabled = StringCallbacks.IsEnabled,
		};
		if (StringCallbacks.GetDefaultValue != nullptr)
		{
			WidgetCallbacks.IsDefault = [StringCallbacks]()
			{
				return StringCallbacks.GetValue() == StringCallbacks.GetDefaultValue();
			};

			WidgetCallbacks.SetToDefault = [StringCallbacks]()
			{
				StringCallbacks.SetValue(StringCallbacks.GetDefaultValue());
				// nb. OnPropertyChanged is called via SCustomLaunchWidgetTableRow's set to default
			};
		};

		auto SetString = [this, StringCallbacks]( const FText& InText, ETextCommit::Type InTextCommit )
		{
			StringCallbacks.SetValue( InText.ToString() );
			TreeData->TreeBuilder->OnPropertyChanged();
		};

		auto GetString = [StringCallbacks]()
		{
			return FText::FromString(StringCallbacks.GetValue());
		};

		auto OnBrowse = [this,StringCallbacks]()
		{
			FString InitialDirectory = StringCallbacks.GetValue();
			if (InitialDirectory.IsEmpty())
			{
				InitialDirectory = this->TreeData->Profile->GetProjectBasePath();
			}
			if (!InitialDirectory.IsEmpty() && FPaths::IsRelative(InitialDirectory))
			{
				InitialDirectory = FPaths::Combine( FPaths::RootDir(), InitialDirectory );
			}

			const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
			FString OutDirectory;
			if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(ParentWindowHandle, LOCTEXT("FolderDialogTitle", "Choose a directory").ToString(), InitialDirectory, OutDirectory))
			{
				if (FPaths::IsUnderDirectory(OutDirectory, FPaths::RootDir()))
				{
					FPaths::MakePathRelativeTo(OutDirectory, *FPaths::RootDir());
				}

				StringCallbacks.SetValue( OutDirectory );
				TreeData->TreeBuilder->OnPropertyChanged();
			}

			return FReply::Handled();
		};


		return AddWidget( InName,
			MoveTemp(WidgetCallbacks),
			SNew(SHorizontalBox)

			// path field
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SNew(SEditableTextBox)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
				.Text_Lambda( GetString )
				.OnTextCommitted_Lambda( SetString )
			]

			// browse button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4, 0, 0, 0))
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FProjectLauncherStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("DirBrowseTip", "Browse for a folder"))
				.OnClicked_Lambda(OnBrowse)
				.ContentPadding(2)
				[
					SNew(SImage)
					.Image(FProjectLauncherStyle::Get().GetBrush("PathPickerButton"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		);
	}


	FLaunchProfileTreeNode& FLaunchProfileTreeNode::AddCommandLineString( FText InName, FStringCallbacks&& StringCallbacks )
	{
		// make sure the extensions system is enabled
		// ...unfortunately this setting is only available in the editor because the ini file is not loaded by UnrealFrontend because its per-project
		bool bEnableProjectLauncherExtensions = false;
		GConfig->GetBool(TEXT("/Script/UnrealEd.EditorExperimentalSettings"), TEXT("bEnableProjectLauncherExtensions"), bEnableProjectLauncherExtensions, GEditorPerProjectIni);

		if (!bEnableProjectLauncherExtensions)
		{
			return AddString(InName, MoveTemp(StringCallbacks));
		}




		check(StringCallbacks.GetValue);
		check(StringCallbacks.SetValue);

		FCallbacks WidgetCallbacks
		{
			.IsVisible = StringCallbacks.IsVisible,
			.IsEnabled = StringCallbacks.IsEnabled,
		};
		if (StringCallbacks.GetDefaultValue != nullptr)
		{
			WidgetCallbacks.IsDefault = [StringCallbacks]()
			{
				return StringCallbacks.GetValue() == StringCallbacks.GetDefaultValue();
			};

			WidgetCallbacks.SetToDefault = [StringCallbacks]()
			{
				StringCallbacks.SetValue(StringCallbacks.GetDefaultValue());
				// nb. OnPropertyChanged is called via SCustomLaunchWidgetTableRow's set to default
			};
		};

		auto SetString = [this, StringCallbacks]( const FText& InText, ETextCommit::Type InTextCommit )
		{
			StringCallbacks.SetValue( InText.ToString() );
			TreeData->TreeBuilder->OnPropertyChanged();
		};

		auto GetString = [StringCallbacks]()
		{
			return FText::FromString(StringCallbacks.GetValue());
		};

		auto OnGetCmdlineParameterMenuContent = [this,StringCallbacks]()
		{
			const bool bShouldCloseWindowAfterMenuSelection = false;
			const bool bCloseSelfOnly = false;
			const bool bSearchable = false;
			FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr, nullptr, bCloseSelfOnly, &FCoreStyle::Get(), bSearchable );

			for (TSharedPtr<FLaunchExtensionInstance> ExtensionInstance : TreeData->ExtensionInstances)
			{
#if 1
				// flat list of extensions - easier to see but risks getting cluttered later on
				MenuBuilder.BeginSection(NAME_None, ExtensionInstance->GetExtension()->GetDisplayName());
				ExtensionInstance->MakeCommandLineSubmenu(MenuBuilder);
				MenuBuilder.EndSection();
#else 
				// extensions in submenus - more scalable, but harder to see everything at a glance
				const bool bInOpenSubMenuOnClick = false;
				MenuBuilder.AddSubMenu(
					ExtensionInstance->GetExtension()->GetDisplayName()),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateSP( ExtensionInstance.ToSharedRef(), &FLaunchExtensionInstance::MakeCommandLineSubmenu),
					bInOpenSubMenuOnClick,
					FSlateIcon(),
					bShouldCloseWindowAfterMenuSelection
				);
#endif
			}

			return MenuBuilder.MakeWidget();
		};

		auto OnGetExtensionsMenuContent = [this,StringCallbacks]()
		{
			const bool bShouldCloseWindowAfterMenuSelection = false;
			const bool bCloseSelfOnly = false;
			const bool bSearchable = false;
			FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr, nullptr, bCloseSelfOnly, &FCoreStyle::Get(), bSearchable );

			for (TSharedPtr<FLaunchExtensionInstance> ExtensionInstance : TreeData->ExtensionInstances)
			{
				if (!ExtensionInstance->HasCustomExtensionMenu())
				{
					continue;
				}

#if 1
				// flat list of extensions - easier to see but risks getting cluttered later on
				MenuBuilder.BeginSection(NAME_None, ExtensionInstance->GetExtension()->GetDisplayName());
				ExtensionInstance->MakeCustomExtensionSubmenu(MenuBuilder);
				MenuBuilder.EndSection();
#else 
				// extensions in submenus - more scalable, but harder to see everything at a glance
				const bool bInOpenSubMenuOnClick = false;
				MenuBuilder.AddSubMenu(
					ExtensionInstance->GetExtension()->GetDisplayName()),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateSP( ExtensionInstance.ToSharedRef(), &FLaunchExtensionInstance::MakeExtensionSubmenu),
					bInOpenSubMenuOnClick,
					FSlateIcon(),
					bShouldCloseWindowAfterMenuSelection
				);
#endif
			}

			return MenuBuilder.MakeWidget();
		};




		return AddWidget( InName,
			MoveTemp(WidgetCallbacks),
			SNew(SHorizontalBox)

			// path field
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SNew(SMultiLineEditableTextBox)
				.AllowMultiLine(false)
				.AutoWrapText(true)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
				.Text_Lambda( GetString )
				.OnTextCommitted_Lambda( SetString )
			]

			// command line parameters button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4, 0, 0, 0))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
				.ToolTipText(LOCTEXT("CmdLineOptionsLabel", "Add a special parameter to command line"))
				.OnGetMenuContent_Lambda(OnGetCmdlineParameterMenuContent)
				.Visibility_Lambda( [this]() { return TreeData->ExtensionInstances.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; } )
				.MenuPlacement(MenuPlacement_BelowRightAnchor)
				.HasDownArrow(false)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.AddCircle"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// command line extension button (@todo: temporary! this will be moved elsewhere)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0, 0, 3, 0))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
				.ToolTipText(LOCTEXT("CmdLineExtensionsLabel", "Show additional command line extension options"))
				.OnGetMenuContent_Lambda(OnGetExtensionsMenuContent)
				.Visibility_Lambda( [this]() { return TreeData->bHasAnyMenuExtensions ? EVisibility::Visible : EVisibility::Collapsed; } )
				.MenuPlacement(MenuPlacement_BelowRightAnchor)
				.HasDownArrow(false)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FProjectLauncherStyle::Get().GetBrush("Icons.EllipsisVerticalNarrow"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		);
	}

}

#undef LOCTEXT_NAMESPACE
