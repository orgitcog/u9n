// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/AudioBusesDashboardViewFactory.h"

#include "AudioInsightsEditorModule.h"
#include "AudioInsightsStyle.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "Providers/AudioBusProvider.h"
#include "Sound/AudioBus.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	namespace AudioBusesPrivate
	{
		const FAudioBusAssetDashboardEntry& CastEntry(const IDashboardDataViewEntry& InData)
		{
			return static_cast<const FAudioBusAssetDashboardEntry&>(InData);
		};
	}

	FAudioBusesDashboardViewFactory::FAudioBusesDashboardViewFactory()
	{
		FAudioBusProvider::OnAudioBusAssetAdded.AddRaw(this, &FAudioBusesDashboardViewFactory::HandleOnAudioBusAssetListUpdated);
		FAudioBusProvider::OnAudioBusAssetRemoved.AddRaw(this, &FAudioBusesDashboardViewFactory::HandleOnAudioBusAssetListUpdated);
		FAudioBusProvider::OnAudioBusAssetListUpdated.AddRaw(this, &FAudioBusesDashboardViewFactory::RequestListRefresh);

		IAudioInsightsTraceModule& AudioInsightsTraceModule = IAudioInsightsEditorModule::GetChecked().GetTraceModule();

		AudioBusProvider = MakeShared<FAudioBusProvider>();

		AudioInsightsTraceModule.AddTraceProvider(AudioBusProvider);

		Providers = TArray<TSharedPtr<FTraceProviderBase>>
		{
			AudioBusProvider
		};

		SortByColumn = "Name";
		SortMode     = EColumnSortMode::Ascending;
	}

	FAudioBusesDashboardViewFactory::~FAudioBusesDashboardViewFactory()
	{
		FAudioBusProvider::OnAudioBusAssetAdded.RemoveAll(this);
		FAudioBusProvider::OnAudioBusAssetRemoved.RemoveAll(this);
		FAudioBusProvider::OnAudioBusAssetListUpdated.RemoveAll(this);
	}

	FName FAudioBusesDashboardViewFactory::GetName() const
	{
		return "AudioBuses";
	}

	FText FAudioBusesDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_AudioBuses_DisplayName", "Audio Buses");
	}

	TSharedRef<SWidget> FAudioBusesDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName)
	{
		if (InColumnName == "Active")
		{
			static const FLinearColor DarkGreen(0.027f, 0.541f, 0.22f);
			static const float Radius = 4.0f;
			static const FVector2f Size(7.0f, 7.0f);
			static const FSlateRoundedBoxBrush GreenRoundedBrush(DarkGreen, Radius, Size);

			return SNew(SBox)
				.Clipping(EWidgetClipping::ClipToBounds)
				.Padding(6.0f)
				.Visibility_Lambda([InRowData]()
				{ 
					const FAudioBusAssetDashboardEntry& AudioBusAssetDashboardEntry = AudioBusesPrivate::CastEntry(InRowData.Get());
					return AudioBusAssetDashboardEntry.bHasActivity ? EVisibility::Visible : EVisibility::Hidden;
				})
				[
					SNew(SImage)
					.Image(&GreenRoundedBrush)
				];
		}
		else if (InColumnName == "Name")
		{
			const FColumnData& ColumnData = GetColumns()[InColumnName];
			const FText ValueText = ColumnData.GetDisplayValue(InRowData.Get());

			if (ValueText.IsEmpty())
			{
				return SNullWidget::NullWidget;
			}

			const FAudioBusAssetDashboardEntry& AudioBusAssetDashboardEntry = AudioBusesPrivate::CastEntry(InRowData.Get());

			if (AudioBusAssetDashboardEntry.EntryType == EAudioBusEntryType::AssetBased)
			{
				const TWeakObjectPtr<UAudioBus> AudioBus = AudioBusAssetDashboardEntry.AudioBus;

				if (!AudioBus.IsValid())
				{
					return SNullWidget::NullWidget;
				}

				const bool bInitValue = AudioBusCheckboxCheckedStates.FindOrAdd(AudioBus, false);
				OnBusAssetInit.Broadcast(bInitValue, AudioBus);

				return SNew(SHorizontalBox)
					.Clipping(EWidgetClipping::ClipToBounds)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this, AudioBus]()
						{
							const bool* FoundAudioBusCheckedState = AudioBusCheckboxCheckedStates.Find(AudioBus);
							const bool bIsAudioBusChecked = FoundAudioBusCheckedState && *FoundAudioBusCheckedState == true;

							return bIsAudioBusChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})

						.OnCheckStateChanged_Lambda([this, AudioBus](ECheckBoxState NewState)
						{
							if (bool* FoundAudioBusCheckedState = AudioBusCheckboxCheckedStates.Find(AudioBus))
							{
								*FoundAudioBusCheckedState = NewState == ECheckBoxState::Checked;
							}

							OnAudioBusAssetChecked.Broadcast(NewState == ECheckBoxState::Checked, AudioBus);
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.MinDesiredWidth(5.0f)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ValueText)
						.MinDesiredWidth(300)
						.OnDoubleClicked_Lambda([this, InRowData](const FGeometry& MyGeometry, const FPointerEvent& PointerEvent)
						{
							if (GEditor)
							{
								const TSharedPtr<IObjectDashboardEntry> ObjectData = StaticCastSharedPtr<IObjectDashboardEntry>(InRowData.ToSharedPtr());
								if (ObjectData.IsValid())
								{
									const TObjectPtr<UObject> Object = ObjectData->GetObject();
									if (Object && Object->IsAsset())
									{
										GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
										return FReply::Handled();
									}
								}
							}

							return FReply::Unhandled();
						})
					];
			}
			else if (AudioBusAssetDashboardEntry.EntryType == EAudioBusEntryType::CodeGenerated)
			{
				// Show only the name since we can't click the checkbox to activate the audio meter nor open with double click a code generated audio bus
				return SNew(SHorizontalBox)
					.Clipping(EWidgetClipping::ClipToBounds)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ValueText)
						.MinDesiredWidth(300)
						.ColorAndOpacity(FSlateColor(FColor(80, 200, 255)))
					];
			}
		}

		return SNullWidget::NullWidget;
	}

	void FAudioBusesDashboardViewFactory::ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason)
	{
		FilterByAudioBusName();
		FilterByAudioBusType();
	}

	FSlateIcon FAudioBusesDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon");
	}

	EDefaultDashboardTabStack FAudioBusesDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Analysis;
	}

	TSharedRef<SWidget> FAudioBusesDashboardViewFactory::MakeAudioBusTypeFilterWidget()
	{
		if (AudioBusTypes.IsEmpty())
		{
			AudioBusTypes.Emplace(MakeShared<FComboboxSelectionItem>(EAudioBusTypeComboboxSelection::AssetBased,    LOCTEXT("AudioBusesDashboard_AudioBusTypeAssetBased",    "Asset")));
			AudioBusTypes.Emplace(MakeShared<FComboboxSelectionItem>(EAudioBusTypeComboboxSelection::CodeGenerated, LOCTEXT("AudioBusesDashboard_AudioBusTypeCodeGenerated", "Code Generated")));
			AudioBusTypes.Emplace(MakeShared<FComboboxSelectionItem>(EAudioBusTypeComboboxSelection::All,           LOCTEXT("AudioBusesDashboard_AudioBusTypeAll",           "All")));

			SelectedAudioBusType = AudioBusTypes[0];
		}

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Margin(FMargin(0.0, 2.0, 0.0, 0.0))
				.Text(LOCTEXT("AudioBusesDashboard_TypeFilterText", "Type Filter:"))
			]
			+ SHorizontalBox::Slot()
			.MaxWidth(2.0f)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SComboBox<TSharedPtr<FComboboxSelectionItem>>)
				.OptionsSource(&AudioBusTypes)
				.OnGenerateWidget_Lambda([this](const TSharedPtr<FComboboxSelectionItem>& AudioBusTypePtr)
				{
					const FText AudioBusTypeDisplayName = AudioBusTypePtr.IsValid() ? AudioBusTypePtr->Value /*DisplayName*/ : FText::GetEmpty();

					return SNew(STextBlock)
						.Text(AudioBusTypeDisplayName);
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FComboboxSelectionItem> InSelectedAudioBusTypePtr, ESelectInfo::Type)
				{
					if (InSelectedAudioBusTypePtr.IsValid())
					{
						SelectedAudioBusType = InSelectedAudioBusTypePtr;
						UpdateFilterReason = EProcessReason::FilterUpdated;
					}
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const int32 FoundIndex = AudioBusTypes.Find(SelectedAudioBusType);
						if (AudioBusTypes.IsValidIndex(FoundIndex) && AudioBusTypes[FoundIndex].IsValid())
						{
							return AudioBusTypes[FoundIndex]->Value;
						}

						return FText::GetEmpty();
					})
				]
			];
	}

	TSharedRef<SWidget> FAudioBusesDashboardViewFactory::MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs)
	{
		if (!DashboardWidget.IsValid())
		{
			SAssignNew(DashboardWidget, SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				MakeAudioBusTypeFilterWidget()
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				FTraceTableDashboardViewFactory::MakeWidget(OwnerTab, SpawnTabArgs)
			];

			if (FilteredEntriesListView.IsValid())
			{
				FilteredEntriesListView->SetSelectionMode(ESelectionMode::Single);
			}
		}
		else
		{
			if (AudioBusProvider.IsValid())
			{
				AudioBusProvider->RequestEntriesUpdate();
			}
		}

		for (const auto& [AudioBus, bIsCheckedState] : AudioBusCheckboxCheckedStates)
		{
			OnBusAssetInit.Broadcast(bIsCheckedState, AudioBus);
		}

		return DashboardWidget->AsShared();
	}

	const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& FAudioBusesDashboardViewFactory::GetColumns() const
	{
		auto CreateColumnData = []()
		{
			return TMap<FName, FTraceTableDashboardViewFactory::FColumnData>
			{
				{
					"Active",
					{
						LOCTEXT("AudioBuses_ActiveDisplayName", "Active"),
						[](const IDashboardDataViewEntry& InData) { return FText::GetEmpty(); },
						false,		/* bDefaultHidden */
						0.08f,		/* FillWidth */
						EHorizontalAlignment::HAlign_Center
					}
				},
				{
					"Name",
					{
						LOCTEXT("AudioBuses_NameColumnDisplayName", "Name"),
						[](const IDashboardDataViewEntry& InData)
						{
							const FAudioBusAssetDashboardEntry& AudioBusAssetDashboardEntry = AudioBusesPrivate::CastEntry(InData);
							return AudioBusAssetDashboardEntry.EntryType == EAudioBusEntryType::AssetBased ? AudioBusAssetDashboardEntry.GetDisplayName() : FText::FromString(AudioBusAssetDashboardEntry.Name);
						},
						false,		/* bDefaultHidden */
						0.92f,		/* FillWidth */
						EHorizontalAlignment::HAlign_Left
					}
				}
			};
		};
		
		static const TMap<FName, FTraceTableDashboardViewFactory::FColumnData> ColumnData = CreateColumnData();
		
		return ColumnData;
	}

	void FAudioBusesDashboardViewFactory::SortTable()
	{
		if (SortByColumn == "Active")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FAudioBusAssetDashboardEntry& AData = AudioBusesPrivate::CastEntry(*A.Get());
					const FAudioBusAssetDashboardEntry& BData = AudioBusesPrivate::CastEntry(*B.Get());

					return !AData.bHasActivity && BData.bHasActivity;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FAudioBusAssetDashboardEntry& AData = AudioBusesPrivate::CastEntry(*A.Get());
					const FAudioBusAssetDashboardEntry& BData = AudioBusesPrivate::CastEntry(*B.Get());

					return !BData.bHasActivity && AData.bHasActivity;
				});
			}
		}
		else if (SortByColumn == "Name")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FAudioBusAssetDashboardEntry& AData = AudioBusesPrivate::CastEntry(*A.Get());
					const FAudioBusAssetDashboardEntry& BData = AudioBusesPrivate::CastEntry(*B.Get());

					return AData.GetDisplayName().CompareToCaseIgnored(BData.GetDisplayName()) < 0;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FAudioBusAssetDashboardEntry& AData = AudioBusesPrivate::CastEntry(*A.Get());
					const FAudioBusAssetDashboardEntry& BData = AudioBusesPrivate::CastEntry(*B.Get());

					return BData.GetDisplayName().CompareToCaseIgnored(AData.GetDisplayName()) < 0;
				});
			}
		}
	}

	void FAudioBusesDashboardViewFactory::FilterByAudioBusName()
	{
		const FString FilterString = GetSearchFilterText().ToString();
		
		FTraceTableDashboardViewFactory::FilterEntries<FAudioBusProvider>([&FilterString](const IDashboardDataViewEntry& Entry)
		{
			const FAudioBusAssetDashboardEntry& AudioBusEntry = static_cast<const FAudioBusAssetDashboardEntry&>(Entry);
			
			return !AudioBusEntry.GetDisplayName().ToString().Contains(FilterString);
		});
	}

	void FAudioBusesDashboardViewFactory::FilterByAudioBusType()
	{
		using namespace UE::Audio::Insights;

		TArray<TSharedPtr<IDashboardDataViewEntry>> EntriesToFilterOut;

		const EAudioBusTypeComboboxSelection SelectedAudioBusTypeEnum = SelectedAudioBusType.IsValid()? SelectedAudioBusType->Key : EAudioBusTypeComboboxSelection::All;

		for (const TSharedPtr<IDashboardDataViewEntry>& Entry : DataViewEntries)
		{
			if (Entry.IsValid())
			{
				const FAudioBusAssetDashboardEntry& AudioBusAssetDashboardEntry = AudioBusesPrivate::CastEntry(*Entry);

				if (SelectedAudioBusTypeEnum != EAudioBusTypeComboboxSelection::All)
				{
					if ((SelectedAudioBusTypeEnum == EAudioBusTypeComboboxSelection::AssetBased    && AudioBusAssetDashboardEntry.EntryType != EAudioBusEntryType::AssetBased) ||
						(SelectedAudioBusTypeEnum == EAudioBusTypeComboboxSelection::CodeGenerated && AudioBusAssetDashboardEntry.EntryType != EAudioBusEntryType::CodeGenerated))
					{
						EntriesToFilterOut.Emplace(Entry);
					}
				}
			}
		}

		for (const TSharedPtr<IDashboardDataViewEntry>& Entry : EntriesToFilterOut)
		{
			DataViewEntries.Remove(Entry);
		}
	}

	void FAudioBusesDashboardViewFactory::RequestListRefresh()
	{
		if (FilteredEntriesListView.IsValid())
		{
			FilteredEntriesListView->RequestListRefresh();
		}
	}

	void FAudioBusesDashboardViewFactory::HandleOnAudioBusAssetListUpdated(const TWeakObjectPtr<UObject> InAsset)
	{
		RequestListRefresh();
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
