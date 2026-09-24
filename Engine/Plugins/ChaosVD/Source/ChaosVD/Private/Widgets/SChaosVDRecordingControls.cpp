// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDRecordingControls.h"

#include "AsyncCompilationHelpers.h"
#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
#include "ChaosVDRecordingDetails.h"
#include "ChaosVDScene.h"
#include "ChaosVDSettingsManager.h"
#include "ChaosVDStyle.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Editor.h"
#include "SChaosVDNameListPicker.h"
#include "SEnumCombo.h"
#include "SocketSubsystem.h"
#include "Input/Reply.h"
#include "Misc/MessageDialog.h"
#include "StatusBarSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Chaos/ChaosVDEngineEditorBridge.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/ScopedSlowTask.h"
#include "Settings/ChaosVDGeneralSettings.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

class UChaosVDGeneralSettings;
const FName SChaosVDRecordingControls::RecordingControlsToolbarName = FName("ChaosVD.MainToolBar.RecordingControls");

void SChaosVDRecordingControls::Construct(const FArguments& InArgs, const TSharedRef<SChaosVDMainTab>& InMainTabSharedRef)
{
	MainTabWeakPtr = InMainTabSharedRef;
	StatusBarID = InMainTabSharedRef->GetStatusBarName();
	
	RecordingAnimation = FCurveSequence();
	RecordingAnimation.AddCurve(0.f, 1.5f, ECurveEaseFunction::Linear);

	ChildSlot
	[
		GenerateToolbarWidget()
	];

	CurrentSelectedSessionId = FChaosVDRemoteSessionsManager::LocalEditorSessionID;

	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		RemoteSessionManager->OnSessionRecordingStarted().AddSP(this, &SChaosVDRecordingControls::HandleRecordingStart);
		RemoteSessionManager->OnSessionRecordingStopped().AddSP(this, &SChaosVDRecordingControls::HandleRecordingStop);
	}
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateToggleRecordingStateButton(EChaosVDRecordingMode RecordingMode, const FText& StartRecordingTooltip)
{
	return SNew(SButton)
			.OnClicked(FOnClicked::CreateRaw(this, &SChaosVDRecordingControls::ToggleRecordingState, RecordingMode))
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.IsEnabled_Raw(this, &SChaosVDRecordingControls::IsRecordingToggleButtonEnabled, RecordingMode)
			.Visibility_Raw(this, &SChaosVDRecordingControls::IsRecordingToggleButtonVisible, RecordingMode)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnHovered_Lambda([this](){ bRecordingButtonHovered = true;})
			.OnUnhovered_Lambda([this](){ bRecordingButtonHovered = false;})
			.ToolTipText_Lambda([this, StartRecordingTooltip]()
			{
				return IsRecording() ? LOCTEXT("StopRecordButtonDesc", "Stop the current recording ") : StartRecordingTooltip;
			})
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(FMargin(0, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image_Raw(this, &SChaosVDRecordingControls::GetRecordOrStopButton, RecordingMode)
					.ColorAndOpacity_Lambda([this]()
					{
						if (IsRecording())
						{
							if (!RecordingAnimation.IsPlaying())
							{
								RecordingAnimation.Play(AsShared(), true);
							}

							const FLinearColor Color = bRecordingButtonHovered ? FLinearColor::Red : FLinearColor::White;
							return FSlateColor(bRecordingButtonHovered ? Color : Color.CopyWithNewOpacity(0.2f + 0.8f * RecordingAnimation.GetLerp()));
						}

						RecordingAnimation.Pause();
						return FSlateColor::UseSubduedForeground();
					})
				]
				+SHorizontalBox::Slot()
				.Padding(FMargin(4, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Visibility_Lambda([this](){return IsRecording() ? EVisibility::Collapsed : EVisibility::Visible;})
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
					.Text_Lambda( [RecordingMode]()
					{
						return RecordingMode == EChaosVDRecordingMode::File ? LOCTEXT("RecordToFileButtonLabel", "Record To File") : LOCTEXT("RecordToLiveButtonLabel", "Record Live Session");
					})
				]
			];
}

FText SChaosVDRecordingControls::GetCurrentSelectedSessionName() const
{
	if (TSharedPtr<FChaosVDSessionInfo> CurrentSessionPtr = GetCurrentSessionInfo())
	{
		return FText::AsCultureInvariant(CurrentSessionPtr->SessionName);
	}

	static FText InvalidSessionName = LOCTEXT("InvalidSessionLabel", "Invalid Session");

	return InvalidSessionName;
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateTargetSessionSelector()
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SComboButton)
			.MenuPlacement(MenuPlacement_AboveAnchor).ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
			.OnGetMenuContent(this, &SChaosVDRecordingControls::GenerateTargetSessionDropdown)
			.HasDownArrow(true)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text_Raw(this, &SChaosVDRecordingControls::GetCurrentSelectedSessionName)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		]];
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateTargetSessionDropdown()
{
	using namespace Chaos::VisualDebugger;

	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("CVDRecordingWidgetTargets", LOCTEXT("CVDRecordingTargetsMenu", "Available Targets"));
	{
		if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
		{
			RemoteSessionManager->EnumerateActiveSessions([this, &MenuBuilder](const TSharedRef<FChaosVDSessionInfo>& InSessionInfoRef)
			{
				if (EnumHasAnyFlags(InSessionInfoRef->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
				{
					return true;
				}
	
				FText SessionNameAsText = FText::AsCultureInvariant(InSessionInfoRef->SessionName);
				MenuBuilder.AddMenuEntry(
				SessionNameAsText,
				FText::Format(LOCTEXT("SingleTargetItemTooltip", "Select {0} session as current target"), SessionNameAsText),
				GetIconForSession(InSessionInfoRef->InstanceId),
				FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::SelectTargetSession, InSessionInfoRef->InstanceId), EUIActionRepeatMode::RepeatDisabled));
				return true;
			});
		}
	}
	
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("CVDRecordingWidgetTargetsMulti", LOCTEXT("CVDRecordingMultiTargetsMenu", "Multi Target"));

	FText AllRemoteTargetsLabel = LOCTEXT("AllRemoteOption", "All Remote");
	MenuBuilder.AddMenuEntry(
			AllRemoteTargetsLabel,
			LOCTEXT("MultiRemoteTargetTooltip", "Select this to act on all remote targets"),
			GetIconForSession(FChaosVDRemoteSessionsManager::AllRemoteSessionsWrapperGUID),
			FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::SelectTargetSession, FChaosVDRemoteSessionsManager::AllRemoteSessionsWrapperGUID),
			FCanExecuteAction::CreateSP(this, &SChaosVDRecordingControls::CanSelectMultiSessionTarget, FChaosVDRemoteSessionsManager::AllRemoteSessionsWrapperGUID),
			EUIActionRepeatMode::RepeatDisabled));

	FText AllRemoteServersTargetsLabel = LOCTEXT("AllRemoteServersOption", "All Remote Servers");
	MenuBuilder.AddMenuEntry(
			AllRemoteServersTargetsLabel,
			LOCTEXT("MultiRemoteServerTargetTooltip", "Select this to act on all remote server targets"),
			GetIconForSession(FChaosVDRemoteSessionsManager::AllRemoteServersWrapperGUID),
			FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::SelectTargetSession, FChaosVDRemoteSessionsManager::AllRemoteServersWrapperGUID),
			FCanExecuteAction::CreateSP(this, &SChaosVDRecordingControls::CanSelectMultiSessionTarget, FChaosVDRemoteSessionsManager::AllRemoteServersWrapperGUID),
			EUIActionRepeatMode::RepeatDisabled));

	FText AllRemoteClientsTargetsLabel = LOCTEXT("AllRemoteClientsOption", "All Remote Clients");
	MenuBuilder.AddMenuEntry(
			AllRemoteClientsTargetsLabel,
			LOCTEXT("MultiRemoteClientTargetTooltip", "Select this to act on all remote client targets"),
			GetIconForSession(FChaosVDRemoteSessionsManager::AllRemoteClientsWrapperGUID),
			FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::SelectTargetSession, FChaosVDRemoteSessionsManager::AllRemoteClientsWrapperGUID),
			FCanExecuteAction::CreateSP(this, &SChaosVDRecordingControls::CanSelectMultiSessionTarget, FChaosVDRemoteSessionsManager::AllRemoteClientsWrapperGUID),
			EUIActionRepeatMode::RepeatDisabled));

	FText AllTargets = LOCTEXT("AllTargetsOption", "All");
	MenuBuilder.AddMenuEntry(
		AllTargets,
		LOCTEXT("MultiAllTargetTooltip", "Select this to act on all targets, both Local and Remote"),
		GetIconForSession(FChaosVDRemoteSessionsManager::AllSessionsWrapperGUID),
		FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::SelectTargetSession, FChaosVDRemoteSessionsManager::AllSessionsWrapperGUID), EUIActionRepeatMode::RepeatDisabled));

	MenuBuilder.AddMenuSeparator();

	FText CustomTargets = LOCTEXT("CustomTargetsOption", "Custom Selection");
	MenuBuilder.AddSubMenu(CustomTargets,
		LOCTEXT("MultiCustomTargetTooltip", "Select this to act on the specific targets you selected"),
		FNewMenuDelegate::CreateSP(this, &SChaosVDRecordingControls::GenerateCustomTargetsMenu)
		);

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateRecordingTimeTextBlock()
{
	return SNew(SBox)
			.VAlign(VAlign_Center)
			.Visibility_Raw(this, &SChaosVDRecordingControls::GetRecordingTimeTextBlockVisibility)
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallButtonText")
				.Text_Raw(this, &SChaosVDRecordingControls::GetRecordingTimeText)
				.ColorAndOpacity(FColor::White)
			];
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateToolbarWidget()
{
	RegisterMenus();

	FToolMenuContext MenuContext;

	UChaosVDRecordingToolbarMenuContext* CommonContextObject = NewObject<UChaosVDRecordingToolbarMenuContext>();
	CommonContextObject->RecordingControlsWidget = SharedThis(this);

	MenuContext.AddObject(CommonContextObject);

	return UToolMenus::Get()->GenerateWidget(RecordingControlsToolbarName, MenuContext);
}

