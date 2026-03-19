// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/SoundDashboardViewFactory.h"

#include "Algo/Accumulate.h"
#include "AudioInsightsModule.h"
#include "AudioInsightsStyle.h"
#include "Messages/SoundTraceMessages.h"
#include "Misc/EnumClassFlags.h"
#include "Providers/SoundTraceProvider.h"
#include "SoundDashboardCommands.h"
#include "Views/SAudioFilterBar.h"

#if WITH_EDITOR
#include "Audio/AudioDebug.h"
#include "AudioDeviceManager.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	/////////////////////////////////////////////////////////////////////////////////////////
	// FSoundDashboardViewFactoryPrivate
	namespace FSoundDashboardViewFactoryPrivate
	{
		const FSoundDashboardEntry& CastEntry(const IDashboardDataTreeViewEntry& InData)
		{
			return static_cast<const FSoundDashboardEntry&>(InData);
		};

		FSoundDashboardEntry& CastEntry(IDashboardDataTreeViewEntry& InData)
		{
			return static_cast<FSoundDashboardEntry&>(InData);
		};

		float GetLastEntryArrayValue(const ::Audio::TCircularAudioBuffer<FDataPoint>& InDataPoints)
		{
			if (InDataPoints.Num() > 0)
			{
				const ::Audio::DisjointedArrayView<const FDataPoint> DataPointsDisjointedArrayView = InDataPoints.PeekInPlace(InDataPoints.Num());
				return DataPointsDisjointedArrayView.FirstBuffer.Last().Value;
			}

			return 0.0f;
		};

		bool SetFilteredVisibility(IDashboardDataTreeViewEntry& InEntry, const FString& InFilterString)
		{
			FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(InEntry);

			bool bEntryMatchesTextFilter = SoundEntry.GetDisplayName().ToString().Contains(InFilterString);

			if (bEntryMatchesTextFilter)
			{
				SoundEntry.bIsVisible = true;
			}
			else
			{
				bool bChildMatchesTextFilter = false;

				for (const TSharedPtr<IDashboardDataTreeViewEntry>& SoundEntryChild : SoundEntry.Children)
				{
					if (SoundEntryChild.IsValid() && SetFilteredVisibility(*SoundEntryChild, InFilterString))
					{
						bChildMatchesTextFilter = true;
						break;
					}
				}

				SoundEntry.bIsVisible = bChildMatchesTextFilter;
			}

			return SoundEntry.bIsVisible;
		}

		void ResetVisibility(IDashboardDataTreeViewEntry& InEntry)
		{
			FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(InEntry);

			SoundEntry.bIsVisible = true;

			for (const TSharedPtr<IDashboardDataTreeViewEntry>& SoundEntryChild : SoundEntry.Children)
			{
				if (SoundEntryChild.IsValid())
				{
					ResetVisibility(*SoundEntryChild);
				}
			}
		}

		bool IsCategoryItem(const IDashboardDataTreeViewEntry& InEntry)
		{
			const FSoundDashboardEntry& SoundEntry = CastEntry(InEntry);
			return SoundEntry.bIsCategory;
		}

		bool IsVisible(const IDashboardDataTreeViewEntry& InEntry, const bool bShowRecentlyStoppedSounds)
		{
			const FSoundDashboardEntry& SoundEntry = CastEntry(InEntry);
			return SoundEntry.bIsVisible && (bShowRecentlyStoppedSounds || SoundEntry.TimeoutTimestamp == INVALID_TIMEOUT);
		}

		void CacheInitExpandStateRecursive(FSoundDashboardEntry& SoundEntry)
		{
			for (TSharedPtr<IDashboardDataTreeViewEntry>& ChildEntry : SoundEntry.Children)
			{
				if (ChildEntry.IsValid())
				{
					CacheInitExpandStateRecursive(CastEntry(*ChildEntry));
				}
			}

			SoundEntry.bShouldForceExpandChildren = SoundEntry.bIsExpanded;
		}

		bool HasPinEntryType(const IDashboardDataTreeViewEntry& InEntry, const FSoundDashboardEntry::EPinnedEntryType PinnedEntryType)
		{
			const FSoundDashboardEntry& SoundEntry = CastEntry(InEntry);
			return SoundEntry.PinnedEntryType == PinnedEntryType;
		}

		int32 GetNumChildrenWithoutPinEntryType(const IDashboardDataTreeViewEntry& InEntry, const FSoundDashboardEntry::EPinnedEntryType ExcludedPinnedEntryType, const bool bShowRecentlyStoppedSounds)
		{
			int32 NumChildrenWithoutType = 0;
			for (const TSharedPtr<IDashboardDataTreeViewEntry>& Child : InEntry.Children)
			{
				if (!Child.IsValid())
				{
					continue;
				}

				if (!HasPinEntryType(*Child, ExcludedPinnedEntryType) && IsVisible(*Child, bShowRecentlyStoppedSounds))
				{
					++NumChildrenWithoutType;
				}
			}

			return NumChildrenWithoutType;
		}

		int32 CountNumChildren(const IDashboardDataTreeViewEntry& InEntry, const bool bShowRecentlyStoppedSounds, const bool bIncludeTimingOutSounds = false)
		{
			const uint32 TotalNumChildren = Algo::Accumulate(InEntry.Children, 0,
				[bShowRecentlyStoppedSounds, bIncludeTimingOutSounds](uint32 Accum, TSharedPtr<IDashboardDataTreeViewEntry> InChild)
				{
					if (InChild.IsValid())
					{
						const FSoundDashboardEntry& SoundDashboardEntry = FSoundDashboardViewFactoryPrivate::CastEntry(*InChild);

						if (bIncludeTimingOutSounds || SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT)
						{
							if (HasPinEntryType(*InChild, FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry) || !SoundDashboardEntry.bIsVisible)
							{
								return Accum;
							}

							const int32 NumNestedChildren = GetNumChildrenWithoutPinEntryType(*InChild, FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry, bShowRecentlyStoppedSounds);

							if (NumNestedChildren > 0)
							{
								return Accum + NumNestedChildren;
							}

							return SoundDashboardEntry.bIsCategory ? Accum : Accum + 1;
						}
					}

					return Accum;
				});

			return TotalNumChildren;
		}

#if WITH_EDITOR
		void SetMuteSolo(const IDashboardDataTreeViewEntry& InEntry, const EMuteSoloMode InMuteSoloMode, const bool bInOnOff)
		{
#if ENABLE_AUDIO_DEBUG
			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				::Audio::FAudioDebugger& AudioDebugger = AudioDeviceManager->GetDebugger();

				const FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(InEntry);

				// Skip setting mute/solo, a copy of this entry is currently in the Pinned category 
				if (SoundEntry.PinnedEntryType == FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry)
				{
					return;
				}

				const FName SoundAssetDisplayName(SoundEntry.GetDisplayName().ToString());

				const bool bIsSoundCueType = SoundEntry.EntryType == ESoundDashboardEntryType::SoundCue || SoundEntry.EntryType == ESoundDashboardEntryType::SoundCueTemplate;

				if (!bIsSoundCueType)
				{
					switch (InMuteSoloMode)
					{
						case EMuteSoloMode::Mute:
							AudioDebugger.SetMuteSoundWave(SoundAssetDisplayName, bInOnOff);
							break;

						case EMuteSoloMode::Solo:
							AudioDebugger.SetSoloSoundWave(SoundAssetDisplayName, bInOnOff);
							break;

						default:
							break;
					}
				}

				for (const TSharedPtr<IDashboardDataTreeViewEntry>& SoundEntryChild : SoundEntry.Children)
				{
					if (SoundEntryChild.IsValid())
					{
						SetMuteSolo(*SoundEntryChild, InMuteSoloMode, bInOnOff);
					}
				}
			}
#endif // ENABLE_AUDIO_DEBUG
		}

		void ToggleMuteSolo(const IDashboardDataTreeViewEntry& InEntry, const EMuteSoloMode InMuteSoloMode)
		{
#if ENABLE_AUDIO_DEBUG
			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				::Audio::FAudioDebugger& AudioDebugger = AudioDeviceManager->GetDebugger();

				const FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(InEntry);

				const FName SoundAssetDisplayName(SoundEntry.GetDisplayName().ToString());

				const bool bIsSoundCueType = SoundEntry.EntryType == ESoundDashboardEntryType::SoundCue || SoundEntry.EntryType == ESoundDashboardEntryType::SoundCueTemplate;

				if (!bIsSoundCueType)
				{
					switch (InMuteSoloMode)
					{
						case EMuteSoloMode::Mute:
							AudioDebugger.ToggleMuteSoundWave(SoundAssetDisplayName);
							break;

						case EMuteSoloMode::Solo:
							AudioDebugger.ToggleSoloSoundWave(SoundAssetDisplayName);
							break;

						default:
							break;
					}
				}

				for (const TSharedPtr<IDashboardDataTreeViewEntry>& SoundEntryChild : SoundEntry.Children)
				{
					if (SoundEntryChild.IsValid())
					{
						ToggleMuteSolo(*SoundEntryChild, InMuteSoloMode);
					}
				}
			}
#endif // ENABLE_AUDIO_DEBUG
		}

		bool IsMuteSolo(const IDashboardDataTreeViewEntry& InEntry, const bool bInCheckChildren, const EMuteSoloMode InMuteSoloMode)
		{
#if ENABLE_AUDIO_DEBUG
			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				::Audio::FAudioDebugger& AudioDebugger = AudioDeviceManager->GetDebugger();

				const FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(InEntry);

				// Treat hidden original entries as muted/soloed to ensure the parent category reflects the correct state
				if (SoundEntry.PinnedEntryType == FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry)
				{
					return true;
				}

				const FName SoundAssetDisplayName(SoundEntry.GetDisplayName().ToString());

				const bool bIsSoundCueType = SoundEntry.EntryType == ESoundDashboardEntryType::SoundCue || SoundEntry.EntryType == ESoundDashboardEntryType::SoundCueTemplate;

				if (!bIsSoundCueType)
				{
					switch (InMuteSoloMode)
					{
						case EMuteSoloMode::Mute:
						{
							if (AudioDebugger.IsMuteSoundWave(SoundAssetDisplayName))
							{
								return true;
							}

							break;
						}

						case EMuteSoloMode::Solo:
						{
							if (AudioDebugger.IsSoloSoundWave(SoundAssetDisplayName))
							{
								return true;
							}

							break;
						}

						default:
							break;
					}
				}

				if (bInCheckChildren)
				{
					uint32 NumChildrenMuteSolo = 0;

					for (const TSharedPtr<IDashboardDataTreeViewEntry>& SoundEntryChild : SoundEntry.Children)
					{
						if (SoundEntryChild.IsValid() && IsMuteSolo(*SoundEntryChild, true /*bInCheckChildren*/, InMuteSoloMode))
						{
							++NumChildrenMuteSolo;
						}
					}

					const bool bAllChildrenMuteSolo = !SoundEntry.Children.IsEmpty() && NumChildrenMuteSolo == SoundEntry.Children.Num();

					return bAllChildrenMuteSolo;
				}
			}
