// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanEditorViewportToolBar.h"

#include "EditorViewportCommands.h"
#include "SEditorViewport.h"
#include "SEditorViewportToolBarMenu.h"
#include "SEditorViewportViewMenu.h"
#include "SABImage.h"

#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "ToolMenus.h"
#include "Styling/AppStyle.h"

#include "MetaHumanEditorViewportClient.h"
#include "MetaHumanToolkitCommands.h"

#include "MetaHumanToolkitStyle.h"

#define LOCTEXT_NAMESPACE "MetaHumanIdentityViewportToolbar"

/**
 * Customized version of an SEditorViewportViewMenu that overrides the behaviour of the button so its not tied to
 * the viewport client directly. This is necessary as we have to keep the state for two of these in the Identity asset editor toolbar
 */
class SMetaHumanViewportViewMenu
	: public SEditorViewportViewMenu
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanViewportViewMenu) {}
		SLATE_ARGUMENT(EABImageViewMode, ViewMode)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, MenuExtenders)
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<SViewportToolBar> InParentToolBar)
	{
		Viewport = InViewport;
		MenuName = BaseMenuName;
		MenuExtenders = InArgs._MenuExtenders;
		CommandList = InArgs._CommandList;
		ViewMode = InArgs._ViewMode;

		SEditorViewportToolbarMenu::Construct(SEditorViewportToolbarMenu::FArguments()
			.ParentToolBar(InParentToolBar)
			.Cursor(EMouseCursor::Default)
			.Label(this, &SMetaHumanViewportViewMenu::GetViewMenuLabelOverride)
			.LabelIcon(this, &SMetaHumanViewportViewMenu::GetViewMenuLabelIconOverride)
			.ToolTipText(this, &SMetaHumanViewportViewMenu::GetViewMenuToolTipTextOverride)
			.OnGetMenuContent(this, &SMetaHumanViewportViewMenu::GenerateViewMenuContent)
		);
	}

