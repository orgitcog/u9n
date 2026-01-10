// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CaptureManagerEditorSettings.h"
#include "Settings/CaptureManagerEditorTemplateTokens.h"
#include "Misc/Paths.h"

#include "Editor.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserItemPath.h"
#include "IContentBrowserSingleton.h"

#include "LiveLinkHubExportServerModule.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkHubMessagingModule.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/SlateTypes.h"
#include "CaptureManagerStyle.h"


#define LOCTEXT_NAMESPACE "CaptureManagerEditorSettings"

DEFINE_LOG_CATEGORY_STATIC(LogCaptureManagerEditorSettings, Log, All);

#include UE_INLINE_GENERATED_CPP_BY_NAME(CaptureManagerEditorSettings)

namespace UE::CaptureManager::Private
{
constexpr FStringView DefaultMediaDirectory = TEXTVIEW("CaptureManager/Media/{project}/{device}/{slate}_{take}");
constexpr FStringView DefaultImportDirectory = TEXTVIEW("CaptureManager/Imports/{device}/{slate}_{take}");

constexpr FStringView DefaultCaptureDataAssetName = TEXTVIEW("CD_{slate}_{take}");

constexpr FStringView DefaultImageSequenceAssetName = TEXTVIEW("IS_V_{name}_{slate}_{take}");
constexpr FStringView DefaultDepthSequenceAssetName = TEXTVIEW("IS_D_{name}_{slate}_{take}");
constexpr FStringView DefaultAudioAssetName = TEXTVIEW("SW_{name}_{slate}_{take}");
constexpr FStringView DefaultCalibAssetName = TEXTVIEW("CC_{slate}_{take}");
constexpr FStringView DefaultLensFileAssetName = TEXTVIEW("LF_{cameraName}_{slate}_{take}");

enum class ServerLaunchState
{
	SUCCESS = 0,
	FAILURE,
	ALREADY_RUNNING
};
}

void DisplayNotification(UE::CaptureManager::Private::ServerLaunchState InState)
{
	using namespace UE::CaptureManager::Private;

	FString Message;
	SNotificationItem::ECompletionState CompletionState;
	switch (InState)
	{
		case ServerLaunchState::ALREADY_RUNNING:
			Message = "The Ingest Server is already running.";
			CompletionState = SNotificationItem::CS_None;
			break;
		case ServerLaunchState::SUCCESS:
			Message = "Successfully launched the Ingest Server";
			CompletionState = SNotificationItem::CS_Success;
			break;
		case ServerLaunchState::FAILURE:
		default:
			Message = "Failed to launch the Ingest Server";
			CompletionState = SNotificationItem::CS_Fail;
			break;
	}
	FNotificationInfo Info(FText::FromString(Message));
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);

	if (Notification)
	{
		Notification->SetCompletionState(CompletionState);
	}
}

bool UCaptureManagerEditorSettings::StartIngestServer()
{
	using namespace UE::CaptureManager::Private;

	FLiveLinkHubExportServerModule& LiveLinkHubExportServerModule =
		FModuleManager::LoadModuleChecked<FLiveLinkHubExportServerModule>("LiveLinkHubExportServer");

	FText IngestServerStartingMessage = LOCTEXT("IngestServerStarting", "Starting the Ingest Server ...");
	UE_LOG(LogCaptureManagerEditorSettings, Display, TEXT("%s"), *IngestServerStartingMessage.ToString());

	bool bResult = LiveLinkHubExportServerModule.StartExportServer(IngestServerPort);

	DisplayNotification(bResult ? ServerLaunchState::SUCCESS : ServerLaunchState::FAILURE);

	FText Message = bResult ? LOCTEXT("IngestServerStart_Success", "Ingest Server started") : LOCTEXT("IngestServerStart_Failure", "Failed to start the Ingest Server");
	UE_LOG(LogCaptureManagerEditorSettings, Display, TEXT("%s"), *Message.ToString());

	return bResult;
}

