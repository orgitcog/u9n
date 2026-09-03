// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMediaSourceWidget.h"
#include "MetaHumanStringCombo.h"
#include "MetaHumanPipelineMediaPlayerNode.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaBundle.h"
#include "MediaCaptureSupport.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetData.h"

#if WITH_EDITOR
#include "DetailLayoutBuilder.h"
#endif

#define LOCTEXT_NAMESPACE "MetaHumanLocalLiveLinkSource"



class SMetaHumanMediaSourceWidgetImpl : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanMediaSourceWidgetImpl) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, SMetaHumanMediaSourceWidget::EMediaType InMediaType);

	void OnAssetsAddedOrDeleted(TConstArrayView<FAssetData> InAssets);
	void OnAssetRenamed(const FAssetData& InAsset, const FString& InOldObjectPath);
	void PopulateDevices();

	SMetaHumanMediaSourceWidget::EMediaType MediaType;

	TArray<SMetaHumanStringCombo::FComboItemType> VideoDeviceItems;
	TArray<SMetaHumanStringCombo::FComboItemType> VideoTrackItems;
	TArray<SMetaHumanStringCombo::FComboItemType> VideoTrackFormatItems;
	bool bVideoTrackFormatItemsFiltered = true;

	TSharedPtr<SMetaHumanStringCombo> VideoDeviceCombo;
	TSharedPtr<SMetaHumanStringCombo> VideoTrackCombo;
	TSharedPtr<SMetaHumanStringCombo> VideoTrackFormatCombo;

	void OnVideoDeviceSelected(SMetaHumanStringCombo::FComboItemType InItem);
	void OnVideoDeviceEvent(EMediaEvent InEvent);
	void OnVideoTrackSelected(SMetaHumanStringCombo::FComboItemType InItem);
	void OnVideoTrackFormatSelected(SMetaHumanStringCombo::FComboItemType InItem);

	TArray<SMetaHumanStringCombo::FComboItemType> AudioDeviceItems;
	TArray<SMetaHumanStringCombo::FComboItemType> AudioTrackItems;
	TArray<SMetaHumanStringCombo::FComboItemType> AudioTrackFormatItems;

	TSharedPtr<SMetaHumanStringCombo> AudioDeviceCombo;
	TSharedPtr<SMetaHumanStringCombo> AudioTrackCombo;
	TSharedPtr<SMetaHumanStringCombo> AudioTrackFormatCombo;

	void OnAudioDeviceSelected(SMetaHumanStringCombo::FComboItemType InItem);
	void OnAudioDeviceEvent(EMediaEvent InEvent);
	void OnAudioTrackSelected(SMetaHumanStringCombo::FComboItemType InItem);
	void OnAudioTrackFormatSelected(SMetaHumanStringCombo::FComboItemType InItem);

	TSharedPtr<SCheckBox> AdvancedCheckBox;

	TSharedPtr<SCheckBox> FilteredWidget;
	TSharedPtr<SNumericEntryBox<float>> StartTimeoutWidget;
	TSharedPtr<SNumericEntryBox<float>> FormatWaitTimeWidget;
	TSharedPtr<SNumericEntryBox<float>> SampleTimeoutWidget;

	bool CanCreate() const;

	TObjectPtr<UMediaPlayer> VideoPlayer;
	TObjectPtr<UMediaPlayer> AudioPlayer;

	double StartTimeout = 5;
	double FormatWaitTime = 0.1;
	double SampleTimeout = 5;

	bool IsBundle() const;
	EVisibility GetTrackVisibility() const;
	bool IsTrackEnabled() const;
	FText GetTrackTooltip() const;
	EVisibility GetAdvancedVisibility() const;

	//~ Begin FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const;

	//~ End FGCObject interface

	FMetaHumanMediaSourceCreateParams GetCreateParams() const;
};