protected:

	TSharedRef<FMetaHumanEditorViewportClient> GetViewportClient() const
	{
		check(Viewport.IsValid());
		return StaticCastSharedPtr<FMetaHumanEditorViewportClient>(Viewport.Pin()->GetViewportClient()).ToSharedRef();
	}

	/** SEditorViewportViewMenu::GetViewMenuLabel is private in the base but needs to be overriden to customize the label it returns */
	FText GetViewMenuLabelOverride() const
	{
		const EViewModeIndex ViewModeIndex = GetViewportClient()->GetViewModeIndexForABViewMode(ViewMode);
		return UViewModeUtils::GetViewModeDisplayName(ViewModeIndex);
	}

	FText GetViewMenuToolTipTextOverride() const
	{
		const FText ViewName = ViewMode == EABImageViewMode::A ? LOCTEXT("ViewMenuAName", "A") : LOCTEXT("ViewMenuBName", "B");
		return FText::Format(LOCTEXT("ViewModeOptionsMenuTooltip", "Set view mode and exposure for View {0}"), { ViewName });
	}

	/** SEditorViewportViewMenu::GetViewMenuLabelIcon is private in the base class but needs to be overriden to customize the icon shown */
	const FSlateBrush* GetViewMenuLabelIconOverride() const
	{
		const EViewModeIndex ViewModeIndex = GetViewportClient()->GetViewModeIndexForABViewMode(ViewMode);
		return UViewModeUtils::GetViewModeDisplayIcon(ViewModeIndex);
	}

	TSharedRef<SWidget> BuildFixedEV100Menu() const // Copied from SEditorViewport::BuildFixedEV100Menu
	{
		const float EV100Min = -10.f;
		const float EV100Max = 20.f;

		return SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush(TEXT("Menu.WidgetBorder")))
					.Padding(FMargin(1.0f))
					[
						SNew(SSpinBox<float>)
						.Style(&FAppStyle::Get(), TEXT("Menu.SpinBox"))
						.Font(FAppStyle::Get().GetFontStyle(TEXT("MenuItem.Font")))
						.MinValue(EV100Min)
						.MaxValue(EV100Max)
						.IsEnabled(GetViewportClient(), &FMetaHumanEditorViewportClient::CanChangeEV100, ViewMode)
						.Value(GetViewportClient(), &FMetaHumanEditorViewportClient::GetEV100, ViewMode)
						.OnValueChanged(GetViewportClient(), &FMetaHumanEditorViewportClient::SetEV100, ViewMode, true)
						.ToolTipText(LOCTEXT("EV100ToolTip", "Sets the exposure value of the camera using the specified EV100. Exposure = 1 / (1.2 * 2^EV100)"))
					]
				]
			];
	};

	void FillViewMenu(UToolMenu* InMenu) const
	{
		const FEditorViewportCommands& BaseViewportActions = FEditorViewportCommands::Get();

		{
			// View modes
			{
				FToolMenuSection& Section = InMenu->AddSection("ViewMode", LOCTEXT("ViewModeHeader", "View Mode"));
				{
					Section.AddMenuEntry(BaseViewportActions.LitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit));
					Section.AddMenuEntry(BaseViewportActions.UnlitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Unlit));
					Section.AddMenuEntry(BaseViewportActions.LightingOnlyMode, UViewModeUtils::GetViewModeDisplayName(VMI_LightingOnly));
				}
			}

			// Auto Exposure
			{
				TSharedRef<SWidget> FixedEV100Menu = BuildFixedEV100Menu();

				FToolMenuSection& Section = InMenu->AddSection("Exposure", LOCTEXT("ExposureHeader", "Exposure"));
				Section.AddEntry(FToolMenuEntry::InitWidget("FixedEV100", FixedEV100Menu, LOCTEXT("FixedEV100", "EV100")));
			}
		}
	}

	virtual TSharedRef<SWidget> GenerateViewMenuContent() const override
	{
		RegisterMenus();

		UMetaHumanEditorViewportViewMenuContext* ContextObject = NewObject<UMetaHumanEditorViewportViewMenuContext>();
		ContextObject->EditorViewportViewMenu = SharedThis(this);
		ContextObject->MetaHumanViewportViewMenu = SharedThis(this);

		FToolMenuContext MenuContext(CommandList, MenuExtenders, ContextObject);
		return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
	}

	virtual void RegisterMenus() const override
	{
		if (!UToolMenus::Get()->IsMenuRegistered(BaseMenuName))
		{
			UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(BaseMenuName);
			Menu->AddDynamicSection("BaseSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				if (UMetaHumanEditorViewportViewMenuContext* Context = InMenu->FindContext<UMetaHumanEditorViewportViewMenuContext>())
				{
					Context->MetaHumanViewportViewMenu.Pin()->FillViewMenu(InMenu);
				}
			}));
		}
	}

private:
	/** The command list to generate the menu from */
	TSharedPtr<FUICommandList> CommandList;

	/** The view mode associated with this toolbar menu */
	EABImageViewMode ViewMode;

	static const FName BaseMenuName;
};

const FName SMetaHumanViewportViewMenu::BaseMenuName("UnrealEd.ViewportToolbar.View.MetaHumanViewport");

const FMargin SMetaHumanEditorViewportToolBar::ToolbarSlotPadding{ 4.0f, 4.0f };