void UCaptureManagerEditorSettings::Initialize()
{
	using namespace UE::CaptureManager::Private;

	CategoryName = "Plugins";

	FLiveLinkHubExportServerModule& LiveLinkHubExportServerModule =
		FModuleManager::LoadModuleChecked<FLiveLinkHubExportServerModule>("LiveLinkHubExportServer");

	HubMessagingModule = &FModuleManager::Get().GetModuleChecked<ILiveLinkHubMessagingModule>("LiveLinkHubMessaging");
	HubMessagingModule->OnConnectionEstablished().AddUObject(this, &UCaptureManagerEditorSettings::OnHubConnectionEstablished);

	LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	IModularFeatures::Get().OnModularFeatureUnregistered().AddLambda(
		[this](const FName& InFeatureName, IModularFeature* InFeature)
		{
			if (InFeature == this->LiveLinkClient)
			{
				this->LiveLinkClient = nullptr;
			}
		}
	);

	if (GEditor)
	{
		// Start timer to monitor live link hub connections
		constexpr bool bLoop = true;
		if (GEditor->IsTimerManagerValid())
		{
			GEditor->GetTimerManager()->SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &UCaptureManagerEditorSettings::CheckHubConnection), CheckConnectionIntervalSeconds, bLoop);
		}

		// Add tool menu to launch the ingest server
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("VirtualProductionSection", LOCTEXT("VirtualProductionSection", "Virtual Production"));

		Section.AddMenuEntry("IngestServer",
			LOCTEXT("IngestServerLabel", "Ingest Server"),
			LOCTEXT("IngestServerTooltip", "Launch the Capture Manager Ingest Server."),
			FSlateIcon(FCaptureManagerStyle::Get().GetStyleSetName(), "CaptureManagerIcon"),
			FUIAction(FExecuteAction::CreateLambda([this, &LiveLinkHubExportServerModule]() {
				if (!LiveLinkHubExportServerModule.IsExportServerRunning())
				{
					StartIngestServer();
				}
				else
				{
					DisplayNotification(ServerLaunchState::ALREADY_RUNNING);
				}
				})
			)
		);
	}
	
	InitializeValuesIfNotSet();

	GeneralNamingTokens = NewObject<UCaptureManagerIngestNamingTokens>(this, UCaptureManagerIngestNamingTokens::StaticClass());
	GeneralNamingTokens->CreateDefaultTokens();

	VideoNamingTokens = NewObject<UCaptureManagerVideoNamingTokens>(this, UCaptureManagerVideoNamingTokens::StaticClass());
	VideoNamingTokens->CreateDefaultTokens();

	AudioNamingTokens = NewObject<UCaptureManagerAudioNamingTokens>(this, UCaptureManagerAudioNamingTokens::StaticClass());
	AudioNamingTokens->CreateDefaultTokens();

	CalibrationNamingTokens = NewObject<UCaptureManagerCalibrationNamingTokens>(this, UCaptureManagerCalibrationNamingTokens::StaticClass());
	CalibrationNamingTokens->CreateDefaultTokens();

	LensFileNamingTokens = NewObject<UCaptureManagerLensFileNamingTokens>(this, UCaptureManagerLensFileNamingTokens::StaticClass());
	LensFileNamingTokens->CreateDefaultTokens();
}

TObjectPtr<const UCaptureManagerIngestNamingTokens> UCaptureManagerEditorSettings::GetGeneralNamingTokens() const
{
	return GeneralNamingTokens;
}

TObjectPtr<const UCaptureManagerVideoNamingTokens> UCaptureManagerEditorSettings::GetVideoNamingTokens() const
{
	return VideoNamingTokens;
}

TObjectPtr<const UCaptureManagerAudioNamingTokens> UCaptureManagerEditorSettings::GetAudioNamingTokens() const
{
	return AudioNamingTokens;
}

TObjectPtr<const UCaptureManagerCalibrationNamingTokens> UCaptureManagerEditorSettings::GetCalibrationNamingTokens() const
{
	return CalibrationNamingTokens;
}

TObjectPtr<const UCaptureManagerLensFileNamingTokens> UCaptureManagerEditorSettings::GetLensFileNamingTokens() const
{
	return LensFileNamingTokens;
}

FString UCaptureManagerEditorSettings::GetVerifiedImportDirectory()
{
	if (ImportDirectory.Path.IsEmpty())
	{
		ResetImportDirectory();
	}
	else
	{
		FString BaseImportDirectory = GetBaseImportDirectory();
		if (BaseImportDirectory != CachedBaseImportDirectory)
		{
			ImportDirectory.Path.ReplaceInline(*CachedBaseImportDirectory, *BaseImportDirectory);
			CachedBaseImportDirectory = MoveTemp(BaseImportDirectory);
		}
	}

	return ImportDirectory.Path;
}