EVisibility SChaosVDRecordingControls::GetRecordingTimeTextBlockVisibility() const
{
	TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo();
	bool bIsVisible = SessionInfo && !EnumHasAnyFlags(SessionInfo->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper) && IsRecording();

	return  bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateDataChannelsButton()
{
	return SNew(SComboButton)
			.ContentPadding(FMargin(6.0f, 0.0f))
			.IsEnabled_Raw(this, &SChaosVDRecordingControls::HasDataChannelsSupport)
			.MenuPlacement(MenuPlacement_AboveAnchor).ComboButtonStyle(&FAppStyle::Get()
			.GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.OnGetMenuContent(this, &SChaosVDRecordingControls::GenerateDataChannelsMenu)
			.HasDownArrow(true)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DataChannelsButton", "Data Channels"))
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("DialogButtonText"))
				]
			];
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateLoadingModeSelector()
{
	TAttribute<int32> GetCurrentValueAttribute;
	GetCurrentValueAttribute.BindSPLambda(this, [this]()
	{
		return static_cast<int32>(CurrentLoadingMode);
	});

	SEnumComboBox::FOnEnumSelectionChanged EnumValueChangedDelegate = SEnumComboBox::FOnEnumSelectionChanged::CreateSPLambda(this, [this](int32 NewValue, ESelectInfo::Type SelectionType)
	{
		CurrentLoadingMode = static_cast<EChaosVDLoadRecordedDataMode>(NewValue);
	});
	
	return SNew(SEnumComboBox, StaticEnum<EChaosVDLoadRecordedDataMode>())
		.IsEnabled_Raw(this, &SChaosVDRecordingControls::CanChangeLoadingMode)
		.CurrentValue(GetCurrentValueAttribute)
		.OnEnumSelectionChanged(EnumValueChangedDelegate);
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateDataChannelsMenu()
{
	using namespace Chaos::VisualDebugger;

	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("CVDRecordingWidget", LOCTEXT("CVDRecordingMenuChannels", "Data Channels"));
	{
		if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
		{
			for (const TPair<FString, FChaosVDDataChannelState>& DataChannelStateWithName : SessionInfo->DataChannelsStatesByName)
			{
				FText ChannelNamesAsText = FText::AsCultureInvariant(DataChannelStateWithName.Key);
				MenuBuilder.AddMenuEntry(
				ChannelNamesAsText,
				FText::Format(LOCTEXT("ChannelDesc", "Enable/disable the {0} channel"), ChannelNamesAsText),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::ToggleChannelEnabledState, DataChannelStateWithName.Key),
					FCanExecuteAction::CreateSP(this, &SChaosVDRecordingControls::CanChangeChannelEnabledState, DataChannelStateWithName.Key), FIsActionChecked::CreateSP(this, &SChaosVDRecordingControls::IsChannelEnabled, DataChannelStateWithName.Key)), NAME_None, EUserInterfaceActionType::ToggleButton);
			}
		}
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SChaosVDRecordingControls::ToggleChannelEnabledState(FString ChannelName)
{
	TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager();
	TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo();
	if (RemoteSessionManager && SessionInfo)
	{
		if (FChaosVDDataChannelState* ChannelState = SessionInfo->DataChannelsStatesByName.Find(ChannelName))
		{
			ChannelState->bWaitingUpdatedState = true;
			RemoteSessionManager->SendDataChannelStateChangeCommand(SessionInfo->Address, {ChannelState->ChannelName, !ChannelState->bIsEnabled });
		}
	}
}

bool SChaosVDRecordingControls::IsChannelEnabled(FString ChannelName)
{
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
	{
		if (FChaosVDDataChannelState* ChannelState = SessionInfo->DataChannelsStatesByName.Find(ChannelName))
		{
			return ChannelState->bIsEnabled;
		}
	}

	return false;
}

bool SChaosVDRecordingControls::CanChangeChannelEnabledState(FString ChannelName)
{
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
	{
		if (FChaosVDDataChannelState* ChannelState = SessionInfo->DataChannelsStatesByName.Find(ChannelName))
		{
			return ChannelState->bCanChangeChannelState && !ChannelState->bWaitingUpdatedState;
		}
	}
	
	return false;
}

void SChaosVDRecordingControls::SelectTargetSession(FGuid SessionId)
{
	CurrentSelectedSessionId = SessionId;
}

void SChaosVDRecordingControls::GenerateCustomTargetsMenu(FMenuBuilder& MenuBuilder)
{
	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		RemoteSessionManager->EnumerateActiveSessions([this, &MenuBuilder](const TSharedRef<FChaosVDSessionInfo>& InSessionInfoRef)
		{
			if (EnumHasAnyFlags(InSessionInfoRef->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
			{
				return true;
			}
	
			FText SessionNameAsText = FText::AsCultureInvariant(InSessionInfoRef->SessionName);
			MenuBuilder.AddMenuEntry(
			SessionNameAsText,
			FText::Format(LOCTEXT("MultiTargetItemTooltip", "Select {0} session as one of the current targets"), SessionNameAsText),
			GetIconForSession(InSessionInfoRef->InstanceId),
			FUIAction(
			FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::ToggleSessionSelectionInCustomTarget, InSessionInfoRef->InstanceId),
			FCanExecuteAction::CreateSP(this, &SChaosVDRecordingControls::CanSelectInCustomTarget, InSessionInfoRef->InstanceId),
			FIsActionChecked::CreateSP(this, &SChaosVDRecordingControls::IsSessionPartOfCustomTargetSelection, InSessionInfoRef->InstanceId)),
			NAME_None, EUserInterfaceActionType::ToggleButton);

			return true;
		});
	}
}

bool SChaosVDRecordingControls::IsSessionPartOfCustomTargetSelection(FGuid SessionGuid)
{
	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		if (TSharedPtr<FChaosVDMultiSessionInfo> CustomSessionTarget = StaticCastSharedPtr<FChaosVDMultiSessionInfo>(RemoteSessionManager->GetSessionInfo(FChaosVDRemoteSessionsManager::CustomSessionsWrapperGUID).Pin()))
		{
			return CustomSessionTarget->InnerSessionsByInstanceID.Find(SessionGuid) != nullptr;
		}
	}

	return false;
}