void SMetaHumanEditorViewportToolBar::Construct(const FArguments& InArgs)
{
	ViewportCommandList = InArgs._ViewportCommandList;
	ABCommandList = InArgs._ABCommandList;
	ViewportClient = InArgs._ViewportClient;
	OnGetABMenuContentsDelegate = InArgs._OnGetABMenuContents;

	check(ViewportClient.IsValid());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
		.Cursor(EMouseCursor::Default)
		[
			SAssignNew(ToolbarMenuHorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(ToolbarSlotPadding)
			.HAlign(HAlign_Fill)
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					[
						CreateViewMenuWidget(EABImageViewMode::A)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					[
						CreateABToggleWidget()
					]
				]
			]
			+ SHorizontalBox::Slot()
				.Padding(ToolbarSlotPadding)
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						CreateViewMixToggleWidget()
					]
				]
			+ SHorizontalBox::Slot()
				.Padding(ToolbarSlotPadding)
				.HAlign(HAlign_Fill)
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						.HAlign(HAlign_Left)
						[
							CreateCameraOptionsToolbarButtonWidget()
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Right)
						[
							CreateViewMenuWidget(EABImageViewMode::B)
						]
					]
				]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SMetaHumanEditorViewportToolBar::CreateViewMixToggleWidget()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(ViewportCommandList, FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), TEXT("EditorViewportToolBar"));
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection(TEXT("ViewTypeSelection"));
	{
		ToolbarBuilder.BeginBlockGroup();
		{
			TAttribute<FSlateIcon> AIcon = TAttribute<FSlateIcon>::CreateLambda([this]
				{
					if (ViewportClient->IsShowingViewA())
					{
						return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.A.Large") };
					}
					else
					{
						return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.A.Small") };
					}
				});

			TAttribute<FSlateIcon> BIcon = TAttribute<FSlateIcon>::CreateLambda([this]
				{
					if (ViewportClient->IsShowingViewB())
					{
						return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.B.Large") };
					}
					else
					{
						return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.B.Small") };
					}
				});

			TAttribute<FSlateIcon> CIcon = TAttribute<FSlateIcon>::CreateLambda([this]
				{
					if (ViewportClient->IsShowingViewB())
					{
						return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.A.Large") };
					}
					else
					{
						return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.A.Small") };
					}
				});


			ToolbarBuilder.AddToolBarButton(
				FMetaHumanToolkitCommands::Get().ViewMixToSingle,
				FMetaHumanToolkitCommands::Get().ViewMixToSingle->GetCommandName(),
				FMetaHumanToolkitCommands::Get().ViewMixToSingle->GetLabel(),
				FMetaHumanToolkitCommands::Get().ViewMixToSingle->GetDescription(),
				FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.Viewport.ABMode.Single") }
			);

			ToolbarBuilder.AddToolBarButton(
				FMetaHumanToolkitCommands::Get().ViewMixToWipe,
				FMetaHumanToolkitCommands::Get().ViewMixToWipe->GetCommandName(),
				FMetaHumanToolkitCommands::Get().ViewMixToWipe->GetLabel(),
				FMetaHumanToolkitCommands::Get().ViewMixToWipe->GetDescription(),
				FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.Viewport.ABMode.Wipe") }
			);

			ToolbarBuilder.AddToolBarButton(
				FMetaHumanToolkitCommands::Get().ViewMixToDual,
				FMetaHumanToolkitCommands::Get().ViewMixToDual->GetCommandName(),
				FMetaHumanToolkitCommands::Get().ViewMixToDual->GetLabel(),
				FMetaHumanToolkitCommands::Get().ViewMixToDual->GetDescription(),
				FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.Viewport.ABMode.Dual") }
			);
		}
		ToolbarBuilder.EndBlockGroup();
	}
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}