#endif // ENABLE_AUDIO_DEBUG

			return false;
		}

		void ClearMutesAndSolos()
		{
#if ENABLE_AUDIO_DEBUG
			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				AudioDeviceManager->GetDebugger().ClearMutesAndSolos();
			}
#endif
		}
#endif // WITH_EDITOR

		const FText FiltersName = LOCTEXT("SoundDashboard_Filter_CategoryText", "Filters");
		const FText FiltersTooltip = LOCTEXT("CurveFiltersToolTip", "Filters what kind of sounds types can be displayed.");

		const FText MetaSoundCategoryName = LOCTEXT("SoundDashboard_Filter_MetaSoundNameText", "MetaSound");
		const FText SoundCueCategoryName = LOCTEXT("SoundDashboard_Filter_SoundCueNameText", "Sound Cue");
		const FText ProceduralSourceCategoryName = LOCTEXT("SoundDashboard_Filter_ProceduralSourceNameText", "Procedural Source");
		const FText SoundWaveCategoryName = LOCTEXT("SoundDashboard_Filter_SoundWaveNameText", "Sound Wave");
		const FText SoundCueTemplateCategoryName = LOCTEXT("SoundDashboard_Filter_SoundCueTemplateNameText", "Sound Cue Template");
		const FText PinnedCategoryName = LOCTEXT("SoundDashboard_Filter_PinnedNameText", "Pinned");
		// @TODO UE-250399: Hide category pending to implement
		//const FText HiddenCategoryName = LOCTEXT("SoundDashboard_Filter_HiddenNameText", "Hidden");
	} // namespace FSoundDashboardViewFactoryPrivate

	/////////////////////////////////////////////////////////////////////////////////////////
	// FPinnedSoundEntryWrapperPrivate
	namespace FPinnedSoundEntryWrapperPrivate
	{
		bool CanBeDeleted(const TSharedPtr<FPinnedSoundEntryWrapper>& Entry)
		{
			using namespace FSoundDashboardViewFactoryPrivate;

			return !Entry->EntryIsValid() || (IsCategoryItem(*Entry->GetPinnedSectionEntry()) && Entry->PinnedWrapperChildren.IsEmpty());
		}

		void CopyDataToPinnedEntry(FSoundDashboardEntry& PinnedEntry, const FSoundDashboardEntry& OriginalEntry)
		{
			// Only copy data that has possibly changed from the other entry
			PinnedEntry.TimeoutTimestamp = OriginalEntry.TimeoutTimestamp;

			PinnedEntry.bShouldForceExpandChildren = OriginalEntry.bShouldForceExpandChildren;
			PinnedEntry.bIsVisible = OriginalEntry.bIsVisible;

			// Just copy the last entry in the buffer rather than the whole buffer
			auto AddLastValueInBuffer = [](::Audio::TCircularAudioBuffer<FDataPoint>& To, const ::Audio::TCircularAudioBuffer<FDataPoint>& From)
			{
				if (From.Num() == 0)
				{
					return;
				}

				if (To.Num() > 0)
				{
					To.Pop(1);
				}

				const ::Audio::DisjointedArrayView<const FDataPoint> DataPointsDisjointedArrayView = From.PeekInPlace(From.Num());
				To.Push(DataPointsDisjointedArrayView.FirstBuffer.Last());
			};

			AddLastValueInBuffer(PinnedEntry.PriorityDataPoints, OriginalEntry.PriorityDataPoints);
			AddLastValueInBuffer(PinnedEntry.DistanceDataPoints, OriginalEntry.DistanceDataPoints);
			AddLastValueInBuffer(PinnedEntry.AmplitudeDataPoints, OriginalEntry.AmplitudeDataPoints);
			AddLastValueInBuffer(PinnedEntry.VolumeDataPoints, OriginalEntry.VolumeDataPoints);
			AddLastValueInBuffer(PinnedEntry.PitchDataPoints, OriginalEntry.PitchDataPoints);
		};
	} // namespace FPinnedSoundEntryWrapperPrivate

	/////////////////////////////////////////////////////////////////////////////////////////
	// FPinnedSoundEntryWrapper
	FPinnedSoundEntryWrapper::FPinnedSoundEntryWrapper(const TSharedPtr<IDashboardDataTreeViewEntry>& OriginalEntry)
		: OriginalDataEntry(OriginalEntry)
	{
		if (!OriginalEntry.IsValid())
		{
			return;
		}

		// Take a deep copy of the original entry to add to the pinned section of the dashboard
		// We need deep copies of any children too
		PinnedSectionEntry = MakeShared<FSoundDashboardEntry>(FSoundDashboardViewFactoryPrivate::CastEntry(*OriginalEntry));
		PinnedSectionEntry->Children.Empty();

		FSoundDashboardEntry& PinnedSectionSoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(*PinnedSectionEntry);
		PinnedSectionSoundEntry.PinnedEntryType = FSoundDashboardEntry::EPinnedEntryType::PinnedCopy;
		PinnedSectionSoundEntry.bIsVisible = true;

		for (const TSharedPtr<IDashboardDataTreeViewEntry>& Child : OriginalEntry->Children)
		{
			AddChildEntry(Child.ToSharedRef());
		}
	}

	TSharedPtr<FPinnedSoundEntryWrapper> FPinnedSoundEntryWrapper::AddChildEntry(const TSharedPtr<IDashboardDataTreeViewEntry> Child)
	{
		TSharedPtr<FPinnedSoundEntryWrapper> NewChild = MakeShared<FPinnedSoundEntryWrapper>(Child);

		FSoundDashboardEntry& NewChildSound = FSoundDashboardViewFactoryPrivate::CastEntry(*NewChild->GetPinnedSectionEntry());

		PinnedWrapperChildren.Add(NewChild);
		PinnedSectionEntry->Children.Add(NewChild->GetPinnedSectionEntry());

		return NewChild;
	}

	void FPinnedSoundEntryWrapper::UpdateParams()
	{
		// If we lose our handle to the original entry, we should stop updating
		if (!EntryIsValid())
		{
			OriginalDataEntry.Reset();
			PinnedSectionEntry.Reset();

			return;
		}

		// Only non-category entries have data to update
		if (OriginalDataEntry.IsValid())
		{
			FSoundDashboardEntry& Pinned = FSoundDashboardViewFactoryPrivate::CastEntry(*PinnedSectionEntry);
			const FSoundDashboardEntry& Original = FSoundDashboardViewFactoryPrivate::CastEntry(*OriginalDataEntry.Pin());

			FPinnedSoundEntryWrapperPrivate::CopyDataToPinnedEntry(Pinned, Original);
		}

		for (const TSharedPtr<FPinnedSoundEntryWrapper>& Child : PinnedWrapperChildren)
		{
			Child->UpdateParams();
		}
	}

	void FPinnedSoundEntryWrapper::CleanUp()
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		// Remove any pinned items whose original data entries have been removed
		
		// Note: Active sounds restart with the same PlayOrderID when realizing after virtualizing, but WaveInstances start with new Play Order IDs, which creates new dashboard entries.
		// To fix this edge case, when a pinned entry loses it's original entry, double check that a new one hasn't appeared in it's place.
		// If it has, recreate the child entries.
		bool bCanBeRecovered = false;
		if (!IsCategoryItem(*PinnedSectionEntry) && OriginalDataEntry.IsValid())
		{
			const FSoundDashboardEntry& OriginalSoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(*OriginalDataEntry.Pin());

			// A sound entry may be recovereable if it is still active, is not timing out, has child entries and it is currently pinned
			bCanBeRecovered = OriginalSoundEntry.PinnedEntryType == FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry 
							&& OriginalSoundEntry.TimeoutTimestamp == INVALID_TIMEOUT
							&& OriginalSoundEntry.Children.Num() > 0;
		}

		bool bRecreateAfterClean = false;
		for (int Index = PinnedWrapperChildren.Num() - 1; Index >= 0; --Index)
		{
			const TSharedPtr<FPinnedSoundEntryWrapper> Child = PinnedWrapperChildren[Index];
			if (FPinnedSoundEntryWrapperPrivate::CanBeDeleted(Child))
			{
				if (bCanBeRecovered)
				{
					// If the parent sound is still alive, but the child is no longer valid, destroy and recreate all pinned child entries
					PinnedSectionEntry->Children.Empty();
					PinnedWrapperChildren.Empty();
					bRecreateAfterClean = true;
					break;
				}

				TSharedPtr<IDashboardDataTreeViewEntry> DataEntry = Child->GetPinnedSectionEntry();
				PinnedSectionEntry->Children.Remove(DataEntry);
				PinnedWrapperChildren.Remove(Child);
			}
			else
			{
				Child->CleanUp();
			}
		}

		if (bRecreateAfterClean)
		{
			for (const TSharedPtr<IDashboardDataTreeViewEntry>& Child : OriginalDataEntry.Pin()->Children)
			{
				AddChildEntry(Child);
			}
		}
	}

	void FPinnedSoundEntryWrapper::MarkToDelete()
	{
		OriginalDataEntry.Reset();
	}

	bool FPinnedSoundEntryWrapper::EntryIsValid() const
	{
		return PinnedSectionEntry.IsValid() && (OriginalDataEntry.IsValid() || FSoundDashboardViewFactoryPrivate::IsCategoryItem(*PinnedSectionEntry));
	}

	/////////////////////////////////////////////////////////////////////////////////////////
	// FSoundDashboardViewFactory
	FSoundDashboardViewFactory::FSoundDashboardViewFactory()
	{
		FTraceModule& AudioInsightsTraceModule = static_cast<FTraceModule&>(FAudioInsightsModule::GetChecked().GetTraceModule());

		const TSharedPtr<FSoundTraceProvider> SoundsTraceProvider = MakeShared<FSoundTraceProvider>();

		AudioInsightsTraceModule.AddTraceProvider(SoundsTraceProvider);

		Providers = TArray<TSharedPtr<FTraceProviderBase>>
		{
			SoundsTraceProvider
		};
		
		FSoundDashboardCommands::Register();

		BindCommands();
	}

	FSoundDashboardViewFactory::~FSoundDashboardViewFactory()
	{
		FSoundDashboardCommands::Unregister();
	}

	FName FSoundDashboardViewFactory::GetName() const
	{
		return "Sounds";
	}

	FText FSoundDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_Sounds_DisplayName", "Sounds");
	}

	FSlateIcon FSoundDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.SoundDashboard.Tab");
	}

	EDefaultDashboardTabStack FSoundDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Analysis;
	}

	void FSoundDashboardViewFactory::BindCommands()
	{
		CommandList = MakeShared<FUICommandList>();

		const FSoundDashboardCommands& Commands = FSoundDashboardCommands::Get();

		CommandList->MapAction(Commands.GetPinCommand(), FExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::PinSound));
		CommandList->MapAction(Commands.GetUnpinCommand(), FExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::UnpinSound));