void SChaosVDRecordingControls::ToggleSessionSelectionInCustomTarget(FGuid SessionGuid)
{
	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		if (TSharedPtr<FChaosVDMultiSessionInfo> CustomSessionTarget = StaticCastSharedPtr<FChaosVDMultiSessionInfo>(RemoteSessionManager->GetSessionInfo(FChaosVDRemoteSessionsManager::CustomSessionsWrapperGUID).Pin()))
		{
			if (CustomSessionTarget->InnerSessionsByInstanceID.Find(SessionGuid) != nullptr)
			{
				CustomSessionTarget->InnerSessionsByInstanceID.Remove(SessionGuid);
			}
			else
			{
				CustomSessionTarget->InnerSessionsByInstanceID.Add(SessionGuid, RemoteSessionManager->GetSessionInfo(SessionGuid));
			}

			SelectTargetSession(FChaosVDRemoteSessionsManager::CustomSessionsWrapperGUID);
		}
	}
}

bool SChaosVDRecordingControls::CanSelectInCustomTarget(FGuid SessionGuid) const
{
	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		if (TSharedPtr<FChaosVDSessionInfo> CustomSessionTarget = RemoteSessionManager->GetSessionInfo(SessionGuid).Pin())
		{
			return CustomSessionTarget->ReadyState == EChaosVDRemoteSessionReadyState::Ready;
		}
	}

	return false;
}