void SMetaHumanMediaSourceWidgetImpl::Construct(const FArguments& InArgs, SMetaHumanMediaSourceWidget::EMediaType InMediaType)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	AssetRegistryModule.Get().OnAssetsAdded().AddSP(this, &SMetaHumanMediaSourceWidgetImpl::OnAssetsAddedOrDeleted);
	AssetRegistryModule.Get().OnAssetsRemoved().AddSP(this, &SMetaHumanMediaSourceWidgetImpl::OnAssetsAddedOrDeleted);
	AssetRegistryModule.Get().OnAssetRenamed().AddSP(this, &SMetaHumanMediaSourceWidgetImpl::OnAssetRenamed);

	MediaType = InMediaType;

	VideoPlayer = NewObject<UMediaPlayer>(GetTransientPackage());
	check(VideoPlayer);
	VideoPlayer->OnMediaEvent().AddSP(this, &SMetaHumanMediaSourceWidgetImpl::OnVideoDeviceEvent);
	VideoPlayer->PlayOnOpen = false;

	AudioPlayer = NewObject<UMediaPlayer>(GetTransientPackage());
	check(AudioPlayer);
	AudioPlayer->OnMediaEvent().AddSP(this, &SMetaHumanMediaSourceWidgetImpl::OnAudioDeviceEvent);
	AudioPlayer->PlayOnOpen = false;

	VideoDeviceCombo = SNew(SMetaHumanStringCombo, &VideoDeviceItems)
					   .OnItemSelected(this, &SMetaHumanMediaSourceWidgetImpl::OnVideoDeviceSelected);

	VideoTrackCombo = SNew(SMetaHumanStringCombo, &VideoTrackItems)
					  .Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
					  .IsEnabled(this, &SMetaHumanMediaSourceWidgetImpl::IsTrackEnabled)
					  .ToolTipText(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackTooltip)
					  .OnItemSelected(this, &SMetaHumanMediaSourceWidgetImpl::OnVideoTrackSelected);

	VideoTrackFormatCombo = SNew(SMetaHumanStringCombo, &VideoTrackFormatItems)
							.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
							.IsEnabled(this, &SMetaHumanMediaSourceWidgetImpl::IsTrackEnabled)
							.ToolTipText(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackTooltip)
							.OnItemSelected(this, &SMetaHumanMediaSourceWidgetImpl::OnVideoTrackFormatSelected);

	AudioDeviceCombo = SNew(SMetaHumanStringCombo, &AudioDeviceItems)
					   .OnItemSelected(this, &SMetaHumanMediaSourceWidgetImpl::OnAudioDeviceSelected);

	AudioTrackCombo = SNew(SMetaHumanStringCombo, &AudioTrackItems)
					  .Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
					  .IsEnabled(this, &SMetaHumanMediaSourceWidgetImpl::IsTrackEnabled)
					  .ToolTipText(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackTooltip)
					  .OnItemSelected(this, &SMetaHumanMediaSourceWidgetImpl::OnAudioTrackSelected);

	AudioTrackFormatCombo = SNew(SMetaHumanStringCombo, &AudioTrackFormatItems)
						    .Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
							.IsEnabled(this, &SMetaHumanMediaSourceWidgetImpl::IsTrackEnabled)
							.ToolTipText(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackTooltip)
							.OnItemSelected(this, &SMetaHumanMediaSourceWidgetImpl::OnAudioTrackFormatSelected);

	AdvancedCheckBox = SNew(SCheckBox);

	FilteredWidget = SNew(SCheckBox)
		.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
		.ToolTipText(LOCTEXT("FilteredTooltip", "Filter the formats to show only the most relevant ones"))
		.IsChecked_Lambda([this]()
		{
			return bVideoTrackFormatItemsFiltered ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
		{
			bVideoTrackFormatItemsFiltered = (InState == ECheckBoxState::Checked);

			OnVideoTrackSelected(VideoTrackCombo->CurrentItem);
		});

	StartTimeoutWidget = SNew(SNumericEntryBox<float>)
		.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
#if WITH_EDITOR
		.Font(IDetailLayoutBuilder::GetDetailFont())
#endif
		.ToolTipText(LOCTEXT("StartTimeoutTooltip", "Timeout for waiting for media to open"))
		.Value_Lambda([this]()
		{
			return StartTimeout;
		})
		.OnValueCommitted_Lambda([this](float InValue, ETextCommit::Type)
		{
			StartTimeout = InValue;
		});

	FormatWaitTimeWidget = SNew(SNumericEntryBox<float>)
		.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
#if WITH_EDITOR
		.Font(IDetailLayoutBuilder::GetDetailFont())
#endif
		.ToolTipText(LOCTEXT("FormatWaitTimeTooltip", "Time to wait for format changes to take effect"))
		.Value_Lambda([this]()
		{
			return FormatWaitTime;
		})
		.OnValueCommitted_Lambda([this](float InValue, ETextCommit::Type)
		{
			FormatWaitTime = InValue;
		});

	SampleTimeoutWidget = SNew(SNumericEntryBox<float>)
		.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
#if WITH_EDITOR
		.Font(IDetailLayoutBuilder::GetDetailFont())
#endif
		.ToolTipText(LOCTEXT("SampleTimeoutTooltip", "Timeout for waiting on first media sample to arrive"))
		.Value_Lambda([this]()
		{
			return SampleTimeout;
		})
		.OnValueCommitted_Lambda([this](float InValue, ETextCommit::Type)
		{
			SampleTimeout = InValue;
		});

	PopulateDevices();

	const float Padding = 5;
	const float FirstColWidth = 140;

	TSharedPtr<SVerticalBox> Layout = SNew(SVerticalBox);

	if (MediaType == SMetaHumanMediaSourceWidget::EMediaType::Video || MediaType == SMetaHumanMediaSourceWidget::EMediaType::VideoAndAudio)
	{
		Layout->AddSlot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(Padding)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("VideoDevice", "Video Device"))
						.MinDesiredWidth(FirstColWidth)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						VideoDeviceCombo.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot()
				.Padding(Padding)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("VideoTrack", "Video Track"))
						.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
						.MinDesiredWidth(FirstColWidth)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						VideoTrackCombo.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot()
				.Padding(Padding)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("VideoTrackFormat", "Video Track Format"))
						.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
						.MinDesiredWidth(FirstColWidth)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						VideoTrackFormatCombo.ToSharedRef()
					]
				]
			];
	}

	if (MediaType == SMetaHumanMediaSourceWidget::EMediaType::Audio || MediaType == SMetaHumanMediaSourceWidget::EMediaType::VideoAndAudio)
	{
		Layout->AddSlot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(Padding)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AudioDevice", "Audio Device"))
						.MinDesiredWidth(FirstColWidth)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						AudioDeviceCombo.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot()
				.Padding(Padding)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AudioTrack", "Audio Track"))
						.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
						.MinDesiredWidth(FirstColWidth)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						AudioTrackCombo.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot()
				.Padding(Padding)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AudioTrackFormat", "Audio Track Format"))
						.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility)
						.MinDesiredWidth(FirstColWidth)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						AudioTrackFormatCombo.ToSharedRef()
					]
				]
			];
	}

	Layout->AddSlot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(Padding)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Advanced", "Advanced"))
					.MinDesiredWidth(FirstColWidth)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.AutoWidth()
				[
					AdvancedCheckBox.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.Padding(Padding * 6, 0)
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(Padding)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Filtered", "Filter Format List"))
							.ToolTipText(LOCTEXT("FilteredTooltip", "Filter the formats to show only the most relevant ones"))
							.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
							.MinDesiredWidth(FirstColWidth)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							FilteredWidget.ToSharedRef()
						]
					]
					+ SVerticalBox::Slot()
					.Padding(Padding)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("StartTimeout", "Start Timeout"))
							.ToolTipText(LOCTEXT("StartTimeoutTooltip", "Timeout for waiting for media to open"))
							.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
							.MinDesiredWidth(FirstColWidth)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							StartTimeoutWidget.ToSharedRef()
						]
					]
					+ SVerticalBox::Slot()
					.Padding(Padding)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FormatWaitTime", "Format Wait Time"))
							.ToolTipText(LOCTEXT("FormatWaitTimeTooltip", "Time to wait for format changes to take effect"))
							.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
							.MinDesiredWidth(FirstColWidth)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							FormatWaitTimeWidget.ToSharedRef()
						]
					]
					+ SVerticalBox::Slot()
					.Padding(Padding)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SampleTimeout", "Sample Timeout"))
							.ToolTipText(LOCTEXT("SampleTimeoutTooltip", "Timeout for waiting on first media sample to arrive"))
							.Visibility(this, &SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility)
							.MinDesiredWidth(FirstColWidth)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SampleTimeoutWidget.ToSharedRef()
						]
					]
				]
			]
		];

	ChildSlot
	[
		Layout.ToSharedRef()
	];
}

