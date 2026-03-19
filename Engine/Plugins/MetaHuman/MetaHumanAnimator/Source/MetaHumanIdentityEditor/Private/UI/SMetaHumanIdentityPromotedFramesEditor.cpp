// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanIdentityPromotedFramesEditor.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityViewportSettings.h"
#include "CaptureData.h"
#include "MetaHumanIdentityStyle.h"
#include "MetaHumanIdentityCommands.h"
#include "MetaHumanIdentityViewportClient.h"
#include "MetaHumanCurveDataController.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/IToolTip.h"
#include "ScopedTransaction.h"
#include "Editor/Transactor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "Dialogs/Dialogs.h"
#include "Styling/StyleColors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SMetaHumanIdentityPromotedFramesEditor)

#define LOCTEXT_NAMESPACE "MetaHumanIdentityPromotedFrames"

const FSlateColorBrush SMetaHumanIdentityPromotedFramesEditor::PromotedFramesTimelineBackgroundBrush(FStyleColors::Panel);
const int32 SMetaHumanIdentityPromotedFramesEditor::NeutralPoseFrameLimit = 5;
const int32 SMetaHumanIdentityPromotedFramesEditor::TeethPoseFrameLimit = 1;

/////////////////////////////////////////////////////
// SPromotedFrameButton

DECLARE_DELEGATE_TwoParams(FIdentityPromotedFrameCheckStateChanged, UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InIndex)
DECLARE_DELEGATE_OneParam(FIdentityPromotedFrameFrontViewSelected, UMetaHumanIdentityPromotedFrame* InPromotedFrame)
DECLARE_DELEGATE_RetVal(int32, FIdentityPromotedFrameGetSelectedIndex)
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FIdentityPromotedFrameGetContentMenu, int32 InIndex)

/**
 * Represents a given frame/angle promotion in the Promoted Frames panel
 */
class SPromotedFrameButton :
	public SCheckBox
{
public:
	SLATE_BEGIN_ARGS(SPromotedFrameButton) {}

		/** The Promoted Frame this button is responsible for */
		SLATE_ARGUMENT(UMetaHumanIdentityPromotedFrame*, PromotedFrame)

		/** The index of this Promoted Frame. Used to create a label for the UI */
		SLATE_ARGUMENT(int32, Index)

		/** Event used to query the current selected frame */
		SLATE_EVENT(FIdentityPromotedFrameGetSelectedIndex, OnIdentityPromotedFrameGetSelectedIndex)

		/** Delegate called when the check state of the button is changed */
		SLATE_EVENT(FIdentityPromotedFrameCheckStateChanged, OnIdentityPromotedFrameCheckStateChanged)

		/** Delegate is called when the front view for promoted frame is selected */
		SLATE_EVENT(FIdentityPromotedFrameFrontViewSelected, OnIdentityPromotedFrameFrontViewToggled)

		/** Delegate called when the context menu is requested for the button, i.e., right-clicking in it */
		SLATE_EVENT(FIdentityPromotedFrameGetContentMenu, OnIdentityPromotedFrameGetContentMenu)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PromotedFrame = InArgs._PromotedFrame;
		PromotedFrameIndex = InArgs._Index;

		OnIdentityPromotedFrameGetSelectedIndexDelegate = InArgs._OnIdentityPromotedFrameGetSelectedIndex;
		OnIdentityPromotedFrameCheckStateChangedDelegate = InArgs._OnIdentityPromotedFrameCheckStateChanged;
		OnIdentityPromotedFrameGetContextMenuDelegate = InArgs._OnIdentityPromotedFrameGetContentMenu;
		OnIdentityPromotedFrameFrontViewSelectedDelegate = InArgs._OnIdentityPromotedFrameFrontViewToggled;

		check(PromotedFrame.IsValid());
		check(PromotedFrameIndex != INDEX_NONE);
		check(OnIdentityPromotedFrameGetSelectedIndexDelegate.IsBound());
		check(OnIdentityPromotedFrameCheckStateChangedDelegate.IsBound());

		//We need Promoted Frame buttons to behave like ToggleButtons, but since Toggle buttons blend with the background until hovered/pressed,
		//it doesn't work well for our use case - there is no clear visual demarcation between the buttons. To fix this, we will make them 
		//look like normal buttons until they are pressed, and then use the toggle button foreground color & style to accentuate the selected one
		ToggleButtonStyle = FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBox");
		//In order for them to work with themes, we cannot just set colors manually, but need to extract the colors from standard elements 
		//We'll borrow the colors from DetailView.NameAreaButton, because that's what was used for Promote Frame and Demote Frame buttons
		FButtonStyle NameAreaButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>("DetailsView.NameAreaButton");
		FSlateBrush UncheckedBrush = NameAreaButtonStyle.Normal;
		FSlateBrush HoverBrush = NameAreaButtonStyle.Hovered;
		ToggleButtonStyle.SetUncheckedImage(UncheckedBrush);
		ToggleButtonStyle.SetUncheckedHoveredImage(HoverBrush);

		// Call the SButton's Construct to customize the behaviour of the Promoted Frame button
		SCheckBox::Construct(
			SCheckBox::FArguments()
			.Style(&ToggleButtonStyle)
			.HAlign(HAlign_Fill)
			.IsChecked_Lambda([this]()
				{
					return IsSelected() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}
			)
			.OnCheckStateChanged(this, &SPromotedFrameButton::HandlePromotedFrameCheckStateChanged)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &SPromotedFrameButton::GetButtonLockIcon)
					.ToolTipText(TAttribute<FText>::CreateSP(this, &SPromotedFrameButton::GetLockIconTooltip)) //separate tooltip for the lock icon, to explain the locked states
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot()
				.Padding(0.5f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.Front"))
					.ToolTipText(LOCTEXT("PromotedFrameButtonFrontIconTooltip", "This frame is marked as Front Frame"))
					.Visibility_Lambda([this]()
					{
						return GetPromotedFrameIsFrontView() ? EVisibility::Visible : EVisibility::Hidden;
					})
				]
				+ SHorizontalBox::Slot()
				.Padding(0.5f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SAssignNew(PromotedFrameLabel, SInlineEditableTextBlock)
					.Text(this, &SPromotedFrameButton::GetPromotedFrameLabel)
					.OnTextCommitted(this, &SPromotedFrameButton::HandlePromotedFrameLabelCommitted)
				]
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.ToolTipText(this, &SPromotedFrameButton::GetTrackingTooltipText)
					.Image(this, &SPromotedFrameButton::GetTrackingIcon)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		);
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			RenamePromotedFrame();

			return FReply::Handled();
		}

		return SCheckBox::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
	}

	virtual FReply OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			if (OnIdentityPromotedFrameGetContextMenuDelegate.IsBound())
			{
				TSharedRef<SWidget> ContextMenuContents = OnIdentityPromotedFrameGetContextMenuDelegate.Execute(PromotedFrameIndex);
				FWidgetPath WidgetPath = InMouseEvent.GetEventPath() != nullptr ? *InMouseEvent.GetEventPath() : FWidgetPath{};
				FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, ContextMenuContents, InMouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
				return FReply::Handled();
			}
		}

		return SCheckBox::OnMouseButtonUp(InMyGeometry, InMouseEvent);
	}

	void SetPromotedFrameIndex(int32 InNewIndex)
	{
		if (PromotedFrameIndex != InNewIndex)
		{
			PromotedFrameIndex = InNewIndex;
		}
	}

	FText GetPromotedFrameLabel() const
	{
		if (PromotedFrame.IsValid())
		{
			if (!PromotedFrame->FrameName.IsEmptyOrWhitespace())
			{
				return PromotedFrame->FrameName;
			}
		}

		return FText::Format(LOCTEXT("PromotedFrameLabel", "Frame {0}"), { PromotedFrameIndex });
	}

	FText GetPromotedFrameTooltip() const
	{
		FText PromotedFrameTitleTooltip = LOCTEXT("PromotedFrameButtonTitleTooltip", "Promoted Frame");
		if (PromotedFrame->bIsFrontView)
		{
			PromotedFrameTitleTooltip = FText::Format(LOCTEXT("PromotedFrameButtonFrontViewSufix", "{0} (Front View)"), PromotedFrameTitleTooltip);
		}

		FText PromotedFrameDescriptionTooltip;
		FText PromotedFrameDescriptionMoreOptions = LOCTEXT("PromotedFrameButtonTooltipMoreOptions", "Right-click to see more options in the context-menu.");
		if (!IsSelected())
		{
			PromotedFrameDescriptionTooltip = LOCTEXT("PromotedFrameButtonNonActiveDescriptionTooltip", "Click to make this Promoted Frame active and show it in the viewport.");
		}
		else
		{
			PromotedFrameDescriptionTooltip = LOCTEXT("PromotedFrameButtonActiveDescriptionTooltip", "This Promoted Frame is currently active.");
			if (PromotedFrame->IsA<UMetaHumanIdentityCameraFrame>())
			{
				if (PromotedFrame->bIsNavigationLocked)
				{
					PromotedFrameDescriptionTooltip = LOCTEXT("PromotedFrameButtonActiveNavigationLockedTooltip", "This Promoted Frame is currently active and navigation is locked to 2D,\nso if the frame is tracked, Marker curves can be edited.");
				}
				else
				{
					PromotedFrameDescriptionMoreOptions = FText::Format(LOCTEXT("PromotedFrameButtonActiveDescriptionMoreOptions", "{0}\nLock Camera in that menu locks the navigation to 2D mode\nand enables Marker curve editing."), PromotedFrameDescriptionMoreOptions);
				}
			}
			else {
				PromotedFrameDescriptionTooltip = LOCTEXT("PromotedFrameButtonActiveLockedTooltip", "This Promoted Frame is currently active.");
			}
		}
		return FText::Format(LOCTEXT("PromotedFrameButtonTooltipFormatting", "{0}\n\n{1}"), PromotedFrameTitleTooltip, PromotedFrameDescriptionTooltip);
	}

	bool GetPromotedFrameIsFrontView() const
	{
		if (PromotedFrame.IsValid())
		{
			return  PromotedFrame->bIsFrontView;
		}
		return false;
	}

	void RenamePromotedFrame()
	{
		if (PromotedFrameLabel.IsValid())
		{
			PromotedFrameLabel->EnterEditingMode();
		}
	}

	void TogglePromotedFrameAsFront()
	{
		if(PromotedFrame.IsValid())
		{
			OnIdentityPromotedFrameFrontViewSelectedDelegate.ExecuteIfBound(PromotedFrame.Get());
		}
	}

	int32 GetPromotedFrameIndex()
	{
		return PromotedFrameIndex;
	}

	int32 GetPromotedFrameNumber()
	{
		int32 Number = INDEX_NONE;
		if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(PromotedFrame))
		{
			Number = FootageFrame->FrameNumber;
		}

		return Number;
	}