bool SChaosVDRecordingControls::CanSelectMultiSessionTarget(FGuid SessionGuid) const
{
	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		if (TSharedPtr<FChaosVDSessionInfo> CustomSessionTarget = RemoteSessionManager->GetSessionInfo(SessionGuid).Pin())
		{
			return CanSelectMultiSessionTarget(CustomSessionTarget.ToSharedRef());
		}
	}

	return false;
}

bool SChaosVDRecordingControls::CanSelectMultiSessionTarget(const TSharedRef<FChaosVDSessionInfo>& SessionInfoRef) const
{
	if (EnumHasAnyFlags(SessionInfoRef->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
	{
		TSharedRef<FChaosVDMultiSessionInfo> AsMultiSessionInfo = StaticCastSharedRef<FChaosVDMultiSessionInfo>(SessionInfoRef);
		return !AsMultiSessionInfo->InnerSessionsByInstanceID.IsEmpty();
	}

	return false;
}

FSlateIcon SChaosVDRecordingControls::GetIconForSession(FGuid SessionId)
{
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetSessionInfo(SessionId))
	{
		return SessionInfo->IsRecording() ? FSlateIcon(FChaosVDStyle::GetStyleSetName(),FName("RecordIcon"), FName("RecordIcon")) : FSlateIcon();
	}

	return FSlateIcon();
}

TSharedPtr<FChaosVDSessionInfo> SChaosVDRecordingControls::GetCurrentSessionInfo() const
{
	return GetSessionInfo(CurrentSelectedSessionId);
}

TSharedPtr<FChaosVDSessionInfo> SChaosVDRecordingControls::GetSessionInfo(FGuid Id) const
{
	TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager();
	TSharedPtr<FChaosVDSessionInfo> SessionInfo = RemoteSessionManager ? RemoteSessionManager->GetSessionInfo(Id).Pin() : nullptr;

	return SessionInfo;
}

bool SChaosVDRecordingControls::HasDataChannelsSupport() const
{
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
	{
		return !SessionInfo->DataChannelsStatesByName.IsEmpty();
	}

	return false;
}

bool SChaosVDRecordingControls::CanChangeLoadingMode() const
{
	if (TSharedPtr<FChaosVDSessionInfo> CurrentSession = GetCurrentSessionInfo())
	{
		// In multi session mode targets, the loading mode is controlled automatically
		if (EnumHasAnyFlags(CurrentSession->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
		{
			return false;
		}
		else if (const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin())
		{
			// If nothing is loaded yet, it does not make sense change the loading mode
			return !MainTabSharedPtr->GetChaosVDEngineInstance()->GetCurrentSessionDescriptors().IsEmpty();
		}
	}

	return false;
}

SChaosVDRecordingControls::~SChaosVDRecordingControls()
{
}

const FSlateBrush* SChaosVDRecordingControls::GetRecordOrStopButton(EChaosVDRecordingMode RecordingMode) const
{
	const FSlateBrush* RecordIconBrush = FChaosVDStyle::Get().GetBrush("RecordIcon");
	return bRecordingButtonHovered && IsRecording() ? FChaosVDStyle::Get().GetBrush("StopIcon") : RecordIconBrush;
}

void SChaosVDRecordingControls::HandleRecordingStop(TWeakPtr<FChaosVDSessionInfo> SessionInfo)
{
	const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin();
	if (!MainTabSharedPtr.IsValid())
	{
		return;
	}

	TSharedPtr<FChaosVDSessionInfo> SessionInfoPtr = SessionInfo.Pin();

	if (!ensure(SessionInfoPtr))
	{
		return;
	}

	FText CurrentTraceTarget = FText::AsCultureInvariant(SessionInfoPtr->LastKnownRecordingState.TraceDetails.TraceTarget);

	const bool bIsLiveSession = SessionInfoPtr->GetRecordingMode() == EChaosVDRecordingMode::Live;

	if (UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr)
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, RecordingMessageHandle);

		if (bIsLiveSession)
		{
			const FText LiveSessionEnded = LOCTEXT("LiveSessionEndedMessage"," Live session has ended");
			LiveSessionEndedMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, LiveSessionEnded);
		}
		else
		{
			const FText RecordingPathMessage = FText::Format(LOCTEXT("RecordingSavedPathMessage"," Recording saved at {0} "), CurrentTraceTarget);
			RecordingPathMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, RecordingPathMessage);
		}
	}

	if (!bIsLiveSession && !EnumHasAnyFlags(SessionInfoPtr->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("OpenLastRecordingMessage", "Do you want to load the recorded file now? ")) == EAppReturnType::Yes)
		{
			MainTabSharedPtr->GetChaosVDEngineInstance()->LoadRecording(CurrentTraceTarget.ToString(), EChaosVDLoadRecordedDataMode::SingleSource);
		}
	}
}

