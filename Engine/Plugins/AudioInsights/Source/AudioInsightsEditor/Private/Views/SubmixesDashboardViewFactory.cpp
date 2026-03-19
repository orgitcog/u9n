// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/SubmixesDashboardViewFactory.h"

#include "AudioInsightsEditorModule.h"
#include "AudioInsightsStyle.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "Providers/SoundSubmixProvider.h"
#include "Sound/SoundSubmix.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	namespace SubmixesPrivate
	{
		const FSoundSubmixAssetDashboardEntry& CastEntry(const IDashboardDataViewEntry& InData)
		{
			return static_cast<const FSoundSubmixAssetDashboardEntry&>(InData);
		};
	}

	FSubmixesDashboardViewFactory::FSubmixesDashboardViewFactory()
	{
		FSoundSubmixProvider::OnSubmixAssetAdded.AddRaw(this, &FSubmixesDashboardViewFactory::HandleOnSubmixAssetListUpdated);
		FSoundSubmixProvider::OnSubmixAssetRemoved.AddRaw(this, &FSubmixesDashboardViewFactory::HandleOnSubmixAssetListUpdated);
		FSoundSubmixProvider::OnSubmixAssetListUpdated.AddRaw(this, &FSubmixesDashboardViewFactory::RequestListRefresh);

		IAudioInsightsTraceModule& AudioInsightsTraceModule = IAudioInsightsEditorModule::GetChecked().GetTraceModule();

		SoundSubmixProvider = MakeShared<FSoundSubmixProvider>();

		AudioInsightsTraceModule.AddTraceProvider(SoundSubmixProvider);

		Providers = TArray<TSharedPtr<FTraceProviderBase>>
		{
			SoundSubmixProvider
		};

		SortByColumn = "Name";
		SortMode     = EColumnSortMode::Ascending;
	}

	FSubmixesDashboardViewFactory::~FSubmixesDashboardViewFactory()
	{
		FSoundSubmixProvider::OnSubmixAssetAdded.RemoveAll(this);
		FSoundSubmixProvider::OnSubmixAssetRemoved.RemoveAll(this);
		FSoundSubmixProvider::OnSubmixAssetListUpdated.RemoveAll(this);
	}

	FName FSubmixesDashboardViewFactory::GetName() const
	{
		return "Submixes";
	}

	FText FSubmixesDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_Submixes_DisplayName", "Submixes");
	}

	TSharedRef<SWidget> FSubmixesDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName)
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
					const FSoundSubmixAssetDashboardEntry& SoundSubmixAssetDashboardEntry = SubmixesPrivate::CastEntry(InRowData.Get());
					return SoundSubmixAssetDashboardEntry.bHasActivity ? EVisibility::Visible : EVisibility::Hidden;
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

			const FSoundSubmixAssetDashboardEntry& SoundSubmixAssetDashboardEntry = SubmixesPrivate::CastEntry(*InRowData);
			const uint32 SubmixId = SoundSubmixAssetDashboardEntry.SubmixId;

			const bool bInitValue = SubmixCheckboxCheckedStates.FindOrAdd(SubmixId);
			OnSubmixAssetInit.Broadcast(bInitValue, SubmixId, SoundSubmixAssetDashboardEntry.Name);

			return SNew(SHorizontalBox)
				.Clipping(EWidgetClipping::ClipToBounds)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this, SubmixId]()
					{
						const bool* FoundSubmixCheckedState = SubmixCheckboxCheckedStates.Find(SubmixId);
						const bool bIsSubmixChecked = FoundSubmixCheckedState && *FoundSubmixCheckedState == true;

						return bIsSubmixChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, InRowData](ECheckBoxState NewState)
					{
						const bool bIsChecked = NewState == ECheckBoxState::Checked;

						const FSoundSubmixAssetDashboardEntry& SoundSubmixAssetDashboardEntry = SubmixesPrivate::CastEntry(*InRowData);

						if (bool* FoundSubmixCheckedState = SubmixCheckboxCheckedStates.Find(SoundSubmixAssetDashboardEntry.SubmixId))
						{
							*FoundSubmixCheckedState = bIsChecked;
						}

						OnSubmixAssetChecked.Broadcast(bIsChecked, SoundSubmixAssetDashboardEntry.SubmixId, SoundSubmixAssetDashboardEntry.Name);
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

		return SNullWidget::NullWidget;
	}

	void FSubmixesDashboardViewFactory::ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason)
	{
		const FString FilterString = GetSearchFilterText().ToString();
		
		FTraceTableDashboardViewFactory::FilterEntries<FSoundSubmixProvider>([&FilterString](const IDashboardDataViewEntry& Entry)
		{
			const FSoundSubmixAssetDashboardEntry& SubmixEntry = static_cast<const FSoundSubmixAssetDashboardEntry&>(Entry);
			
			return !SubmixEntry.GetDisplayName().ToString().Contains(FilterString);
		});
	}

	FSlateIcon FSubmixesDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Submix");
	}

	EDefaultDashboardTabStack FSubmixesDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Analysis;
	}

	TSharedRef<SWidget> FSubmixesDashboardViewFactory::MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs)
	{
		if (!DashboardWidget.IsValid())
		{
			DashboardWidget = FTraceTableDashboardViewFactory::MakeWidget(OwnerTab, SpawnTabArgs);

			if (FilteredEntriesListView.IsValid())
			{
				FilteredEntriesListView->SetSelectionMode(ESelectionMode::Single);
			}
		}
		else
		{
			if (SoundSubmixProvider.IsValid())
			{
				SoundSubmixProvider->RequestEntriesUpdate();
			}
		}

		for (const TSharedPtr<IDashboardDataViewEntry>& Entry : DataViewEntries)
		{
			const FSoundSubmixAssetDashboardEntry& SoundSubmixAssetDashboardEntry = SubmixesPrivate::CastEntry(*Entry);

			if (const bool* bFoundSubmixCheckboxCheckedState = SubmixCheckboxCheckedStates.Find(SoundSubmixAssetDashboardEntry.SubmixId))
			{
				OnSubmixAssetInit.Broadcast(*bFoundSubmixCheckboxCheckedState, SoundSubmixAssetDashboardEntry.SubmixId, SoundSubmixAssetDashboardEntry.Name);
			}
		}

		return DashboardWidget->AsShared();
	}

	const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& FSubmixesDashboardViewFactory::GetColumns() const
	{
		auto CreateColumnData = []()
		{
			return TMap<FName, FTraceTableDashboardViewFactory::FColumnData>
			{
				{
					"Active",
					{
						LOCTEXT("Submixes_ActiveDisplayName", "Active"),
						[](const IDashboardDataViewEntry& InData) { return FText::GetEmpty(); },
						false,		/* bDefaultHidden */
						0.08f,		/* FillWidth */
						EHorizontalAlignment::HAlign_Center
					}
				},
				{
					"Name",
					{
						LOCTEXT("Submixes_NameColumnDisplayName", "Name"),
						[](const IDashboardDataViewEntry& InData) { return SubmixesPrivate::CastEntry(InData).GetDisplayName(); },
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

	void FSubmixesDashboardViewFactory::SortTable()
	{
		if (SortByColumn == "Active")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FSoundSubmixAssetDashboardEntry& AData = SubmixesPrivate::CastEntry(*A.Get());
					const FSoundSubmixAssetDashboardEntry& BData = SubmixesPrivate::CastEntry(*B.Get());

					return !AData.bHasActivity && BData.bHasActivity;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FSoundSubmixAssetDashboardEntry& AData = SubmixesPrivate::CastEntry(*A.Get());
					const FSoundSubmixAssetDashboardEntry& BData = SubmixesPrivate::CastEntry(*B.Get());

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
					const FSoundSubmixAssetDashboardEntry& AData = SubmixesPrivate::CastEntry(*A.Get());
					const FSoundSubmixAssetDashboardEntry& BData = SubmixesPrivate::CastEntry(*B.Get());

					return AData.GetDisplayName().CompareToCaseIgnored(BData.GetDisplayName()) < 0;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FSoundSubmixAssetDashboardEntry& AData = SubmixesPrivate::CastEntry(*A.Get());
					const FSoundSubmixAssetDashboardEntry& BData = SubmixesPrivate::CastEntry(*B.Get());

					return BData.GetDisplayName().CompareToCaseIgnored(AData.GetDisplayName()) < 0;
				});
			}
		}
	}

	void FSubmixesDashboardViewFactory::OnSelectionChanged(TSharedPtr<IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo)
	{
		if (SelectedItem.IsValid())
		{
			const FSoundSubmixAssetDashboardEntry& SoundSubmixAssetDashboardEntry = SubmixesPrivate::CastEntry(*SelectedItem);

			if (const TObjectPtr<UObject> LoadedSubmix = FSoftObjectPath(SoundSubmixAssetDashboardEntry.Name).TryLoad())
			{
				OnSubmixSelectionChanged.Broadcast(Cast<USoundSubmix>(LoadedSubmix));
			}
		}
	}

	void FSubmixesDashboardViewFactory::RequestListRefresh()
	{
		if (FilteredEntriesListView.IsValid())
		{
			FilteredEntriesListView->RequestListRefresh();
		}
	}

	void FSubmixesDashboardViewFactory::HandleOnSubmixAssetListUpdated(const uint32 InSubmixId)
	{
		RequestListRefresh();
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