void SMetaHumanMediaSourceWidgetImpl::OnAssetsAddedOrDeleted(TConstArrayView<FAssetData> InAssets)
{
	PopulateDevices();
}

void SMetaHumanMediaSourceWidgetImpl::OnAssetRenamed(const FAssetData& InAsset, const FString& InOldObjectPath)
{
	PopulateDevices();
}

void SMetaHumanMediaSourceWidgetImpl::PopulateDevices()
{
	VideoDeviceItems.Reset();
	AudioDeviceItems.Reset();

	TArray<FMediaCaptureDeviceInfo> VideoDeviceInfos;
	MediaCaptureSupport::EnumerateVideoCaptureDevices(VideoDeviceInfos);

	for (const FMediaCaptureDeviceInfo& VideoDeviceInfo : VideoDeviceInfos)
	{
		VideoDeviceItems.Add(MakeShared<TPair<FString, FString>>(VideoDeviceInfo.DisplayName.ToString(), VideoDeviceInfo.Url));
	}

	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	TArray<FAssetData> MediaBundles;
	AssetRegistry.GetAssetsByClass(UMediaBundle::StaticClass()->GetClassPathName(), MediaBundles);

	for (const FAssetData& MediaBundle : MediaBundles)
	{
		UObject* Bundle = MediaBundle.GetAsset();
		if (Bundle)
		{
			VideoDeviceItems.Add(MakeShared<TPair<FString, FString>>(Bundle->GetName(), UE::MetaHuman::Pipeline::FMediaPlayerNode::BundleURL + Bundle->GetPathName()));
		}
	}

	VideoDeviceCombo->RefreshOptions();

	OnVideoDeviceSelected(VideoDeviceItems.IsEmpty() ? nullptr : VideoDeviceItems[0]);

	TArray<FMediaCaptureDeviceInfo> AudioDeviceInfos;
	MediaCaptureSupport::EnumerateAudioCaptureDevices(AudioDeviceInfos);

	for (const FMediaCaptureDeviceInfo& AudioDeviceInfo : AudioDeviceInfos)
	{
		AudioDeviceItems.Add(MakeShared<TPair<FString, FString>>(AudioDeviceInfo.DisplayName.ToString(), AudioDeviceInfo.Url));
	}

	for (const FAssetData& MediaBundle : MediaBundles)
	{
		UObject* Bundle = MediaBundle.GetAsset();
		if (Bundle)
		{
			AudioDeviceItems.Add(MakeShared<TPair<FString, FString>>(Bundle->GetName(), UE::MetaHuman::Pipeline::FMediaPlayerNode::BundleURL + Bundle->GetPathName()));
		}
	}

	AudioDeviceCombo->RefreshOptions();

	OnAudioDeviceSelected(AudioDeviceItems.IsEmpty() ? nullptr : AudioDeviceItems[0]);
}