void SChaosVDRecordingControls::HandleRecordingStart(TWeakPtr<FChaosVDSessionInfo> SessionInfo)
{
	UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr;
	if (!StatusBarSubsystem)
	{
		return;
	}
	
	if (RecordingPathMessageHandle.IsValid())
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, RecordingPathMessageHandle);
		RecordingPathMessageHandle = FStatusBarMessageHandle();
	}
	
	if (LiveSessionEndedMessageHandle.IsValid())
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, LiveSessionEndedMessageHandle);
		LiveSessionEndedMessageHandle = FStatusBarMessageHandle();
	}

	RecordingMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, LOCTEXT("RecordingMessage", "Recording..."));
}

void SChaosVDRecordingControls::ExecuteAsyncConnectionAttemptTaskWithRetry(int32 RemainingRetries, const TFunction<bool()>& InRecordingStartAttemptCallback, const TFunction<void()>& InRecordingFailedCallback)
{
	TSharedPtr<SNotificationItem> AttemptNotification = PushConnectionAttemptNotification();

	ExecutePostRecordingAsyncTaskWithRetry_Internal(RemainingRetries, InRecordingStartAttemptCallback, AttemptNotification, InRecordingFailedCallback);
}

void SChaosVDRecordingControls::ExecutePostRecordingAsyncTaskWithRetry_Internal(int32 RemainingRetries, TFunction<bool()> RecordingStartAttemptCallback, const TSharedPtr<SNotificationItem>& InProgressNotification, const TFunction<void()> InRecordingFailedCallback)
{
	// We need to wait at least one tick before attempting to connect to give it time to the trace to be initialized, write to disk, and for the
	// session manager to hear back from a remote instance

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([InProgressNotification, RemainingRetries, WeakThis = AsWeak(), RecordingStartAttemptCallback, InRecordingFailedCallback](float DeltaTime)
	{
		if (const TSharedPtr<SChaosVDRecordingControls> RecordingControlsPtr = StaticCastSharedPtr<SChaosVDRecordingControls>(WeakThis.Pin()))
		{
			if (!RecordingStartAttemptCallback)
			{
				RecordingControlsPtr->HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult::Failed, InProgressNotification);
				UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to session | Invalid callback provided..."), ANSI_TO_TCHAR(__FUNCTION__));
				return false;
			}

			if (const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = RecordingControlsPtr->MainTabWeakPtr.Pin())
			{
				const int32 NewRemainingRetries = RemainingRetries -1;

				RecordingControlsPtr->UpdateConnectionAttemptNotification(InProgressNotification, NewRemainingRetries);

				// CVD needs the trace session name to be able to load a live session. Although the session exist, the session name might not be written right away
				// Trace files don't really have metadata, it is all part of the same stream, so we need to wait until it is written which might take a few ticks.
				// Therefore if it is not ready, try again a few times.
				if (!RecordingStartAttemptCallback())
				{
					if (NewRemainingRetries > 0)
					{
						UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%s] Failed to connect to live session | Attempting again in [%f]..."), ANSI_TO_TCHAR(__FUNCTION__), RecordingControlsPtr->IntervalBetweenAutoplayConnectionAttemptsSeconds);
						RecordingControlsPtr->ExecutePostRecordingAsyncTaskWithRetry_Internal(NewRemainingRetries, RecordingStartAttemptCallback, InProgressNotification, InRecordingFailedCallback);
					}
					else
					{
						RecordingControlsPtr->HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult::Failed, InProgressNotification);
						InRecordingFailedCallback();
						UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to live session | attempts exhausted..."), ANSI_TO_TCHAR(__FUNCTION__));	
					}
				}
				else
				{
					RecordingControlsPtr->HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult::Success, InProgressNotification);
					InRecordingFailedCallback();
				}
			}
		}
		return false;
	}), IntervalBetweenAutoplayConnectionAttemptsSeconds);
}