#if WITH_EDITOR
		CommandList->MapAction(Commands.GetBrowseCommand(), FExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::BrowseSoundAsset), FCanExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::SelectedItemsIncludesAnAsset));
		CommandList->MapAction(Commands.GetEditCommand(),   FExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::OpenSoundAsset), FCanExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::SelectedItemsIncludesAnAsset));
#endif // WITH_EDITOR

		// @TODO UE-250399: Hide category pending to implement
		//CommandList->MapAction(Commands.GetHideCommand(), FExecuteAction::CreateRaw(this, &FSoundDashboardViewFactory::HideSound));
	}

#if WITH_EDITOR
	TSharedRef<SWidget> FSoundDashboardViewFactory::MakeMuteSoloWidget()
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		const auto CreateButtonContentWidget = [](const FName& InIconName = FName(), const FText& InLabel = FText::GetEmpty(), const FName& InTextStyle = TEXT("ButtonText"))
        {
        	TSharedRef<SHorizontalBox> ButtonContainerWidget = SNew(SHorizontalBox);

			// Button icon (optional)
        	if (!InIconName.IsNone())
        	{
				ButtonContainerWidget->AddSlot()
	        	.AutoWidth()
	            .VAlign(VAlign_Center)
	            [
	                SNew(SImage)
	                .ColorAndOpacity(FSlateColor::UseForeground())
	                .Image(FSlateStyle::Get().GetBrush(InIconName))
	            ];
        	}

			// Button text (optional)
        	if (!InLabel.IsEmpty())
        	{
				const float LeftPadding = InIconName.IsNone() ? 0.0f : 4.0f;

				ButtonContainerWidget->AddSlot()
        		.VAlign(VAlign_Center)
	            .Padding(LeftPadding, 0.0f, 0.0f, 0.0f)
	            .AutoWidth()
	            [
	                SNew(STextBlock)
	                .TextStyle(&FSlateStyle::Get().GetWidgetStyle<FTextBlockStyle>(InTextStyle))
	                .Justification(ETextJustify::Center)
	                .Text(InLabel)
	            ];
        	}

        	return SNew(SBox)
				.HeightOverride(16.0f)
				[
					ButtonContainerWidget
				];
        };

		return SNew(SHorizontalBox)
			// Mute button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 6.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
				.ToolTipText(LOCTEXT("SoundDashboard_MuteTooltipText", "Toggles the mute state of the selected items."))
				.OnClicked_Lambda([this]()
				{
					if (FilteredEntriesListView.IsValid())
					{
						ToggleMuteSoloEntries(FilteredEntriesListView->GetSelectedItems(), EMuteSoloMode::Mute);
					}

					return FReply::Handled();
				})
				[
					CreateButtonContentWidget("AudioInsights.Icon.SoundDashboard.Mute", LOCTEXT("SoundDashboard_MuteButtonText", "Mute Selected"), "SmallButtonText")
				]
			]
			// Solo button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 6.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
				.ToolTipText(LOCTEXT("SoundDashboard_SoloTooltipText", "Toggles the solo state of the selected items."))
				.OnClicked_Lambda([this]()
				{
					if (FilteredEntriesListView.IsValid())
					{
						ToggleMuteSoloEntries(FilteredEntriesListView->GetSelectedItems(), EMuteSoloMode::Solo);
					}

					return FReply::Handled();
				})
				[
					CreateButtonContentWidget("AudioInsights.Icon.SoundDashboard.Solo", LOCTEXT("SoundDashboard_SoloButtonText", "Solo Selected"), "SmallButtonText")
				]
			]
			// Clear Mutes/Solos button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 6.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
				.ToolTipText(LOCTEXT("SoundsDashboard_ClearMutesAndSolosTooltipText", "Clears all assigned mute/solo states."))
				.OnClicked_Lambda([]()
				{
					ClearMutesAndSolos();

					return FReply::Handled();
				})
				[
					CreateButtonContentWidget("AudioInsights.Icon.SoundDashboard.Reset", LOCTEXT("SoundsDashboard_ClearMutesAndSolosButtonText", "Clear All Mutes/Solos"), "SmallButtonText")
				]
			]
			// Empty Spacing
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			// Show recently stopped sounds button
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				MakeShowRecentlyStoppedSoundsWidget()
			];
	}
#endif // WITH_EDITOR

	TSharedPtr<SWidget> FSoundDashboardViewFactory::GetFilterBarWidget()
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		if (!SoundsFilterBar.IsValid())
		{
			const TSharedPtr<FFilterCategory> FilterCategory = MakeShared<FFilterCategory>(FiltersName, FiltersTooltip);
			
			TArray<TSharedRef<FFilterBase<ESoundDashboardFilterFlags>>> Filters{
				MakeShared<FSoundDashboardFilter>(
				ESoundDashboardFilterFlags::MetaSound,
				"MetaSound",
				MetaSoundCategoryName,
				"AudioInsights.Icon.SoundDashboard.MetaSound",
				FText::GetEmpty(),
				FSlateStyle::Get().GetColor("SoundDashboard.MetaSoundColor"),
				FilterCategory),

				MakeShared<FSoundDashboardFilter>(
				ESoundDashboardFilterFlags::SoundCue,
				"SoundCue",
				SoundCueCategoryName,
				"AudioInsights.Icon.SoundDashboard.SoundCue",
				FText::GetEmpty(),
				FSlateStyle::Get().GetColor("SoundDashboard.SoundCueColor"),
				FilterCategory),

				MakeShared<FSoundDashboardFilter>(
				ESoundDashboardFilterFlags::ProceduralSource,
				"ProceduralSource",
				ProceduralSourceCategoryName,
				"AudioInsights.Icon.SoundDashboard.ProceduralSource",
				FText::GetEmpty(),
				FSlateStyle::Get().GetColor("SoundDashboard.ProceduralSourceColor"),
				FilterCategory),

				MakeShared<FSoundDashboardFilter>(
				ESoundDashboardFilterFlags::SoundWave,
				"SoundWave",
				SoundWaveCategoryName,
				"AudioInsights.Icon.SoundDashboard.SoundWave",
				FText::GetEmpty(),
				FSlateStyle::Get().GetColor("SoundDashboard.SoundWaveColor"),
				FilterCategory),

				MakeShared<FSoundDashboardFilter>(
				ESoundDashboardFilterFlags::SoundCueTemplate,
				"SoundCueTemplate",
				SoundCueTemplateCategoryName,
				"AudioInsights.Icon.SoundDashboard.SoundCue",
				FText::GetEmpty(),
				FSlateStyle::Get().GetColor("SoundDashboard.SoundCueTemplateColor"),
				FilterCategory),

				MakeShared<FSoundDashboardFilter>(
				ESoundDashboardFilterFlags::Pinned,
				"Pinned",
				PinnedCategoryName,
				"AudioInsights.Icon.SoundDashboard.Pin",
				FText::GetEmpty(),
				FSlateStyle::Get().GetColor("SoundDashboard.PinnedColor"),
				FilterCategory)

				// @TODO UE-250399: Hide category pending to implement
				//MakeShared<FSoundDashboardFilter>(
				//ESoundDashboardFilterFlags::Hidden,
				//"Hidden",
				//HiddenCategoryName,
				//"AudioInsights.Icon.SoundDashboard.Hide",
				//FText::GetEmpty(),
				//FSlateStyle::Get().GetColor("SoundDashboard.HiddenColor"),
				//FilterCategory)
			};

			SAssignNew(SoundsFilterBar, SAudioFilterBar<ESoundDashboardFilterFlags>)
			.CustomFilters(Filters)
			.OnFilterChanged_Lambda([this, Filters]()
			{
				auto GetActiveFilterFlags = [&Filters]()
				{
					ESoundDashboardFilterFlags ActiveFilterFlags = ESoundDashboardFilterFlags::None;

					for (const TSharedRef<FFilterBase<ESoundDashboardFilterFlags>>& Filter : Filters)
					{
						TSharedRef<FSoundDashboardFilter> SoundDashboardFilter = StaticCastSharedRef<FSoundDashboardFilter>(Filter);

						if (SoundDashboardFilter->IsActive())
						{
							ActiveFilterFlags |= SoundDashboardFilter->GetFlags();
						}
					}

					// By default, if there are no active filters selected it means that all filters are enabled
					return ActiveFilterFlags != ESoundDashboardFilterFlags::None ? ActiveFilterFlags : AllFilterFlags;
				};

				SelectedFilterFlags = GetActiveFilterFlags();
				bIsPinnedCategoryFilterEnabled = EnumHasAnyFlags(SelectedFilterFlags, ESoundDashboardFilterFlags::Pinned);

				UpdateFilterReason = EProcessReason::FilterUpdated;
			});
		}

		return SoundsFilterBar;
	}

	TSharedPtr<SWidget> FSoundDashboardViewFactory::GetFilterBarButtonWidget()
	{
		if (!SoundsFilterBarButton.IsValid())
		{
			if (!SoundsFilterBar.IsValid())
			{
				GetFilterBarWidget();
			}

			SoundsFilterBarButton = SBasicFilterBar<ESoundDashboardFilterFlags>::MakeAddFilterButton(StaticCastSharedPtr<SAudioFilterBar<ESoundDashboardFilterFlags>>(SoundsFilterBar).ToSharedRef()).ToSharedPtr();
		}

		return SoundsFilterBarButton;
	}

	TSharedRef<SWidget> FSoundDashboardViewFactory::MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs)
	{
		return SNew(SVerticalBox)
#if WITH_EDITOR
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				MakeMuteSoloWidget()
			]
