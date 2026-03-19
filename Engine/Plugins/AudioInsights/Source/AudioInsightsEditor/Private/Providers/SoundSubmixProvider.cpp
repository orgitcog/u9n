// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/SoundSubmixProvider.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioInsightsEditorDashboardFactory.h"
#include "AudioInsightsEditorModule.h"
#include "AudioMixerTrace.h"
#include "Sound/SoundSubmix.h"

namespace UE::Audio::Insights
{
	FSoundSubmixProvider::FSoundSubmixProvider()
		: TDeviceDataMapTraceProvider<uint32, TSharedPtr<FSoundSubmixAssetDashboardEntry>>(GetName_Static())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FSoundSubmixProvider::OnAssetAdded);
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FSoundSubmixProvider::OnAssetRemoved);
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FSoundSubmixProvider::OnFilesLoaded);

		FEditorDashboardFactory::OnActiveAudioDeviceChanged.AddRaw(this, &FSoundSubmixProvider::OnActiveAudioDeviceChanged);
		
#if UE_AUDIO_PROFILERTRACE_ENABLED
		FTraceAuxiliary::OnTraceStarted.AddRaw(this, &FSoundSubmixProvider::OnTraceStarted);
#endif // UE_AUDIO_PROFILERTRACE_ENABLED
	}
	
	FSoundSubmixProvider::~FSoundSubmixProvider()
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

	FName FSoundSubmixProvider::GetName_Static()
	{
		return "SubmixesProvider";
	}

	void FSoundSubmixProvider::OnAssetAdded(const FAssetData& InAssetData)
	{
		if (bAreFilesLoaded && InAssetData.AssetClassPath == FTopLevelAssetPath(USoundSubmix::StaticClass()))
		{
			AddSubmixAsset(InAssetData);
		}
	}

	void FSoundSubmixProvider::OnAssetRemoved(const FAssetData& InAssetData)
	{
		if (InAssetData.AssetClassPath == FTopLevelAssetPath(USoundSubmix::StaticClass()))
		{
			RemoveSubmixAsset(InAssetData);
		}
	}

	void FSoundSubmixProvider::OnFilesLoaded()
	{
		bAreFilesLoaded = true;
		UpdateSubmixAssetNames();
	}

	void FSoundSubmixProvider::OnActiveAudioDeviceChanged()
	{
		UpdateSubmixAssetNames();
	}

	void FSoundSubmixProvider::RequestEntriesUpdate()
	{
		UpdateSubmixAssetNames();
	}

	void FSoundSubmixProvider::OnTraceStarted(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination)
	{
#if UE_AUDIO_PROFILERTRACE_ENABLED
		UpdateSubmixAssetNames();
#endif // UE_AUDIO_PROFILERTRACE_ENABLED
	}

	void FSoundSubmixProvider::AddSubmixAsset(const FAssetData& InAssetData)
	{
		const bool bIsSubmixAssetAlreadAdded = SubmixDataViewEntries.ContainsByPredicate(
			[AssetName = InAssetData.GetObjectPathString()](const TSharedPtr<FSoundSubmixAssetDashboardEntry>& SoundSubmixAssetDashboardEntry)
			{
				return SoundSubmixAssetDashboardEntry->Name == AssetName;
			});

		if (!bIsSubmixAssetAlreadAdded)
		{
			const FAudioInsightsEditorModule AudioInsightsEditorModule = FAudioInsightsEditorModule::GetChecked();
			const ::Audio::FDeviceId AudioDeviceId = AudioInsightsEditorModule.GetDeviceId();

			TSharedPtr<FSoundSubmixAssetDashboardEntry>& SoundSubmixAssetDashboardEntry = SubmixDataViewEntries.Add_GetRef(MakeShared<FSoundSubmixAssetDashboardEntry>());
			SoundSubmixAssetDashboardEntry->DeviceId    = AudioDeviceId;
			SoundSubmixAssetDashboardEntry->Name        = InAssetData.GetObjectPathString();
			SoundSubmixAssetDashboardEntry->SubmixId    = GetTypeHash(SoundSubmixAssetDashboardEntry->Name);

			OnSubmixAssetAdded.Broadcast(SoundSubmixAssetDashboardEntry->SubmixId);

			bAssetEntriesNeedRefreshing = true;
			++LastUpdateId;
		}
	}

	void FSoundSubmixProvider::RemoveSubmixAsset(const FAssetData& InAssetData)
	{
		const int32 FoundSubmixAssetNameIndex = SubmixDataViewEntries.IndexOfByPredicate(
			[AssetName = InAssetData.GetObjectPathString()](const TSharedPtr<FSoundSubmixAssetDashboardEntry>& SoundSubmixAssetDashboardEntry)
			{
				return SoundSubmixAssetDashboardEntry->Name == AssetName;
			});

		if (FoundSubmixAssetNameIndex != INDEX_NONE)
		{
			const FAudioInsightsEditorModule AudioInsightsEditorModule = FAudioInsightsEditorModule::GetChecked();
			const ::Audio::FDeviceId AudioDeviceId = AudioInsightsEditorModule.GetDeviceId();

			const uint32 SubmixId = SubmixDataViewEntries[FoundSubmixAssetNameIndex]->SubmixId;

			RemoveDeviceEntry(AudioDeviceId, SubmixId);

			SubmixDataViewEntries.RemoveAt(FoundSubmixAssetNameIndex);

			OnSubmixAssetRemoved.Broadcast(SubmixId);

			bAssetEntriesNeedRefreshing = true;
			++LastUpdateId;
		}
	}

	void FSoundSubmixProvider::UpdateSubmixAssetNames()
	{
		// Get all USoundSubmix assets
		TArray<FAssetData> AssetDataArray;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(USoundSubmix::StaticClass()), AssetDataArray);

		// Build SubmixDataViewEntries
		Reset();
		SubmixDataViewEntries.Empty();

		for (const FAssetData& AssetData : AssetDataArray)
		{
			AddSubmixAsset(AssetData);
		}

		SubmixDataViewEntries.Sort([](const TSharedPtr<FSoundSubmixAssetDashboardEntry>& A, const TSharedPtr<FSoundSubmixAssetDashboardEntry>& B)
		{
			return A->GetDisplayName().CompareToCaseIgnored(B->GetDisplayName()) < 0;
		});

		OnSubmixAssetListUpdated.Broadcast();
	}

	bool FSoundSubmixProvider::ProcessMessages()
	{
		// Helper lambdas
		auto GetEntry = [this](const FSoundSubmixMessageBase& Msg)
		{
			return FindDeviceEntry(Msg.DeviceId, Msg.SubmixId);
		};


		// Process messages
		if (bAssetEntriesNeedRefreshing)
		{
			const FAudioInsightsEditorModule AudioInsightsEditorModule = FAudioInsightsEditorModule::GetChecked();
			const ::Audio::FDeviceId AudioDeviceId = AudioInsightsEditorModule.GetDeviceId();

			for (const TSharedPtr<FSoundSubmixAssetDashboardEntry>& SubmixDataViewEntry : SubmixDataViewEntries)
			{
				if (SubmixDataViewEntry.IsValid())
				{
					UpdateDeviceEntry(AudioDeviceId, SubmixDataViewEntry->SubmixId, [&SubmixDataViewEntry](TSharedPtr<FSoundSubmixAssetDashboardEntry>& Entry)
					{
						if (!Entry.IsValid())
						{
							Entry = SubmixDataViewEntry;
						}
					});
				}
			}

			bAssetEntriesNeedRefreshing = false;
		}

		ProcessMessageQueue<FSoundSubmixHasActivityMessage>(TraceMessages.HasActivityMessages, GetEntry,
		[](const FSoundSubmixHasActivityMessage& Msg, TSharedPtr<FSoundSubmixAssetDashboardEntry>* OutEntry)
		{
			if (OutEntry)
			{
				FSoundSubmixAssetDashboardEntry& EntryRef = *OutEntry->Get();
				EntryRef.bHasActivity = Msg.bHasActivity;
				EntryRef.Timestamp = Msg.Timestamp;
			}
		});

		return true;
	}

	UE::Trace::IAnalyzer* FSoundSubmixProvider::ConstructAnalyzer(TraceServices::IAnalysisSession& InSession)
	{
		class FSoundSubmixTraceAnalyzer : public FTraceAnalyzerBase
		{
		public:
			FSoundSubmixTraceAnalyzer(TSharedRef<FSoundSubmixProvider> InProvider, TraceServices::IAnalysisSession& InSession)
				: FTraceAnalyzerBase(InProvider)
				, Session(InSession)
			{
			}

			virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
			{
				FTraceAnalyzerBase::OnAnalysisBegin(Context);

				UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;

				Builder.RouteEvent(RouteId_HasActivity, "Audio", "SoundSubmixHasActivity");
			}

			virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override
			{
				LLM_SCOPE_BYNAME(TEXT("Insights/FSoundSubmixTraceAnalyzer"));

				FSoundSubmixMessages& Messages = GetProvider<FSoundSubmixProvider>().TraceMessages;

				switch (RouteId)
				{
					case RouteId_HasActivity:
					{
						Messages.HasActivityMessages.Enqueue(FSoundSubmixHasActivityMessage{ Context });
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
				RouteId_HasActivity
			};

			TraceServices::IAnalysisSession& Session;
		};

		bAssetEntriesNeedRefreshing = true;

		return new FSoundSubmixTraceAnalyzer(AsShared(), InSession);
	}
} // namespace UE::Audio::Insights