private:

	bool IsSelected() const
	{
		const int32 SelectedIndex = OnIdentityPromotedFrameGetSelectedIndexDelegate.Execute();
		return SelectedIndex != INDEX_NONE && SelectedIndex == PromotedFrameIndex;
	}

	const FSlateBrush* GetButtonLockIcon() const
	{
		if (PromotedFrame.IsValid())
		{
			if (PromotedFrame->IsA<UMetaHumanIdentityCameraFrame>())
			{
				if (PromotedFrame->IsNavigationLocked())
				{
					return FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.CameraLocked");
				}
				else
				{
					return FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.Camera");
				}
			}
			else //UMetaHumanIdentityFootageFrame
			{
				if (PromotedFrame->IsNavigationLocked())
				{
					return FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.FrameLocked");
				}
				else
				{
					return FAppStyle::Get().GetBrush("Icons.Unlock");
				}
			}
		}

		return FAppStyle::GetNoBrush();
	}

	const FSlateBrush* GetTrackingIcon() const
	{
		if (PromotedFrame.IsValid())
		{
			if (PromotedFrame->IsTrackingManually())
			{
				return FAppStyle::GetNoBrush();
			}
			else 
			{
				return FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.Autotracked");
			}
		}
		return FAppStyle::GetNoBrush();
	}

	FText GetTrackingTooltipText() const
	{
		if (PromotedFrame.IsValid())
		{
			if (!PromotedFrame->IsTrackingManually())
			{
				return LOCTEXT("PromotedFrameNavigationTrackingIconTooltip", "This frame is auto-tracked\n\nUse right-click context menu to turn auto-tracking off");
			}
		}
		return FText();
	}

	FText GetLockIconTooltip() const
	{
		if (PromotedFrame.IsValid())
		{
			if (PromotedFrame->IsA<UMetaHumanIdentityCameraFrame>())
			{
				if (PromotedFrame->IsNavigationLocked())
				{
					return LOCTEXT("PromotedFrameNavigationIconCameraLockedTooltip", "Navigation for this frame is in 2D mode and locked to the promoted camera view,\nso if the frame is tracked, the Marker Curves can be edited.");
				}
				else
				{
					FText NavigationTooltip = LOCTEXT("PromotedFrameNavigationIconCameraUnlockedTooltip", "Navigation for this frame is in 3D mode.");
					FText EnableEditing = LOCTEXT("PromotedFrameNavigationIconCameraUnlockedEditCurvesTooltip", "To enable editing of Marker Curves, lock the navigation to 2D mode by selecting a Promoted Frame,\nand then choose Lock Camera option in the right-click context menu of the selected frame.");
					FText ShowCurvesTooltip = LOCTEXT("PromotedFrameNavigationIconCameraUnlockedShowCurvesTooltip", "NOTE: The Marker Curves are only shown when in Single View Mix Mode of AB Viewport,\nfor frames that have been tracked.");
					if (IsSelected())
					{
						EnableEditing = LOCTEXT("PromotedFrameNavigationIconCameraUnlockedAlreadySelectedTooltip", "To enable editing of Marker Curves in this Promoted Frame, lock the navigation to 2D mode\nby using Lock Camera option in the right-click context menu.");
					}

					return FText::Format(LOCTEXT("PromotedFrameNavigationFormattingTooltip", "{0}\n{1}\n{2}"), NavigationTooltip, EnableEditing, ShowCurvesTooltip);
				}
			}
			else //UMetaHumanIdentityFootageFrame
			{
				return LOCTEXT("PromotedFrameNavigationIconFrameLockedTooltip", "Footage Capture Data automaticaly tracks the active frame and locks the navigation for the frame to 2D mode\nThe Marker Curves are shown and can be edited in Single View Mix Mode of AB Viewport.");
			}
		}

		return FText();
	}

	void HandlePromotedFrameCheckStateChanged(ECheckBoxState InCheckedState)
	{
		if (PromotedFrame.IsValid())
		{
			OnIdentityPromotedFrameCheckStateChangedDelegate.ExecuteIfBound(PromotedFrame.Get(), PromotedFrameIndex);
		}
	}

	void HandlePromotedFrameLabelCommitted(const FText& InNewText, ETextCommit::Type /*InCommitInfo*/)
	{
		if (PromotedFrame.IsValid())
		{
			const FScopedTransaction Transaction(LOCTEXT("EditFrameNameTransactionLabel", "Edit Frame Name"));
			PromotedFrame->Modify();
			PromotedFrame->FrameName = InNewText;
		}
	}