#else
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				MakeShowRecentlyStoppedSoundsWidget()
			]
#endif // WITH_EDITOR
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				FTraceTreeDashboardViewFactory::MakeWidget(OwnerTab, SpawnTabArgs)
			];
	}

	TSharedRef<SWidget> FSoundDashboardViewFactory::GenerateWidgetForRootColumn(const TSharedRef<FTraceTreeDashboardViewFactory::SRowWidget>& InRowWidget, const TSharedRef<IDashboardDataTreeViewEntry>& InRowData, const FName& InColumn, const FText& InValueText)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		const FColumnData& ColumnData = GetColumns()[InColumn];

		if (InColumn == "Name")
		{
			const FName IconName = ColumnData.GetIconName.IsSet() ? ColumnData.GetIconName(InRowData.Get()) : NAME_None;

			// Custom color for root item icons
			const auto GetIconColor = [](const TSharedPtr<IDashboardDataTreeViewEntry>& InEntry)
			{
				const FSoundDashboardEntry& SoundDashboardEntry = FSoundDashboardViewFactoryPrivate::CastEntry(*InEntry);

				switch (SoundDashboardEntry.EntryType)
				{
				case ESoundDashboardEntryType::MetaSound:
					return FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.MetaSoundColor"));

				case ESoundDashboardEntryType::SoundCue:
					return FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.SoundCueColor"));

				case ESoundDashboardEntryType::ProceduralSource:
					return FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.ProceduralSourceColor"));

				case ESoundDashboardEntryType::SoundWave:
					return FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.SoundWaveColor"));

				case ESoundDashboardEntryType::SoundCueTemplate:
					return FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.SoundCueTemplateColor"));

				case ESoundDashboardEntryType::Pinned:
					return FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.PinnedColor"));

				case ESoundDashboardEntryType::None:
				default:
					return FSlateColor(FColor::White);
				}
			};

			return SNew(SHorizontalBox)
				// Tree expander arrow
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SExpanderArrow, InRowWidget)
				]
				// Icon
				+ SHorizontalBox::Slot()
				.Padding(IconName != NAME_None ? 2.0f : 0.0f, 2.0f)
				.AutoWidth()
				[
					IconName != NAME_None 
						? SNew(SImage).ColorAndOpacity(GetIconColor(InRowData)).Image(FSlateStyle::Get().GetBrush(IconName)) 
						: SNullWidget::NullWidget
				]
				// Text
				+ SHorizontalBox::Slot()
				.Padding(IconName != NAME_None ? 10.0f : 0.0f, 2.0f, 0.0f, 2.0f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::Get().GetFontStyle("BoldFont"))
					.Text(InValueText)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				// Number of children text
				+ SHorizontalBox::Slot()
				.Padding(6.0f, 2.0f, 0.0f, 2.0f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor(FColor::White.WithAlpha(128)))
					.Text_Lambda([this, InRowData]()
					{
						const uint32 TotalNumChildren = CountNumChildren(*InRowData, bShowRecentlyStoppedSounds);
						return FText::FromString("(" + FString::FromInt(TotalNumChildren) + ")");
					})
				];
		}

		return SNullWidget::NullWidget;
	}

	bool FSoundDashboardViewFactory::IsRootItem(const TSharedRef<IDashboardDataTreeViewEntry>& InEntry) const
	{
		return FilteredEntriesListView.IsValid() && FilteredEntriesListView->GetRootItems().Contains(InEntry);
	}

	bool FSoundDashboardViewFactory::EntryCanHaveChildren(const TSharedRef<IDashboardDataTreeViewEntry>& InEntry) const
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		const FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(*InEntry);

		const bool bIsSoundCueType = SoundEntry.EntryType == ESoundDashboardEntryType::SoundCue || SoundEntry.EntryType == ESoundDashboardEntryType::SoundCueTemplate;

		return IsCategoryItem(*InEntry) || bIsSoundCueType;
	}


	bool FSoundDashboardViewFactory::IsDescendant(const TSharedPtr<IDashboardDataTreeViewEntry>& InEntry, const TSharedPtr<IDashboardDataTreeViewEntry>& InChildCandidate) const
	{
		if (InEntry.IsValid() && EntryCanHaveChildren(InEntry.ToSharedRef()))
		{
			for (const TSharedPtr<IDashboardDataTreeViewEntry>& ChildEntry : InEntry->Children)
			{
				if (ChildEntry == InChildCandidate || IsDescendant(ChildEntry, InChildCandidate))
				{
					return true;
				}
			}
		}

		return false;
	}

	TSharedRef<SWidget> FSoundDashboardViewFactory::GenerateWidgetForColumn(const TSharedRef<FTraceTreeDashboardViewFactory::SRowWidget>& InRowWidget, const TSharedRef<IDashboardDataTreeViewEntry>& InRowData, const FName& InColumn)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		const FColumnData& ColumnData = GetColumns()[InColumn];

		const FText ValueText  = ColumnData.GetDisplayValue.IsSet() ? ColumnData.GetDisplayValue(InRowData.Get()) : FText::GetEmpty();
		const FName& ValueIcon = ColumnData.GetIconName.IsSet() ? ColumnData.GetIconName(InRowData.Get()) : NAME_None;

		if (ValueText.IsEmpty() && ValueIcon.IsNone())
		{
			return SNullWidget::NullWidget;
		}

#if WITH_EDITOR
		if (InColumn == "Mute")
		{
			return CreateMuteSoloButton(InRowData, InColumn, 
				[this](const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& InEntries)
				{
					ToggleMuteSoloEntries(InEntries, EMuteSoloMode::Mute);
				},
				[](const IDashboardDataTreeViewEntry& InEntry, const bool bInCheckChildren)
				{
					return IsMuteSolo(InEntry, bInCheckChildren, EMuteSoloMode::Mute);
				});
		}
		else if (InColumn == "Solo")
		{
			return CreateMuteSoloButton(InRowData, InColumn, 
				[this](const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& InEntries)
				{
					ToggleMuteSoloEntries(InEntries, EMuteSoloMode::Solo);
				}, 
				[](const IDashboardDataTreeViewEntry& InEntry, const bool bInCheckChildren)
				{
					return IsMuteSolo(InEntry, bInCheckChildren, EMuteSoloMode::Solo);
				});
		}
		else