void UCaptureManagerEditorSettings::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	InitializeValuesIfNotSet();
}

FString UCaptureManagerEditorSettings::GetBaseImportDirectory() const
{
	static const FString DefaultRelativePath = TEXT("/Game/");
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	// Default asset creation path is usually the root project folder
	return ContentBrowserModule.Get().GetInitialPathToSaveAsset(FContentBrowserItemPath(DefaultRelativePath, EContentBrowserPathType::Internal)).GetInternalPathString();
}

void UCaptureManagerEditorSettings::ResetImportDirectory()
{
	using namespace UE::CaptureManager;

	CachedBaseImportDirectory = GetBaseImportDirectory();
	ImportDirectory.Path = FPaths::Combine(CachedBaseImportDirectory, Private::DefaultImportDirectory);
}

void UCaptureManagerEditorSettings::InitializeValuesIfNotSet()
{
	using namespace UE::CaptureManager;

	if (MediaDirectory.Path.IsEmpty())
	{
		MediaDirectory.Path = FPaths::Combine(FPlatformProcess::UserDir(), Private::DefaultMediaDirectory);
	}

	if (ImportDirectory.Path.IsEmpty())
	{
		ResetImportDirectory();
	}

	if (CaptureDataAssetName.IsEmpty())
	{
		CaptureDataAssetName = Private::DefaultCaptureDataAssetName;
	}

	if (ImageSequenceAssetName.IsEmpty())
	{
		ImageSequenceAssetName = Private::DefaultImageSequenceAssetName;
	}

	if (DepthSequenceAssetName.IsEmpty())
	{
		DepthSequenceAssetName = Private::DefaultDepthSequenceAssetName;
	}

	if (SoundwaveAssetName.IsEmpty())
	{
		SoundwaveAssetName = Private::DefaultAudioAssetName;
	}

	if (CalibrationAssetName.IsEmpty())
	{
		CalibrationAssetName = Private::DefaultCalibAssetName;
	}

	if (LensFileAssetName.IsEmpty())
	{
		LensFileAssetName = Private::DefaultLensFileAssetName;
	}
}

void UCaptureManagerEditorSettings::OnHubConnectionEstablished(FGuid SourceId)
{
	using namespace UE::CaptureManager::Private;

	DetectedHubsArray.Emplace(SourceId);

	if (bLaunchIngestServerOnLiveLinkHubConnection)
	{
		FLiveLinkHubExportServerModule& LiveLinkHubExportServerModule =
			FModuleManager::LoadModuleChecked<FLiveLinkHubExportServerModule>("LiveLinkHubExportServer");

		if (!LiveLinkHubExportServerModule.IsExportServerRunning())
		{
			FText ConnectedToHubMessage = FText::Format(LOCTEXT("ConnectedToHubInstance", "Connected to Live Link Hub instance [{0}]"), FText::FromString(SourceId.ToString()));
			UE_LOG(LogCaptureManagerEditorSettings, Display, TEXT("%s"), *ConnectedToHubMessage.ToString());
			StartIngestServer();
		}
	}
}

void UCaptureManagerEditorSettings::CheckHubConnection()
{
	if (bLaunchIngestServerOnLiveLinkHubConnection)
	{
		using namespace UE::CaptureManager::Private;

		FLiveLinkHubExportServerModule& LiveLinkHubExportServerModule =
			FModuleManager::LoadModuleChecked<FLiveLinkHubExportServerModule>("LiveLinkHubExportServer");

		TArray<FGuid> InvalidSources;

		for (const FGuid& HubId : DetectedHubsArray)
		{
			if (LiveLinkClient && LiveLinkClient->IsSourceStillValid(HubId))
			{
				// At least one source is still valid, so try to launch server if not already running
				if (!LiveLinkHubExportServerModule.IsExportServerRunning())
				{	
					if (StartIngestServer())
					{
						return;
					}
				}
			}
			else
			{
				InvalidSources.Add(HubId);
			}
		}

		DetectedHubsArray.RemoveAll([&InvalidSources](const auto& Item)
			{
				return InvalidSources.Contains(Item);
			});
	}
}

#undef LOCTEXT_NAMESPACE
