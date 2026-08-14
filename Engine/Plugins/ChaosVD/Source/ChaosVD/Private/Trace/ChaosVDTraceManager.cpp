// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceManager.h"

#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
#include "Features/IModularFeatures.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Trace/ChaosVDTraceModule.h"
#include "Trace/StoreClient.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/Model/AnalysisSession.h"

FChaosVDTraceManager::FChaosVDTraceManager() 
{
	ChaosVDTraceModule = MakeShared<FChaosVDTraceModule>();
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, ChaosVDTraceModule.Get());

	FString ChannelNameFString(TEXT("ChaosVD"));
	UE::Trace::ToggleChannel(ChannelNameFString.GetCharArray().GetData(), true);
}

FChaosVDTraceManager::~FChaosVDTraceManager()
{
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, ChaosVDTraceModule.Get());
}

FString FChaosVDTraceManager::LoadTraceFile(const FString& InTraceFilename, const TSharedPtr<FChaosVDRecording>& ExistingRecordingPtr)
{
	CloseSession(InTraceFilename);

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
	{
		SetPendingExternalRecordingToProcess(ExistingRecordingPtr);

		if (const TSharedPtr<const TraceServices::IAnalysisSession> NewSession = TraceAnalysisService->StartAnalysis(*InTraceFilename))
		{
			AnalysisSessionByName.Add(InTraceFilename, NewSession.ToSharedRef());

			return NewSession->GetName();
		}
	}

	return FString();
}

FString FChaosVDTraceManager::LoadTraceFile(TUniquePtr<IFileHandle>&& InFileHandle, const FString& InTraceSessionName, const TSharedPtr<FChaosVDRecording>& ExistingRecordingPtr)
{
	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
	{
		SetPendingExternalRecordingToProcess(ExistingRecordingPtr);

		if (const TSharedPtr<const TraceServices::IAnalysisSession> NewSession = TraceAnalysisService->StartAnalysis(~0, *InTraceSessionName, CreateFileDataStream(MoveTemp(InFileHandle))))
		{
			AnalysisSessionByName.Add(InTraceSessionName, NewSession.ToSharedRef());

			return NewSession->GetName();
		}
	}

	return FString();
}

const UE::Trace::FStoreClient::FSessionInfo* FChaosVDTraceManager::GetTraceSessionInfo(FStringView InSessionHost, FGuid TraceGuid)
{
	if (InSessionHost.IsEmpty())
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to trace store. Provided session host is empty"), ANSI_TO_TCHAR(__FUNCTION__));
		return nullptr;
	}

	using namespace UE::Trace;
	const FStoreClient* StoreClient = FStoreClient::Connect(InSessionHost.GetData());

	if (!StoreClient)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to trace store at [%s]"), ANSI_TO_TCHAR(__FUNCTION__), InSessionHost.GetData())
		return nullptr;
	}

	return StoreClient->GetSessionInfoByGuid(TraceGuid);
}

TUniquePtr<UE::Trace::IInDataStream> FChaosVDTraceManager::CreateFileDataStream(TUniquePtr<IFileHandle>&& InFileHandle)
{
	check(InFileHandle);

	struct FFileDataStream : public UE::Trace::FFileDataStream
	{
		virtual int32 Read(void* Data, uint32 Size) override
		{
			if (Remaining <= 0)
			{
				return 0;
			}
			if (Size > Remaining)
			{
				Size = static_cast<uint32>(Remaining);
			}
			Remaining -= Size;
			if (!Handle->Read(static_cast<uint8*>(Data), Size))
			{
				return 0;
			}
			return Size;
		}

		TUniquePtr<IFileHandle> Handle;
		uint64 Remaining;
	};

	FFileDataStream* FileStream = new FFileDataStream();
	FileStream->Handle = MoveTemp(InFileHandle);
	FileStream->Remaining = FileStream->Handle->Size();

	TUniquePtr<UE::Trace::IInDataStream> DataStream(FileStream);

	return DataStream;
}

