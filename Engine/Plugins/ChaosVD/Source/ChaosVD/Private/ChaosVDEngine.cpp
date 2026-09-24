// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEngine.h"

#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDScene.h"
#include "ChaosVDSettingsManager.h"
#include "Chaos/ChaosVDEngineEditorBridge.h"
#include "ChaosVisualDebugger/ChaosVDOptionalDataChannel.h"
#include "Settings/ChaosVDMiscSettings.h"
#include "Trace/ChaosVDCombinedTraceFile.h"
#include "Trace/ChaosVDTraceManager.h"

void FChaosVDEngine::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}

	// Create an Empty Scene
	CurrentScene = MakeShared<FChaosVDScene>();
	CurrentScene->Initialize();

	PlaybackController = MakeShared<FChaosVDPlaybackController>(CurrentScene);

	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		LiveSessionStoppedDelegateHandle = RemoteSessionManager->OnSessionRecordingStopped().AddLambda([WeakThis = AsWeak()](TWeakPtr<FChaosVDSessionInfo> Session)
		{
			if (const TSharedPtr<FChaosVDEngine> CVDEngine = WeakThis.Pin())
			{
				if (CVDEngine->HasAnyLiveSessionActive())
				{
					CVDEngine->StopActiveTraceSessions();

					if (CVDEngine->PlaybackController)
					{
						CVDEngine->PlaybackController->HandleDisconnectedFromSession();
					}
				}
			}
		});
	}

	RestoreDataChannelsEnabledStateFromSave();
	
	bIsInitialized = true;
}

void FChaosVDEngine::CloseActiveTraceSessions()
{
	if (const TSharedPtr<FChaosVDTraceManager> CVDTraceManager = FChaosVDModule::Get().GetTraceManager())
	{
		for (FChaosVDTraceSessionDescriptor& SessionDescriptor : CurrentSessionDescriptors)
		{
			OnSessionClosed().Broadcast(SessionDescriptor);

			CVDTraceManager->CloseSession(SessionDescriptor.SessionName);
			SessionDescriptor.bIsLiveSession = false;
		}
	}

	CurrentSessionDescriptors.Reset();

	if (PlaybackController)
	{
		PlaybackController->UnloadCurrentRecording(EChaosVDUnloadRecordingFlags::BroadcastChanges);
	}
}

void FChaosVDEngine::StopActiveTraceSessions()
{
	if (const TSharedPtr<FChaosVDTraceManager> CVDTraceManager = FChaosVDModule::Get().GetTraceManager())
	{
		for (FChaosVDTraceSessionDescriptor& SessionDescriptor : CurrentSessionDescriptors)
		{
			CVDTraceManager->StopSession(SessionDescriptor.SessionName);
		}
	}
}

void FChaosVDEngine::DeInitialize()
{
	if (!bIsInitialized)
	{
		return;
	}

	CurrentScene->DeInitialize();
	CurrentScene.Reset();
	PlaybackController.Reset();

	CloseActiveTraceSessions();

	if (TSharedPtr<FChaosVDRemoteSessionsManager> RemoteSessionManager = FChaosVDEngineEditorBridge::Get().GetRemoteSessionsManager())
	{
		RemoteSessionManager->OnSessionRecordingStopped().Remove(LiveSessionStoppedDelegateHandle);
	}

	LiveSessionStoppedDelegateHandle = FDelegateHandle();

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

#if WITH_CHAOS_VISUAL_DEBUGGER

	if (DataChannelStateUpdatedHandle.IsValid())
	{
		Chaos::VisualDebugger::FChaosVDDataChannelsManager::Get().OnChannelStateChanged().Remove(DataChannelStateUpdatedHandle);
	}

#endif

	bIsInitialized = false;
}

void FChaosVDEngine::RestoreDataChannelsEnabledStateFromSave()
{
#if WITH_CHAOS_VISUAL_DEBUGGER

	UChaosVDMiscSettings* MiscSettings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDMiscSettings>();
	if (!MiscSettings)
	{
		return;
	}

	using namespace Chaos::VisualDebugger;
	FChaosVDDataChannelsManager::Get().EnumerateChannels([MiscSettings](const TSharedRef<FChaosVDOptionalDataChannel>& Channel)
	{
		if (bool* SavedEnabledState = MiscSettings->DataChannelEnabledState.Find(Channel->GetId().ToString()))
		{
			Channel->SetChannelEnabled(*SavedEnabledState);
		}
		return true;
	});

	DataChannelStateUpdatedHandle = FChaosVDDataChannelsManager::Get().OnChannelStateChanged().AddSP(this, &FChaosVDEngine::UpdateSavedDataChannelsEnabledState);

#endif
}