private:

	/** Delegate called when a Promoted Frame button check state is changed */
	FIdentityPromotedFrameCheckStateChanged OnIdentityPromotedFrameCheckStateChangedDelegate;

	/** Delegate called to obtain the current selected index. Used to highlight this button if this is the one selected */
	FIdentityPromotedFrameGetSelectedIndex OnIdentityPromotedFrameGetSelectedIndexDelegate;

	/** Delegate called when front view is selected */
	FIdentityPromotedFrameFrontViewSelected OnIdentityPromotedFrameFrontViewSelectedDelegate;

	/** Delegate called to obtain the context menu to show when right-clicking in the button */
	FIdentityPromotedFrameGetContentMenu OnIdentityPromotedFrameGetContextMenuDelegate;

	/** The index for this Promoted Frame button */
	int32 PromotedFrameIndex;

	/** A weak reference to the Promoted Frame associated with this widget */
	TWeakObjectPtr<UMetaHumanIdentityPromotedFrame> PromotedFrame;

	/** A reference to the label displayed in the button used to trigger the text edit event */
	TSharedPtr<SInlineEditableTextBlock> PromotedFrameLabel;

	/** A modified version of the ToggleButton checkbox style */
	static FCheckBoxStyle ToggleButtonStyle;
};

FCheckBoxStyle SPromotedFrameButton::ToggleButtonStyle;

/////////////////////////////////////////////////////
// SMetaHumanIdentityPromotedFramesEditor

const TCHAR* SMetaHumanIdentityPromotedFramesEditor::PromotedFramesTransactionContext = TEXT("IdentityTransaction");

void SMetaHumanIdentityPromotedFramesEditor::Construct(const FArguments& InArgs)
{
	Identity = InArgs._Identity;
	ViewportClient = InArgs._ViewportClient;
	CommandList = InArgs._CommandList;
	FrameRange = InArgs._FrameRange;
	IsCurrentFrameValid = InArgs._IsCurrentFrameValid;
	IsTrackingCurrentFrame = InArgs._IsTrackingCurrentFrame;
	OnPromotedFrameSelectionChangedDelegate = InArgs._OnPromotedFrameSelectionChanged;
	OnPromotedFrameAddedDelegate = InArgs._OnPromotedFrameAdded;
	OnPromotedFrameRemovedDelegate = InArgs._OnPromotedFrameRemoved;
	OnPromotedFrameNavigationLockedChangedDelegate = InArgs._OnPromotedFrameNavigationLockedChanged;
	OnPromotedFrameTrackingModeChangedDelegate = InArgs._OnPromotedFrameTrackingModeChanged;

	check(ViewportClient);
	check(CommandList);

	BindCommands();

	IndexHolder = NewObject<USelectedPromotedFrameIndexHolder>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
	IndexHolder->AddToRoot();

	const FButtonStyle RoundButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");

	ChildSlot
	[
		SNew(SHorizontalBox)

		//Promote Frame
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0f, 1.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "DetailsView.NameAreaButton")
			.OnClicked_Lambda([this]
			{
				CommandList->TryExecuteAction(FMetaHumanIdentityEditorCommands::Get().PromoteFrame.ToSharedRef());
				return FReply::Handled();
			})
			.IsEnabled(this, &SMetaHumanIdentityPromotedFramesEditor::CanAddPromotedFrame)
			[
				SNew(SImage)
				.Image(FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.PromoteFrameOnTimeline"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			.ToolTipText(this, &SMetaHumanIdentityPromotedFramesEditor::GetPromoteFrameButtonTooltip)
		]

		//Demote Frame
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0f, 1.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "DetailsView.NameAreaButton")
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Lambda([this]
			{
				CommandList->TryExecuteAction(FMetaHumanIdentityEditorCommands::Get().DemoteFrame.ToSharedRef());
				return FReply::Handled();
			})
			.IsEnabled(this, &SMetaHumanIdentityPromotedFramesEditor::IsSelectionValid)
			[
				SNew(SImage)
				.Image(FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.DemoteFrameOnTimeline"))
			]
			.ToolTipText(this, &SMetaHumanIdentityPromotedFramesEditor::GetDemoteFrameButtonTooltip)
		]

		//Free Roaming Camera
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(1.0f, 1.5f) //using 1.5 vertically because toggle button when pressed enlarges a bit 
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.WidthOverride(64.0f)
			[
				SNew(SCheckBox)
				.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBox"))
				.IsChecked(this, &SMetaHumanIdentityPromotedFramesEditor::IsFreeRoamingCameraButtonCheckedHandler)
				.OnCheckStateChanged(this, &SMetaHumanIdentityPromotedFramesEditor::OnFreeRoamingCameraCheckStateChangedHandler)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(64.0f)
					.HeightOverride(32.0f)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FMetaHumanIdentityStyle::Get().GetBrush("Identity.PromotedFrames.CameraFreeRoam"))
					]
				]
				.ToolTipText_Lambda([this]()->FText
				{
					FText CameraFreeRoamingMode = LOCTEXT("CameraFreeRoamingModeTooltip", "Free Roaming Camera Mode");
					if (this->IsSelectionValid())
					{
						return FText::Format(LOCTEXT("FreeRoamInactiveTooltip", "{0} (Inactive)\nClick to switch camera to Free Roaming mode"), CameraFreeRoamingMode);
					}
					else
					{
						FText CameraFreeRoamingModeActive = LOCTEXT("CameraInFreeRoamingModeActiveTooltip", "(Active)");
						if (PromotedFramesContainer->NumSlots() > 0)
						{
							return FText::Format(LOCTEXT("FreeRoamSomePromotedFramesTooltip", "{0} {1}\nClick on any Promoted Frame on the right to set the Viewport camera to it"), CameraFreeRoamingMode, CameraFreeRoamingModeActive);
						}
						else
						{
							return FText::Format(LOCTEXT("FreeRoamNoPromotedFramesTooltip", "{0} {1}\nUse the '+' button on the left to add Promoted Frames to the Promoted Frames Timeline\nand then click on any Promoted Frame button that appears on the left to leave Free Roaming mode"), CameraFreeRoamingMode, CameraFreeRoamingModeActive);
						}
					}
				})
			]
		]

		//Promoted Frames Timeline
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.Padding(1.0f, 1.5f) //using 1.5 vertically because toggle button when pressed enlarges a bit 
		[
			//need an overlay so we can show the tooltip on hover above the empty timeline
			SNew(SOverlay)
			+SOverlay::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				SNew(SImage)
				.Image(&PromotedFramesTimelineBackgroundBrush)
				.ToolTipText_Lambda([this]()->FText
				{
					if (this->PromotedFramesContainer->NumSlots() == 0)
					{
						return LOCTEXT("PromotedFramesPanelEmptyTooltip", "Promoted Frames Timeline\n\nUse '+' button on the left to create a new Promoted Frame from the current camera view.");
					}
					else
					{
						return LOCTEXT("PromotedFramesPanelNonEmptyTooltip", "This is the Promoted Frames Timeline");
					}
				})
			]

			+ SOverlay::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				// TODO: A simple HorizontalBox might not be the best container for a large number of Promoted Frame. Look for a better solution
				SAssignNew(PromotedFramesContainer, SHorizontalBox)
			]
		]
	];

	ViewportClient->OnCameraMovedDelegate.AddSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleViewportCameraMoved);
	ViewportClient->OnCameraStoppedDelegate.AddSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleViewportCameraStopped);
	ViewportClient->OnShouldUnlockNavigationDelegate.BindSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleShouldUnlockNavigation);
	ViewportClient->OnGetSelectedPromotedFrameDelegate.BindSP(this, &SMetaHumanIdentityPromotedFramesEditor::GetSelectedPromotedFrame);
	Identity->ViewportSettings->OnSettingsChangedDelegate.AddSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleViewportSettingsChanged);
}