EVisibility SMetaHumanEditorViewportToolBar::GetShowAVisibility() const
{
	return ViewportClient->IsShowingViewA() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SMetaHumanEditorViewportToolBar::GetShowBVisibility() const
{
	return ViewportClient->IsShowingViewB() ? EVisibility::Visible : EVisibility::Hidden;
}

TSharedRef<SWidget> SMetaHumanEditorViewportToolBar::CreateViewMenuWidget(EABImageViewMode InViewMode)
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(ABCommandList.GetCommandList(InViewMode), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), TEXT("EditorViewportToolBar"));
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);

	ToolbarBuilder.BeginSection(InViewMode == EABImageViewMode::A ? TEXT("ViewMenuA") : TEXT("ViewMenuB"));
	{
		const FText ViewName = InViewMode == EABImageViewMode::A ? LOCTEXT("ViewA", "A") : LOCTEXT("ViewB", "B");

		TSharedRef<SMetaHumanViewportViewMenu> ViewRenderingModeDropdownMenu =
			SNew(SMetaHumanViewportViewMenu, ViewportClient->GetEditorViewportWidget().ToSharedRef(), SharedThis(this))
			.ViewMode(InViewMode)
			.CommandList(ABCommandList.GetCommandList(InViewMode));

		TSharedRef<SEditorViewportToolbarMenu> ViewDisplayOptionsDropdownMenu = SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.ToolTipText(FText::Format(LOCTEXT("ViewDisplayOptionsMenuToolTip", "Display Options for View {0}"), { ViewName }))
			.Label(FText::Format(LOCTEXT("ViewDisplayOptionsMenu", "{0}"), { ViewName }))
			.OnGetMenuContent(this, &SMetaHumanEditorViewportToolBar::FillDisplayOptionsForViewMenu, InViewMode);

		if (InViewMode == EABImageViewMode::A)
		{
			ToolbarBuilder.AddSeparator();
			ToolbarBuilder.AddWidget(ViewRenderingModeDropdownMenu);
			ToolbarBuilder.AddSeparator();
			ToolbarBuilder.AddWidget(ViewDisplayOptionsDropdownMenu);
		}
		else
		{
			ToolbarBuilder.AddSeparator();
			ToolbarBuilder.AddWidget(ViewDisplayOptionsDropdownMenu);
			ToolbarBuilder.AddSeparator();
			ToolbarBuilder.AddWidget(ViewRenderingModeDropdownMenu);
		}
	}
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

TSharedRef<SWidget> SMetaHumanEditorViewportToolBar::CreateABToggleWidget()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(ViewportCommandList, FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), TEXT("EditorViewportToolBar"));
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection(TEXT("ViewTypeSelection"));
	{
		ToolbarBuilder.BeginBlockGroup();
		{
			TAttribute<FSlateIcon> AIcon = TAttribute<FSlateIcon>::CreateLambda([this]
			{
				if (ViewportClient->IsShowingViewA())
				{
					return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.A.Large") };
				}
				else
				{
					return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.A.Small") };
				}
			});

			TAttribute<FSlateIcon> BIcon = TAttribute<FSlateIcon>::CreateLambda([this]
			{
				if (ViewportClient->IsShowingViewB())
				{
					return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.B.Large") };
				}
				else
				{
					return FSlateIcon{ FMetaHumanToolkitCommands::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.ABSplit.B.Small") };
				}
			});

			const FMetaHumanToolkitCommands& Commands = FMetaHumanToolkitCommands::Get();

			ToolbarBuilder.AddToolBarButton(Commands.ToggleSingleViewToA,
				Commands.ToggleSingleViewToA->GetCommandName(),
				Commands.ToggleSingleViewToA->GetLabel(),
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SMetaHumanEditorViewportToolBar::GetABToggleButtonATooltip)),
				AIcon);

			ToolbarBuilder.AddToolBarButton(Commands.ToggleSingleViewToB,
				Commands.ToggleSingleViewToB->GetCommandName(),
				Commands.ToggleSingleViewToB->GetLabel(),
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SMetaHumanEditorViewportToolBar::GetABToggleButtonBTooltip)),
				BIcon);
		}
		ToolbarBuilder.EndBlockGroup();
	}
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

FText SMetaHumanEditorViewportToolBar::GetABToggleButtonATooltip() const
{
	const FMetaHumanToolkitCommands& Commands = FMetaHumanToolkitCommands::Get();
	return GetABToggleButtonTooltip(Commands.ToggleSingleViewToA->GetDescription());
}

FText SMetaHumanEditorViewportToolBar::GetABToggleButtonBTooltip() const
{
	const FMetaHumanToolkitCommands& Commands = FMetaHumanToolkitCommands::Get();
	return GetABToggleButtonTooltip(Commands.ToggleSingleViewToB->GetDescription());
}