SChaosVDRecordingControls::FLiveConnectionSettings SChaosVDRecordingControls::GetTargetLiveConnectionSettings(const TSharedRef<FChaosVDSessionInfo>& InSessionInfoRef)
{
	FLiveConnectionSettings ConnectionSettings;

	if (!ensure(!EnumHasAnyFlags(InSessionInfoRef->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper)))
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Cannot not be called with a multi session wrapper."), ANSI_TO_TCHAR(__FUNCTION__));

		return ConnectionSettings;
	}

	if (InSessionInfoRef->IsConnected() && InSessionInfoRef->GetRecordingMode() == EChaosVDRecordingMode::Live)
	{
		FChaosVDTraceDetails& RecordingSessionDetails = InSessionInfoRef->LastKnownRecordingState.TraceDetails;

		if (const UE::Trace::FStoreClient::FSessionInfo* TraceSessionInfo = FChaosVDTraceManager::GetTraceSessionInfo(RecordingSessionDetails.TraceTarget, RecordingSessionDetails.TraceGuid))
		{
			ConnectionSettings.SessionAddress = RecordingSessionDetails.TraceTarget;
			ConnectionSettings.TraceID = TraceSessionInfo->GetTraceId();
			return ConnectionSettings;
		}
	}

	return ConnectionSettings;
}

void SChaosVDRecordingControls::ToggleMultiSessionSessionRecordingState(EChaosVDRecordingMode RecordingMode, const TSharedRef<FChaosVDMultiSessionInfo>& InSessionInfoRef)
{
	const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin();
	if (!ensure(MainTabSharedPtr))
	{
		return;
	}

	bool bNewRecordingState = !IsRecording();

	if (bNewRecordingState)
	{
		CurrentLoadingMode = EChaosVDLoadRecordedDataMode::MultiSource;
		MainTabSharedPtr->GetChaosVDEngineInstance()->CloseActiveTraceSessions();
	}

	InSessionInfoRef->EnumerateInnerSessions([this, RecordingMode, bNewRecordingState](const TSharedRef<FChaosVDSessionInfo>& InInnerSessionRef)
	{
		SetSessionRecordingState(bNewRecordingState, RecordingMode, InInnerSessionRef);
		return true;
	});
}

void SChaosVDRecordingControls::ToggleSingleSessionRecordingState(EChaosVDRecordingMode RecordingMode, const TSharedRef<FChaosVDSessionInfo>& SessionInfoRef)
{
	SetSessionRecordingState(!SessionInfoRef->IsRecording(), RecordingMode, SessionInfoRef);
}