void SMetaHumanMediaSourceWidgetImpl::OnVideoDeviceSelected(SMetaHumanStringCombo::FComboItemType InItem)
{
	VideoDeviceCombo->CurrentItem = InItem;

	VideoTrackItems.Reset();
	OnVideoTrackSelected(nullptr);

	VideoPlayer->Close();

	if (InItem)
	{
		VideoPlayer->OpenUrl(InItem->Value); // async call, picked up in OnVideoDeviceEvent function below
	}
}

void SMetaHumanMediaSourceWidgetImpl::OnVideoDeviceEvent(EMediaEvent InEvent)
{
	// Dont rely on InEvent == EMediaEvent::MediaOpened in this function, EMediaEvent::MediaOpenFailed
	// may also suffice for our needs here to just list tracks/format. 
	// We get the failed case for the BRIO camera which has a strange video track 0 (a MSN audio track).
	// Without the codec for that we can get the "failed" case even though that track wont be used in practice. 
	// Video track 1 contains all the useable formats for the BRIO.
	// One solution would be to install the codec, but that would be a step required of all end-users
	// and is a codec thats never needed in practice. Better to treat the "MediaOpenFailed" more
	// as a warning and carry on. Error handling when you actually select a video track/format and
	// attempt to play it will catch any real errors.

	if (InEvent == EMediaEvent::MediaOpened || InEvent == EMediaEvent::MediaOpenFailed)
	{
		const int32 NumTracks = VideoPlayer->GetNumTracks(EMediaPlayerTrack::Video);

		for (int32 Track = 0; Track < NumTracks; ++Track)
		{
			VideoTrackItems.Add(MakeShared<TPair<FString, FString>>(FString::FromInt(Track), FString::FromInt(Track)));
		}

		VideoTrackCombo->RefreshOptions();

		if (VideoTrackItems.IsEmpty())
		{
			OnVideoTrackSelected(nullptr);
		}
		else
		{
			for (int32 VideoTrack = 0; VideoTrack < NumTracks; ++VideoTrack)
			{
				OnVideoTrackSelected(VideoTrackItems[VideoTrack]);

				if (!VideoTrackFormatItems.IsEmpty())
				{
					break;
				}
			}
		}
	}
}