FText SMetaHumanEditorViewportToolBar::GetABToggleButtonTooltip(FText InDefaultTooltipText) const
{
	if (!ViewportClient->IsShowingSingleView())
	{
		return FText::Format(LOCTEXT("ABToggleButtonTooltipDisabled", "{0}\n\nTo enable this option, switch to Single View Mix Mode"), InDefaultTooltipText);
	}

	return InDefaultTooltipText;
}

TSharedRef<SWidget> SMetaHumanEditorViewportToolBar::FillDisplayOptionsForViewMenu(EABImageViewMode InViewMode)
{
	check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);

	const bool bShouldCloseWindowAfterMenuSelection = true;
	TSharedPtr<FUICommandList> CommandList = ABCommandList.GetCommandList(InViewMode);
	FMenuBuilder MenuBuilder{ bShouldCloseWindowAfterMenuSelection, CommandList };

	OnGetABMenuContentsDelegate.ExecuteIfBound(InViewMode, MenuBuilder);

	const FMetaHumanToolkitCommands& Commands = FMetaHumanToolkitCommands::Get();

	MenuBuilder.BeginSection(TEXT("TrackingExtensionHook"), LOCTEXT("TrackingSectionLabel", "Tracking"));
	{
		MenuBuilder.AddMenuEntry(Commands.ToggleCurves);
		MenuBuilder.AddMenuEntry(Commands.ToggleControlVertices);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SMetaHumanEditorViewportToolBar::CreateCameraOptionsToolbarButtonWidget()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(nullptr, FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), TEXT("EditorViewportToolBar"));
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection(TEXT("CameraOptions"));
	{
		TSharedRef<SWidget> ViewMixOptionsButton = SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.ToolTipText(LOCTEXT("ViewOptionsToolTip", "Viewport Options\nParameters to tweak display options of the A|B Viewport"))
			.LabelIcon(FMetaHumanToolkitStyle::Get().GetBrush("MetaHuman Toolkit.Viewport.CameraOptions"))
			.Label(this, &SMetaHumanEditorViewportToolBar::GetCameraSpeedLabel)
			.OnGetMenuContent(this, &SMetaHumanEditorViewportToolBar::CreateCameraOptionsDropDownMenuWidget);

		ToolbarBuilder.AddWidget(ViewMixOptionsButton);
	}
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

bool SMetaHumanEditorViewportToolBar::CanChangeFOV() const
{
	return !ViewportClient->IsNavigationLocked();
}

TOptional<float> SMetaHumanEditorViewportToolBar::GetFOVValue() const
{
	return ViewportClient->ViewFOV;
}

void SMetaHumanEditorViewportToolBar::HandleFOVValueChanged(float InNewValue)
{
	ViewportClient->ViewFOV = InNewValue;

	// Tell the viewport client of the change to propagate it to promoted frame
	ViewportClient->NotifyViewportSettingsChanged();
	ViewportClient->StoreCameraStateInViewportSettings();
	ViewportClient->Invalidate();
}

bool SMetaHumanEditorViewportToolBar::CanChangeFootageDepthData() const
{
	return ViewportClient->IsDepthMeshVisible(EABImageViewMode::Current);
}

TOptional<float> SMetaHumanEditorViewportToolBar::GetFootageDepthDataNear() const
{
	return ViewportClient->GetFootageDepthData().GetNear();
}

void SMetaHumanEditorViewportToolBar::HandleFootageDepthDataNearChanged(float InNewValue)
{
	FMetaHumanViewportClientDepthData DepthData = ViewportClient->GetFootageDepthData();
	DepthData.SetNear(InNewValue);
	ViewportClient->SetFootageDepthData(DepthData);
}

TOptional<float> SMetaHumanEditorViewportToolBar::GetFootageDepthDataFar() const
{
	return ViewportClient->GetFootageDepthData().GetFar();
}