void SChaosVDRecordingControls::SetSessionRecordingState(bool bIsRecording, EChaosVDRecordingMode RecordingMode, const TSharedRef<FChaosVDSessionInfo>& SessionInfoRef)
{
	TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager();
	if (!RemoteSessionManager)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Session Manager is not available"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (bIsRecording)
	{
		TArray<FString, TInlineAllocator<1>> RecordingArgs;

		int32 RemainingRetries = 4;

		if (UChaosVDGeneralSettings* Settings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDGeneralSettings>())
		{
			RemainingRetries = Settings->MaxConnectionRetries;
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("Failed to obtain setting object. Setting the retries attempts to connect to a session to 4 as a fallback."))
		}

		SessionInfoRef->ReadyState = EChaosVDRemoteSessionReadyState::Busy;

		auto RecordingAttemptFailedCallback = [SessionGUID = SessionInfoRef->InstanceId]()
		{
			TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManagerPtr = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager();
			TSharedPtr<FChaosVDSessionInfo> SessionInfoPtr = RemoteSessionManagerPtr ? RemoteSessionManagerPtr->GetSessionInfo(SessionGUID).Pin() : nullptr;

			if (!SessionInfoPtr)
			{
				return;
			}

			SessionInfoPtr->ReadyState = EChaosVDRemoteSessionReadyState::Ready;
		};

		if (RecordingMode == EChaosVDRecordingMode::Live)
		{
			FChaosVDStartRecordingCommandMessage RecordingParams;
			RecordingParams.RecordingMode = EChaosVDRecordingMode::Live;

			check(GLog);
			bool bOutCanBindAll = false;
			//TODO: Add a way to specify a local address in case we have multiple adapters?
			TSharedRef<FInternetAddr> LocalIP = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bOutCanBindAll);

			constexpr bool bAppendPort = false;
			RecordingParams.Target = LocalIP->ToString(bAppendPort);

			RemoteSessionManager->SendStartRecordingCommand(SessionInfoRef->Address, RecordingParams);

			// Once the start recording command is issue, we can try to connect to the created session (if everything went well).
			// If it didn't go well, this will take care of update the UI to notify the user
			ExecuteAsyncConnectionAttemptTaskWithRetry(RemainingRetries, [WeakThis = AsWeak(), SessionInstanceId = SessionInfoRef->InstanceId]()
			{
				TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManagerPtr = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager();
				TSharedPtr<FChaosVDSessionInfo> SessionInfoPtr = RemoteSessionManagerPtr ? RemoteSessionManagerPtr->GetSessionInfo(SessionInstanceId).Pin() : nullptr;

				if (!SessionInfoPtr)
				{
					return false;
				}
				
				TSharedPtr<SChaosVDRecordingControls> Controls =  StaticCastSharedPtr<SChaosVDRecordingControls>(WeakThis.Pin());
				const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = Controls ? Controls->MainTabWeakPtr.Pin() : nullptr;
				if (!MainTabSharedPtr)
				{
					return false;
				}

				FLiveConnectionSettings ConnectionDetails = Controls->GetTargetLiveConnectionSettings(SessionInfoPtr.ToSharedRef());
				return MainTabSharedPtr->ConnectToLiveSession(ConnectionDetails.TraceID, ConnectionDetails.SessionAddress, Controls->CurrentLoadingMode);
			},
			RecordingAttemptFailedCallback);
		}
		else
		{
			FChaosVDStartRecordingCommandMessage RecordingParams;
			RecordingParams.RecordingMode = EChaosVDRecordingMode::File;
			RemoteSessionManager->SendStartRecordingCommand(SessionInfoRef->Address, RecordingParams);

			// Once the start recording command is issued, we need to check if the recording started, which might take a few frames.
			// This will take care of retrying, waiting and update the UI to notify the user if needed.
			ExecuteAsyncConnectionAttemptTaskWithRetry(RemainingRetries, [WeakThis = AsWeak()]()
			{
				TSharedPtr<SChaosVDRecordingControls> Controls = StaticCastSharedPtr<SChaosVDRecordingControls>(WeakThis.Pin());
				return Controls ? Controls->IsRecording() : false;
			},
			RecordingAttemptFailedCallback);
		}
	}
	else
	{
		RemoteSessionManager->SendStopRecordingCommand(SessionInfoRef->Address);
	}
}