#endif // WITH_EDITOR
		{
			if (IsRootItem(InRowData))
			{
				return GenerateWidgetForRootColumn(InRowWidget, InRowData, InColumn, ValueText);
			}

			return SNew(SHorizontalBox)
				// Tree expander arrow (only for Name column)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					InColumn == "Name" ? SNew(SExpanderArrow, InRowWidget) : SNullWidget::NullWidget
				]
				// Icon (optional)
				+ SHorizontalBox::Slot()
				.Padding(2.0f, 2.0f, 2.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image_Lambda([this, InRowData, InColumn]()
					{
						const FColumnData& ColumnData = GetColumns()[InColumn];
						const FName IconName = ColumnData.GetIconName.IsSet() ? ColumnData.GetIconName(InRowData.Get()) : NAME_None;
						
						return IconName != NAME_None ? FSlateStyle::Get().GetBrush(IconName) : nullptr;
					})
					.ColorAndOpacity_Lambda([this, InRowData, InColumn]()
					{
						const FColumnData& ColumnData = GetColumns()[InColumn];
						const FSlateColor TextColor = ColumnData.GetTextColorValue.IsSet() ? ColumnData.GetTextColorValue(InRowData.Get()) : FSlateColor::UseForeground();
						
						return TextColor;
					})
				]
				// Text (optional)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 2.0f, 2.0f, 2.0f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this, InRowData, InColumn]()
					{
						const FColumnData& ColumnData = GetColumns()[InColumn];
						const FText ValueText = ColumnData.GetDisplayValue.IsSet() ? ColumnData.GetDisplayValue(InRowData.Get()) : FText::GetEmpty();

						return ValueText;
					})
					.ColorAndOpacity_Lambda([this, InRowData, InColumn]()
					{
						const FColumnData& ColumnData = GetColumns()[InColumn];
						const FSlateColor TextColor = ColumnData.GetTextColorValue.IsSet() ? ColumnData.GetTextColorValue(InRowData.Get()) : FSlateColor::UseForeground();

						return TextColor;
					})
				]
				// Number of children text (if it is not leaf item)
				+ SHorizontalBox::Slot()
				.Padding(6.0f, 2.0f, 0.0f, 2.0f)
				.AutoWidth()
				[
					InColumn == "Name" && !InRowData->Children.IsEmpty()
						? SNew(STextBlock)
						  .ColorAndOpacity(FSlateColor(FColor::White.WithAlpha(128)))
						  .Text_Lambda([this, InRowData]()
						  {
							  const uint32 TotalNumChildren = CountNumChildren(*InRowData, bShowRecentlyStoppedSounds);
							  return FText::FromString("(" + FString::FromInt(TotalNumChildren) + ")");
						  })
						: SNullWidget::NullWidget
				];
		}
	}

	TSharedRef<ITableRow> FSoundDashboardViewFactory::OnGenerateRow(TSharedPtr<IDashboardDataTreeViewEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		return SNew(SRowWidget, OwnerTable, Item, AsShared())
			.Visibility_Lambda([this, Item]()
			{
				const FSoundDashboardEntry& SoundEntry = CastEntry(*Item);

				if (SoundEntry.PinnedEntryType == FSoundDashboardEntry::EPinnedEntryType::PinnedCopy)
				{
					if (!bIsPinnedCategoryFilterEnabled || (SoundEntry.bIsCategory && CountNumChildren(*Item, bShowRecentlyStoppedSounds, true /*bIncludeTimingOutSounds*/) == 0))
					{
						return EVisibility::Hidden;
					}
				}

				const int32 NumUnpinnedChildren = GetNumChildrenWithoutPinEntryType(*Item, FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry, bShowRecentlyStoppedSounds);

				const bool bRowShouldBeVisible = SoundEntry.bIsVisible 
												&& (bShowRecentlyStoppedSounds || SoundEntry.TimeoutTimestamp == INVALID_TIMEOUT)
												&& SoundEntry.PinnedEntryType != FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry 
												&& (!IsCategoryItem(*Item) || (NumUnpinnedChildren > 0));

				return bRowShouldBeVisible ? EVisibility::Visible : EVisibility::Hidden;
			});
	}

	void FSoundDashboardViewFactory::ProcessEntries(FTraceTreeDashboardViewFactory::EProcessReason InReason)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		// Filter by category
		FTraceTreeDashboardViewFactory::FilterEntries<FSoundTraceProvider>([this](IDashboardDataTreeViewEntry& InEntry)
		{
			if (SelectedFilterFlags == AllFilterFlags)
			{
				return true;
			}

			FSoundDashboardEntry& SoundCategoryEntry = FSoundDashboardViewFactoryPrivate::CastEntry(InEntry);
			bool bEntryTypePassesFilter = false;

			switch (SoundCategoryEntry.EntryType)
			{
				case ESoundDashboardEntryType::MetaSound:
					bEntryTypePassesFilter = EnumHasAnyFlags(SelectedFilterFlags, ESoundDashboardFilterFlags::MetaSound);
					break;
				case ESoundDashboardEntryType::SoundCue:
					bEntryTypePassesFilter = EnumHasAnyFlags(SelectedFilterFlags, ESoundDashboardFilterFlags::SoundCue);
					break;
				case ESoundDashboardEntryType::ProceduralSource:
					bEntryTypePassesFilter = EnumHasAnyFlags(SelectedFilterFlags, ESoundDashboardFilterFlags::ProceduralSource);
					break;
				case ESoundDashboardEntryType::SoundWave:
					bEntryTypePassesFilter = EnumHasAnyFlags(SelectedFilterFlags, ESoundDashboardFilterFlags::SoundWave);
					break;
				case ESoundDashboardEntryType::SoundCueTemplate:
					bEntryTypePassesFilter = EnumHasAnyFlags(SelectedFilterFlags, ESoundDashboardFilterFlags::SoundCueTemplate);
					break;
				default:
					break;
			}

			if (!bEntryTypePassesFilter)
			{
				CacheInitExpandStateRecursive(SoundCategoryEntry);
			}

			return bEntryTypePassesFilter;
		});

		// Filter by text
		const FString FilterString = GetSearchFilterText().ToString();
		const bool bFilterHasText  = !FilterString.IsEmpty();

		for (const TSharedPtr<IDashboardDataTreeViewEntry>& CategoryEntry : DataViewEntries)
		{
			if (!CategoryEntry.IsValid())
			{
				continue;
			}

			ResetVisibility(*CategoryEntry);

			for (const TSharedPtr<IDashboardDataTreeViewEntry>& Entry : CategoryEntry->Children)
			{
				if (Entry.IsValid() && bFilterHasText)
				{
					SetFilteredVisibility(*Entry, FilterString);
				}
			}
		}
	}

	TSharedPtr<SWidget> FSoundDashboardViewFactory::OnConstructContextMenu()
	{
		using namespace FSoundDashboardViewFactoryPrivate;


		const FSoundDashboardCommands& Commands = FSoundDashboardCommands::Get();

		constexpr bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

		MenuBuilder.BeginSection("SoundDashboardActions", LOCTEXT("SoundDashboard_Actions_HeaderText", "Sound Options"));

		{
			MenuBuilder.AddMenuEntry(
				Commands.GetPinCommand(),
				NAME_None,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateStyle::Get().CreateIcon("AudioInsights.Icon.SoundDashboard.Pin"),
				NAME_None,
				TAttribute<EVisibility>::CreateLambda([this]() { return SelectionIncludesUnpinnedItem() ? EVisibility::Visible : EVisibility::Collapsed; })
			);
			
			MenuBuilder.AddMenuEntry(
				Commands.GetUnpinCommand(),
				NAME_None,
				TAttribute<FText>(),
				TAttribute<FText>(),
				FSlateStyle::Get().CreateIcon("AudioInsights.Icon.SoundDashboard.Pin"),
				NAME_None,
				TAttribute<EVisibility>::CreateLambda([this]() { return SelectionIncludesUnpinnedItem() ? EVisibility::Collapsed : EVisibility::Visible; })
			);

#if WITH_EDITOR
			MenuBuilder.AddMenuEntry(Commands.GetBrowseCommand(), NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateStyle::Get().CreateIcon("AudioInsights.Icon.SoundDashboard.Browse"));
			MenuBuilder.AddMenuEntry(Commands.GetEditCommand(),   NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateStyle::Get().CreateIcon("AudioInsights.Icon.SoundDashboard.Edit"));
#endif // WITH_EDITOR

			// @TODO UE-250399: Hide category pending to implement
			//MenuBuilder.AddMenuEntry(Commands.GetHideCommand(), NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateStyle::Get().CreateIcon("AudioInsights.Icon.SoundDashboard.Hide"));
		}

		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	FReply FSoundDashboardViewFactory::OnDataRowKeyInput(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) const
	{
		return (CommandList && CommandList->ProcessCommandBindings(InKeyEvent)) ? FReply::Handled() : FReply::Unhandled();
	}

	const TMap<FName, FTraceTreeDashboardViewFactory::FHeaderRowColumnData>& FSoundDashboardViewFactory::GetHeaderRowColumns() const
	{
		static const TMap<FName, FTraceTreeDashboardViewFactory::FHeaderRowColumnData> HeaderRowColumnData =
		{
#if WITH_EDITOR
			{
				"Mute",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_MuteColumnDisplayName", "Mute"),
					.IconName         = "AudioInsights.Icon.SoundDashboard.Mute",
					.bShowDisplayName = false,
					.bDefaultHidden   = false,
					.FillWidth        = 0.05f,
					.Alignment        = EHorizontalAlignment::HAlign_Center
				}
			},
			{
				"Solo",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_SoloColumnDisplayName", "Solo"),
					.IconName         = "AudioInsights.Icon.SoundDashboard.Solo",
					.bShowDisplayName = false,
					.bDefaultHidden   = false,
					.FillWidth        = 0.05f,
					.Alignment        = EHorizontalAlignment::HAlign_Center
				}
			},
#endif
			{
				"Name",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_NameColumnDisplayName", "Name"),
					.IconName         = NAME_None,
					.bShowDisplayName = true,
					.bDefaultHidden   = false,
					.FillWidth        = 0.5f,
					.Alignment        = EHorizontalAlignment::HAlign_Left
				}
			},
			{
				"Priority",
				{
					.DisplayName	  = LOCTEXT("SoundDashboard_PriorityColumnDisplayName", "Priority"),
					.IconName		  = NAME_None,
					.bShowDisplayName = true,
					.bDefaultHidden	  = false,
					.FillWidth		  = 0.08f,
					.Alignment		  = EHorizontalAlignment::HAlign_Left
				}
			},
			{
				"Distance",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_DistanceColumnDisplayName", "Distance"),
					.IconName         = NAME_None,
					.bShowDisplayName = true,
					.bDefaultHidden   = false,
					.FillWidth        = 0.1f,
					.Alignment        = EHorizontalAlignment::HAlign_Left
				}
			},
			{
				"Amplitude",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_AmplitudeColumnDisplayName", "Amp (Peak)"),
					.IconName         = NAME_None,
					.bShowDisplayName = true,
					.bDefaultHidden   = false,
					.FillWidth        = 0.12f,
					.Alignment        = EHorizontalAlignment::HAlign_Left
				}
			},
			{
				"Volume",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_VolumeColumnDisplayName", "Volume"),
					.IconName         = NAME_None,
					.bShowDisplayName = true,
					.bDefaultHidden   = false,
					.FillWidth        = 0.1f,
					.Alignment        = EHorizontalAlignment::HAlign_Left
				}
			},
			{
				"Pitch",
				{
					.DisplayName      = LOCTEXT("SoundDashboard_PitchColumnDisplayName", "Pitch"),
					.IconName         = NAME_None,
					.bShowDisplayName = true,
					.bDefaultHidden   = false,
					.FillWidth        = 0.1f,
					.Alignment        = EHorizontalAlignment::HAlign_Left
				}
			}
		};

		return HeaderRowColumnData;
	}

	const TMap<FName, FTraceTreeDashboardViewFactory::FColumnData>& FSoundDashboardViewFactory::GetColumns() const
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		static const TMap<FName, FTraceTreeDashboardViewFactory::FColumnData> ColumnData =
		{
#if WITH_EDITOR
			{
				"Mute",
				{
					.GetIconName = [this](const IDashboardDataTreeViewEntry& InData)
					{
						return "AudioInsights.Icon.SoundDashboard.Mute";
					}
				}
			},
			{
				"Solo",
				{
					.GetIconName = [this](const IDashboardDataTreeViewEntry& InData)
					{
						return "AudioInsights.Icon.SoundDashboard.Solo";
					}
				}
			},
#endif
			{
				"Name",
				{
					.GetDisplayValue = [](const IDashboardDataTreeViewEntry& InData)
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);

						return SoundDashboardEntry.GetDisplayName();
					},
					.GetIconName = [this](const IDashboardDataTreeViewEntry& InData) -> FName
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);

						switch (SoundDashboardEntry.EntryType)
						{
							case ESoundDashboardEntryType::MetaSound:
								return FName("AudioInsights.Icon.SoundDashboard.MetaSound");
							case ESoundDashboardEntryType::SoundCue:
								return FName("AudioInsights.Icon.SoundDashboard.SoundCue");
							case ESoundDashboardEntryType::ProceduralSource:
								return FName("AudioInsights.Icon.SoundDashboard.ProceduralSource");
							case ESoundDashboardEntryType::SoundWave:
								return FName("AudioInsights.Icon.SoundDashboard.SoundWave");
							case ESoundDashboardEntryType::SoundCueTemplate:
								return FName("AudioInsights.Icon.SoundDashboard.SoundCue");
							case ESoundDashboardEntryType::Pinned:
								return FName("AudioInsights.Icon.SoundDashboard.Pin");
							case ESoundDashboardEntryType::None:
							default:
								break;
						}

						return NAME_None;
					},
					.GetTextColorValue = [](const IDashboardDataTreeViewEntry& InData) -> FSlateColor
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						return SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT ? FSlateColor::UseForeground()
																					   : FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.TimingOutTextColor"));
					}
				}
			},
			{
				"Priority",
				{
					.GetDisplayValue = [](const IDashboardDataTreeViewEntry& InData)
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						if (SoundDashboardEntry.bIsCategory)
						{
							return FText();
						}

						const ::Audio::TCircularAudioBuffer<FDataPoint>& PriorityDataPoints = SoundDashboardEntry.PriorityDataPoints;
						const float PriorityValue = GetLastEntryArrayValue(PriorityDataPoints);

						// Max priority as defined in SoundWave.cpp
						static constexpr float VolumeWeightedMaxPriority = TNumericLimits<float>::Max() / MAX_VOLUME;

						if (PriorityValue >= VolumeWeightedMaxPriority)
						{
							return LOCTEXT("AudioDashboard_Sounds_Max", "MAX");
						}
						else
						{
							return FText::AsNumber(GetLastEntryArrayValue(PriorityDataPoints), FSlateStyle::Get().GetAmpFloatFormat());
						}
					},
					.GetTextColorValue = [](const IDashboardDataTreeViewEntry& InData) -> FSlateColor
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						return SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT ? FSlateColor::UseForeground()
																					   : FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.TimingOutTextColor"));
					}
				}
			},
			{
				"Distance",
				{
					.GetDisplayValue = [](const IDashboardDataTreeViewEntry& InData)
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						if (SoundDashboardEntry.bIsCategory)
						{
							return FText();
						}

						const ::Audio::TCircularAudioBuffer<FDataPoint>& DistanceDataPoints = SoundDashboardEntry.DistanceDataPoints;
						return FText::AsNumber(GetLastEntryArrayValue(DistanceDataPoints), FSlateStyle::Get().GetDefaultFloatFormat());
					},
					.GetTextColorValue = [](const IDashboardDataTreeViewEntry& InData) -> FSlateColor
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						return SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT ? FSlateColor::UseForeground()
																					   : FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.TimingOutTextColor"));
					}
				}
			},
			{
				"Amplitude",
				{
					.GetDisplayValue = [](const IDashboardDataTreeViewEntry& InData)
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						if (SoundDashboardEntry.bIsCategory)
						{
							return FText();
						}

						const ::Audio::TCircularAudioBuffer<FDataPoint>& AmplitudeDataPoints = SoundDashboardEntry.AmplitudeDataPoints;
						return FText::AsNumber(GetLastEntryArrayValue(AmplitudeDataPoints), FSlateStyle::Get().GetAmpFloatFormat());
					},
					.GetTextColorValue = [](const IDashboardDataTreeViewEntry& InData) -> FSlateColor
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						return SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT ? FSlateColor::UseForeground()
																					   : FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.TimingOutTextColor"));
					}
				}
			},
			{
				"Volume",
				{
					.GetDisplayValue = [](const IDashboardDataTreeViewEntry& InData)
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						if (SoundDashboardEntry.bIsCategory)
						{
							return FText();
						}

						const ::Audio::TCircularAudioBuffer<FDataPoint>& VolumeDataPoints = SoundDashboardEntry.VolumeDataPoints;
						return FText::AsNumber(GetLastEntryArrayValue(VolumeDataPoints), FSlateStyle::Get().GetAmpFloatFormat());
					},
					.GetTextColorValue = [](const IDashboardDataTreeViewEntry& InData) -> FSlateColor
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						return SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT ? FSlateColor::UseForeground()
																					   : FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.TimingOutTextColor"));
					}
				}
			},
			{
				"Pitch",
				{
					.GetDisplayValue = [](const IDashboardDataTreeViewEntry& InData)
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						if (SoundDashboardEntry.bIsCategory)
						{
							return FText();
						}

						const ::Audio::TCircularAudioBuffer<FDataPoint>& PitchDataPoints = SoundDashboardEntry.PitchDataPoints;
						return FText::AsNumber(GetLastEntryArrayValue(PitchDataPoints), FSlateStyle::Get().GetPitchFloatFormat());
					},
					.GetTextColorValue = [](const IDashboardDataTreeViewEntry& InData) -> FSlateColor
					{
						const FSoundDashboardEntry& SoundDashboardEntry = CastEntry(InData);
						return SoundDashboardEntry.TimeoutTimestamp == INVALID_TIMEOUT ? FSlateColor::UseForeground()
																					   : FSlateColor(FSlateStyle::Get().GetColor("SoundDashboard.TimingOutTextColor"));
					}
				}
			}
		};

		return ColumnData;
	}

	void FSoundDashboardViewFactory::SortTable()
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		UpdatePinnedSection();

		auto SortByPlayOrder = [](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			return First.PlayOrder < Second.PlayOrder;
		};
		
		auto SortByName = [&SortByPlayOrder](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			const int32 Comparison = First.GetDisplayName().CompareToCaseIgnored(Second.GetDisplayName());
			if (Comparison == 0)
			{
				return SortByPlayOrder(First, Second);
			}
			
			return Comparison < 0;
		};

		auto SortByPriority = [&SortByPlayOrder](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			const float ComparisonDiff = GetLastEntryArrayValue(First.PriorityDataPoints) - GetLastEntryArrayValue(Second.PriorityDataPoints);
			if (FMath::IsNearlyZero(ComparisonDiff, UE_KINDA_SMALL_NUMBER))
			{
				return SortByPlayOrder(First, Second);
			}

			return ComparisonDiff < 0.0f;
		};

		auto SortByDistance = [&SortByPlayOrder](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			const float ComparisonDiff = GetLastEntryArrayValue(First.DistanceDataPoints) - GetLastEntryArrayValue(Second.DistanceDataPoints);
			if (FMath::IsNearlyZero(ComparisonDiff, UE_KINDA_SMALL_NUMBER))
			{
				return SortByPlayOrder(First, Second);
			}

			return ComparisonDiff < 0.0f;
		};

		auto SortByAmplitude = [&SortByPlayOrder](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			const float ComparisonDiff = GetLastEntryArrayValue(First.AmplitudeDataPoints) - GetLastEntryArrayValue(Second.AmplitudeDataPoints);
			if (FMath::IsNearlyZero(ComparisonDiff, UE_KINDA_SMALL_NUMBER))
			{
				return SortByPlayOrder(First, Second);
			}

			return ComparisonDiff < 0.0f;
		};

		auto SortByVolume = [&SortByPlayOrder](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			const float ComparisonDiff = GetLastEntryArrayValue(First.VolumeDataPoints) - GetLastEntryArrayValue(Second.VolumeDataPoints);
			if (FMath::IsNearlyZero(ComparisonDiff, UE_KINDA_SMALL_NUMBER))
			{
				return SortByPlayOrder(First, Second);
			}

			return ComparisonDiff < 0.0f;
		};

		auto SortByPitch = [&SortByPlayOrder](const FSoundDashboardEntry& First, const FSoundDashboardEntry& Second)
		{
			const float ComparisonDiff = GetLastEntryArrayValue(First.PitchDataPoints) - GetLastEntryArrayValue(Second.PitchDataPoints);
			if (FMath::IsNearlyZero(ComparisonDiff, UE_KINDA_SMALL_NUMBER))
			{
				return SortByPlayOrder(First, Second);
			}

			return ComparisonDiff < 0.0f;
		};

		if (SortByColumn == "Name")
		{
			SortByPredicate(SortByName);
		}
		else if (SortByColumn == "Priority")
		{
			SortByPredicate(SortByPriority);
		}
		else if (SortByColumn == "Distance")
		{
			SortByPredicate(SortByDistance);
		}
		else if (SortByColumn == "Amplitude")
		{
			SortByPredicate(SortByAmplitude);
		}
		else if (SortByColumn == "Volume")
		{
			SortByPredicate(SortByVolume);
		}
		else if (SortByColumn == "Pitch")
		{
			SortByPredicate(SortByPitch);
		}

		FullTree.Reset();

		if (PinnedItemEntries.IsValid())
		{
			FullTree.Add(PinnedItemEntries->GetPinnedSectionEntry());
		}
		
		FullTree.Append(DataViewEntries);
	}

	bool FSoundDashboardViewFactory::ResetTreeData()
	{
		bool bDataReset = false;
		if (!DataViewEntries.IsEmpty())
		{
			DataViewEntries.Empty();
			bDataReset = true;
		}

		if (PinnedItemEntries.IsValid())
		{
			PinnedItemEntries.Reset();
			bDataReset = true;
		}

		if (!FullTree.IsEmpty())
		{
			FullTree.Empty();
			bDataReset = true;
		}

		return bDataReset;
	}

	void FSoundDashboardViewFactory::RecursiveSort(TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& OutTree, TFunctionRef<bool(const FSoundDashboardEntry&, const FSoundDashboardEntry&)> Predicate)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		for (TSharedPtr<IDashboardDataTreeViewEntry>& Entry : OutTree)
		{
			if (Entry->Children.Num() > 0)
			{
				RecursiveSort(Entry->Children, Predicate);
			}
		}

		for (const TSharedPtr<IDashboardDataTreeViewEntry>& Entry : OutTree)
		{
			const FSoundDashboardEntry& EntryData = CastEntry(*Entry);
			if (IsCategoryItem(*Entry))
			{
				return;
			}
		}

		auto SortDashboardEntries = [this](const TSharedPtr<IDashboardDataTreeViewEntry>& First, const TSharedPtr<IDashboardDataTreeViewEntry>& Second, TFunctionRef<bool(const FSoundDashboardEntry&, const FSoundDashboardEntry&)> Predicate)
		{
			return Predicate(CastEntry(*First), CastEntry(*Second));
		};

		if (SortMode == EColumnSortMode::Ascending)
		{
			OutTree.Sort([&SortDashboardEntries, &Predicate](const TSharedPtr<IDashboardDataTreeViewEntry>& A, const TSharedPtr<IDashboardDataTreeViewEntry>& B)
			{
				return SortDashboardEntries(A, B, Predicate);
			});
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			OutTree.Sort([&SortDashboardEntries, &Predicate](const TSharedPtr<IDashboardDataTreeViewEntry>& A, const TSharedPtr<IDashboardDataTreeViewEntry>& B)
			{
				return SortDashboardEntries(B, A, Predicate);
			});
		}
	}

	void FSoundDashboardViewFactory::SortByPredicate(TFunctionRef<bool(const FSoundDashboardEntry&, const FSoundDashboardEntry&)> Predicate)
	{
		if (PinnedItemEntries.IsValid())
		{
			RecursiveSort(PinnedItemEntries->GetPinnedSectionEntry()->Children, Predicate);
		}

		RecursiveSort(DataViewEntries, Predicate);
	}

	TSharedRef<SWidget> FSoundDashboardViewFactory::MakeShowRecentlyStoppedSoundsWidget()
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 8.0f)
			[
				SNew(SCheckBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Style(&FSlateStyle::Get().GetWidgetStyle<FCheckBoxStyle>("SoundDashboard.MuteSoloButton"))
				.ToolTipText(LOCTEXT("SoundsDashboard_ShowStoppedSoundsTooltip", "Shows sounds that have recently stopped playing"))
				.IsChecked(bShowRecentlyStoppedSounds ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bShowRecentlyStoppedSounds = NewState == ECheckBoxState::Checked;
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SImage)
				 		.Image_Lambda([this]()
				 		{
				 			const FName IconName = bShowRecentlyStoppedSounds ? "AudioInsights.Icon.SoundDashboard.Visible" : "AudioInsights.Icon.SoundDashboard.Invisible";
		 
				 			return FSlateStyle::Get().GetBrush(IconName);
				 		})
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SoundsDashboard_ShowStoppedSounds", "Show Stopped"))
					]
				]
			];
	}