void SMetaHumanEditorViewportToolBar::HandleFootageDepthDataFarChanged(float InNewValue)
{
	FMetaHumanViewportClientDepthData DepthData = ViewportClient->GetFootageDepthData();
	DepthData.SetFar(InNewValue);
	ViewportClient->SetFootageDepthData(DepthData);
}

TSharedRef<SWidget> SMetaHumanEditorViewportToolBar::CreateCameraOptionsDropDownMenuWidget()
{
	constexpr float FOVMin = 5.f;
	constexpr float FOVMax = 170.f;
	const TRange<float> FootageDepthRangeNear = ViewportClient->GetFootageDepthData().GetRangeNear();
	const TRange<float> FootageDepthRangeFar = ViewportClient->GetFootageDepthData().GetRangeFar();

	TSharedPtr<SNumericEntryBox<float>> FieldOfView = SNew(SNumericEntryBox<float>)
		.Font(FAppStyle::Get().GetFontStyle(TEXT("MenuItem.Font")))
		.AllowSpin(true)
		.MinValue(FOVMin)
		.MaxValue(FOVMax)
		.MinSliderValue(FOVMin)
		.MaxSliderValue(FOVMax)
		.IsEnabled(this, &SMetaHumanEditorViewportToolBar::CanChangeFOV)
		.Value(this, &SMetaHumanEditorViewportToolBar::GetFOVValue)
		.OnValueChanged(this, &SMetaHumanEditorViewportToolBar::HandleFOVValueChanged);

	TSharedPtr<SNumericEntryBox<float>> DepthDataNear = SNew(SNumericEntryBox<float>)
		.Font(FAppStyle::Get().GetFontStyle(TEXT("MenuItem.Font")))
		.AllowSpin(true)
		.MinValue(FootageDepthRangeNear.GetLowerBoundValue())
		.MaxValue(FootageDepthRangeNear.GetUpperBoundValue())
		.MinSliderValue(FootageDepthRangeNear.GetLowerBoundValue())
		.MaxSliderValue(FootageDepthRangeNear.GetUpperBoundValue())
		.IsEnabled(this, &SMetaHumanEditorViewportToolBar::CanChangeFootageDepthData)
		.Value(this, &SMetaHumanEditorViewportToolBar::GetFootageDepthDataNear)
		.OnValueChanged(this, &SMetaHumanEditorViewportToolBar::HandleFootageDepthDataNearChanged);
		
	TSharedPtr<SNumericEntryBox<float>> DepthDataFar = SNew(SNumericEntryBox<float>)
		.Font(FAppStyle::Get().GetFontStyle(TEXT("MenuItem.Font")))
		.AllowSpin(true)
		.MinValue(FootageDepthRangeFar.GetLowerBoundValue())
		.MaxValue(FootageDepthRangeFar.GetUpperBoundValue())
		.MinSliderValue(FootageDepthRangeFar.GetLowerBoundValue())
		.MaxSliderValue(FootageDepthRangeFar.GetUpperBoundValue())
		.IsEnabled(this, &SMetaHumanEditorViewportToolBar::CanChangeFootageDepthData)
		.Value(this, &SMetaHumanEditorViewportToolBar::GetFootageDepthDataFar)
		.OnValueChanged(this, &SMetaHumanEditorViewportToolBar::HandleFootageDepthDataFarChanged);

	FieldOfView->GetSpinBox()->SetToolTipText(LOCTEXT("ABViewFOVToolTip", "Field of View"));
	DepthDataNear->GetSpinBox()->SetToolTipText(LOCTEXT("ABViewDepthDataNearToolTip", "The nearest distance for the depth data visualization"));
	DepthDataFar->GetSpinBox()->SetToolTipText(LOCTEXT("ABViewDepthDataFarToolTip", "The farthest distance for the depth data visualization"));

	TSharedRef<SWidget> CameraControlsWidget = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Background")))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 60.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MouseSettingsCamSpeed", "Camera Speed"))
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(FMargin(0.0f, 2.0f))
				[
					SAssignNew(CamSpeedSlider, SSlider)
					.Value(this, &SMetaHumanEditorViewportToolBar::GetCamSpeedSliderPosition)
					.OnValueChanged(this, &SMetaHumanEditorViewportToolBar::OnSetCamSpeed)
					.ToolTipText(LOCTEXT("CameraSpeedSlider_ToolTip", "Adjust Camera Speed"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(this, &SMetaHumanEditorViewportToolBar::GetCameraSpeedLabel)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				]
			] // Camera Speed Scalar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 60.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MouseSettingsCamSpeedScalar", "Camera Speed Scalar"))
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(FMargin(0.0f, 2.0f))
				[
					SNew(SSpinBox<float>)
					.MinValue(1)
					.MaxValue(TNumericLimits<int32>::Max())
					.MinSliderValue(1)
					.MaxSliderValue(128)
					.Value(this, &SMetaHumanEditorViewportToolBar::GetCamSpeedScalarBoxValue)
					.OnValueChanged(this, &SMetaHumanEditorViewportToolBar::OnSetCamSpeedScalarBoxValue)
					.ToolTipText(LOCTEXT("CameraSpeedScalar_ToolTip", "Scalar to increase camera movement range"))
				]
			]
			// Camera Field of View
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 60.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CameraSettingFieldOfView", "Field of View (H)"))
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(0.0f, 2.0f))
				[
					FieldOfView.ToSharedRef()
				]
			]
			// Depth data near
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 60.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DepthDataNear", "Depth Data Near (cm)"))
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(0.0f, 2.0f))
				[
					DepthDataNear.ToSharedRef()
				]
			]
			// Depth data far
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 60.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DepthDataFar", "Depth Data Far (cm)"))
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(0.0f, 2.0f))
				[
					DepthDataFar.ToSharedRef()
				]
			]
		];

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, ViewportCommandList);

	const FMetaHumanToolkitCommands& Commands = FMetaHumanToolkitCommands::Get();

	MenuBuilder.BeginSection(TEXT("CameraControlsExtensionHook"), LOCTEXT("CameraControlsSectionLabel", "Camera"));
	{
		MenuBuilder.AddWidget(CameraControlsWidget, FText::GetEmpty());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("CameraViewportControlsExtensionHook"), LOCTEXT("CameraViewportControlsSection", "Viewport"));
	{
		MenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().FocusViewportToSelection);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText SMetaHumanEditorViewportToolBar::GetCameraSpeedLabel() const
{
	return FText::AsNumber(ViewportClient->GetCameraSpeedSetting());
}