SMetaHumanIdentityPromotedFramesEditor::~SMetaHumanIdentityPromotedFramesEditor()
{
	// Destructor is required here as there is a TUniquePtr<class FScopedTransaction> in this class.
	IndexHolder->RemoveFromRoot();
}

ECheckBoxState SMetaHumanIdentityPromotedFramesEditor::IsFreeRoamingCameraButtonCheckedHandler() const
{
	//if anything is selected in the PromotedFrames, the FreeRoaming should be unchecked
	//if nothing is selected, it should be checked
	return IsSelectionValid() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}

void SMetaHumanIdentityPromotedFramesEditor::OnFreeRoamingCameraCheckStateChangedHandler(ECheckBoxState InCheckState)
{
	if (InCheckState == ECheckBoxState::Checked) {
		ClearSelection();
	}
}

void SMetaHumanIdentityPromotedFramesEditor::OnCheckStateChangedHandler(ECheckBoxState InCheckState)
{
	if (InCheckState == ECheckBoxState::Checked) {
		ClearSelection();
	}
}

void SMetaHumanIdentityPromotedFramesEditor::SetIdentityPose(UMetaHumanIdentityPose* InPose)
{
	if (IdentityPose != InPose)
	{
		RemoveAllPromotedFrameButtons();

		if (IdentityPose != nullptr)
		{
			// Unbind the CaptureDataChanged delegate from the previous Identity pose
			IdentityPose->OnCaptureDataChanged().RemoveAll(this);
		}

		// InPose can be nullptr to indicate no pose is being edited
		IdentityPose = InPose;

		if (IdentityPose != nullptr)
		{
			IdentityPose->OnCaptureDataChanged().AddSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleIdentityPoseCaptureDataChanged);

			for (UMetaHumanIdentityPromotedFrame* PromotedFrame : IdentityPose->PromotedFrames)
			{
				RegisterPromotedFrameCameraTransformChange(PromotedFrame);
			}
		}

		AddAllPromotedFrameButtons();

		// Restore the selection
		if (Identity.IsValid() && IdentityPose.IsValid())
		{
			SetSelection(Identity->ViewportSettings->GetSelectedPromotedFrame(IdentityPose->PoseType), true);
		}
		else
		{
			ClearSelection();
		}
	}
}

UMetaHumanIdentityPose* SMetaHumanIdentityPromotedFramesEditor::GetIdentityPose() const
{
	if (IdentityPose.IsValid())
	{
		return IdentityPose.Get();
	}

	return nullptr;
}

UMetaHumanIdentityPromotedFrame* SMetaHumanIdentityPromotedFramesEditor::GetSelectedPromotedFrame() const
{
	if (IsSelectionValid() && IdentityPose.Get() && IndexHolder && !IdentityPose->PromotedFrames.IsEmpty())
	{
		return IdentityPose->PromotedFrames[IndexHolder->PromotedFrameIndex];
	}

	return nullptr;
}