#if WITH_EDITOR
	TSharedRef<SWidget> FSoundDashboardViewFactory::CreateMuteSoloButton(const TSharedRef<IDashboardDataTreeViewEntry>& InRowData, 
		const FName& InColumn, 
		TFunction<void(const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>&)> MuteSoloToggleFunc, 
		TFunctionRef<bool(const IDashboardDataTreeViewEntry&, const bool)> IsMuteSoloFunc)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Style(&FSlateStyle::Get().GetWidgetStyle<FCheckBoxStyle>("SoundDashboard.MuteSoloButton"))
				.IsChecked(ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([InRowData, MuteSoloToggleFunc](ECheckBoxState NewState)
				{
					MuteSoloToggleFunc({ InRowData });
				})
				[
					SNew(SImage)
					.Image_Lambda([this, InRowData, InColumn, IsMuteSoloFunc]()
					{
						const FColumnData& ColumnData = GetColumns()[InColumn];
						const FName IconName = IsMuteSoloFunc(*InRowData, EntryCanHaveChildren(InRowData) /*bInCheckChildren*/) && ColumnData.GetIconName.IsSet() ? ColumnData.GetIconName(InRowData.Get()) : "AudioInsights.Icon.SoundDashboard.Transparent";

						return FSlateStyle::Get().GetBrush(IconName);
					})
				]
			];
	}

	void FSoundDashboardViewFactory::ToggleMuteSoloEntries(const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& InEntries, const EMuteSoloMode InMuteSoloMode)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		TArray<TSharedPtr<IDashboardDataTreeViewEntry>> EntriesToMuteSolo;

		// In multiple selection we need to discard children entries to avoid double mute/solo toggling
		if (InEntries.Num() > 1)
		{
			EntriesToMuteSolo.Reserve(InEntries.Num());

			for (const TSharedPtr<IDashboardDataTreeViewEntry>& Entry : InEntries)
			{
				if (!Entry.IsValid())
				{
					continue;
				}

				bool bIsTopLevelEntry = true;

				for (const TSharedPtr<IDashboardDataTreeViewEntry>& Other : InEntries)
				{
					if (Other != Entry && IsDescendant(Other, Entry))
					{
						bIsTopLevelEntry = false;
						break;
					}
				}

				if (bIsTopLevelEntry)
				{
					EntriesToMuteSolo.Add(Entry);
				}
			}
		}
		else
		{
			EntriesToMuteSolo = InEntries;
		}

		for (const TSharedPtr<IDashboardDataTreeViewEntry>& Entry : EntriesToMuteSolo)
		{
			if (!Entry.IsValid())
			{
				continue;
			}

			if (EntryCanHaveChildren(Entry.ToSharedRef()))
			{
				const bool bAreChildrenMuteSolo = FSoundDashboardViewFactoryPrivate::IsMuteSolo(*Entry, true /*bInCheckChildren*/, InMuteSoloMode);

				for (const TSharedPtr<IDashboardDataTreeViewEntry>& ChildEntry : Entry->Children)
				{
					if (ChildEntry.IsValid())
					{
						FSoundDashboardViewFactoryPrivate::SetMuteSolo(*ChildEntry, InMuteSoloMode, !bAreChildrenMuteSolo);
					}
				}
			}
			else
			{
				FSoundDashboardViewFactoryPrivate::ToggleMuteSolo(*Entry, InMuteSoloMode);
			}
		}
	}

	TArray<TObjectPtr<UObject>> FSoundDashboardViewFactory::GetSelectedEditableAssets() const
	{
		TArray<TObjectPtr<UObject>> Objects;

		if (!FilteredEntriesListView.IsValid())
		{
			return Objects;
		}

		const TArray<TSharedPtr<IDashboardDataTreeViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();

		Algo::TransformIf(SelectedItems, Objects,
			[](const TSharedPtr<IDashboardDataTreeViewEntry>& SelectedItem)
			{
				if (SelectedItem.IsValid())
				{
					IObjectTreeDashboardEntry& RowData = *StaticCastSharedPtr<IObjectTreeDashboardEntry>(SelectedItem).Get();

					if (TObjectPtr<UObject> Object = RowData.GetObject())
					{
						return Object->IsAsset();
					}
				}

				return false;
			},
			[](const TSharedPtr<IDashboardDataTreeViewEntry>& SelectedItem) -> TObjectPtr<UObject>
			{
				if (SelectedItem.IsValid())
				{
					IObjectTreeDashboardEntry& RowData = *StaticCastSharedPtr<IObjectTreeDashboardEntry>(SelectedItem).Get();
					return RowData.GetObject();
				}

				return nullptr;
			}
		);

		return Objects;
	}