void SMetaHumanMediaSourceWidgetImpl::OnVideoTrackSelected(SMetaHumanStringCombo::FComboItemType InItem)
{
	VideoTrackCombo->CurrentItem = InItem;

	VideoTrackFormatItems.Reset();

	if (InItem)
	{
		const int32 Track = FCString::Atoi(*InItem->Key);
		const int32 NumTrackFormats = VideoPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, Track);
		TMap<FString, TTuple<FIntPoint, float, FString>> TrackFormatInfo;

		for (int32 TrackFormat = NumTrackFormats - 1; TrackFormat >= 0; --TrackFormat)
		{
			const FIntPoint Res = VideoPlayer->GetVideoTrackDimensions(Track, TrackFormat);
			const float FrameRate = VideoPlayer->GetVideoTrackFrameRate(Track, TrackFormat);
			const FString Type = VideoPlayer->GetVideoTrackType(Track, TrackFormat);

			if (!bVideoTrackFormatItemsFiltered ||
				((Type == TEXT("NV12") || Type == TEXT("YUY2") || Type == TEXT("UYVY") || Type == TEXT("BGRA")) && Res.X > 500 && Res.Y > 500 && FrameRate >= 24))
			{
				FString DisplayText = FString::Printf(TEXT("%d: %s %ix%i"), TrackFormat, *Type, Res.X, Res.Y);

				int32 IntFrameRate = FMath::RoundToInt(FrameRate);
				if (FMath::Abs(FrameRate - IntFrameRate) > 0.0001)
				{
					DisplayText += FString::Printf(TEXT(" %.2f fps"), FrameRate);
				}
				else
				{
					DisplayText += FString::Printf(TEXT(" %i fps"), IntFrameRate);
				}

				VideoTrackFormatItems.Add(MakeShared<TPair<FString, FString>>(DisplayText, FString::FromInt(TrackFormat)));

				TrackFormatInfo.Add(FString::FromInt(TrackFormat), TTuple<FIntPoint, float, FString>(Res, FrameRate, Type));
			}
		}

		VideoTrackFormatItems.Sort([TrackFormatInfo](const SMetaHumanStringCombo::FComboItemType& InItem1, const SMetaHumanStringCombo::FComboItemType& InItem2)
		{
			// Sort first by fps, then res, then type
			const TTuple<FIntPoint, float, FString>& Item1Info = TrackFormatInfo[InItem1->Value];
			const TTuple<FIntPoint, float, FString>& Item2Info = TrackFormatInfo[InItem2->Value];

			if (Item1Info.Get<float>() == Item2Info.Get<float>())
			{
				if (Item1Info.Get<FIntPoint>() == Item2Info.Get<FIntPoint>())
				{
					return Item1Info.Get<FString>() > Item2Info.Get<FString>();
				}
				else
				{
					return Item1Info.Get<FIntPoint>().Size() > Item2Info.Get<FIntPoint>().Size();
				}
			}
			else
			{
				return Item1Info.Get<float>() > Item2Info.Get<float>();
			}
		});
	}

	VideoTrackFormatCombo->RefreshOptions();

	OnVideoTrackFormatSelected(VideoTrackFormatItems.IsEmpty() ? nullptr : VideoTrackFormatItems[0]);
}

