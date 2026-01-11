// Copyright Epic Games, Inc. All Rights Reserved.
#include "Insights/Providers/ModulationMatrixTraceProvider.h"

#include "AudioDeviceManager.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace AudioModulationEditor
{
	FModulationMatrixTraceProvider::FModulationMatrixTraceProvider()
		: UE::Audio::Insights::TDeviceDataMapTraceProvider<uint32, TSharedPtr<FModulationMatrixDashboardEntry>>(GetName_Static())
	{
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddRaw(this, &FModulationMatrixTraceProvider::OnAudioDeviceDestroyed);
	}

	FModulationMatrixTraceProvider::~FModulationMatrixTraceProvider()
	{
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.RemoveAll(this);
	}

	FName FModulationMatrixTraceProvider::GetName_Static()
	{
		static const FName ModulationMatrixProviderName = "ModulationMatrixProvider";
		return ModulationMatrixProviderName;
	}

	void FModulationMatrixTraceProvider::UpdateActiveControlBusesToAdd(const TMap<FBusId, FString>& InBusIdToBusNameMap)
	{
		for (const auto& [BusId, BusName] : InBusIdToBusNameMap)
		{
			if (BusInfo* FoundBusInfoPtr = ActiveControlBuses.Find(BusId))
			{
				BusInfo& FoundBusInfo = *FoundBusInfoPtr;
				FoundBusInfo.RefCount++;
			}
			else
			{
				ActiveControlBuses.Emplace(BusId, { BusName, 1 });
			}
		}

		ActiveControlBuses.ValueSort([](const BusInfo& A, const BusInfo& B)
		{
			return A.BusName.ToLower() < B.BusName.ToLower();
		});
	}

	void FModulationMatrixTraceProvider::UpdateActiveControlBusesToRemove(const TMap<FBusId, float>& InBusIdToValueMap)
	{
		TArray<FBusId> BusesPendingToRemove;

		for (const auto& [BusId, BusValue] : InBusIdToValueMap)
		{
			if (BusInfo* FoundBusInfoPtr = ActiveControlBuses.Find(BusId))
			{
				BusInfo& FoundBusInfo = *FoundBusInfoPtr;
				FoundBusInfo.RefCount--;

				if (FoundBusInfo.RefCount <= 0)
				{
					RemovedControlBusesNames.Emplace(FoundBusInfo.BusName);
					BusesPendingToRemove.Emplace(BusId);
				}
			}
		}

		for (const FBusId& BusId : BusesPendingToRemove)
		{
			ActiveControlBuses.Remove(BusId);
		}
	}

	bool FModulationMatrixTraceProvider::ProcessMessages()
	{
		// Helper lambdas
		auto BumpEntryFunc = [this](const FModulationMatrixMessageBase& Msg)
		{
			TSharedPtr<FModulationMatrixDashboardEntry>* ToReturn = nullptr;

			UpdateDeviceEntry(Msg.DeviceId, Msg.SourceId, [&ToReturn, &Msg](TSharedPtr<FModulationMatrixDashboardEntry>& Entry)
			{
				if (!Entry.IsValid())
				{
					Entry = MakeShared<FModulationMatrixDashboardEntry>();
					Entry->DeviceId = Msg.DeviceId;
					Entry->SourceId = Msg.SourceId;
				}

				Entry->Timestamp = Msg.Timestamp;

				ToReturn = &Entry;
			});

			return ToReturn;
		};

		auto GetEntry = [this](const FModulationMatrixMessageBase& Msg)
		{
			return FindDeviceEntry(Msg.DeviceId, Msg.SourceId);
		};

		// Process messages
		uint32 NumActiveBuses = ActiveControlBuses.Num();

		if (!(TraceMessages.RegisterBusMessages.IsEmpty() && (TraceMessages.BusMixActivateMessages.IsEmpty() || TraceMessages.GeneratorActivateMessages.IsEmpty())))
		{
			TMap<FSourceId, TMap<FBusId, FString>> ModulatingSourceIdToBusesInfoMap;

			auto UpdateActiveModulatorSourceIds = [&](const Audio::FDeviceId DeviceId, const FSourceId SourceId)
			{
				TSet<uint32>& ActiveModulatorSourceIds = DeviceIdToActiveModulatorSourceIdsMap.FindOrAdd(DeviceId);

				if (!ActiveModulatorSourceIds.Contains(SourceId))
				{
					ActiveModulatorSourceIds.Add(SourceId);

					if (const TMap<FBusId, FString>* FoundBusInfoPtr = ModulatingSourceIdToBusesInfoMap.Find(SourceId))
					{
						UpdateActiveControlBusesToAdd(*FoundBusInfoPtr);
					}
				}
			};

			ProcessMessageQueue<FModulationMatrixRegisterBusMessage>(TraceMessages.RegisterBusMessages,
			[](const FModulationMatrixMessageBase& Msg)	{ return nullptr; }, // No entry operation
			[this, &ModulatingSourceIdToBusesInfoMap](const FModulationMatrixRegisterBusMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
			{
				if (TMap<FBusId, FString>* FoundBusInfoPtr = ModulatingSourceIdToBusesInfoMap.Find(Msg.ModulatingSourceId))
				{
					FoundBusInfoPtr->FindOrAdd(Msg.SourceId, Msg.BusName);
				}
				else
				{
					ModulatingSourceIdToBusesInfoMap.Emplace(Msg.ModulatingSourceId).Emplace(Msg.SourceId, Msg.BusName);
				}
			});

			ProcessMessageQueue<FBusMixActivateMessage>(TraceMessages.BusMixActivateMessages, BumpEntryFunc,
			[this, &UpdateActiveModulatorSourceIds](const FBusMixActivateMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
			{
				FModulationMatrixDashboardEntry& EntryRef = *OutEntry->Get();
				EntryRef.Name = *Msg.Name;
				EntryRef.EntryType = EModulationMatrixEntryType::BusMix;

				UpdateActiveModulatorSourceIds(Msg.DeviceId, EntryRef.SourceId);
			});

			ProcessMessageQueue<FGeneratorActivateMessage>(TraceMessages.GeneratorActivateMessages, BumpEntryFunc,
			[this, &UpdateActiveModulatorSourceIds](const FGeneratorActivateMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
			{
				FModulationMatrixDashboardEntry& EntryRef = *OutEntry->Get();
				EntryRef.Name = *Msg.Name;
				EntryRef.EntryType = EModulationMatrixEntryType::Generator;

				UpdateActiveModulatorSourceIds(Msg.DeviceId, EntryRef.SourceId);
			});
		}

		ProcessMessageQueue<FModulationMatrixDeactivateMessage>(TraceMessages.DeactivateMessages, GetEntry,
		[this](const FModulationMatrixDeactivateMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
		{
			if (OutEntry && (*OutEntry)->Timestamp < Msg.Timestamp)
			{
				const FModulationMatrixDashboardEntry& EntryRef = *OutEntry->Get();
				UpdateActiveControlBusesToRemove(EntryRef.BusIdToValueMap);

				if (DeviceIdToActiveModulatorSourceIdsMap.Contains(Msg.DeviceId))
				{
					DeviceIdToActiveModulatorSourceIdsMap[Msg.DeviceId].Remove(EntryRef.SourceId);
				}

				RemoveDeviceEntry(Msg.DeviceId, Msg.SourceId);
			}
		});

		ProcessMessageQueue<FBusMixUpdateMessage>(TraceMessages.BusMixUpdateMessages, GetEntry,
		[this](const FBusMixUpdateMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
		{
			if (OutEntry)
			{
				FModulationMatrixDashboardEntry& EntryRef = *OutEntry->Get();
				EntryRef.BusIdToValueMap = Msg.BusIdToValueMap;
				EntryRef.Timestamp = Msg.Timestamp;
			}
		});

		ProcessMessageQueue<FGeneratorUpdateMessage>(TraceMessages.GeneratorUpdateMessages, GetEntry,
		[this](const FGeneratorUpdateMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
		{
			if (OutEntry)
			{
				FModulationMatrixDashboardEntry& EntryRef = *OutEntry->Get();
				EntryRef.BusIdToValueMap = Msg.BusIdToValueMap;
				EntryRef.Timestamp = Msg.Timestamp;
			}
		});

		ProcessMessageQueue<FBusFinalValuesUpdateMessage>(TraceMessages.BusFinalValuesUpdateMessages, BumpEntryFunc,
		[this](const FBusFinalValuesUpdateMessage& Msg, TSharedPtr<FModulationMatrixDashboardEntry>* OutEntry)
		{
			FModulationMatrixDashboardEntry& EntryRef = *OutEntry->Get();

			if (EntryRef.Name.IsEmpty())
			{
				static const FString BusFinalValuesEntryName("Final Values: ");
				EntryRef.Name      = BusFinalValuesEntryName;
				EntryRef.EntryType = EModulationMatrixEntryType::BusFinalValues;
			}

			EntryRef.BusIdToValueMap = Msg.BusIdToValueMap;
		});

		// Notify Control Buses update
		if (NumActiveBuses != ActiveControlBuses.Num() && !ActiveControlBuses.IsEmpty())
		{
			OnControlBusesAdded.ExecuteIfBound(ActiveControlBuses);
		}

		if (!RemovedControlBusesNames.IsEmpty())
		{
			OnControlBusesRemoved.ExecuteIfBound(RemovedControlBusesNames);
			RemovedControlBusesNames.Reset();
		}

		return true;
	}

	void FModulationMatrixTraceProvider::OnAudioDeviceDestroyed(::Audio::FDeviceId InDeviceId)
	{
		DeviceIdToActiveModulatorSourceIdsMap.Reset();
		ActiveControlBuses.Reset();
	}

	UE::Trace::IAnalyzer* FModulationMatrixTraceProvider::ConstructAnalyzer(TraceServices::IAnalysisSession& InSession)
	{
		class FModulationMatrixTraceAnalyzer : public UE::Audio::Insights::FTraceProviderBase::FTraceAnalyzerBase
		{
		public:
			FModulationMatrixTraceAnalyzer(TSharedRef<FModulationMatrixTraceProvider> InProvider, TraceServices::IAnalysisSession& InSession)
				: UE::Audio::Insights::FTraceProviderBase::FTraceAnalyzerBase(InProvider)
				, Session(InSession)
			{
			}

			virtual void OnAnalysisBegin(const UE::Trace::IAnalyzer::FOnAnalysisContext& Context) override
			{
				UE::Audio::Insights::FTraceProviderBase::FTraceAnalyzerBase::OnAnalysisBegin(Context);

				UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;

				Builder.RouteEvent(RouteId_BusMixRegisterBus,    "Audio", "BusMixRegisterBus");
				Builder.RouteEvent(RouteId_GeneratorRegisterBus, "Audio", "GeneratorRegisterBus");

				Builder.RouteEvent(RouteId_BusMixActivate,       "Audio", "BusMixActivate");
				Builder.RouteEvent(RouteId_GeneratorActivate,    "Audio", "GeneratorActivate");

				Builder.RouteEvent(RouteId_BusMixUpdate,         "Audio", "BusMixUpdate");
				Builder.RouteEvent(RouteId_GeneratorUpdate,      "Audio", "GeneratorUpdate");
				Builder.RouteEvent(RouteId_BusFinalValuesUpdate, "Audio", "BusFinalValuesUpdate");

				Builder.RouteEvent(RouteId_Deactivate, "Audio", "ModulatingSourceDeactivate");
			}

			virtual bool OnEvent(uint16 RouteId, UE::Trace::IAnalyzer::EStyle Style, const UE::Trace::IAnalyzer::FOnEventContext& Context) override
			{
				LLM_SCOPE_BYNAME(TEXT("Insights/FModulationMatrixTraceAnalyzer"));

				FModulationMatrixMessages& Messages = GetProvider<FModulationMatrixTraceProvider>().TraceMessages;

				switch (RouteId)
				{
					case RouteId_BusMixRegisterBus:
					case RouteId_GeneratorRegisterBus:
					{
						Messages.RegisterBusMessages.Enqueue(FModulationMatrixRegisterBusMessage{ Context });
						break;
					}

					case RouteId_BusMixActivate:
					{
						Messages.BusMixActivateMessages.Enqueue(FBusMixActivateMessage{ Context });
						break;
					}

					case RouteId_GeneratorActivate:
					{
						Messages.GeneratorActivateMessages.Enqueue(FGeneratorActivateMessage{ Context });
						break;
					}

					case RouteId_BusMixUpdate:
					{
						Messages.BusMixUpdateMessages.Enqueue(FBusMixUpdateMessage{ Context });
						break;
					}

					case RouteId_GeneratorUpdate:
					{
						Messages.GeneratorUpdateMessages.Enqueue(FGeneratorUpdateMessage{ Context });
						break;
					}

					case RouteId_BusFinalValuesUpdate:
					{
						Messages.BusFinalValuesUpdateMessages.Enqueue(FBusFinalValuesUpdateMessage{ Context });
						break;
					}

					case RouteId_Deactivate:
					{
						Messages.DeactivateMessages.Enqueue(FModulationMatrixDeactivateMessage{ Context });
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
				RouteId_BusMixRegisterBus,
				RouteId_GeneratorRegisterBus,

				RouteId_BusMixActivate,
				RouteId_GeneratorActivate,

				RouteId_BusMixUpdate,
				RouteId_GeneratorUpdate,
				RouteId_BusFinalValuesUpdate,

				RouteId_Deactivate
			};

			TraceServices::IAnalysisSession& Session;
		};

		return new FModulationMatrixTraceAnalyzer(AsShared(), InSession);
	}
} // namespace AudioModulationEditor