#endif // WITH_EDITOR

	bool FSoundDashboardViewFactory::SelectedItemsIncludesAnAsset() const
	{
		using namespace FSoundDashboardViewFactoryPrivate;
		if (!FilteredEntriesListView.IsValid())
		{
			return false;
		}

		for (const TSharedPtr<IDashboardDataTreeViewEntry>& SelectedEntry : FilteredEntriesListView->GetSelectedItems())
		{
			if (!IsCategoryItem(*SelectedEntry))
			{
				return true;
			}
		}

		return false;
	}

	void FSoundDashboardViewFactory::PinSound()
	{
		if (FilteredEntriesListView.IsValid())
		{
			const TArray<TSharedPtr<IDashboardDataTreeViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();
			const bool bSelectionContainsAssets = SelectedItemsIncludesAnAsset();

			for (TSharedPtr<IDashboardDataTreeViewEntry>& Entry : DataViewEntries)
			{
				// If only categories are selected, pin the entire category
				if (SelectedItems.Contains(Entry) && !bSelectionContainsAssets)
				{
					for (TSharedPtr<IDashboardDataTreeViewEntry>& ChildEntry : Entry->Children)
					{
						MarkBranchAsPinned(ChildEntry, true /*bIsPinned*/);
						CreatePinnedEntry(ChildEntry);
					}
				}
				else
				{
					PinSelectedItems(Entry, SelectedItems);
				}
			}
			
			// Make sure to clear the selection and the internal selector in SListView
			// to ensure no shared references keep the entry alive past the point
			// it has been removed from the dashboard
			FilteredEntriesListView->ClearSelection();
			FilteredEntriesListView->SetSelection(nullptr);
		}
	}

	void FSoundDashboardViewFactory::UnpinSound()
	{
		if (FilteredEntriesListView.IsValid() && PinnedItemEntries.IsValid())
		{
			const TArray<TSharedPtr<IDashboardDataTreeViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();
			const bool bSelectionContainsAssets = SelectedItemsIncludesAnAsset();

			// If the user has only selected the pinned item row, unpin everything
			if (SelectedItems.Num() == 1 && SelectedItems[0] == PinnedItemEntries->GetPinnedSectionEntry())
			{
				for (TSharedPtr<IDashboardDataTreeViewEntry>& OriginalChildEntry : DataViewEntries)
				{
					MarkBranchAsPinned(OriginalChildEntry, false /*bIsPinned*/);
				}
				
				PinnedItemEntries.Reset();
			}
			else
			{
				UnpinSelectedItems(PinnedItemEntries, SelectedItems, bSelectionContainsAssets);
			}
			
			// Make sure to clear the selection and the internal selector in SListView
			// to ensure no shared references keep the entry alive past the point
			// it has been removed from the dashboard
			FilteredEntriesListView->ClearSelection();
			FilteredEntriesListView->SetSelection(nullptr);
		}
	}

	bool FSoundDashboardViewFactory::SelectionIncludesUnpinnedItem() const
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		if (!FilteredEntriesListView.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<IDashboardDataTreeViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();
		for (const TSharedPtr<IDashboardDataTreeViewEntry>& SelectedItem : SelectedItems)
		{
			if (!SelectedItem.IsValid())
			{
				continue;
			}

			const FSoundDashboardEntry& SelectedSoundEntry = CastEntry(*SelectedItem);
			if (SelectedSoundEntry.PinnedEntryType == FSoundDashboardEntry::EPinnedEntryType::None)
			{
				return true;
			}
		}

		return false;
	}

	void FSoundDashboardViewFactory::PinSelectedItems(const TSharedPtr<IDashboardDataTreeViewEntry>& Entry, const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& SelectedItems)
	{
		for (const TSharedPtr<IDashboardDataTreeViewEntry>& Child : Entry->Children)
		{
			if (SelectedItems.Contains(Child))
			{
				if (IsRootItem(Entry.ToSharedRef()))
				{
					MarkBranchAsPinned(Child, true /*bIsPinned*/);
					CreatePinnedEntry(Child);
				}
				else
				{
					MarkBranchAsPinned(Entry, true /*bIsPinned*/);
					CreatePinnedEntry(Entry);
				}
			}
			else
			{
				PinSelectedItems(Child, SelectedItems);
			}
		}
	}

	void FSoundDashboardViewFactory::UnpinSelectedItems(const TSharedPtr<FPinnedSoundEntryWrapper>& PinnedWrapperEntry, const TArray<TSharedPtr<IDashboardDataTreeViewEntry>>& SelectedItems, const bool bSelectionContainsAssets)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		// Run through all the pinned items in the dashboard and check if they are in the list of selected items.
		// If the child of a non-category parent is selected, we will move the parent and all of it's children back to the unpinned section
		for (const TSharedPtr<FPinnedSoundEntryWrapper>& PinnedWrapperChild : PinnedWrapperEntry->PinnedWrapperChildren)
		{
			const TSharedPtr<IDashboardDataTreeViewEntry> OriginalChildEntry = PinnedWrapperChild->GetOriginalDataEntry();
			if (!OriginalChildEntry.IsValid())
			{
				UnpinSelectedItems(PinnedWrapperChild, SelectedItems, bSelectionContainsAssets);
				continue;
			}

			const FSoundDashboardEntry& OriginalChildSoundEntry = CastEntry(*OriginalChildEntry);

			const TSharedPtr<IDashboardDataTreeViewEntry>* FoundMatchingSelectedEntry = SelectedItems.FindByPredicate([&OriginalChildSoundEntry, bSelectionContainsAssets](const TSharedPtr<IDashboardDataTreeViewEntry> SelectedEntry)
			{
				if (!SelectedEntry.IsValid())
				{
					return false;
				}

				const FSoundDashboardEntry& SelectedSoundEntry = CastEntry(*SelectedEntry);
				if (SelectedSoundEntry.PinnedEntryType != FSoundDashboardEntry::EPinnedEntryType::PinnedCopy)
				{
					return false;
				}

				if (SelectedSoundEntry.bIsCategory && !bSelectionContainsAssets)
				{
					return OriginalChildSoundEntry.EntryType == SelectedSoundEntry.EntryType;
				}
				
				return OriginalChildSoundEntry.PlayOrder == SelectedSoundEntry.PlayOrder;
			});

			if (FoundMatchingSelectedEntry != nullptr)
			{
				if (IsCategoryItem(*PinnedWrapperEntry->GetPinnedSectionEntry()))
				{
					// If the current parent pinned item is a category, move the child to the unpinned area and continue
					MarkBranchAsPinned(OriginalChildEntry, false /*bIsPinned*/);
					PinnedWrapperChild->MarkToDelete();
				}
				else
				{
					// If the current parent pinned item is not a category, move it and all of it's children to unpinned.
					// There is no need to check the other children so break.
					const TWeakPtr<IDashboardDataTreeViewEntry> OriginalPinnedEntry = PinnedWrapperEntry->GetOriginalDataEntry();
					if (OriginalPinnedEntry.IsValid())
					{
						MarkBranchAsPinned(OriginalPinnedEntry.Pin(), false /*bIsPinned*/);
						PinnedWrapperEntry->MarkToDelete();
						break;
					}
				}
			}
			else
			{
				// If this child item is not selected, check it's children.
				UnpinSelectedItems(PinnedWrapperChild, SelectedItems, bSelectionContainsAssets);
			}
		}
	}

	void FSoundDashboardViewFactory::MarkBranchAsPinned(const TSharedPtr<IDashboardDataTreeViewEntry> Entry, const bool bIsPinned)
	{
		FSoundDashboardEntry& SoundEntry = FSoundDashboardViewFactoryPrivate::CastEntry(*Entry);
		SoundEntry.PinnedEntryType = bIsPinned ? FSoundDashboardEntry::EPinnedEntryType::HiddenOriginalEntry : FSoundDashboardEntry::EPinnedEntryType::None;

		for (TSharedPtr<IDashboardDataTreeViewEntry> Child : Entry->Children)
		{
			MarkBranchAsPinned(Child, bIsPinned);
		}
	}

	void FSoundDashboardViewFactory::InitPinnedItemEntries()
	{
		if (PinnedItemEntries.IsValid())
		{
			return;
		}

		TSharedPtr<FSoundDashboardEntry> PinnedCategory = MakeShared<FSoundDashboardEntry>();
		PinnedCategory->Name = FSoundDashboardViewFactoryPrivate::PinnedCategoryName.ToString();
		PinnedCategory->EntryType = ESoundDashboardEntryType::Pinned;
		PinnedCategory->PinnedEntryType = FSoundDashboardEntry::EPinnedEntryType::PinnedCopy;
		PinnedCategory->bIsCategory = true;

		PinnedItemEntries = MakeShared<FPinnedSoundEntryWrapper>(PinnedCategory);
	}

	void FSoundDashboardViewFactory::CreatePinnedEntry(TSharedPtr<IDashboardDataTreeViewEntry> Entry)
	{
		using namespace FSoundDashboardViewFactoryPrivate;

		// If we have at least one entry that is pinned, ensure the pinned section has been created
		// The pinned area will delete itself once empty
		InitPinnedItemEntries();

		const FSoundDashboardEntry& SoundEntry = CastEntry(*Entry);

		// Check if category is already in the list, if so we need to merge
		bool bFoundExistingCategory = false;
		for (TSharedPtr<FPinnedSoundEntryWrapper> PinnedCategoryEntry : PinnedItemEntries->PinnedWrapperChildren)
		{
			if (!PinnedCategoryEntry.IsValid())
			{
				continue;
			}

			const FSoundDashboardEntry& PinnedCategorySoundDashboardEntry = CastEntry(*PinnedCategoryEntry->GetPinnedSectionEntry());

			if (PinnedCategorySoundDashboardEntry.EntryType == SoundEntry.EntryType)
			{
				bFoundExistingCategory = true;

				const TSharedPtr<IDashboardDataTreeViewEntry>* FoundExisingEntry = PinnedCategorySoundDashboardEntry.Children.FindByPredicate([&SoundEntry](const TSharedPtr<IDashboardDataTreeViewEntry> PinnedEntry)
				{
					if (!PinnedEntry.IsValid())
					{
						return false;
					}

					const FSoundDashboardEntry& ExistingPinnedEntry = CastEntry(*PinnedEntry);
					return ExistingPinnedEntry.PlayOrder == SoundEntry.PlayOrder;
				});

				// If we didn't find this entry already inside the pinned area, add it here
				if (FoundExisingEntry == nullptr)
				{
					PinnedCategoryEntry->AddChildEntry(Entry);
				}
				break;
			}
		}

		// if we haven't found an existing pinned category, create a new one and add this item
		if (!bFoundExistingCategory)
		{
			for (const TSharedPtr<IDashboardDataTreeViewEntry>& DataCategoryEntry : DataViewEntries)
			{
				if (SoundEntry.EntryType == CastEntry(*DataCategoryEntry).EntryType)
				{
					TSharedPtr<FSoundDashboardEntry> NewPinnedCategoryEntry = MakeShared<FSoundDashboardEntry>(CastEntry(*DataCategoryEntry));
					NewPinnedCategoryEntry->Children.Reset();
					NewPinnedCategoryEntry->bShouldForceExpandChildren = true;

					TSharedPtr<FPinnedSoundEntryWrapper> PinnedCategory = PinnedItemEntries->AddChildEntry(NewPinnedCategoryEntry);
					PinnedCategory->AddChildEntry(Entry);

					break;
				}
			}
		}
	}

	void FSoundDashboardViewFactory::UpdatePinnedSection()
	{
		if (!PinnedItemEntries.IsValid())
		{
			return;
		}

		PinnedItemEntries->CleanUp();

		if (FPinnedSoundEntryWrapperPrivate::CanBeDeleted(PinnedItemEntries))
		{
			PinnedItemEntries.Reset();
		}
		else
		{
			PinnedItemEntries->UpdateParams();
		}
	}

#if WITH_EDITOR
	void FSoundDashboardViewFactory::BrowseSoundAsset() const
	{
		if (GEditor)
		{
			TArray<UObject*> EditableAssets = GetSelectedEditableAssets();
			GEditor->SyncBrowserToObjects(EditableAssets);
		}
	}

	void FSoundDashboardViewFactory::OpenSoundAsset() const
	{
		if (GEditor && FilteredEntriesListView.IsValid())
		{
			TArray<UObject*> Objects = GetSelectedEditableAssets();
			if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetSubsystem->OpenEditorForAssets(Objects);
			}
		}
	}
#endif // WITH_EDITOR

	// @TODO UE-250399: Hide category pending to implement
	//void FSoundDashboardViewFactory::HideSound()
	//{
	//
	//}
} // namespace UE::Audio::Insights

#undef LOCTEXT_NAMESPACE