void FChaosVDEngine::UpdateSavedDataChannelsEnabledState(TWeakPtr<Chaos::VisualDebugger::FChaosVDOptionalDataChannel> DataChannelChanged)
{
#if WITH_CHAOS_VISUAL_DEBUGGER

	UChaosVDMiscSettings* MiscSettings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDMiscSettings>();
	if (!MiscSettings)
	{
		return;
	}

	if (TSharedPtr<Chaos::VisualDebugger::FChaosVDOptionalDataChannel> DataChannelPtr = DataChannelChanged.Pin())
	{
		MiscSettings->DataChannelEnabledState.FindOrAdd(DataChannelPtr->GetId().ToString()) = DataChannelPtr->IsChannelEnabled();
	}

	MiscSettings->SaveConfig();

#endif
}

void FChaosVDEngine::UpdateRecentFilesList(const FString& InFilename)
{
	UChaosVDMiscSettings* MiscSettings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDMiscSettings>();
	if (!MiscSettings)
	{
		return;
	}

	FDateTime CurrentTime = FDateTime::UtcNow();
	if (FChaosVDRecentFile* RecentProject = MiscSettings->RecentFiles.FindByKey(InFilename))
	{
		RecentProject->LastOpenTime = CurrentTime;
	}
	else
	{
		FChaosVDRecentFile RecentFileEntry(InFilename, CurrentTime);
		MiscSettings->RecentFiles.Emplace(RecentFileEntry);
	}
	
	MiscSettings->RecentFiles.Sort(FChaosVDRecentFile::FRecentFilesSortPredicate());

	if (MiscSettings->RecentFiles.Num() > MiscSettings->MaxRecentFilesNum)
	{
		MiscSettings->RecentFiles.SetNum(MiscSettings->MaxRecentFilesNum);
	}

	MiscSettings->SaveConfig();
}

bool FChaosVDEngine::HasAnyLiveSessionActive() const
{
	return PlaybackController && PlaybackController->IsPlayingLiveSession();
}

void FChaosVDEngine::LoadRecording_Internal(const TFunction<FString(const TSharedPtr<FChaosVDRecording>&)>& LoadCallback, EChaosVDLoadRecordedDataMode LoadingMode)
{
	FChaosVDTraceSessionDescriptor NewSessionFromFileDescriptor;

	TSharedPtr<FChaosVDRecording> ExistingRecordingInstance = nullptr;
	if (LoadingMode == EChaosVDLoadRecordedDataMode::MultiSource && !CurrentSessionDescriptors.IsEmpty())
	{
		if (const TSharedPtr<const TraceServices::IAnalysisSession> TraceSession = FChaosVDModule::Get().GetTraceManager()->GetSession(CurrentSessionDescriptors[0].SessionName))
		{
			if (const FChaosVDTraceProvider* ChaosVDProvider = TraceSession->ReadProvider<FChaosVDTraceProvider>(FChaosVDTraceProvider::ProviderName))
			{
				ExistingRecordingInstance = ChaosVDProvider->GetRecordingForSession();
			}
		}
	}
	
	NewSessionFromFileDescriptor.SessionName = LoadCallback(ExistingRecordingInstance);
	
	NewSessionFromFileDescriptor.bIsLiveSession = false;

	if (NewSessionFromFileDescriptor.IsValid())
	{
		OpenSession(NewSessionFromFileDescriptor, LoadingMode);
	}
}

void FChaosVDEngine::LoadRecording(const FString& FilePath, EChaosVDLoadRecordedDataMode LoadingMode)
{
	if (FilePath.EndsWith(TEXT("cvdmulti")))
	{
		LoadCombinedMultiRecording(FilePath);
		return;
	}

	auto LoadSingleFileFromTraceManager = [&FilePath](const TSharedPtr<FChaosVDRecording>& ExistingRecordingInstance)
	{
		return FChaosVDModule::Get().GetTraceManager()->LoadTraceFile(FilePath, ExistingRecordingInstance);
	};
	
	LoadRecording_Internal(LoadSingleFileFromTraceManager, LoadingMode);
	UpdateRecentFilesList(FilePath);
}

void FChaosVDEngine::LoadCombinedMultiRecording(const FString& FilePath)
{
	if (!ensure(FilePath.EndsWith(TEXT("cvdmulti"))))
	{
		return;
	}

	using namespace Chaos::VisualDebugger;

	TArray<TUniquePtr<IFileHandle>> ExtractedHandles = CombinedTraceFile::GetInnerFileHandles(FilePath);
	
	if (!ensure(!ExtractedHandles.IsEmpty()))
	{
		return;
	}

	int32 CurrentFileIndex = 0;
	for (TUniquePtr<IFileHandle>& Handle : ExtractedHandles)
	{
		const FStringFormatOrderedArguments Args {FilePath, FString::FromInt(CurrentFileIndex)};
		FString SessionName = FString::Format(TEXT("{0}-{1}"), Args);
		auto LoadSingleFileHandleFromTraceManager = [&Handle, &SessionName](const TSharedPtr<FChaosVDRecording>& ExistingRecordingInstance)
		{
			return FChaosVDModule::Get().GetTraceManager()->LoadTraceFile(MoveTemp(Handle), SessionName, ExistingRecordingInstance);
		};

		LoadRecording_Internal(LoadSingleFileHandleFromTraceManager, EChaosVDLoadRecordedDataMode::MultiSource);
		CurrentFileIndex++;
	}
	
	UpdateRecentFilesList(FilePath);
}