void SMetaHumanMediaSourceWidgetImpl::OnVideoTrackFormatSelected(SMetaHumanStringCombo::FComboItemType InItem)
{
	VideoTrackFormatCombo->CurrentItem = InItem;
}

void SMetaHumanMediaSourceWidgetImpl::OnAudioDeviceSelected(SMetaHumanStringCombo::FComboItemType InItem)
{
	AudioDeviceCombo->CurrentItem = InItem;

	AudioTrackItems.Reset();
	OnAudioTrackSelected(nullptr);

	AudioPlayer->Close();

	if (InItem)
	{
		AudioPlayer->OpenUrl(InItem->Value); // async call, picked up in OnAudioDeviceEvent function below
	}
}

void SMetaHumanMediaSourceWidgetImpl::OnAudioDeviceEvent(EMediaEvent InEvent)
{
	if (InEvent == EMediaEvent::MediaOpened)
	{
		const int32 NumTracks = AudioPlayer->GetNumTracks(EMediaPlayerTrack::Audio);

		for (int32 Track = 0; Track < NumTracks; ++Track)
		{
			AudioTrackItems.Add(MakeShared<TPair<FString, FString>>(FString::FromInt(Track), FString::FromInt(Track)));
		}

		AudioTrackCombo->RefreshOptions();

		OnAudioTrackSelected(AudioTrackItems.IsEmpty() ? nullptr : AudioTrackItems[0]);
	}
}