FReply SChaosVDRecordingControls::ToggleRecordingState(EChaosVDRecordingMode RecordingMode)
{
	TSharedPtr<FChaosVDSessionInfo> SessionInfoPtr = GetCurrentSessionInfo();
	if (!SessionInfoPtr)
	{
		return FReply::Handled();
	}

	if (EnumHasAnyFlags(SessionInfoPtr->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
	{
		ToggleMultiSessionSessionRecordingState(RecordingMode, StaticCastSharedPtr<FChaosVDMultiSessionInfo>(SessionInfoPtr).ToSharedRef());
	}
	else
	{
		ToggleSingleSessionRecordingState(RecordingMode, SessionInfoPtr.ToSharedRef());
	}

	return FReply::Handled();
}

bool SChaosVDRecordingControls::IsRecordingToggleButtonEnabled(EChaosVDRecordingMode RecordingMode) const
{
	if (CurrentSelectedSessionId == FChaosVDRemoteSessionsManager::InvalidSessionGUID)
	{
		return false;
	}

	if (!IsRecording())
	{
		return true;
	}

	// If we are recording, don't show the stop button for the mode that is disabled
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
	{
		if (SessionInfo->GetRecordingMode() == RecordingMode)
		{
			if (EnumHasAnyFlags(SessionInfo->GetSessionTypeAttributes(), EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper))
			{
				TSharedPtr<FChaosVDMultiSessionInfo> AsMultiSessionInfo = StaticCastSharedPtr<FChaosVDMultiSessionInfo>(SessionInfo);

				return CanSelectMultiSessionTarget(SessionInfo.ToSharedRef());
			}

			return true;
		}

		return false;
	}

	return false;
}

EVisibility SChaosVDRecordingControls::IsRecordingToggleButtonVisible(EChaosVDRecordingMode RecordingMode) const
{
	if (!IsRecording())
	{
		return EVisibility::Visible;
	}

	// If we are recording, don't show the stop button for the mode that is disabled
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
	{
		return SessionInfo->GetRecordingMode() == RecordingMode ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

void SChaosVDRecordingControls::RegisterMenus()
{
	const UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(RecordingControlsToolbarName))
	{
		return;
	}

	UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(RecordingControlsToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);

	FToolMenuSection& Section = ToolBar->AddSection("LoadRecording");
	Section.AddDynamicEntry("OpenFile", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UChaosVDRecordingToolbarMenuContext* Context = InSection.FindContext<UChaosVDRecordingToolbarMenuContext>();
		TSharedRef<SChaosVDRecordingControls> RecordingControls = Context->RecordingControlsWidget.Pin().ToSharedRef();

		TSharedRef<SWidget> RecordToFileButton = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateToggleRecordingStateButton(EChaosVDRecordingMode::File, LOCTEXT("RecordToFileButtonDesc", "Starts a recording for the current session, saving it directly to file")) ];
		TSharedRef<SWidget> RecordToLiveButton = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateToggleRecordingStateButton(EChaosVDRecordingMode::Live, LOCTEXT("RecordLiveButtonDesc", "Starts a recording and automatically connects to it playing it back in real time")) ];
		TSharedRef<SWidget> SessionsDropdown = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateTargetSessionSelector() ];
		TSharedRef<SWidget> RecordingTime = RecordingControls->GenerateRecordingTimeTextBlock();
		TSharedRef<SWidget> DataChannelsButton = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateDataChannelsButton() ];
		TSharedRef<SWidget> LoadingModeSelector = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateLoadingModeSelector() ];

		InSection.AddSeparator("RecordingControlsDivider");

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"AvailableSessions",
				SessionsDropdown,
				FText::GetEmpty(),
				false,
				false
			));
		
		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"LoadingModeSelector",
				LoadingModeSelector,
				FText::GetEmpty(),
				false,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"RecordToFileButton",
				RecordToFileButton,
				FText::GetEmpty(),
				true,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"RecordToLiveButton",
				RecordToLiveButton,
				FText::GetEmpty(),
				false,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"RecordingTime",
				RecordingTime,
				FText::GetEmpty(),
				false,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"DataChannelsButton",
				DataChannelsButton,
				FText::GetEmpty(),
				false,
				false
			));
	}));
}

bool SChaosVDRecordingControls::IsRecording() const
{
	TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo();
	return SessionInfo ? SessionInfo->IsRecording() : false;
}

FText SChaosVDRecordingControls::GetRecordingTimeText() const
{
	if (TSharedPtr<FChaosVDSessionInfo> SessionInfo = GetCurrentSessionInfo())
	{
		FNumberFormattingOptions FormatOptions;
		FormatOptions.MinimumFractionalDigits = 2;
		FormatOptions.MaximumFractionalDigits = 2;

		FText SecondsText = FText::AsNumber(SessionInfo->LastKnownRecordingState.ElapsedTime, &FormatOptions);
		return FText::Format(LOCTEXT("RecordingTimer","{0} s"), SecondsText);
	}

	return LOCTEXT("RecordingTimerError","Failed to get time information");
}

TSharedPtr<SNotificationItem> SChaosVDRecordingControls::PushConnectionAttemptNotification()
{
	FNotificationInfo Info(LOCTEXT("ConnectingToSessionMessage", "Connecting Session ..."));
	Info.bFireAndForget = false;
	Info.FadeOutDuration = 3.0f;
	Info.ExpireDuration = 0.0f;

	TSharedPtr<SNotificationItem> ConnectionAttemptNotification = FSlateNotificationManager::Get().AddNotification(Info);
	
	if (ConnectionAttemptNotification.IsValid())
	{
		ConnectionAttemptNotification->SetCompletionState(SNotificationItem::CS_Pending);
		return ConnectionAttemptNotification;
	}

	return nullptr;
}

void SChaosVDRecordingControls::UpdateConnectionAttemptNotification(const TSharedPtr<SNotificationItem>& InNotification, int32 AttemptsRemaining)
{
	if (InNotification)
	{
		InNotification->SetSubText(FText::FormatOrdered(LOCTEXT("SessionConnectionAttemptSubText", "Attempts Remaining {0}"), AttemptsRemaining));
	}
	
}

void SChaosVDRecordingControls::HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult Result, const TSharedPtr<SNotificationItem>& InNotification)
{
	if (InNotification)
	{
		if (Result == EChaosVDLiveConnectionAttemptResult::Success)
		{
			InNotification->SetText(LOCTEXT("SessionConnectionSuccess", "Connected!"));
			InNotification->SetSubText(FText::GetEmpty());
			InNotification->SetCompletionState(SNotificationItem::ECompletionState::CS_Success);
		}
		else
		{
			InNotification->SetText(LOCTEXT("SessionConnectionFailedText", "Failed to connect"));
			InNotification->SetSubText(LOCTEXT("SessionConnectionFailedSubText", "See the logs for more details..."));
			InNotification->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
		}

		InNotification->ExpireAndFadeout();
	}
}

#undef LOCTEXT_NAMESPACE 
