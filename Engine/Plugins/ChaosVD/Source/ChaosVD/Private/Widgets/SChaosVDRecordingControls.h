// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatusBarSubsystem.h"
#include "Chaos/ChaosVDRemoteSessionsManager.h"
#include "ChaosVDEngine.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

#include "SChaosVDRecordingControls.generated.h"

class FMenuBuilder;

namespace Chaos::VisualDebugger
{
	struct FChaosVDOptionalDataChannel;
}

class FAsyncCompilationNotification;
class FReply;
class SButton;
class SChaosVDMainTab;
struct FSlateBrush;
struct FSlateIcon;

UCLASS()
class UChaosVDRecordingToolbarMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<class SChaosVDRecordingControls> RecordingControlsWidget;
};

UENUM()
enum class EChaosVDLiveConnectionAttemptResult
{
	Success,
	Failed
};

typedef Chaos::VisualDebugger::FChaosVDOptionalDataChannel FCVDDataChannel;

class SChaosVDRecordingControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SChaosVDRecordingControls ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SChaosVDMainTab>& InMainTabSharedRef);

	virtual ~SChaosVDRecordingControls() override;

protected:

	TSharedRef<SWidget> GenerateToggleRecordingStateButton(EChaosVDRecordingMode RecordingMode, const FText& StartRecordingTooltip);
	TSharedRef<SWidget> GenerateTargetSessionSelector();
	TSharedRef<SWidget> GenerateTargetSessionDropdown();
	TSharedRef<SWidget> GenerateDataChannelsMenu();
	TSharedRef<SWidget> GenerateDataChannelsButton();
	TSharedRef<SWidget> GenerateLoadingModeSelector();
	TSharedRef<SWidget> GenerateRecordingTimeTextBlock();
	TSharedRef<SWidget> GenerateToolbarWidget();

	EVisibility GetRecordingTimeTextBlockVisibility() const;

	void ToggleChannelEnabledState(FString ChannelName);
	bool IsChannelEnabled(FString ChannelName);
	bool CanChangeChannelEnabledState(FString ChannelName);

	void SelectTargetSession(FGuid SessionId);

	void GenerateCustomTargetsMenu(FMenuBuilder& MenuBuilder);

	bool IsSessionPartOfCustomTargetSelection(FGuid SessionGuid);
	void ToggleSessionSelectionInCustomTarget(FGuid SessionGuid);
	bool CanSelectInCustomTarget(FGuid SessionGuid) const;

	bool CanSelectMultiSessionTarget(FGuid SessionGuid) const;
	bool CanSelectMultiSessionTarget(const TSharedRef<FChaosVDSessionInfo>& SessionInfoRef) const;

	FSlateIcon GetIconForSession(FGuid SessionId);

	TSharedPtr<FChaosVDSessionInfo> GetCurrentSessionInfo() const;
	TSharedPtr<FChaosVDSessionInfo> GetSessionInfo(FGuid Id) const;

	bool HasDataChannelsSupport() const;

	bool CanChangeLoadingMode() const;
	
	const FSlateBrush* GetRecordOrStopButton(EChaosVDRecordingMode RecordingMode) const;
	
	void HandleRecordingStop(TWeakPtr<FChaosVDSessionInfo> SessionInfo);
	void HandleRecordingStart(TWeakPtr<FChaosVDSessionInfo> SessionInfo);

	void ExecuteAsyncConnectionAttemptTaskWithRetry(int32 RemainingRetries, const TFunction<bool()>& InRecordingStartAttemptCallback, const TFunction<void()>& InRecordingFailedCallback);

private:
	void ExecutePostRecordingAsyncTaskWithRetry_Internal(int32 RemainingRetries, TFunction<bool()> RecordingStartAttemptCallback, const TSharedPtr<SNotificationItem>& InProgressNotification, const TFunction<void()> InRecordingFailedCallback);
protected:

	FReply ToggleRecordingState(EChaosVDRecordingMode RecordingMode);
	void ToggleMultiSessionSessionRecordingState(EChaosVDRecordingMode RecordingMode, const TSharedRef<FChaosVDMultiSessionInfo>& InSessionInfoRef);
	void ToggleSingleSessionRecordingState(EChaosVDRecordingMode RecordingMode, const TSharedRef<FChaosVDSessionInfo>& SessionInfoRef);
	void SetSessionRecordingState(bool bIsRecording, EChaosVDRecordingMode RecordingMode, const TSharedRef<FChaosVDSessionInfo>& SessionInfoRef);

	bool IsRecordingToggleButtonEnabled(EChaosVDRecordingMode RecordingMode) const;
	EVisibility IsRecordingToggleButtonVisible(EChaosVDRecordingMode RecordingMode) const;

	void RegisterMenus();

	bool IsRecording() const;

	FText GetRecordingTimeText() const;

	TSharedPtr<SNotificationItem> PushConnectionAttemptNotification();
	void UpdateConnectionAttemptNotification(const TSharedPtr<SNotificationItem>& InNotification, int32 AttemptsRemaining);

	void HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult Result, const TSharedPtr<SNotificationItem>& InNotification);

	struct FLiveConnectionSettings
	{
		int32 TraceID = INDEX_NONE;
		FString SessionAddress;
	};

	FLiveConnectionSettings GetTargetLiveConnectionSettings(const TSharedRef<FChaosVDSessionInfo>& InSessionInfoRef);

	FText GetCurrentSelectedSessionName() const;

	FGuid CurrentSelectedSessionId = FGuid();

	FName StatusBarID;
	
	FStatusBarMessageHandle RecordingMessageHandle;
	FStatusBarMessageHandle RecordingPathMessageHandle;
	FStatusBarMessageHandle LiveSessionEndedMessageHandle;
	FDelegateHandle RecordingStartedHandle;
	FDelegateHandle RecordingStoppedHandle;

	TWeakPtr<SChaosVDMainTab> MainTabWeakPtr;

	float IntervalBetweenAutoplayConnectionAttemptsSeconds = 1.0f;

	bool bRecordingButtonHovered = false;

	FCurveSequence RecordingAnimation;

	EChaosVDLoadRecordedDataMode CurrentLoadingMode = EChaosVDLoadRecordedDataMode::SingleSource;

	struct FPostRecordingCommandAsyncTask
	{
		FGuid OwningSessionId;
		int32 RemainingAttempts;
		
		TFunction<bool()> TaskCallback;
	};

	static const FName RecordingControlsToolbarName;
};
