// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/AudioBusProvider.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioInsightsEditorDashboardFactory.h"
#include "AudioInsightsEditorModule.h"
#include "AudioMixerTrace.h"
#include "Sound/AudioBus.h"

namespace UE::Audio::Insights
{
	FAudioBusProvider::FAudioBusProvider()
		: TDeviceDataMapTraceProvider<uint32, TSharedPtr<FAudioBusAssetDashboardEntry>>(GetName_Static())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FAudioBusProvider::OnAssetAdded);
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FAudioBusProvider::OnAssetRemoved);
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAudioBusProvider::OnFilesLoaded);

		FEditorDashboardFactory::OnActiveAudioDeviceChanged.AddRaw(this, &FAudioBusProvider::OnActiveAudioDeviceChanged);

#if UE_AUDIO_PROFILERTRACE_ENABLED
		FTraceAuxiliary::OnTraceStarted.AddRaw(this, &FAudioBusProvider::OnTraceStarted);
#endif
	}

	FAudioBusProvider::~FAudioBusProvider()
	{
		if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
		{
			AssetRegistryModule->Get().OnAssetAdded().RemoveAll(this);
			AssetRegistryModule->Get().OnAssetRemoved().RemoveAll(this);
			AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
		}

		FEditorDashboardFactory::OnActiveAudioDeviceChanged.RemoveAll(this);

#if UE_AUDIO_PROFILERTRACE_ENABLED
		FTraceAuxiliary::OnTraceStarted.RemoveAll(this);
#endif // UE_AUDIO_PROFILERTRACE_ENABLED
	}

	FName FAudioBusProvider::GetName_Static()
	{
		return "AudioBusesProvider";
	}

		void FAudioBusProvider::OnAssetAdded(const FAssetData& InAssetData)
	{
		if (bAreFilesLoaded && InAssetData.AssetClassPath == FTopLevelAssetPath(UAudioBus::StaticClass()))
		{
			AddAudioBusAsset(InAssetData);
		}
	}

	void FAudioBusProvider::OnAssetRemoved(const FAssetData& InAssetData)
	{
		if (InAssetData.AssetClassPath == FTopLevelAssetPath(UAudioBus::StaticClass()))
		{
			RemoveAudioBusAsset(InAssetData);
		}
	}

	void FAudioBusProvider::OnFilesLoaded()
	{
		bAreFilesLoaded = true;
		UpdateAudioBusAssetNames();
	}

	void FAudioBusProvider::OnActiveAudioDeviceChanged()
	{
		UpdateAudioBusAssetNames();
	}

	void FAudioBusProvider::RequestEntriesUpdate()
	{
		UpdateAudioBusAssetNames();
	}

	void FAudioBusProvider::OnTraceStarted(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination)
	{
#if UE_AUDIO_PROFILERTRACE_ENABLED
		UpdateAudioBusAssetNames();
#endif // UE_AUDIO_PROFILERTRACE_ENABLED
	}

	void FAudioBusProvider::AddAudioBusAsset(const FAssetData& InAssetData)
	{
		const bool bIsAudioBusAssetAlreadAdded = AudioBusDataViewEntries.ContainsByPredicate(
			[AssetName = InAssetData.GetObjectPathString()](const TSharedPtr<FAudioBusAssetDashboardEntry>& AudioBusAssetDashboardEntry)
			{
				return AudioBusAssetDashboardEntry->Name == AssetName;
			});

		if (!bIsAudioBusAssetAlreadAdded)
		{
			const FAudioInsightsEditorModule AudioInsightsEditorModule = FAudioInsightsEditorModule::GetChecked();
			const ::Audio::FDeviceId AudioDeviceId = AudioInsightsEditorModule.GetDeviceId();

			TSharedPtr<FAudioBusAssetDashboardEntry> AudioBusAssetDashboardEntry = MakeShared<FAudioBusAssetDashboardEntry>();
			AudioBusAssetDashboardEntry->DeviceId   = AudioDeviceId;
			AudioBusAssetDashboardEntry->EntryType  = EAudioBusEntryType::AssetBased;
			AudioBusAssetDashboardEntry->Name       = InAssetData.GetObjectPathString();
			AudioBusAssetDashboardEntry->AudioBus   = Cast<UAudioBus>(InAssetData.GetAsset());
			AudioBusAssetDashboardEntry->AudioBusId = AudioBusAssetDashboardEntry->AudioBus.IsValid() ? AudioBusAssetDashboardEntry->AudioBus->GetUniqueID() : INDEX_NONE;

			AudioBusDataViewEntries.Add(MoveTemp(AudioBusAssetDashboardEntry));

			OnAudioBusAssetAdded.Broadcast(InAssetData.GetAsset());

			bAssetEntriesNeedRefreshing = true;
			++LastUpdateId;
		}
	}

	void FAudioBusProvider::RemoveAudioBusAsset(const FAssetData& InAssetData)
	{
		const int32 FoundAudioBusAssetNameIndex = AudioBusDataViewEntries.IndexOfByPredicate(
			[AssetName = InAssetData.GetObjectPathString()](const TSharedPtr<FAudioBusAssetDashboardEntry>& AudioBusAssetDashboardEntry)
			{
				return AudioBusAssetDashboardEntry->Name == AssetName;
			});

		if (FoundAudioBusAssetNameIndex != INDEX_NONE)
		{
			const FAudioInsightsEditorModule AudioInsightsEditorModule = FAudioInsightsEditorModule::GetChecked();
			const ::Audio::FDeviceId AudioDeviceId = AudioInsightsEditorModule.GetDeviceId();

			RemoveDeviceEntry(AudioDeviceId, AudioBusDataViewEntries[FoundAudioBusAssetNameIndex]->AudioBus->GetUniqueID());

			AudioBusDataViewEntries.RemoveAt(FoundAudioBusAssetNameIndex);

			OnAudioBusAssetRemoved.Broadcast(InAssetData.GetAsset());

			bAssetEntriesNeedRefreshing = true;
			++LastUpdateId;
		}
	}

	void FAudioBusProvider::UpdateAudioBusAssetNames()
	{
		// Get all UAudioBus assets
		TArray<FAssetData> AssetDataArray;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(UAudioBus::StaticClass()), AssetDataArray);

		// Build AudioBusDataViewEntries
		Reset();
		AudioBusDataViewEntries.Empty();

		for (const FAssetData& AssetData : AssetDataArray)
		{
			AddAudioBusAsset(AssetData);
		}

		AudioBusDataViewEntries.Sort([](const TSharedPtr<FAudioBusAssetDashboardEntry>& A, const TSharedPtr<FAudioBusAssetDashboardEntry>& B)
		{
			return A->GetDisplayName().CompareToCaseIgnored(B->GetDisplayName()) < 0;
		});

		OnAudioBusAssetListUpdated.Broadcast();
	}

	bool FAudioBusProvider::ProcessMessages()
	{
		// Helper lambdas
		auto BumpEntryFunc = [this](const FAudioBusMessageBase& Msg)
		{
			TSharedPtr<FAudioBusAssetDashboardEntry>* ToReturn = nullptr;

			UpdateDeviceEntry(Msg.DeviceId, Msg.AudioBusId, [&ToReturn, &Msg](TSharedPtr<FAudioBusAssetDashboardEntry>& Entry)
			{
				if (!Entry.IsValid())
				{
					Entry = MakeShared<FAudioBusAssetDashboardEntry>();
					Entry->DeviceId = Msg.DeviceId;
					Entry->AudioBusId = Msg.AudioBusId;
				}

				Entry->Timestamp = Msg.Timestamp;

				ToReturn = &Entry;
			});

			return ToReturn;
		};

		auto GetEntry = [this](const FAudioBusMessageBase& Msg)
		{
			return FindDeviceEntry(Msg.DeviceId, Msg.AudioBusId);
		};


		// Process messages
		if (bAssetEntriesNeedRefreshing)
		{
			const FAudioInsightsEditorModule AudioInsightsEditorModule = FAudioInsightsEditorModule::GetChecked();
			const ::Audio::FDeviceId AudioDeviceId = AudioInsightsEditorModule.GetDeviceId();

			for (const TSharedPtr<FAudioBusAssetDashboardEntry>& AudioBusDataViewEntry : AudioBusDataViewEntries)
			{
				if (AudioBusDataViewEntry.IsValid() && AudioBusDataViewEntry->AudioBus.IsValid())
				{
					UpdateDeviceEntry(AudioDeviceId, AudioBusDataViewEntry->AudioBusId, [&AudioBusDataViewEntry](TSharedPtr<FAudioBusAssetDashboardEntry>& Entry)
					{
						if (!Entry.IsValid())
						{
							Entry = AudioBusDataViewEntry;
						}
					});
				}
			}

			bAssetEntriesNeedRefreshing = false;
		}

		ProcessMessageQueue<FAudioBusActivateMessage>(TraceMessages.ActivateMessages, BumpEntryFunc,
		[](const FAudioBusActivateMessage& Msg, TSharedPtr<FAudioBusAssetDashboardEntry>* OutEntry)
		{
			FAudioBusAssetDashboardEntry& EntryRef = *OutEntry->Get();

			if (EntryRef.Name.IsEmpty())
			{
				EntryRef.Name = *Msg.Name;
			}

			if (EntryRef.EntryType == EAudioBusEntryType::None)
			{
				EntryRef.EntryType = FSoftObjectPath(EntryRef.Name).IsAsset() ? EAudioBusEntryType::AssetBased : EAudioBusEntryType::CodeGenerated;
			}
		});

		ProcessMessageQueue<FAudioBusDeactivateMessage>(TraceMessages.DeactivateMessages, GetEntry,
		[this](const FAudioBusDeactivateMessage& Msg, TSharedPtr<FAudioBusAssetDashboardEntry>* OutEntry)
		{
			if (OutEntry && (*OutEntry)->Timestamp < Msg.Timestamp)
			{
				FAudioBusAssetDashboardEntry& EntryRef = *OutEntry->Get();

				if (EntryRef.EntryType == EAudioBusEntryType::CodeGenerated)
				{
					RemoveDeviceEntry(Msg.DeviceId, Msg.AudioBusId);
				}
			}
		});

		ProcessMessageQueue<FAudioBusHasActivityMessage>(TraceMessages.HasActivityMessages, GetEntry,
		[](const FAudioBusHasActivityMessage& Msg, TSharedPtr<FAudioBusAssetDashboardEntry>* OutEntry)
		{
			if (OutEntry)
			{
				FAudioBusAssetDashboardEntry& EntryRef = *OutEntry->Get();

				if (!EntryRef.Name.IsEmpty() && EntryRef.EntryType != EAudioBusEntryType::None)
				{
					EntryRef.bHasActivity = Msg.bHasActivity;
					EntryRef.Timestamp = Msg.Timestamp;
				}
			}
		});

		return true;
	}

	UE::Trace::IAnalyzer* FAudioBusProvider::ConstructAnalyzer(TraceServices::IAnalysisSession& InSession)
	{
		class FAudioBusTraceAnalyzer : public FTraceAnalyzerBase
		{
		public:
			FAudioBusTraceAnalyzer(TSharedRef<FAudioBusProvider> InProvider, TraceServices::IAnalysisSession& InSession)
				: FTraceAnalyzerBase(InProvider)
				, Session(InSession)
			{
			}

			virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
			{
				FTraceAnalyzerBase::OnAnalysisBegin(Context);

				UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;

				Builder.RouteEvent(RouteId_Activate,    "Audio", "AudioBusActivate");
				Builder.RouteEvent(RouteId_Deactivate,  "Audio", "AudioBusDeactivate");
				Builder.RouteEvent(RouteId_HasActivity, "Audio", "AudioBusHasActivity");
			}

			virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override
			{
				LLM_SCOPE_BYNAME(TEXT("Insights/FAudioBusTraceAnalyzer"));

				FAudioBusMessages& Messages = GetProvider<FAudioBusProvider>().TraceMessages;

				switch (RouteId)
				{
					case RouteId_Activate:
					{
						Messages.ActivateMessages.Enqueue(FAudioBusActivateMessage { Context });
						break;
					}

					case RouteId_Deactivate:
					{
						Messages.DeactivateMessages.Enqueue(FAudioBusDeactivateMessage{ Context });
						break;
					}

					case RouteId_HasActivity:
					{
						Messages.HasActivityMessages.Enqueue(FAudioBusHasActivityMessage{ Context });
						break;
					}

					default:
					{
						return OnEventFailure(RouteId, Style, Context);
					}
				}

				const double Timestamp = Context.EventTime.AsSeconds(Context.EventData.GetValue<uint64>("Timestamp"));

				{
					TraceServices::FAnalysisSessionEditScope SessionEditScope(Session);
					Session.UpdateDurationSeconds(Timestamp);
				}

				return OnEventSuccess(RouteId, Style, Context);
			}

		private:
			enum : uint16
			{
				RouteId_Activate,
				RouteId_Deactivate,
				RouteId_HasActivity
			};

			TraceServices::IAnalysisSession& Session;
		};

		bAssetEntriesNeedRefreshing = true;

		return new FAudioBusTraceAnalyzer(AsShared(), InSession);
	}
} // namespace UE::Audio::Insights