float SMetaHumanEditorViewportToolBar::GetCamSpeedSliderPosition() const
{
	return (ViewportClient->GetCameraSpeedSetting() - 1) / ((float)FEditorViewportClient::MaxCameraSpeeds - 1);
}

void SMetaHumanEditorViewportToolBar::OnSetCamSpeed(float NewValue)
{
	const int32 OldSpeedSetting = ViewportClient->GetCameraSpeedSetting();
	const int32 NewSpeedSetting = NewValue * ((float)FEditorViewportClient::MaxCameraSpeeds - 1) + 1;

	if (OldSpeedSetting != NewSpeedSetting)
	{
		ViewportClient->SetCameraSpeedSetting(NewSpeedSetting);
		OnCamSpeedChanged.ExecuteIfBound(NewSpeedSetting);
	}
}

FText SMetaHumanEditorViewportToolBar::GetCameraSpeedScalarLabel() const
{
	return FText::AsNumber(ViewportClient->GetCameraSpeedScalar());
}

float SMetaHumanEditorViewportToolBar::GetCamSpeedScalarBoxValue() const
{
	return ViewportClient->GetCameraSpeedScalar();
}

void SMetaHumanEditorViewportToolBar::OnSetCamSpeedScalarBoxValue(float NewValue)
{
	ViewportClient->SetCameraSpeedScalar(NewValue);
	OnCamSpeedScalarChanged.ExecuteIfBound(NewValue);
}

#undef LOCTEXT_NAMESPACE