void FChaosVDTraceManager::SetPendingExternalRecordingToProcess(const TSharedPtr<FChaosVDRecording>& ExistingRecordingPtr)
{
	TWeakPtr<FChaosVDRecording>& PendingRecording = FChaosVDTraceManagerThreadContext::Get().PendingExternalRecordingWeakPtr;
	ensureMsgf(!PendingRecording.Pin(), TEXT("Attempted to start a secondary trace session before a pending recording instance was processed"));
	PendingRecording = ExistingRecordingPtr;
}

FString FChaosVDTraceManager::ConnectToLiveSession(FStringView InSessionHost, uint32 SessionID, const TSharedPtr<FChaosVDRecording>& ExistingRecordingPtr)
{
	FString SessionName;

	if (InSessionHost.IsEmpty())
	{
		return SessionName;
	}

	using namespace UE::Trace;
	FStoreClient* StoreClient = FStoreClient::Connect(InSessionHost.GetData());

	if (!StoreClient)
	{
		return SessionName;
	}

	FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(SessionID);
	if (!TraceData.IsValid())
	{
		return SessionName;
	}

	FString TraceName(StoreClient->GetStatus()->GetStoreDir());
	const FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(SessionID);
	if (TraceInfo != nullptr)
	{
		const FUtf8StringView Utf8NameView = TraceInfo->GetName();
		FString Name(Utf8NameView);
		if (!Name.EndsWith(TEXT(".utrace")))
		{
			Name += TEXT(".utrace");
		}
		TraceName = FPaths::Combine(TraceName, Name);
		FPaths::NormalizeFilename(TraceName);
	}

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
	{
		// Close this session in case we were already analysing it
		CloseSession(TraceName);
		
		SetPendingExternalRecordingToProcess(ExistingRecordingPtr);
	
		if (const TSharedPtr<const TraceServices::IAnalysisSession> NewSession = TraceAnalysisService->StartAnalysis(SessionID, *TraceName, MoveTemp(TraceData)))
		{
			AnalysisSessionByName.Add(TraceName, NewSession.ToSharedRef());

			SessionName = NewSession->GetName();
		}
	}

	return SessionName;
}

FString FChaosVDTraceManager::GetLocalTraceStoreDirPath()
{
	UE::Trace::FStoreClient* StoreClient = UE::Trace::FStoreClient::Connect(TEXT("localhost"));

	if (!StoreClient)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to local Trace Store client"), ANSI_TO_TCHAR(__FUNCTION__));
		return TEXT("");
	}

	const UE::Trace::FStoreClient::FStatus* Status = StoreClient->GetStatus();
	if (!Status)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to to get Trace Store status"), ANSI_TO_TCHAR(__FUNCTION__));
		return TEXT("");
	}

	return FString(Status->GetStoreDir());
}

TSharedPtr<const TraceServices::IAnalysisSession> FChaosVDTraceManager::GetSession(const FString& InSessionName)
{
	if (TSharedPtr<const TraceServices::IAnalysisSession>* FoundSession = AnalysisSessionByName.Find(InSessionName))
	{
		return *FoundSession;
	}

	return nullptr;
}

void FChaosVDTraceManager::CloseSession(const FString& InSessionName)
{
	if (const TSharedPtr<const TraceServices::IAnalysisSession>* Session = AnalysisSessionByName.Find(InSessionName))
	{
		if (Session->IsValid())
		{
			(*Session)->Stop(true);
		}

		AnalysisSessionByName.Remove(InSessionName);
	}
}

void FChaosVDTraceManager::StopSession(const FString& InSessionName)
{
	if (const TSharedPtr<const TraceServices::IAnalysisSession>* Session = AnalysisSessionByName.Find(InSessionName))
	{
		if (Session->IsValid())
		{
			(*Session)->Stop(true);
		}
	}
}