void SMetaHumanMediaSourceWidgetImpl::OnAudioTrackSelected(SMetaHumanStringCombo::FComboItemType InItem)
{
	AudioTrackCombo->CurrentItem = InItem;

	AudioTrackFormatItems.Reset();

	if (InItem)
	{
		const int32 Track = FCString::Atoi(*InItem->Key);
		const int32 NumTrackFormats = AudioPlayer->GetNumTrackFormats(EMediaPlayerTrack::Audio, Track);

		for (int32 TrackFormat = 0; TrackFormat < NumTrackFormats; ++TrackFormat)
		{
			const int32 NumChannels = AudioPlayer->GetAudioTrackChannels(Track, TrackFormat);
			const int32 SampleRate = AudioPlayer->GetAudioTrackSampleRate(Track, TrackFormat);
			const FString Type = AudioPlayer->GetAudioTrackType(Track, TrackFormat);

			const FString DisplayText = FString::Printf(TEXT("%d: %s %i channels @ %i Hz"), TrackFormat, *Type, NumChannels, SampleRate);

			AudioTrackFormatItems.Add(MakeShared<TPair<FString, FString>>(DisplayText, FString::FromInt(TrackFormat)));
		}
	}

	AudioTrackFormatCombo->RefreshOptions();

	OnAudioTrackFormatSelected(AudioTrackFormatItems.IsEmpty() ? nullptr : AudioTrackFormatItems[0]);
}

void SMetaHumanMediaSourceWidgetImpl::OnAudioTrackFormatSelected(SMetaHumanStringCombo::FComboItemType InItem)
{
	AudioTrackFormatCombo->CurrentItem = InItem;
}

bool SMetaHumanMediaSourceWidgetImpl::CanCreate() const
{
	if (MediaType == SMetaHumanMediaSourceWidget::EMediaType::Video || MediaType == SMetaHumanMediaSourceWidget::EMediaType::VideoAndAudio)
	{
		return VideoDeviceCombo->CurrentItem.IsValid() && (IsBundle() || (VideoTrackCombo->CurrentItem.IsValid() && VideoTrackFormatCombo->CurrentItem.IsValid()));
	}
	else
	{
		return AudioDeviceCombo->CurrentItem.IsValid() && (IsBundle() || (AudioTrackCombo->CurrentItem.IsValid() && AudioTrackFormatCombo->CurrentItem.IsValid()));
	}
}

void SMetaHumanMediaSourceWidgetImpl::AddReferencedObjects(FReferenceCollector& InCollector)
{
	InCollector.AddReferencedObject(VideoPlayer);
	InCollector.AddReferencedObject(AudioPlayer);
}

FString SMetaHumanMediaSourceWidgetImpl::GetReferencerName() const
{
	return TEXT("SMetaHumanMediaSourceWidgetImpl");
}

bool SMetaHumanMediaSourceWidgetImpl::IsBundle() const
{
	SMetaHumanStringCombo::FComboItemType CurrentItem;
	
	if (MediaType == SMetaHumanMediaSourceWidget::EMediaType::Video || MediaType == SMetaHumanMediaSourceWidget::EMediaType::VideoAndAudio)
	{
		CurrentItem = VideoDeviceCombo->CurrentItem;
	}
	else
	{
		CurrentItem = AudioDeviceCombo->CurrentItem;
	}

	return CurrentItem.IsValid() ? CurrentItem->Value.StartsWith(UE::MetaHuman::Pipeline::FMediaPlayerNode::BundleURL) : false;
}

EVisibility SMetaHumanMediaSourceWidgetImpl::GetTrackVisibility() const
{
	return EVisibility::Visible;
}

EVisibility SMetaHumanMediaSourceWidgetImpl::GetAdvancedVisibility() const
{
	return EVisibility::Visible;
}

bool SMetaHumanMediaSourceWidgetImpl::IsTrackEnabled() const 
{ 
	return !IsBundle(); 
}

FText SMetaHumanMediaSourceWidgetImpl::GetTrackTooltip() const 
{ 
	return IsBundle() ? LOCTEXT("DisabledBundle", "Disabled for Media Bundles") : FText();
}