void SMetaHumanIdentityPromotedFramesEditor::HandleUndoOrRedoTransaction(const FTransaction* InTransaction)
{
	// Get the selection state before recreating the promoted frame buttons
	UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = GetSelectedPromotedFrame();

	// Check to see if the number of promoted frames changed, if it did, recreate all the buttons
	const bool bRecreateButtons = IdentityPose.IsValid() && PromotedFramesContainer->NumSlots() != IdentityPose->PromotedFrames.Num();

	// If only a vertex control was modified, then no more updates are needed
	const bool bRevertedControlVertexOnly = UndoControlVertexManipulation(InTransaction, SelectedPromotedFrame, false);

	if (!bRevertedControlVertexOnly && bRecreateButtons)
	{
		RecreatePromotedFrameButtonsForUndoRedo(SelectedPromotedFrame);
	}

	if (SelectedPromotedFrame != nullptr && !bRevertedControlVertexOnly)
	{
		// Reset the selection to notify observers that the frame changed
		constexpr bool bForceNotify = true;
		SetSelection(IndexHolder->PromotedFrameIndex, bForceNotify);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::RecreatePromotedFrameButtonsForUndoRedo(const UMetaHumanIdentityPromotedFrame* InSelectedPromotedFrame)
{
	int32 PreviousFrameIndex = IndexHolder->PromotedFrameIndex;

	if (IdentityPose.IsValid())
	{
		RemoveAllPromotedFrameButtons();
		AddAllPromotedFrameButtons();

		SetSelection(PreviousFrameIndex);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	if (InPropertyChangedEvent.GetNumObjectsBeingEdited() > 0)
	{
		if (const UMetaHumanIdentityPromotedFrame* PromotedFrame = Cast<UMetaHumanIdentityPromotedFrame>(InPropertyChangedEvent.GetObjectBeingEdited(0)))
		{
			if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityPromotedFrame, bIsNavigationLocked))
			{
				ViewportClient->SetNavigationLocked(PromotedFrame->IsNavigationLocked());
			}
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::BindCommands()
{
	const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();

	CommandList->MapAction(Commands.PromoteFrame, FUIAction(FExecuteAction::CreateSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleOnAddPromotedFrameClicked),
															FCanExecuteAction::CreateSP(this, &SMetaHumanIdentityPromotedFramesEditor::CanAddPromotedFrame)));

	CommandList->MapAction(Commands.DemoteFrame, FUIAction(FExecuteAction::CreateSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandleOnRemovePromotedFrameClicked, static_cast<int32>(INDEX_NONE), true),
															FCanExecuteAction::CreateSP(this, &SMetaHumanIdentityPromotedFramesEditor::IsSelectionValid)));

}

bool SMetaHumanIdentityPromotedFramesEditor::IsSelectionValid() const
{
	return (IndexHolder->PromotedFrameIndex != INDEX_NONE) && (IndexHolder->PromotedFrameIndex < PromotedFramesContainer->NumSlots());
}

void SMetaHumanIdentityPromotedFramesEditor::AddPromotedFrameButton(UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InIndex)
{
	const int32 InsertIndex = (InIndex < PromotedFramesContainer->NumSlots()) ? InIndex : INDEX_NONE;
	TSharedRef<SPromotedFrameButton> PromotedFrameButton =
		SNew(SPromotedFrameButton)
		.PromotedFrame(InPromotedFrame)
		.Index(InIndex)
		.OnIdentityPromotedFrameCheckStateChanged(this, &SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameButtonClicked)
		.OnIdentityPromotedFrameGetSelectedIndex_Lambda([this] { return IndexHolder->PromotedFrameIndex; })
		.OnIdentityPromotedFrameGetContentMenu(this, &SMetaHumanIdentityPromotedFramesEditor::GetPromotedFrameContextMenu)
		.OnIdentityPromotedFrameFrontViewToggled(this, &SMetaHumanIdentityPromotedFramesEditor::HandleFrontViewToggled);

	PromotedFrameButton->SetToolTipText(TAttribute<FText>(PromotedFrameButton, &SPromotedFrameButton::GetPromotedFrameTooltip));

	PromotedFramesContainer->InsertSlot(InsertIndex)
	.Padding(1.0f, 0.0f)
	[
		PromotedFrameButton
	];
}

void SMetaHumanIdentityPromotedFramesEditor::RemovePromotedFrameButton(int32 InIndex)
{
	if ((InIndex != INDEX_NONE) && (InIndex < PromotedFramesContainer->NumSlots()))
	{
		PromotedFramesContainer->RemoveSlot(GetPromotedFrameButton(InIndex));

		// Update the index of the existing Promoted Frames in the widget
		for (int32 SlotIndex = InIndex; SlotIndex < PromotedFramesContainer->NumSlots(); ++SlotIndex)
		{
			// TODO: Need to adapt for the footage case. Should probably delegate this to the IdentityPose itself
			GetPromotedFrameButton(SlotIndex)->SetPromotedFrameIndex(SlotIndex);
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::AddAllPromotedFrameButtons()
{
	if (IdentityPose.IsValid() && IdentityPose->GetCaptureData() != nullptr)
	{
		if (IdentityPose->GetCaptureData()->IsA(UFootageCaptureData::StaticClass()))
		{
			TArray<UMetaHumanIdentityPromotedFrame*> FramesWithinRange;
			TRange<int32> PlaybackRange = FrameRange.Get();
			for (UMetaHumanIdentityPromotedFrame* Frame : IdentityPose->PromotedFrames)
			{
				if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(Frame))
				{
					if (PlaybackRange.Contains(FootageFrame->FrameNumber))
					{
						FramesWithinRange.Add(FootageFrame);
					}
				}
			}

			AddButtonsForPromotedFrames(FramesWithinRange);
		}
		else
		{
			AddButtonsForPromotedFrames(IdentityPose->PromotedFrames);
		}
	}
	else
	{
		ClearSelection();
	}
}

void SMetaHumanIdentityPromotedFramesEditor::AddButtonsForPromotedFrames(const TArray<UMetaHumanIdentityPromotedFrame*>& InPromotedFrames)
{
	if (InPromotedFrames.IsEmpty())
	{
		ClearSelection();
	}
	else
	{
		for (int32 PromotedFrameIndex = 0; PromotedFrameIndex < InPromotedFrames.Num(); ++PromotedFrameIndex)
		{
			if (UMetaHumanIdentityPromotedFrame* PromotedFrame = InPromotedFrames[PromotedFrameIndex])
			{
				AddPromotedFrameButton(PromotedFrame, PromotedFrameIndex);
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Trying to add invalid Promoted Frame of index %d for Pose '%s'"), PromotedFrameIndex, *IdentityPose->GetName());
			}
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::SetSelection(int32 InIndex, bool bForceNotify)
{
	const bool bSelectionChanged = IndexHolder->PromotedFrameIndex != InIndex;

	IndexHolder->Modify();
	IndexHolder->PromotedFrameIndex = InIndex;

	if (IsSelectionValid())
	{
		UMetaHumanIdentityPromotedFrame* PromotedFrame = GetSelectedPromotedFrame();

		LoadRenderingState(PromotedFrame);

		ViewportClient->SetNavigationLocked(PromotedFrame->bIsNavigationLocked);
		ViewportClient->SetCurveDataController(PromotedFrame->GetCurveDataController());
	}
	else
	{
		ViewportClient->SetNavigationLocked(false);
		ViewportClient->SetCurveDataController(nullptr);
	}

	if (Identity.IsValid() && IdentityPose.IsValid() && Identity->ViewportSettings->GetSelectedPromotedFrame(IdentityPose->PoseType) != InIndex)
	{
		Identity->ViewportSettings->SetSelectedPromotedFrame(IdentityPose->PoseType, IsSelectionValid() ? InIndex : INDEX_NONE);
	}

	if (bSelectionChanged || bForceNotify)
	{
		OnPromotedFrameSelectionChangedDelegate.ExecuteIfBound(GetSelectedPromotedFrame(), bForceNotify);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::RecreateAllPromotedFramesButtons()
{
	RemoveAllPromotedFrameButtons();
	AddAllPromotedFrameButtons();
}

void SMetaHumanIdentityPromotedFramesEditor::RemoveAllPromotedFrameButtons()
{
	// Remove all the Promoted Frame buttons from the UI
	PromotedFramesContainer->ClearChildren();
}

void SMetaHumanIdentityPromotedFramesEditor::ClearSelection()
{
	SetSelection(INDEX_NONE);
}

bool SMetaHumanIdentityPromotedFramesEditor::IsPoseValid() const
{
	return IdentityPose.IsValid() && (IdentityPose->PromotedFrameClass != nullptr);
}

bool SMetaHumanIdentityPromotedFramesEditor::IsPromotedFrameNumberBelowLimit() const
{
	bool bLimitReached = true;

	if (IdentityPose.IsValid())
	{
		EIdentityPoseType PoseType = IdentityPose->PoseType;
		if (PoseType == EIdentityPoseType::Neutral)
		{
			return IdentityPose->PromotedFrames.Num() < SMetaHumanIdentityPromotedFramesEditor::NeutralPoseFrameLimit;
		}
		else if(PoseType == EIdentityPoseType::Teeth)
		{
			return IdentityPose->PromotedFrames.Num() < SMetaHumanIdentityPromotedFramesEditor::TeethPoseFrameLimit;
		}
	}	

	return bLimitReached;
}

bool SMetaHumanIdentityPromotedFramesEditor::CanAddPromotedFrame() const
{
	return IsPoseValid() && IdentityPose->IsCaptureDataValid() && IsCurrentFrameValid.Get() == UMetaHumanIdentityPose::ECurrentFrameValid::Valid && IsPromotedFrameNumberBelowLimit() && !IsTrackingCurrentFrame.Get();
}

bool SMetaHumanIdentityPromotedFramesEditor::CanSetViewAsFront() const
{
	return IdentityPose->IsCaptureDataValid();
}

bool SMetaHumanIdentityPromotedFramesEditor::UndoControlVertexManipulation(const FTransaction* InTransaction, const UMetaHumanIdentityPromotedFrame* InSelectedPromotedFrame, bool InIsRedo) const
{
	bool bUndoMarkerManipulation = false;

	if (InSelectedPromotedFrame)
	{
		const FTransactionDiff Diff = InTransaction->GenerateDiff();

		for (const TPair<FName, TSharedPtr<FTransactionObjectEvent>>& DiffMapPair : Diff.DiffMap)
		{
			const TSharedPtr<FTransactionObjectEvent>& TransactionObjectEvent = DiffMapPair.Value;
			if (TransactionObjectEvent->HasPropertyChanges())
			{
				const TArray<FName>& ChangedPropertyNames = TransactionObjectEvent->GetChangedProperties();
				bUndoMarkerManipulation = ChangedPropertyNames.Contains(GET_MEMBER_NAME_CHECKED(UMetaHumanContourData, ReducedContourData)) ||
					ChangedPropertyNames.Contains(GET_MEMBER_NAME_CHECKED(UMetaHumanContourData, ManuallyModifiedCurves)) ||
					ChangedPropertyNames.Contains(GET_MEMBER_NAME_CHECKED(UMetaHumanContourData, FrameTrackingContourData));
			}
		}

		if (bUndoMarkerManipulation)
		{
			InSelectedPromotedFrame->GetCurveDataController()->HandleUndoOperation();
		}
	}

	return bUndoMarkerManipulation;
}

bool SMetaHumanIdentityPromotedFramesEditor::SetFrontFrameFromDialog()
{
	FSuppressableWarningDialog::FSetupInfo Info( 
					LOCTEXT("ShouldSetFrontView", "Current promoted frame will be set as front view"), 
					LOCTEXT("ShouldRecordTitle", "Setting the front view"), 
					TEXT("FrontViewPromotedFrame") );
	Info.ConfirmText = LOCTEXT("ShouldRecord_ConfirmText", "Ok");
	Info.CancelText = LOCTEXT("ShouldRecord_CancelText", "Cancel");

	FSuppressableWarningDialog ShouldRecordDialog( Info );
	FSuppressableWarningDialog::EResult UserInput = ShouldRecordDialog.ShowModal();

	return UserInput == FSuppressableWarningDialog::EResult::Cancel ? false : true; 
}

TSharedRef<SPromotedFrameButton> SMetaHumanIdentityPromotedFramesEditor::GetPromotedFrameButton(int32 InIndex) const
{
	const int32 NumSlots = PromotedFramesContainer->NumSlots();
	check(0 <= InIndex && InIndex < NumSlots);
	const SHorizontalBox::FSlot& Slot = PromotedFramesContainer->GetSlot(InIndex);
	return StaticCastSharedRef<SPromotedFrameButton>(Slot.GetWidget());
}

TSharedRef<SWidget> SMetaHumanIdentityPromotedFramesEditor::GetPromotedFrameContextMenu(int32 InPromotedFrameIndex)
{
	const FName PromotedFrameContextMenuName = TEXT("PromotedFrameContextMenu");
	if (!UToolMenus::Get()->IsMenuRegistered(PromotedFrameContextMenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(PromotedFrameContextMenuName);

		// Can't capture this in this lambda, when the editor is closed the captured "this" value will be gone.
		// Inside the lambda use the Context object to obtain a reference to the editor itself
		Menu->AddDynamicSection(TEXT("PromotedFrameCommands"), FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();

			if (USelectedPromotedFrameIndexHolder* Context = InMenu->FindContext<USelectedPromotedFrameIndexHolder>())
			{
				TSharedPtr<SMetaHumanIdentityPromotedFramesEditor> PromotedFramesEditor = Context->PromotedFramesEditor.Pin();

				// Sanity checks, this should never fail
				check(PromotedFramesEditor.IsValid());
				check(PromotedFramesEditor->IdentityPose.IsValid() && Context->PromotedFrameIndex < PromotedFramesEditor->IdentityPose->PromotedFrames.Num());

				TSharedRef<SPromotedFrameButton> PromotedFrameButton = PromotedFramesEditor->GetPromotedFrameButton(Context->PromotedFrameIndex);
				UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFramesEditor->IdentityPose->PromotedFrames[Context->PromotedFrameIndex];

				if(PromotedFrame->IsA<UMetaHumanIdentityCameraFrame>())
				{
					FToolMenuSection& TrackingSection = InMenu->AddSection(TEXT("PromotedFrameTrackingMode"), LOCTEXT("PromotedFrameTrackingModeMenuSection", "Tracking Mode"));
					{
						if (Context->PromotedFrameIndex == PromotedFramesEditor->IndexHolder->PromotedFrameIndex)
						{
							TrackingSection.AddMenuEntry(TEXT("AutoTrackingOnMenuEntry"),
														 LOCTEXT("TrackOnChangeLabel", "Autotracking On"),
														 LOCTEXT("TrackOnChangeTooltip", "Run face tracker when the camera stops moving"),
														 FSlateIcon{},
														 FUIAction(FExecuteAction::CreateSP(PromotedFramesEditor.ToSharedRef(), &SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameTrackingModeChanged, PromotedFrame, true),
																   FCanExecuteAction::CreateLambda([PromotedFrame] { return !PromotedFrame->IsNavigationLocked(); }),
																   FIsActionChecked::CreateUObject(PromotedFrame, &UMetaHumanIdentityPromotedFrame::IsTrackingOnChange)),
														 EUserInterfaceActionType::RadioButton);

							TrackingSection.AddMenuEntry(TEXT("AutoTrackingOffMenuEntry"),
														 LOCTEXT("TrackManuallyLabel", "Autotracking Off"),
														 LOCTEXT("TrackManuallyTooltip", "Run the face tracker manually"),
														 FSlateIcon{},
														 FUIAction(FExecuteAction::CreateSP(PromotedFramesEditor.ToSharedRef(), &SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameTrackingModeChanged, PromotedFrame, false),
																   FCanExecuteAction::CreateLambda([PromotedFrame] { return !PromotedFrame->IsNavigationLocked(); }),
																   FIsActionChecked::CreateUObject(PromotedFrame, &UMetaHumanIdentityPromotedFrame::IsTrackingManually)),
														 EUserInterfaceActionType::RadioButton);

							TrackingSection.AddMenuEntry(TEXT("LockCameraMenuEntry"),
														 LOCTEXT("LockNavigationLabel", "Lock Camera"),
														 LOCTEXT("LockNavigationTooltip", "Locks the camera navigation for this frame and switches to 2D navigation mode"),
														 FSlateIcon{},
														 FUIAction(FExecuteAction::CreateSP(PromotedFramesEditor.ToSharedRef(), &SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameToggleNavigationLocked, PromotedFrame),
																   FCanExecuteAction{},
																   FIsActionChecked::CreateUObject(PromotedFrame, &UMetaHumanIdentityPromotedFrame::IsNavigationLocked)),
														 EUserInterfaceActionType::ToggleButton);
						}
					}
				}

				FToolMenuSection& CommandsSection = InMenu->AddSection(TEXT("PromotedFrameCommands"), LOCTEXT("PromotedFrameCommandsMenuSection", "Commands"));
				{
					if (Context->PromotedFrameIndex == PromotedFramesEditor->IndexHolder->PromotedFrameIndex)
					{
						// Only show TrackCurrent if this is the current selected frame
						CommandsSection.AddMenuEntry(Commands.TrackCurrent);
					}

					CommandsSection.AddMenuEntry(TEXT("RenamePromotedFrameMenuEntry"),
												 FText::Format(LOCTEXT("RenamePromotedFrameContextMenuEntry", "Rename {0}"), { PromotedFrameButton->GetPromotedFrameLabel() }),
												 LOCTEXT("RenamePromotedFrameTooltip", "Rename the Promoted Frame"),
												 FSlateIcon{},
												 FUIAction(FExecuteAction::CreateSP(PromotedFrameButton, &SPromotedFrameButton::RenamePromotedFrame)));

					if(PromotedFramesEditor->CanSetViewAsFront())
					{
						CommandsSection.AddMenuEntry(TEXT("TogglePromotedFrameAsFrontView"),
													 PromotedFrame->bIsFrontView ? LOCTEXT("RemoveFrontViewLabel", "Remove Front View") : LOCTEXT("SetFrontViewLabel", "Set Front View"),
													 LOCTEXT("SetFrontViewTooltip", "Select which promoted frame is front view"),
													 FSlateIcon{},
													 FUIAction(FExecuteAction::CreateSP(PromotedFrameButton, &SPromotedFrameButton::TogglePromotedFrameAsFront)));
					}

					CommandsSection.AddMenuEntry(Commands.DemoteFrame->GetCommandName(),
												 FText::Format(LOCTEXT("DemotePromotedFrameContextMenuEntry", "Demote {0}"), { PromotedFrameButton->GetPromotedFrameLabel() }),
												 Commands.DemoteFrame->GetDescription(),
												 Commands.DemoteFrame->GetIcon(),
												 FUIAction(FExecuteAction::CreateSP(PromotedFramesEditor.ToSharedRef(), &SMetaHumanIdentityPromotedFramesEditor::HandleOnRemovePromotedFrameClicked, Context->PromotedFrameIndex, true)));
				}
			}
		}));
	}

	// Creates a context object that the menu itself can access
	USelectedPromotedFrameIndexHolder* ContextObject = NewObject<USelectedPromotedFrameIndexHolder>();
	ContextObject->PromotedFrameIndex = InPromotedFrameIndex;
	ContextObject->PromotedFramesEditor = SharedThis(this);

	FToolMenuContext MenuContext(CommandList, TSharedPtr<FExtender>(), ContextObject);
	return UToolMenus::Get()->GenerateWidget(PromotedFrameContextMenuName, MenuContext);
}

void SMetaHumanIdentityPromotedFramesEditor::HandleOnAddPromotedFrameClicked()
{
	if (CanAddPromotedFrame())
	{
		const FScopedTransaction Transaction(PromotedFramesTransactionContext, LOCTEXT("AddPromotedFrame", "Promote Frame"), IdentityPose.Get());

		IdentityPose->Modify();

		int32 PromotedFrameIndex = INDEX_NONE;
		if (UMetaHumanIdentityPromotedFrame* PromotedFrame = IdentityPose->AddNewPromotedFrame(PromotedFrameIndex))
		{
			StoreRenderingState(PromotedFrame);

			RegisterPromotedFrameCameraTransformChange(PromotedFrame);

			AddPromotedFrameButton(PromotedFrame, PromotedFrameIndex);

			// Signal to the toolkit that a new frame is added, so it can initialize its Curve states
			OnPromotedFrameAddedDelegate.ExecuteIfBound(PromotedFrame);

			// Select the newly added Promoted Frame
			SetSelection(PromotedFrameIndex);

			// Setting the first promoted frame as front
			if (PromotedFrameIndex == 0)
			{
				PromotedFrame->bIsFrontView = SetFrontFrameFromDialog();
			}
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Error creating new Promoted Frame for Pose '%s'"), *IdentityPose->GetName());
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandleOnRemovePromotedFrameClicked(int32 InPromotedFrameIndex, bool bInBroadcast)
{
	const int32 PromotedFrameIndexToRemove = InPromotedFrameIndex != INDEX_NONE ? InPromotedFrameIndex : IndexHolder->PromotedFrameIndex;

	if (IsPoseValid() && PromotedFrameIndexToRemove < IdentityPose->PromotedFrames.Num())
	{
		const FScopedTransaction Transaction(PromotedFramesTransactionContext, LOCTEXT("RemotePromotedFrame", "Remove Promoted Frame"), IdentityPose.Get());

		IdentityPose->Modify();

		RemovePromotedFrameButton(PromotedFrameIndexToRemove);

		UMetaHumanIdentityPromotedFrame* PromotedFrame = IdentityPose->PromotedFrames[PromotedFrameIndexToRemove];

		IdentityPose->RemovePromotedFrame(PromotedFrame);

		// Signal to the toolkit that a Promoted frame has been removed from the pose
		if (bInBroadcast)
		{
			OnPromotedFrameRemovedDelegate.ExecuteIfBound(PromotedFrame);
		}

		const int32 NumPromotedFrames = IdentityPose->PromotedFrames.Num();

		if (PromotedFrameIndexToRemove < IndexHolder->PromotedFrameIndex)
		{
			SetSelection(IndexHolder->PromotedFrameIndex - 1);
		}
		else if (PromotedFrameIndexToRemove == IndexHolder->PromotedFrameIndex)
		{
			if (NumPromotedFrames > 0)
			{
				// Select the previous Promoted Frame in the list or keep the current index if valid
				if (PromotedFrameIndexToRemove >= NumPromotedFrames)
				{
					SetSelection(NumPromotedFrames - 1);
				}
				else
				{
					const bool bForceNotify = true;
					SetSelection(PromotedFrameIndexToRemove, bForceNotify);
				}
			}
			else
			{
				ClearSelection();
			}
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameRemovedFromSequencer(int32 InFrameNumber)
{
	FChildren* Children = PromotedFramesContainer->GetChildren();
	// It is possible to have multiple promoted frames at the same frame on timeline
	TArray<int32> FrameIndices;

	Children->ForEachWidget([InFrameNumber, &FrameIndices](SWidget& InWidget)
	{
		if(SPromotedFrameButton* Button = static_cast<SPromotedFrameButton*>(&InWidget))
		{
			bool Remove = Button->GetPromotedFrameNumber() == InFrameNumber;
			if(Remove)
			{
				FrameIndices.Add(Button->GetPromotedFrameIndex());
			}
		}
	});

	// Ensure indices are removed in reverse order as otherwise they are invalidated by the removal of each index
	FrameIndices.Sort([](int32 a, int32 b) { return a > b; });
	for(int32 Index : FrameIndices)
	{
		HandleOnRemovePromotedFrameClicked(Index, false);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameButtonClicked(UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InIndex)
{
	if (IndexHolder->PromotedFrameIndex != InIndex)
	{
		const FScopedTransaction Transaction(LOCTEXT("PromotedFrameSelectedTransaction", "Promoted Frame Selected"));
		SetSelection(InIndex, true);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandleIdentityPoseCaptureDataChanged(bool bInResetRanges)
{
	RecreateAllPromotedFramesButtons();
}

void SMetaHumanIdentityPromotedFramesEditor::HandleViewportCameraMoved()
{
	if (IsSelectionValid())
	{
		UMetaHumanIdentityPromotedFrame* PromotedFrame = GetSelectedPromotedFrame();

		if (!ScopedTransaction.IsValid())
		{
			ScopedTransaction = MakeUnique<FScopedTransaction>(PromotedFramesTransactionContext, LOCTEXT("CameraMovedTransactionLabel", "Camera Moved"), PromotedFrame);
		}

		PromotedFrame->Modify();

		if (!PromotedFrame->IsNavigationLocked())
		{
			StoreRenderingState(PromotedFrame);
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandleViewportCameraStopped()
{
	if (ScopedTransaction.IsValid())
	{
		ScopedTransaction.Reset();
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandleViewportSettingsChanged()
{
	if (UMetaHumanIdentityPromotedFrame* PromotedFrame = GetSelectedPromotedFrame())
	{
		if (!PromotedFrame->IsNavigationLocked())
		{
			StoreRenderingState(PromotedFrame);
		}
	}
}

bool SMetaHumanIdentityPromotedFramesEditor::HandleShouldUnlockNavigation() const
{
	if (UMetaHumanIdentityPromotedFrame* SelectedFrame = GetSelectedPromotedFrame())
	{
		return !SelectedFrame->IsNavigationLocked();
	}

	return true;
}

void SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameTrackingModeChanged(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bInTrackOnChange) const
{
	if (InPromotedFrame != nullptr && InPromotedFrame->bTrackOnChange != bInTrackOnChange)
	{
		const FScopedTransaction Transaction(PromotedFramesTransactionContext, LOCTEXT("EditTrackOnChangePromotedFrameTransaction", "Edit Track On Change"), IdentityPose.Get());

		const bool bMarkDirty = false;
		InPromotedFrame->Modify(bMarkDirty);

		InPromotedFrame->bTrackOnChange = bInTrackOnChange;

		OnPromotedFrameTrackingModeChangedDelegate.ExecuteIfBound(InPromotedFrame);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameCameraTransformChanged(UMetaHumanIdentityPromotedFrame* InPromotedFrame) const
{
	if (InPromotedFrame != nullptr && InPromotedFrame == GetSelectedPromotedFrame())
	{
		LoadRenderingState(InPromotedFrame);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameToggleNavigationLocked(UMetaHumanIdentityPromotedFrame* InPromotedFrame) const
{
	if (InPromotedFrame != nullptr)
	{
		const FScopedTransaction Transaction(PromotedFramesTransactionContext, LOCTEXT("EditNavigationIsLockedTransaction", "Edit Is Navigation Locked"), IdentityPose.Get());

		InPromotedFrame->Modify();

		InPromotedFrame->ToggleNavigationLocked();

		if (InPromotedFrame == GetSelectedPromotedFrame())
		{
			ViewportClient->SetNavigationLocked(InPromotedFrame->IsNavigationLocked());
		}

		OnPromotedFrameNavigationLockedChangedDelegate.ExecuteIfBound(InPromotedFrame);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::HandleFrontViewToggled(UMetaHumanIdentityPromotedFrame* InPromotedFrame)
{
	bool bRemovingFrontViewFlag = false;
	// If new frame is set to Front, need to reset the old one, so just setting false on all promoted frames
	for (UMetaHumanIdentityPromotedFrame* PromotedFrame : IdentityPose->PromotedFrames)
	{
		if(InPromotedFrame == PromotedFrame)
		{
			bRemovingFrontViewFlag = PromotedFrame->bIsFrontView;
		}
		PromotedFrame->bIsFrontView = false;
	}

	if(!bRemovingFrontViewFlag)
	{
		InPromotedFrame->bIsFrontView = true;
	}
}

void SMetaHumanIdentityPromotedFramesEditor::RegisterPromotedFrameCameraTransformChange(UMetaHumanIdentityPromotedFrame* InPromotedFrame) const
{
	if (InPromotedFrame != nullptr)
	{
		if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(InPromotedFrame))
		{
			CameraFrame->OnCameraTransformChanged().BindSP(this, &SMetaHumanIdentityPromotedFramesEditor::HandlePromotedFrameCameraTransformChanged, InPromotedFrame);
		}
	}
}

void SMetaHumanIdentityPromotedFramesEditor::StoreRenderingState(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const
{
	if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(InPromotedFrame))
	{
		// Need to disable orbit camera before setting actor position so that the viewport camera location is converted back
		ViewportClient->ToggleOrbitCamera(false);

		CameraFrame->ViewLocation = ViewportClient->GetViewLocation();
		CameraFrame->ViewRotation = ViewportClient->GetViewRotation();
		CameraFrame->LookAtLocation = ViewportClient->GetLookAtLocation();
		CameraFrame->CameraViewFOV = ViewportClient->ViewFOV;
		CameraFrame->ViewMode = ViewportClient->GetViewMode();
		CameraFrame->FixedEV100 = ViewportClient->GetEV100(EABImageViewMode::Current);
	}
}

void SMetaHumanIdentityPromotedFramesEditor::LoadRenderingState(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const
{
	if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(InPromotedFrame))
	{
		bool bNotifyChanged = false;
		ViewportClient->SetLookAtLocation(CameraFrame->LookAtLocation);
		ViewportClient->SetViewLocation(CameraFrame->ViewLocation);
		ViewportClient->SetViewRotation(CameraFrame->ViewRotation);
		ViewportClient->ViewFOV = CameraFrame->CameraViewFOV;
		ViewportClient->SetViewModeIndex(EABImageViewMode::Current, CameraFrame->ViewMode, bNotifyChanged);
		ViewportClient->SetEV100(CameraFrame->FixedEV100, EABImageViewMode::Current, bNotifyChanged);

		// Now store the values after the update
		ViewportClient->StoreCameraStateInViewportSettings();
	}
}

FText SMetaHumanIdentityPromotedFramesEditor::GetPromotedFramesContainerTooltip() const
{
	FText PromotedFramesTimelineTooltipTitle = LOCTEXT("PromotedFramesTimelineTooltipTitle", "Promoted Frames Timeline");
	if (IdentityPose.IsValid())
	{
		if (IdentityPose->GetCaptureData() != nullptr)
		{
			return FText::Format(LOCTEXT("PromotedFramesPanelNoFramesTooltip", "{0}\nUse Promote Frame command to create a new Promoted Frame from the current camera view."), PromotedFramesTimelineTooltipTitle);
		}
		else
		{
			return FText::Format(LOCTEXT("PromotedFramesPanelNoCaptureDataTooltip", "{0}\n\nTo enable this panel, set Capture Data property in the Details panel of the selected Pose."), PromotedFramesTimelineTooltipTitle);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("PromotedFramesPanelFramesTooltip", "{0}\n\nTo enable this panel, select a Pose in MetaHuman Identity Parts Tree View."), PromotedFramesTimelineTooltipTitle);
	}
}

FText SMetaHumanIdentityPromotedFramesEditor::GetPromoteFrameButtonTooltip() const
{
	FText PromoteFrameDefaultTooltipText = LOCTEXT("PromoteFrameToolbarButtonDefaultTooltip", "Promote a frame for Tracking");

	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
		{
			// if no Capture Data is set for Neutral, it cannot be set for other poses, so guide the user to do that first
			if (NeutralPose->GetCaptureData() != nullptr)
			{
				//now cover Teeth Pose and all future poses
				if (IdentityPose != nullptr)
				{
					// Check if we can add more promoted frames
					if (IsPromotedFrameNumberBelowLimit())
					{
						if (IdentityPose->GetCaptureData() != nullptr)
						{
							// Check if the current frame is valid
							UMetaHumanIdentityPose::ECurrentFrameValid CurrentFrameValid = IsCurrentFrameValid.Get();
							if (CurrentFrameValid == UMetaHumanIdentityPose::ECurrentFrameValid::Valid)
							{
								return PromoteFrameDefaultTooltipText;
							}
							else if (CurrentFrameValid == UMetaHumanIdentityPose::ECurrentFrameValid::Invalid_Excluded)
							{
								return FText::Format(LOCTEXT("PromoteFrameToolbarButtonExcludedFrameTooltip", "{0}\n\nTo enable this option, make sure current frame is not excluded"), PromoteFrameDefaultTooltipText);
							}
							else
							{
								return FText::Format(LOCTEXT("PromoteFrameToolbarButtonNeedImageAndDepthDataTooltip", "{0}\n\nTo enable this option, make sure current frame has both image and depth data"), PromoteFrameDefaultTooltipText);
							}
						}
						else
						{
							return FText::Format(LOCTEXT("PromoteFrameToolbarButtonNoSelectedPoseCaptureDataTooltip", "{0}\n\nTo enable this option, set Capture Data in the Details panel of the selected Pose in MetaHuman Identity Parts Tree View"), PromoteFrameDefaultTooltipText);
						}
					}
					else
					{
						return FText::Format(LOCTEXT("PromoteFrameToolbarButtonMaxFramesReachedTooltip", "{0}\n\nMaximum number of promoted frames for this pose has been reached"), PromoteFrameDefaultTooltipText);
					}
				}
				else
				{
					return FText::Format(LOCTEXT("PromoteFrameToolbarButtonNoSelectPoseTooltip", "{0}\n\nTo enable this option, select a Pose in the MetaHuman Identity Parts Tree View"), PromoteFrameDefaultTooltipText);
				}
			}
			else
			{
				return FText::Format(LOCTEXT("PromoteFrameToolbarButtonNoNeutralCaptureDataTooltip", "{0}\n\nTo enable this option, set Capture Data in the Details panel of\nthe Neutral Pose in MetaHuman Identity Parts Tree View"), PromoteFrameDefaultTooltipText);
			}
		}
		else
		{
			return FText::Format(LOCTEXT("PromoteFrameToolbarButtonNoNeutralTooltip", "{0}\n\nTo enable this option, add Neutral Pose to Face Part of Identity by using\n+Add->Add Pose->Add Neutral in the MetaHuman Identity Parts Tree View"), PromoteFrameDefaultTooltipText);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("PromoteFrameToolbarButtonNoFaceTooltip", "{0}\nTo enable this option, first add Face Part to MetaHuman Identity by using\n+Add->Add Part->Add Face in MetaHuman Identity Parts Tree View,\nor Create Components button on the Toolbar"), PromoteFrameDefaultTooltipText);
	}
}

FText SMetaHumanIdentityPromotedFramesEditor::GetDemoteFrameButtonTooltip() const
{
	if (IsSelectionValid())
	{
		return LOCTEXT("DemoteFrameButtonEnabledTooltip", "Demote a frame");
	}
	else
	{
		return LOCTEXT("DemoteFrameButtonDisabledTooltip", "Demote a frame\n\nTo enable this option, use Promote Frame button on the Promoted Frames Timeline");
	}
}

#undef LOCTEXT_NAMESPACE
