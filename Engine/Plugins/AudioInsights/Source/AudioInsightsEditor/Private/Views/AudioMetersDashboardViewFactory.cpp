// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMetersDashboardViewFactory.h"

#include "AudioInsightsStyle.h"
#include "Providers/AudioBusProvider.h"
#include "Providers/SoundSubmixProvider.h"
#include "Sound/AudioBus.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundSubmix.h"
#include "Views/AudioBusesDashboardViewFactory.h"
#include "Views/SubmixesDashboardViewFactory.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	FName FAudioMetersDashboardViewFactory::GetName() const
	{
		return "AudioMeters";
	}

	FText FAudioMetersDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_AudioMetersTab_DisplayName", "Audio Meters");
	}

	EDefaultDashboardTabStack FAudioMetersDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::AudioMeters;
	}

	FSlateIcon FAudioMetersDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Submix");
	}

	TSharedRef<SWidget> FAudioMetersDashboardViewFactory::MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs)
	{
		if (!OnSubmixAssetInit.IsValid())
		{
			OnSubmixAssetInit = FSubmixesDashboardViewFactory::OnSubmixAssetInit.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnSubmixAssetInit);
		}

		if (!OnAudioBusAssetInit.IsValid())
		{
			OnAudioBusAssetInit = FAudioBusesDashboardViewFactory::OnBusAssetInit.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetInit);
		}

		if (!OnSubmixAssetCheckedHandle.IsValid())
		{
			OnSubmixAssetCheckedHandle = FSubmixesDashboardViewFactory::OnSubmixAssetChecked.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnSubmixAssetChecked);
		}

		if (!OnAudioBusAssetCheckedHandle.IsValid())
		{
			OnAudioBusAssetCheckedHandle = FAudioBusesDashboardViewFactory::OnAudioBusAssetChecked.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetChecked);
		}

		if (!OnSubmixAssetRemovedHandle.IsValid())
		{
			OnSubmixAssetRemovedHandle = FSoundSubmixProvider::OnSubmixAssetRemoved.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnSubmixAssetRemoved);
		}

		if (!OnAudioBusAssetRemovedHandle.IsValid())
		{
			OnAudioBusAssetRemovedHandle = FAudioBusProvider::OnAudioBusAssetRemoved.AddSP(this, &FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetRemoved);
		}
		
		if (!MeterViewsScrollBox.IsValid())
		{
			SAssignNew(MeterViewsScrollBox, SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SAssignNew(AudioMeterViewsContainer, SHorizontalBox)
			];
		}

		return MeterViewsScrollBox.ToSharedRef();
	}

	void FAudioMetersDashboardViewFactory::HandleOnSubmixAssetInit(const bool bInIsChecked, const uint32 InSubmixId, const FString& InSubmixName)
	{
		if (!bInIsChecked)
		{
			return;
		}

		if (!AudioMeterViews.Contains(InSubmixId))
		{
			return;
		}

		const TObjectPtr<UObject> LoadedSubmix = FSoftObjectPath(InSubmixName).TryLoad();
		if (LoadedSubmix == nullptr)
		{
			return;
		}

		// Tear down and re-create meter analyzers on initialization to ensure we receive the correct volume data
		const int32 SlotIndex = AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[InSubmixId]->GetWidget());

		AudioMeterViews.Remove(InSubmixId);

		AudioMeterViews.Add(InSubmixId, MakeShared<FAudioMeterView>(FAudioMeterView::FAudioAssetVariant(TInPlaceType<TWeakObjectPtr<USoundSubmix>>(), Cast<USoundSubmix>(LoadedSubmix))));

		AudioMeterViewsContainer->InsertSlot(SlotIndex)
		.AutoWidth()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		[
			AudioMeterViews[InSubmixId]->GetWidget()
		];

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetInit(const bool bInIsChecked, const TWeakObjectPtr<UAudioBus> InAudioBus)
	{
		if (!bInIsChecked || !InAudioBus.IsValid())
		{
			return;
		}

		const uint32 AudioBusUniqueId = InAudioBus->GetUniqueID();
		if (!AudioMeterViews.Contains(AudioBusUniqueId))
		{
			return;
		}

		// Tear down and re-create meter analyzers on initialization to ensure we receive the correct volume data
		const int32 SlotIndex = AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[AudioBusUniqueId]->GetWidget());

		AudioMeterViews.Remove(AudioBusUniqueId);
		AudioMeterViews.Add(AudioBusUniqueId, MakeShared<FAudioMeterView>(FAudioMeterView::FAudioAssetVariant(TInPlaceType<TWeakObjectPtr<UAudioBus>>(), InAudioBus)));

		AudioMeterViewsContainer->InsertSlot(SlotIndex)
		.AutoWidth()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		[
			AudioMeterViews[AudioBusUniqueId]->GetWidget()
		];

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnSubmixAssetChecked(const bool bInIsChecked, const uint32 InSubmixId, const FString& InSoundSubmixName)
	{
		if (bInIsChecked)
		{
			const TObjectPtr<UObject> LoadedSubmix = FSoftObjectPath(InSoundSubmixName).TryLoad();
			if (LoadedSubmix == nullptr)
			{
				return;
			}

			AudioMeterViews.Add(InSubmixId, MakeShared<FAudioMeterView>(FAudioMeterView::FAudioAssetVariant(TInPlaceType<TWeakObjectPtr<USoundSubmix>>(), Cast<USoundSubmix>(LoadedSubmix))));

			AudioMeterViewsContainer->AddSlot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			[
				AudioMeterViews[InSubmixId]->GetWidget()
			];
		}
		else
		{
			if (AudioMeterViews.Contains(InSubmixId))
			{
				AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[InSubmixId]->GetWidget());
				AudioMeterViews.Remove(InSubmixId);
			}
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetChecked(const bool bInIsChecked, const TWeakObjectPtr<UAudioBus> InAudioBus)
	{
		if (!InAudioBus.IsValid())
		{
			return;
		}

		const uint32 AudioBusUniqueId = InAudioBus->GetUniqueID();

		if (bInIsChecked)
		{
			AudioMeterViews.Add(AudioBusUniqueId, MakeShared<FAudioMeterView>(FAudioMeterView::FAudioAssetVariant(TInPlaceType<TWeakObjectPtr<UAudioBus>>(), InAudioBus)));

			AudioMeterViewsContainer->AddSlot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			[
				AudioMeterViews[AudioBusUniqueId]->GetWidget()
			];
		}
		else
		{
			if (AudioMeterViews.Contains(AudioBusUniqueId))
			{
				AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[AudioBusUniqueId]->GetWidget());
				AudioMeterViews.Remove(AudioBusUniqueId);
			}
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnSubmixAssetRemoved(const uint32 InSubmixId)
	{
		if (AudioMeterViews.Contains(InSubmixId))
		{
			AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[InSubmixId]->GetWidget());
			AudioMeterViews.Remove(InSubmixId);
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}

	void FAudioMetersDashboardViewFactory::HandleOnAudioBusAssetRemoved(const TWeakObjectPtr<UObject> InAudioBusAsset)
	{
		if (!InAudioBusAsset.IsValid())
		{
			return;
		}

		if (const uint32 AssetUniqueId = InAudioBusAsset->GetUniqueID();
			AudioMeterViews.Contains(AssetUniqueId))
		{
			AudioMeterViewsContainer->RemoveSlot(AudioMeterViews[AssetUniqueId]->GetWidget());
			AudioMeterViews.Remove(AssetUniqueId);
		}

		if (MeterViewsScrollBox.IsValid())
		{
			MeterViewsScrollBox->Invalidate(EInvalidateWidget::Layout);
		}
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