FMetaHumanMediaSourceCreateParams SMetaHumanMediaSourceWidgetImpl::GetCreateParams() const
{
	FMetaHumanMediaSourceCreateParams CreateParams;

	CreateParams.VideoName = VideoDeviceCombo->CurrentItem.IsValid() ? VideoDeviceCombo->CurrentItem->Key : TEXT("");
	CreateParams.VideoURL = VideoDeviceCombo->CurrentItem.IsValid() ? VideoDeviceCombo->CurrentItem->Value : TEXT("");
	CreateParams.VideoTrack = VideoTrackCombo->CurrentItem.IsValid() ? FCString::Atoi(*VideoTrackCombo->CurrentItem->Value) : -1;
	CreateParams.VideoTrackFormat = VideoTrackFormatCombo->CurrentItem.IsValid() ? FCString::Atoi(*VideoTrackFormatCombo->CurrentItem->Value) : -1;
	CreateParams.VideoTrackFormatName = VideoTrackFormatCombo->CurrentItem.IsValid() ? VideoTrackFormatCombo->CurrentItem->Key : TEXT("");

	CreateParams.AudioName = AudioDeviceCombo->CurrentItem.IsValid() ? AudioDeviceCombo->CurrentItem->Key : TEXT("");
	CreateParams.AudioURL = AudioDeviceCombo->CurrentItem.IsValid() ? AudioDeviceCombo->CurrentItem->Value : TEXT("");
	CreateParams.AudioTrack = AudioTrackCombo->CurrentItem.IsValid() ? FCString::Atoi(*AudioTrackCombo->CurrentItem->Value) : -1;
	CreateParams.AudioTrackFormat = AudioTrackFormatCombo->CurrentItem.IsValid() ? FCString::Atoi(*AudioTrackFormatCombo->CurrentItem->Value) : -1;
	CreateParams.AudioTrackFormatName = AudioTrackFormatCombo->CurrentItem.IsValid() ? AudioTrackFormatCombo->CurrentItem->Key : TEXT("");

	CreateParams.StartTimeout = StartTimeout;
	CreateParams.FormatWaitTime = FormatWaitTime;
	CreateParams.SampleTimeout = SampleTimeout;

	return CreateParams;
}



void SMetaHumanMediaSourceWidget::Construct(const FArguments& InArgs, EMediaType InMediaType)
{
	Impl = SNew(SMetaHumanMediaSourceWidgetImpl, InMediaType);

	ChildSlot
	[
		Impl.ToSharedRef()
	];
}

bool SMetaHumanMediaSourceWidget::CanCreate() const
{
	return Impl->CanCreate();
}

FMetaHumanMediaSourceCreateParams SMetaHumanMediaSourceWidget::GetCreateParams() const
{
	return Impl->GetCreateParams();
}

TSharedPtr<SWidget> SMetaHumanMediaSourceWidget::GetWidget(EWidgetType InWidgetType) const
{
	TSharedPtr<SWidget> Widget;

	switch (InWidgetType)
	{
		case EWidgetType::VideoDevice:
			Widget = Impl->VideoDeviceCombo;
			break;

		case EWidgetType::VideoTrack:
			Widget = Impl->VideoTrackCombo;
			break;

		case EWidgetType::VideoTrackFormat:
			Widget = Impl->VideoTrackFormatCombo;
			break;

		case EWidgetType::AudioDevice:
			Widget = Impl->AudioDeviceCombo;
			break;

		case EWidgetType::AudioTrack:
			Widget = Impl->AudioTrackCombo;
			break;

		case EWidgetType::AudioTrackFormat:
			Widget = Impl->AudioTrackFormatCombo;
			break;

		case EWidgetType::Filtered:
			Widget = Impl->FilteredWidget;
			break;

		case EWidgetType::StartTimeout:
			Widget = Impl->StartTimeoutWidget;
			break;

		case EWidgetType::FormatWaitTime:
			Widget = Impl->FormatWaitTimeWidget;
			break;

		case EWidgetType::SampleTimeout:
			Widget = Impl->SampleTimeoutWidget;
			break;

		default:
			check(false);
			break;
	}

	return Widget;
}

void SMetaHumanMediaSourceWidget::Repopulate()
{
	Impl->PopulateDevices();
}

#undef LOCTEXT_NAMESPACE