bool FChaosVDEngine::ConnectToLiveSession(uint32 SessionID, const FString& InSessionAddress, EChaosVDLoadRecordedDataMode LoadingMode)
{
	FChaosVDTraceSessionDescriptor NewSessionFromFileDescriptor;

	NewSessionFromFileDescriptor.bIsLiveSession = true;
	
	TSharedPtr<FChaosVDRecording> ExistingRecordingInstance = nullptr;
	if (LoadingMode == EChaosVDLoadRecordedDataMode::MultiSource && PlaybackController)
	{
		ExistingRecordingInstance = PlaybackController->GetCurrentRecording().Pin();
	}

	NewSessionFromFileDescriptor.SessionName = FChaosVDModule::Get().GetTraceManager()->ConnectToLiveSession(InSessionAddress, SessionID, ExistingRecordingInstance);

	bool bSuccess = false;

	if (NewSessionFromFileDescriptor.IsValid())
	{
		OpenSession(NewSessionFromFileDescriptor, LoadingMode);
		bSuccess = true;
	}

	return bSuccess;
}

void FChaosVDEngine::OpenSession(const FChaosVDTraceSessionDescriptor& SessionDescriptor, EChaosVDLoadRecordedDataMode LoadingMode)
{
	if (LoadingMode == EChaosVDLoadRecordedDataMode::SingleSource)
	{
		CloseActiveTraceSessions();
	}

	CurrentSessionDescriptors.Emplace(SessionDescriptor);

	if (CurrentSessionDescriptors.Num() <= 1)
	{
		PlaybackController->LoadChaosVDRecordingFromTraceSession(SessionDescriptor);
	}

	OnSessionOpened().Broadcast(SessionDescriptor);
}

bool FChaosVDEngine::SaveOpenSessionToCombinedFile(const FString& InTargetFilePath)
{
	if (!ensure(CurrentSessionDescriptors.Num() > 0))
	{
		return false;
	}

	IPlatformFile& FileSystem = IPlatformFile::GetPlatformPhysical();

	TArray<TUniquePtr<IFileHandle>> FilePathsToCombine;
	FilePathsToCombine.Reserve(CurrentSessionDescriptors.Num());

	for (FChaosVDTraceSessionDescriptor& SessionDescriptor : CurrentSessionDescriptors)
	{
		if (SessionDescriptor.SessionName.EndsWith(TEXT("cvdmulti")))
		{
			UE_LOG(LogChaosVDEditor, Error, TEXT("Failed to combine files, because one of the files is a multi cvd file and that is not supported yet | [%s]"), *SessionDescriptor.SessionName)
			return false;
		}
	
		IFileHandle* ContainerFileHandle = FileSystem.OpenRead(*SessionDescriptor.SessionName);
		if (!ContainerFileHandle)
		{
			UE_LOG(LogChaosVDEditor, Error, TEXT("Failed to combine files, because one fo the files cannot be open | [%s]"), *SessionDescriptor.SessionName)
			return false;
		}

		FilePathsToCombine.Emplace(TUniquePtr<IFileHandle>(ContainerFileHandle));
	}

	FString FinalFilePath = InTargetFilePath;

	if (FinalFilePath.IsEmpty())
	{
		FString PathPart;
		FString FilenamePart;
		FString ExtensionPart;
		FPaths::Split(CurrentSessionDescriptors[0].SessionName, PathPart, FilenamePart, ExtensionPart);

		FStringFormatOrderedArguments NameArgs { FilenamePart, CurrentSessionDescriptors.Num(),FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) };
		FinalFilePath = FPaths::Combine(PathPart, FString::Format(TEXT("ChaosVD-{0}-Combined-{1}-Sessions-{2}.cvdmulti"), NameArgs));
	}
	
	using namespace Chaos::VisualDebugger::CombinedTraceFile;

	return ensure(CombineFiles(FilePathsToCombine, FinalFilePath));
}

bool FChaosVDEngine::CanCombineOpenSessions() const
{
	if (CurrentSessionDescriptors.Num() > 1)
	{
		for (const FChaosVDTraceSessionDescriptor& Session : CurrentSessionDescriptors)
		{
			if (Session.SessionName.EndsWith(TEXT("cvdmulti")))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool FChaosVDEngine::Tick(float DeltaTime)
{
	return true;
}
