// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraProtronPlayerImpl.h"
#include "ElectraProtronPrivate.h"
#include "IElectraCodecFactoryModule.h"
#include "IElectraCodecFactory.h"
#include "IElectraDecoderFeaturesAndOptions.h"
#include "Utils/MPEG/ElectraUtilsMPEGVideo_H264.h"
#include "Utils/MPEG/ElectraUtilsMPEGVideo_H265.h"
#include "Utils/MPEG/ElectraUtilsMPEGAudio.h"
#include "TrackFormatInfo.h"
#include "Core/MediaThreads.h"


namespace ElectraProtronOptionNames
{
	const FName StartTimecodeValue(TEXT("StartTimecodeValue"));				// maybe use:  UMediaPlayer::MediaInfoNameSourceNumTiles.Resolve()
	const FName StartTimecodeFrameRate(TEXT("StartTimecodeFrameRate"));
	const FName KeyframeInterval(TEXT("KeyframeInterval"));
}


FElectraProtronPlayer::FImpl::FImpl()
{
	// Create the playback parameter structure that has members changing at any moment in time.
	// This information is shared with the frame loader.
	SharedPlayParams = MakeShared<FSharedPlayParams, ESPMode::ThreadSafe>();

	// Create the track-by-type array upfront in case queries to tracks are made before opening a source.
	UsableTrackArrayIndicesByType.SetNum(UE_ARRAY_COUNT(kCodecTrackIndexMap));

	// Create the sample queue interface.
	const int32 kVideoFramesToCacheAhead = 8;
	const int32 kVideoFramesToCacheBehind = 8;
	CurrentSampleQueueInterface = MakeShared<FSampleQueueInterface, ESPMode::ThreadSafe>(kVideoFramesToCacheAhead, kVideoFramesToCacheBehind);
}

FElectraProtronPlayer::FImpl::~FImpl()
{
}

void FElectraProtronPlayer::FImpl::StartThread()
{
	if (!Thread)
	{
		Thread = FRunnableThread::Create(this, TEXT("Electra Protron"), 0, TPri_Normal);
	}
}


void FElectraProtronPlayer::FImpl::Open(const FOpenParam& InParam, FElectraProtronPlayer::FImpl::FCompletionDelegate InCompletionDelegate)
{
	FWorkerThreadMessage Msg;
	FWorkerThreadMessage::FParamOpen open { InParam };
	open.Param.SampleQueueInterface = CurrentSampleQueueInterface;
	Msg.Param.Emplace<FWorkerThreadMessage::FParamOpen>(MoveTemp(open));
	Msg.Self = AsShared();
	Msg.Type = FWorkerThreadMessage::EType::Open;
	Msg.CompletionDelegate = MoveTemp(InCompletionDelegate);
	SendWorkerThreadMessage(MoveTemp(Msg));
	StartThread();
}

void FElectraProtronPlayer::FImpl::Close(FElectraProtronPlayer::FImpl::FCompletionDelegate InCompletionDelegate)
{
	bAbort = true;
	if (Thread)
	{
		FWorkerThreadMessage Msg;
		Msg.Param.Emplace<FWorkerThreadMessage::FParamTerminate>(FWorkerThreadMessage::FParamTerminate());
		Msg.Self = AsShared();
		Msg.Type = FWorkerThreadMessage::EType::Terminate;
		Msg.CompletionDelegate = MoveTemp(InCompletionDelegate);
		SendWorkerThreadMessage(MoveTemp(Msg));
	}
	else
	{
		InCompletionDelegate.ExecuteIfBound(AsShared());
	}
}

FString FElectraProtronPlayer::FImpl::GetLastError()
{
	return LastErrorMessage;
}

bool FElectraProtronPlayer::FImpl::HasReachedEnd()
{
	const bool bIsVideoActive = TrackSelection.ActiveTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)] != -1;
	const bool bIsAudioActive = TrackSelection.ActiveTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)] != -1;

	bool bAllReachedEnd = true;
	if (bIsVideoActive && !VideoDecoderThread.HasReachedEnd())
	{
		bAllReachedEnd = false;
	}
	if (bIsAudioActive && !AudioDecoderThread.HasReachedEnd())
	{
		bAllReachedEnd = false;
	}
	return bAllReachedEnd;
}


void FElectraProtronPlayer::FImpl::SendWorkerThreadMessage(FElectraProtronPlayer::FImpl::FWorkerThreadMessage&& InMessage)
{
	WorkMessages.Enqueue(MoveTemp(InMessage));
	WorkMessageSignal.Signal();
}

void FElectraProtronPlayer::FImpl::Exit()
{
	// We are still within our own thread here, so we cannot wait for completion.
	// Use an async task to this if possible.
	if (GIsRunning)
	{
		FMediaRunnable::EnqueueAsyncTask([this]()
		{
			Thread->WaitForCompletion();
			delete Thread;
			SelfDuringTerminate.Reset();
		});
	}
	else
	{
		// Leave the thread dangling, we can't clean it up here.
		SelfDuringTerminate.Reset();
	}
}

uint32 FElectraProtronPlayer::FImpl::Run()
{
	bool bDone = false;
	while(!bDone)
	{
		WorkMessageSignal.WaitTimeoutAndReset(1000 * 20);
		FWorkerThreadMessage Msg;
		while(WorkMessages.Dequeue(Msg))
		{
			switch(Msg.Type)
			{
				case FWorkerThreadMessage::EType::Open:
				{
					const FWorkerThreadMessage::FParamOpen& open(Msg.Param.Get<FWorkerThreadMessage::FParamOpen>());
					InternalOpen(open.Param.Filename);
					// Start loader threads when opening was successful.
					if (LastErrorMessage.IsEmpty())
					{
						// Set the duration of the movie on the sample queue for looping/wrapping purposes.
						CurrentSampleQueueInterface->SetMovieDuration(Duration);
						// If there an initial playback range set then apply it, otherwise set the entire movie.
						SetPlaybackTimeRange(open.Param.InitialPlaybackRange.Get(TRange<FTimespan>::Empty()));
						// By default we start at the beginning of the playback range.
						CurrentPlayPosTime = CurrentPlaybackRange.GetLowerBoundValue();

						VideoLoaderThread.StartThread(open.Param.Filename, SharedPlayParams);
						AudioLoaderThread.StartThread(open.Param.Filename, SharedPlayParams);

						VideoDecoderThread.StartThread(open.Param, SharedPlayParams);
						AudioDecoderThread.StartThread(open.Param, SharedPlayParams);

						// Select the first video and audio track by default (if they exist).
						SelectTrack(EMediaTrackType::Video, 0);
						SelectTrack(EMediaTrackType::Audio, 0);
					}
					break;
				}
				case FWorkerThreadMessage::EType::Terminate:
				{
					bDone = true;
					// Hold on to ourselves while we exit the loop.
					// Otherwise, if there are no other owners we may get destroyed too soon on our way out.
					SelfDuringTerminate = AsShared();

					// Stop decoder threads
					AudioDecoderThread.StopThread();
					VideoDecoderThread.StopThread();

					// Stop loader threads
					AudioLoaderThread.StopThread();
					VideoLoaderThread.StopThread();
					break;
				}
				default:
				{
					unimplemented();
					break;
				}
			}
			Msg.CompletionDelegate.ExecuteIfBound(AsShared());
		}

		// Is there a new seek request pending?
		SeekRequestLock.Lock();
		TOptional<FSeekRequest> NewSeekRequest(PendingSeekRequest);
		PendingSeekRequest.Reset();
		SeekRequestLock.Unlock();
		if (NewSeekRequest.IsSet())
		{
			HandleSeekRequest(NewSeekRequest.GetValue());
		}
	}
	return 0;
}


/**
 * Opens the given file and verifies that it can be used.
 */
void FElectraProtronPlayer::FImpl::InternalOpen(const FString& InFilename)
{
	// Open the file.
	TSharedPtr<Electra::IFileDataReader, ESPMode::ThreadSafe> Reader = Electra::IFileDataReader::Create();
	if (!Reader->Open(InFilename))
	{
		LastErrorMessage = Reader->GetLastError();
		return;
	}

	// Read the mp4 box structure.
	Electra::UtilitiesMP4::FMP4BoxLocatorReader BoxLocator;
	const TArray<uint32> FirstBoxes { Electra::UtilitiesMP4::MakeBoxAtom('f','t','y','p'), Electra::UtilitiesMP4::MakeBoxAtom('s','t','y','p'), Electra::UtilitiesMP4::MakeBoxAtom('s','i','d','x'), Electra::UtilitiesMP4::MakeBoxAtom('f','r','e','e'), Electra::UtilitiesMP4::MakeBoxAtom('s','k','i','p') };
	const TArray<uint32> ReadBoxes;			// Empty means to read in all boxes except `mdat`
	const TArray<uint32> StopAfterBoxes;	// Empty means to read all boxes and not stop after a specific one.
	TArray<TSharedPtr<Electra::UtilitiesMP4::FMP4BoxData, ESPMode::ThreadSafe>> RootBoxes;
	if (!BoxLocator.LocateAndReadRootBoxes(RootBoxes, Reader, FirstBoxes, StopAfterBoxes, ReadBoxes, Electra::IBaseDataReader::FCancellationCheckDelegate::CreateLambda([&](){return bAbort;})))
	{
		LastErrorMessage = BoxLocator.GetLastError();
		return;
	}

	// In order to be usable the mp4 needs to have a `moov` box.
	if (!RootBoxes.FindByPredicate([](const TSharedPtr<Electra::UtilitiesMP4::FMP4BoxData, ESPMode::ThreadSafe>& In){ return In->Type == Electra::UtilitiesMP4::MakeBoxAtom('m','o','o','v'); }))
	{
		LastErrorMessage = FString::Printf(TEXT("No `moov` box found in this file. It does not appear to be an mp4/mov file."));
		return;
	}

	// Parse all the root boxes. There are typically no more than 4, maybe 6 unless it is a fragmented mp4.
	ParsedRootBoxes.SetNum(RootBoxes.Num());
	for(int32 i=0; i<RootBoxes.Num(); ++i)
	{
		if (!ParsedRootBoxes[i].ParseBoxTree(RootBoxes[i]))
		{
			LastErrorMessage = FString::Printf(TEXT("Failed to parse the file's box structure."));
			return;
		}
	}

	// We need the `moov` box now to determine what is inside this file.
	// Yes, we looked at the presence of it above already as a quick reject before parsing the boxes.
	// Now we need it for real and we know that it needs to be there, so here we go
	TSharedPtr<Electra::UtilitiesMP4::FMP4BoxMOOV, ESPMode::ThreadSafe> MoovBox = StaticCastSharedPtr<Electra::UtilitiesMP4::FMP4BoxMOOV>(
															(ParsedRootBoxes.FindByPredicate([](const Electra::UtilitiesMP4::FMP4BoxTreeParser& In)
															{
																return In.GetBoxTree()->GetType() == Electra::UtilitiesMP4::MakeBoxAtom('m','o','o','v');
															})->GetBoxTree()));
	// Get the `mvhd` box for the duration of the movie and the timescale other values are measured in.
	auto MvhdBox = MoovBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxMVHD>(Electra::UtilitiesMP4::MakeBoxAtom('m','v','h','d'), 0);
	if (!MvhdBox.IsValid())
	{
		LastErrorMessage = FString::Printf(TEXT("No `mvhd` box found. This is not a usable mp4/mov file."));
		return;
	}
	MovieDuration = MvhdBox->GetDuration();
	if (!MovieDuration.IsValid())
	{
		LastErrorMessage = FString::Printf(TEXT("Duration in `mvhd` box is set to indefinite or is not valid. This is not a usable mp4/mov file."));
		return;
	}
	Duration = MovieDuration.GetAsTimebase(ETimespan::TicksPerSecond);


	IElectraCodecFactoryModule* FactoryModule = static_cast<IElectraCodecFactoryModule*>(FModuleManager::Get().GetModule(TEXT("ElectraCodecFactory")));
	check(FactoryModule);
	if (!FactoryModule)
	{
		LastErrorMessage = FString::Printf(TEXT("Electra decoder factory not found. Unable to use any track."));
		return;
	}

	// Get all the tracks.
	TArray<TSharedPtr<Electra::UtilitiesMP4::FMP4BoxTRAK, ESPMode::ThreadSafe>> AllTracks;
	MoovBox->GetAllBoxInstances(AllTracks, Electra::UtilitiesMP4::MakeBoxAtom('t','r','a','k'));
	// Prepare the internal track structure and check which tracks we can use and which ones we cannot.
	Tracks.SetNum(AllTracks.Num());
	int32 NumTimecodeTracks = 0;
	int64 LongestTrackDuration = -1;
	int64 LongestSupportedTrackDuration = -1;
	int64 ShortestSupportedTrackDuration = TNumericLimits<int64>::Max();
	for(int32 i=0; i<AllTracks.Num(); ++i)
	{
		Tracks[i] = MakeShared<FTrackInfo>();
		Tracks[i]->TrackBox = AllTracks[i];

		auto Tkhd = AllTracks[i]->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxTKHD>(Electra::UtilitiesMP4::MakeBoxAtom('t','k','h','d'), 1);
		if (!Tkhd)
		{
			// If not found the file is broken.
			LastErrorMessage = FString::Printf(TEXT("No `tkhd` box found on track %d. This file cannot be used."), i);
			Tracks.Empty();
			return;
		}
		uint32 TrackID = Tracks[i]->TrackID = Tkhd->GetTrackID();
		/*
			From the standard:
				"Tracks that are marked as not enabled (track_enabled set to 0) shall be ignored and treated as if
				not present."
		*/
		if (!Tkhd->IsEnabled())
		{
			UE_LOG(LogElectraProtron, Warning, TEXT("Track #%u is flagged as disabled, ignoring this track."), TrackID);
			continue;
		}

		// This track's duration must not be indefinite.
		if (Tkhd->GetDuration() == TNumericLimits<int64>::Max())
		{
			UE_LOG(LogElectraProtron, Warning, TEXT("Track #%u has indefinite duration, ignoring this track."), TrackID);
			continue;
		}

		// Take note of the track with the longest duration (any track, even unsupported ones)
		LongestTrackDuration = LongestTrackDuration < Tkhd->GetDuration() ? Tkhd->GetDuration() : LongestTrackDuration;

		ElectraProtronUtils::FCodecInfo& CodecInfo(Tracks[i]->CodecInfo);
		GetTrackCodecInfo(CodecInfo, AllTracks[i], TrackID);

		// Timecode tracks are inherently usable.
		if (CodecInfo.Type == ElectraProtronUtils::FCodecInfo::EType::Timecode)
		{
			Tracks[i]->bIsUsable = true;
			Tracks[i]->bIsKeyframeOnlyFormat = true;
			++NumTimecodeTracks;
		}
		// Check with the decoder factory if this format can be decoded.
		else if (CodecInfo.Type == ElectraProtronUtils::FCodecInfo::EType::Video || CodecInfo.Type == ElectraProtronUtils::FCodecInfo::EType::Audio)
		{
			TMap<FString, FVariant> Params;
			switch(CodecInfo.Type)
			{
				case ElectraProtronUtils::FCodecInfo::EType::Video:
				{
					const ElectraProtronUtils::FCodecInfo::FVideo& Video(CodecInfo.Properties.Get<ElectraProtronUtils::FCodecInfo::FVideo>());
					Params.Add(TEXT("width"), FVariant(Video.Width));
					Params.Add(TEXT("height"), FVariant(Video.Height));
					if (Video.FrameRate.IsValid())
					{
						Params.Add(TEXT("fps"), FVariant((double)Video.FrameRate.GetAsDouble()));
						Params.Add(TEXT("fps_n"), FVariant((int64)Video.FrameRate.GetNumerator()));
						Params.Add(TEXT("fps_d"), FVariant((uint32)Video.FrameRate.GetDenominator()));
					}
					else
					{
						Params.Add(TEXT("fps"), FVariant((double)0.0));
						Params.Add(TEXT("fps_n"), FVariant((int64)0));
						Params.Add(TEXT("fps_d"), FVariant((uint32)1));
					}
					break;
				}
				case ElectraProtronUtils::FCodecInfo::EType::Audio:
				{
					const ElectraProtronUtils::FCodecInfo::FAudio& Audio(CodecInfo.Properties.Get<ElectraProtronUtils::FCodecInfo::FAudio>());
					Params.Add(TEXT("channel_configuration"), FVariant(Audio.ChannelConfiguration));
					Params.Add(TEXT("num_channels"), FVariant((int32) Audio.NumChannels));
					Params.Add(TEXT("sample_rate"), FVariant((int32) Audio.SampleRate));
					break;
				}
			}
			Params.Add(TEXT("dcr"), FVariant(CodecInfo.DCR));
			Params.Add(TEXT("codec_4cc"), FVariant(CodecInfo.FourCC));
			Params.Add(TEXT("codec_name"), FVariant(CodecInfo.RFC6381));
			// Add children box data
			for(auto& Ch : CodecInfo.ExtraBoxes)
			{
				FString BoxName = FString::Printf(TEXT("$%s_box"), *Electra::UtilitiesMP4::GetPrintableBoxAtom(Ch.Key));
				Params.Add(BoxName, FVariant(Ch.Value));
			}

			TMap<FString, FVariant> FormatInfo;
			auto Factory = FactoryModule->GetBestFactoryForFormat(FormatInfo, CodecInfo.RFC6381, false, Params);
			if (Factory.IsValid())
			{
				Tracks[i]->bIsUsable = true;
				TMap<FString, FVariant> ConfigOptions;
				Factory->GetConfigurationOptions(ConfigOptions);
				// Every non-video format is assumed to be keyframe-only. For video we ask the factory.
				Tracks[i]->bIsKeyframeOnlyFormat = CodecInfo.Type != ElectraProtronUtils::FCodecInfo::EType::Video || !!ElectraDecodersUtil::GetVariantValueSafeI64(FormatInfo, IElectraDecoderFormatInfo::IsEveryFrameKeyframe, 0);
				// See if the decoder provides a nice name for the format used.
				Tracks[i]->HumanReadableCodecFormat = ElectraDecodersUtil::GetVariantValueFString(FormatInfo, IElectraDecoderFormatInfo::HumanReadableFormatName);
				if (Tracks[i]->HumanReadableCodecFormat.IsEmpty())
				{
					Tracks[i]->HumanReadableCodecFormat = CodecInfo.HumanReadableFormatInfo;
					if (Tracks[i]->HumanReadableCodecFormat.IsEmpty())
					{
						Tracks[i]->HumanReadableCodecFormat = CodecInfo.RFC6381;
					}
				}

				// Take note of the supported track with the longest duration.
				LongestSupportedTrackDuration = LongestSupportedTrackDuration < Tkhd->GetDuration() ? Tkhd->GetDuration() : LongestSupportedTrackDuration;
				// Likewise for the shortest.
				ShortestSupportedTrackDuration = ShortestSupportedTrackDuration > Tkhd->GetDuration() ? Tkhd->GetDuration() : ShortestSupportedTrackDuration;
			}
			else
			{
				UE_LOG(LogElectraProtron, Warning, TEXT("No decoder to handle sample type \"%s\" of track #%u, ignoring this track."), *CodecInfo.RFC6381, TrackID);
			}
		}
	}

	// Check that the duration given in the `mvhd` box is not larger than the longest track is.
	int64 MvhdDur = MovieDuration.GetNumerator();

	if (MvhdDur > LongestTrackDuration)
	{
		UE_LOG(LogElectraProtron, Warning, TEXT("Movie duration in `mvhd` box (%lld) is larger than the longest track (%lld) in the file, adjusting movie duration down."), (long long int)MvhdDur, (long long int)LongestTrackDuration);
		MovieDuration.SetFromND(LongestTrackDuration, MovieDuration.GetDenominator());
		Duration = MovieDuration.GetAsTimebase(ETimespan::TicksPerSecond);
		MvhdDur = LongestTrackDuration;
	}
	Electra::FTimeFraction EntireMovieDuration(MovieDuration);

#if 0
	// Check that the movie duration is not larger than the longest supported track.
	if (LongestSupportedTrackDuration > 0 && MvhdDur > LongestSupportedTrackDuration)
	{
		UE_LOG(LogElectraProtron, Warning, TEXT("Movie duration in `mvhd` box (%lld) is larger than the longest supported track (%lld) in the file, adjusting movie duration down."), (long long int)MvhdDur, (long long int)LongestSupportedTrackDuration);
		MovieDuration.SetFromND(LongestSupportedTrackDuration, MovieDuration.GetDenominator());
		Duration = MovieDuration.GetAsTimebase(ETimespan::TicksPerSecond);
	}
#else
	// Check that the movie duration is not larger than the longest supported track.
	if (ShortestSupportedTrackDuration < TNumericLimits<int64>::Max() && MvhdDur > ShortestSupportedTrackDuration)
	{
		UE_LOG(LogElectraProtron, Warning, TEXT("Movie duration in `mvhd` box (%lld) is larger than the shortest supported track (%lld) in the file, adjusting movie duration down."), (long long int)MvhdDur, (long long int)ShortestSupportedTrackDuration);
		MovieDuration.SetFromND(ShortestSupportedTrackDuration, MovieDuration.GetDenominator());
		Duration = MovieDuration.GetAsTimebase(ETimespan::TicksPerSecond);
	}
#endif

	// If there are timecode tracks, find which tracks references them.
	// If there are none, then - if there is only a single timecode track - we apply it to all other tracks.
	// Other references are of no interest at the moment.
	if (NumTimecodeTracks)
	{
		bool bAnyTrackReferencesTimecodeExplicitly = false;
		for(int32 TrkNum=0; TrkNum<Tracks.Num(); ++TrkNum)
		{
			// We need to check every track, even the ones we cannot use. Otherwise we
			// may think the timecode is not referenced and use it for everything!
			// We do not check if a timecode track references another one as this would be silly.
			if (Tracks[TrkNum]->CodecInfo.Type != ElectraProtronUtils::FCodecInfo::EType::Timecode)
			{
				auto Tref = Tracks[TrkNum]->TrackBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxTREF>(Electra::UtilitiesMP4::MakeBoxAtom('t','r','e','f'), 1);
				if (Tref.IsValid())
				{
					// Get timecode references, if any.
					auto References = Tref->GetEntriesOfType(Electra::UtilitiesMP4::MakeBoxAtom('t','m','c','d'));
					if (References.Num())
					{
						if (References.Num() > 1)
						{
							UE_LOG(LogElectraProtron, Warning, TEXT("Track #u contains more than one `tmcd` reference box. Using first reference only."), Tracks[TrkNum]->TrackID);
						}
						if (References[0].TrackIDs.Num())
						{
							if (References[0].TrackIDs.Num() > 1)
							{
								UE_LOG(LogElectraProtron, Warning, TEXT("Track #u references more than one timecode track. Using first reference only."), Tracks[TrkNum]->TrackID);
							}
							// Whether the reference is actually valid or not, a track makes an explicit reference
							// and so we must not assign unreferenced timecode tracks later.
							bAnyTrackReferencesTimecodeExplicitly = true;
							// Either way we need to tag every referenced track.
							for(auto& RefTkId : References[0].TrackIDs)
							{
								TSharedPtr<FTrackInfo, ESPMode::ThreadSafe>* ReferencedTrack = Tracks.FindByPredicate([RefTkId](const TSharedPtr<FTrackInfo, ESPMode::ThreadSafe>& e){ return e->TrackID == RefTkId; });
								if (ReferencedTrack)
								{
									if (!Tracks[TrkNum]->ReferencedTimecodeTrack.IsValid())
									{
										Tracks[TrkNum]->ReferencedTimecodeTrack = *ReferencedTrack;
									}
									(*ReferencedTrack)->IsReferencedByTracks.Add(Tracks[TrkNum]);
								}
								else
								{
									UE_LOG(LogElectraProtron, Warning, TEXT("Track #u references a non-existing timecode track #%u. Ignoring."), Tracks[TrkNum]->TrackID, RefTkId);
									Tracks[TrkNum]->ReferencedTimecodeTrack.Reset();
								}
							}
						}
					}
				}
			}
		}

		// Now check for tracks that are not explicitly referencing a timecode track, but only when NO track
		// makes an explicit reference. If some do and other don't we do not interfere.
		if (!bAnyTrackReferencesTimecodeExplicitly)
		{
			if (NumTimecodeTracks == 1)
			{
				// Which track is the timecode?
				TSharedPtr<FTrackInfo, ESPMode::ThreadSafe>* TimecodeTrack = Tracks.FindByPredicate([](const TSharedPtr<FTrackInfo, ESPMode::ThreadSafe>& In){ return In->CodecInfo.Type == ElectraProtronUtils::FCodecInfo::EType::Timecode; });
				check(TimecodeTrack); // there has to be one, otherwise we would not even get here, but for safeties sake...
				if (TimecodeTrack)
				{
					// We set this up for video tracks only.
					for(int32 i=0; i<Tracks.Num(); ++i)
					{
						if (Tracks[i]->CodecInfo.Type != ElectraProtronUtils::FCodecInfo::EType::Video)
						{
							continue;
						}
						Tracks[i]->ReferencedTimecodeTrack = *TimecodeTrack;
						(*TimecodeTrack)->IsReferencedByTracks.Add(Tracks[i]);
					}
				}
			}
			else
			{
				UE_LOG(LogElectraProtron, Warning, TEXT("There are %d timecode tracks that are not referenced by any other track. Ignoring all of them."), NumTimecodeTracks);
			}
		}
	}
	else
	{
		// See if there is an `udta` box in the `moov` that contains `(c)TIM` and `(C)TSC` boxes.
		auto UdtaBox = MoovBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxUDTA>(Electra::UtilitiesMP4::MakeBoxAtom('u','d','t','a'), 0);
		if (UdtaBox.IsValid())
		{
			auto CTIMBox = UdtaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxBase>(Electra::UtilitiesMP4::MakeBoxAtom(0xa9U,'T','I','M'), 0);
			auto CTSCBox = UdtaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxBase>(Electra::UtilitiesMP4::MakeBoxAtom(0xa9U,'T','S','C'), 0);
			//auto CTSZBox = UdtaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxBase>(Electra::UtilitiesMP4::MakeBoxAtom(0xa9U,'T','S','Z'), 0);
			if (CTIMBox.IsValid() && CTSCBox.IsValid())
			{
				auto GetValue = [](const TSharedPtr<Electra::UtilitiesMP4::FMP4BoxBase, ESPMode::ThreadSafe>& InBox) -> FString
				{
					Electra::UtilitiesMP4::FMP4AtomReaderHelper timReader(InBox->GetBoxData());
					FString String;
					uint16 StringLength, UnknownValue;
					if (timReader.Read(StringLength)&&  timReader.Read(UnknownValue))
					{
						if (timReader.ReadString(String, StringLength))
						{
							return String;
						}
					}
					return FString();
				};
				FTrackInfo::FFirstSampleTimecode tc;
				FFrameRate fr;
				tc.Timecode = GetValue(CTIMBox);
				tc.Framerate = GetValue(CTSCBox);
				TOptional<FTimecode> ptc = FTimecode::ParseTimecode(tc.Timecode);
				if (ptc.IsSet() && TryParseString(fr, *tc.Framerate))
				{
					FFrameNumber fn = ptc.GetValue().ToFrameNumber(fr);
					tc.TimecodeValue = (uint32) fn.Value;
				}
				for(int32 i=0; i<Tracks.Num(); ++i)
				{
					if (Tracks[i]->bIsUsable)
					{
						Tracks[i]->FirstSampleTimecode = tc;
					}
				}
			}
		}
	}

	// One last pass to count how many usable tracks per type we have.
	for(int32 TkTypIdx=0; TkTypIdx<UE_ARRAY_COUNT(kCodecTrackIndexMap); ++TkTypIdx)
	{
		for(int32 TkIdx=0; TkIdx<Tracks.Num(); ++TkIdx)
		{
			if (Tracks[TkIdx]->bIsUsable && TkTypIdx==(int32) Tracks[TkIdx]->CodecInfo.Type)
			{
				UsableTrackArrayIndicesByType[TkTypIdx].Emplace(TkIdx);
			}
		}
	}

	// Neither video nor audio?
	if (UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num() == 0 &&
		UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num() == 0)
	{
		LastErrorMessage = FString::Printf(TEXT("This file contains no playable video or audio tracks."));
		return;
	}

	// Prepare the tracks
	for(int32 i=0; i<Tracks.Num(); ++i)
	{
		if (!Tracks[i]->bIsUsable)
		{
			continue;
		}
		Tracks[i]->MP4Track = Electra::UtilitiesMP4::FMP4Track::Create(Tracks[i]->TrackBox);
		if (!Tracks[i]->MP4Track->Prepare(EntireMovieDuration, MovieDuration))
		{
			LastErrorMessage = Tracks[i]->MP4Track->GetLastError();
			check(!LastErrorMessage.IsEmpty());
			return;
		}

		// If this is a timecode track we may want to read in the first timecode.
		if (Config.bReadFirstTimecode && Tracks[i]->CodecInfo.Type == ElectraProtronUtils::FCodecInfo::EType::Timecode &&
			Tracks[i]->CodecInfo.FourCC == Electra::UtilitiesMP4::MakeBoxAtom('t','m','c','d'))
		{
			auto It = Tracks[i]->MP4Track->CreateIterator();
			if (!It.IsValid())
			{
				LastErrorMessage = Tracks[i]->MP4Track->GetLastError();
				check(!LastErrorMessage.IsEmpty());
				return;
			}
			// Read the first sample.
			int64 SampleSize = It->GetSampleSize();
			int64 SampleFileOffset = It->GetSampleFileOffset();
			check(SampleSize == 4);		// If that triggers this is not a tmcd sample.
			TArray<uint32> TimecodeBuffer;
			TimecodeBuffer.SetNumUninitialized(Align(SampleSize, sizeof(uint32)));
			int64 NumRead = Reader->ReadData(TimecodeBuffer.GetData(), SampleSize, SampleFileOffset, Electra::IBaseDataReader::FCancellationCheckDelegate::CreateLambda([&](){return bAbort;}));
			if (NumRead != SampleSize)
			{
				LastErrorMessage = FString::Printf(TEXT("Failed to read first timecode sample."));
				return;
			}
			// Get the timecode description from the codec info.
			ElectraProtronUtils::FCodecInfo::FTMCDTimecode TimecodeInfo(Tracks[i]->CodecInfo.Properties.Get<ElectraProtronUtils::FCodecInfo::FTMCDTimecode>());
			// Set the first timecode sample on the track.
			FTrackInfo::FFirstSampleTimecode firstTC;
			firstTC.TimecodeValue = Electra::GetFromBigEndian(TimecodeBuffer[0]);
			firstTC.Framerate = TimecodeInfo.GetFrameRate().ToPrettyText().ToString();
			firstTC.Timecode = TimecodeInfo.ConvertToTimecode(firstTC.TimecodeValue).ToString();
			Tracks[i]->FirstSampleTimecode.Emplace(MoveTemp(firstTC));

			// For convenience also set this on the tracks that reference this timecode track
			for(auto ReferencingTrack : Tracks[i]->IsReferencedByTracks)
			{
				if (auto PinnedRefTrk = ReferencingTrack.Pin())
				{
					PinnedRefTrk->FirstSampleTimecode = Tracks[i]->FirstSampleTimecode;
				}
			}
		}
	}
}

void FElectraProtronPlayer::FImpl::GetTrackCodecInfo(ElectraProtronUtils::FCodecInfo& OutCodecInfo, const TSharedPtr<Electra::UtilitiesMP4::FMP4BoxTRAK, ESPMode::ThreadSafe>& InTrack, uint32 InTrackID)
{
	// There needs to be an `stsd` box in this track. We do not try the expected path of `trak`->`mdia`->`minf`->`stbl`->`stsd` as
	// this is not that much faster and if the file is somewhat ill-formed we may not find it if it's grouped under elsewhere.
	auto Stsd = InTrack->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTSD>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','s','d'), 6);
	if (!Stsd)
	{
		// If not found we just ignore the track. That's a warning but not an error.
		UE_LOG(LogElectraProtron, Warning, TEXT("No `stsd` box found, ignoring track #%u."), InTrackID);
		return;
	}

	auto AddChildren = [&OutCodecInfo](const TSharedPtr<Electra::UtilitiesMP4::FMP4BoxBase, ESPMode::ThreadSafe>& InSample) -> void
	{
		for(auto& Ch : InSample->GetChildren())
		{
			OutCodecInfo.ExtraBoxes.Emplace(Ch->GetType(), Ch->GetBoxData());
		}
	};

	// Get the sample type of this track. This call is necessary to trigger parsing of child nodes.
	Electra::UtilitiesMP4::FMP4BoxSampleEntry::ESampleType SampleType = Stsd->GetSampleType();
	// If already knows to not be supported, skip it.
	if (SampleType == Electra::UtilitiesMP4::FMP4BoxSampleEntry::ESampleType::Unsupported)
	{
		UE_LOG(LogElectraProtron, Warning, TEXT("Unsupported sample type, ignoring track #%u."), InTrackID);
		return;
	}
	// Several entries are permitted, but we need this to be unambiguous.
	if (Stsd->GetChildren().Num() > 1)
	{
		UE_LOG(LogElectraProtron, Warning, TEXT("Multiple sample descriptions found in `stsd` box, ignoring track #%u."), InTrackID);
		return;
	}
	else if (Stsd->GetChildren().Num() == 0)
	{
		UE_LOG(LogElectraProtron, Warning, TEXT("No sample description found in `stsd` box, ignoring track #%u."), InTrackID);
		return;
	}

	bool bIsEncrypted = false;
	// Based on the sample type, get it and see if it is using codec we support.
	if (SampleType == Electra::UtilitiesMP4::FMP4BoxSampleEntry::ESampleType::Video)
	{
		auto SetResoAndFPSFromBox = [InTrack](ElectraProtronUtils::FCodecInfo::FVideo& InOutVideo, const TSharedPtr<Electra::UtilitiesMP4::FMP4BoxVisualSampleEntry, ESPMode::ThreadSafe>& InVisual) -> void
		{
			InOutVideo.Width = InVisual->GetWidth();
			InOutVideo.Height = InVisual->GetHeight();
			auto Mdhd = InTrack->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxMDHD>(Electra::UtilitiesMP4::MakeBoxAtom('m','d','h','d'), 2);
			auto Stts = InTrack->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTTS>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','t','s'), 5);
			if (Mdhd.IsValid() && Stts.IsValid() && Stts->GetEntries().Num())
			{
				InOutVideo.FrameRate.SetFromND(Mdhd->GetTimescale(), Stts->GetEntries()[0].sample_delta);
			}
		};

		TArray<TSharedPtr<Electra::UtilitiesMP4::FMP4BoxVisualSampleEntry, ESPMode::ThreadSafe>> Visuals;
		Stsd->GetSampleDescriptions(Visuals);
		check(Visuals.Num() == 1);
		switch(Visuals[0]->GetType())
		{
			case Electra::UtilitiesMP4::MakeBoxAtom('e','n','c','v'):
			{
				bIsEncrypted = true;
				break;
			}
			case Electra::UtilitiesMP4::MakeBoxAtom('a','v','c','1'):
			case Electra::UtilitiesMP4::MakeBoxAtom('a','v','c','3'):
			{
				if (Visuals[0]->GetFrameCount() == 1)
				{
					auto Avcc = Visuals[0]->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxAVCC>(Electra::UtilitiesMP4::MakeBoxAtom('a','v','c','C'), 0);
					if (Avcc.IsValid())
					{
						ElectraDecodersUtil::MPEG::H264::FAVCDecoderConfigurationRecord dcr;
						if (dcr.Parse(Avcc->GetAVCDecoderConfigurationRecord()))
						{
							ElectraProtronUtils::FCodecInfo::FVideo Video;
							ElectraDecodersUtil::MPEG::H264::FSequenceParameterSet sps;
							const TCHAR* Prefix = nullptr;
							if (dcr.GetSequenceParameterSets().Num() && ElectraDecodersUtil::MPEG::H264::ParseSequenceParameterSet(sps, dcr.GetSequenceParameterSets()[0]))
							{
								ElectraDecodersUtil::FFractionalValue fr = sps.GetTiming();
								sps.GetDisplaySize(Video.Width, Video.Height);
								if (fr.Num && fr.Denom)
								{
									Video.FrameRate.SetFromND(fr.Num, fr.Denom);
								}
								else
								{
									SetResoAndFPSFromBox(Video, Visuals[0]);
								}
								Prefix = TEXT("avc1");
							}
							else
							{
								SetResoAndFPSFromBox(Video, Visuals[0]);
								Prefix = TEXT("avc3");
							}
							OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FVideo>(MoveTemp(Video));
							OutCodecInfo.RFC6381 = dcr.GetCodecSpecifierRFC6381(Prefix);
							OutCodecInfo.FourCC = Visuals[0]->GetType();
							OutCodecInfo.DCR = Avcc->GetAVCDecoderConfigurationRecord();
							OutCodecInfo.CSD = dcr.GetCodecSpecificData();
							OutCodecInfo.HumanReadableFormatInfo = dcr.GetFormatInfo();
							OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Video;
							AddChildren(Visuals[0]);
						}
					}
				}
				else
				{
					UE_LOG(LogElectraProtron, Warning, TEXT("Track #%u has a frame count other than 1 in the VisualSampleEntry, ignoring this track."), InTrackID);
				}
				break;
			}
			case Electra::UtilitiesMP4::MakeBoxAtom('h','v','c','1'):
			case Electra::UtilitiesMP4::MakeBoxAtom('h','e','v','1'):
			{
				if (Visuals[0]->GetFrameCount() == 1)
				{
					auto Hvcc = Visuals[0]->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxHVCC>(Electra::UtilitiesMP4::MakeBoxAtom('h','v','c','C'), 0);
					if (Hvcc.IsValid())
					{
						ElectraDecodersUtil::MPEG::H265::FHEVCDecoderConfigurationRecord dcr;
						if (dcr.Parse(Hvcc->GetHEVCDecoderConfigurationRecord()))
						{
							ElectraProtronUtils::FCodecInfo::FVideo Video;
							ElectraDecodersUtil::MPEG::H265::FSequenceParameterSet sps;
							const TCHAR* Prefix = nullptr;
							if (dcr.GetSequenceParameterSets().Num() && ElectraDecodersUtil::MPEG::H265::ParseSequenceParameterSet(sps, dcr.GetSequenceParameterSets()[0]))
							{
								ElectraDecodersUtil::FFractionalValue fr = sps.GetTiming();
								sps.GetDisplaySize(Video.Width, Video.Height);
								Video.FrameRate.SetFromND(fr.Num, fr.Denom);
								Prefix = TEXT("hvc1");
							}
							else
							{
								SetResoAndFPSFromBox(Video, Visuals[0]);
								Prefix = TEXT("hev1");
							}
							OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FVideo>(MoveTemp(Video));
							OutCodecInfo.RFC6381 = dcr.GetCodecSpecifierRFC6381(Prefix);
							OutCodecInfo.FourCC = Visuals[0]->GetType();
							OutCodecInfo.DCR = Hvcc->GetHEVCDecoderConfigurationRecord();
							OutCodecInfo.CSD = dcr.GetCodecSpecificData();
							OutCodecInfo.HumanReadableFormatInfo = dcr.GetFormatInfo();
							OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Video;
							AddChildren(Visuals[0]);
						}
					}
				}
				else
				{
					UE_LOG(LogElectraProtron, Warning, TEXT("Track #%u has a frame count other than 1 in the VisualSampleEntry, ignoring this track."), InTrackID);
				}
				break;
			}
			default:
			{
				ElectraProtronUtils::FCodecInfo::FVideo Video;
				SetResoAndFPSFromBox(Video, Visuals[0]);
				OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FVideo>(MoveTemp(Video));
				OutCodecInfo.RFC6381 = Electra::UtilitiesMP4::GetPrintableBoxAtom(Visuals[0]->GetType());
				OutCodecInfo.FourCC = Visuals[0]->GetType();
				OutCodecInfo.DCR = Visuals[0]->GetBoxData();
				OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Video;
				AddChildren(Visuals[0]);
				break;
			}
		}
	}
	else if (SampleType == Electra::UtilitiesMP4::FMP4BoxSampleEntry::ESampleType::Audio)
	{
		TArray<TSharedPtr<Electra::UtilitiesMP4::FMP4BoxAudioSampleEntry, ESPMode::ThreadSafe>> Audios;
		Stsd->GetSampleDescriptions(Audios);
		check(Audios.Num() == 1);

		switch(Audios[0]->GetType())
		{
			case Electra::UtilitiesMP4::MakeBoxAtom('e','n','c','a'):
			{
				bIsEncrypted = true;
				break;
			}
			case Electra::UtilitiesMP4::MakeBoxAtom('m','p','4','a'):
			{
				// Search down one extra level since the `esds` might be contained within a `wave` in QuickTime.
				auto Esds = Audios[0]->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxESDS>(Electra::UtilitiesMP4::MakeBoxAtom('e','s','d','s'), 1);
				ElectraDecodersUtil::MPEG::FESDescriptor esds;
				if (Esds.IsValid() && esds.Parse(Esds->GetESDescriptor()))
				{
					// AAC audio?
					if (esds.GetObjectTypeID() == ElectraDecodersUtil::MPEG::FESDescriptor::FObjectTypeID::MPEG4_Audio &&
						esds.GetStreamType() == ElectraDecodersUtil::MPEG::FESDescriptor::FStreamType::AudioStream)
					{
						ElectraDecodersUtil::MPEG::FAACDecoderConfigurationRecord dcr;
						if (dcr.Parse(esds.GetCodecSpecificData()))
						{
							ElectraProtronUtils::FCodecInfo::FAudio Audio;
							Audio.SampleRate = dcr.SamplingRate;
							Audio.ChannelConfiguration = dcr.ChannelConfiguration;
							Audio.NumChannels = ElectraDecodersUtil::MPEG::AACUtils::GetNumberOfChannelsFromChannelConfiguration(dcr.ChannelConfiguration);
							OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FAudio>(MoveTemp(Audio));
							OutCodecInfo.RFC6381 = dcr.GetCodecSpecifierRFC6381();
							OutCodecInfo.FourCC = Audios[0]->GetType();
							OutCodecInfo.DCR = esds.GetCodecSpecificData();
							OutCodecInfo.CSD = dcr.GetCodecSpecificData();
							OutCodecInfo.HumanReadableFormatInfo = dcr.GetFormatInfo();
							OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Audio;
							AddChildren(Audios[0]);
						}
					}
					// MPEG audio?
					else if (esds.GetObjectTypeID() == ElectraDecodersUtil::MPEG::FESDescriptor::FObjectTypeID::MPEG1_Audio &&
							 esds.GetStreamType() == ElectraDecodersUtil::MPEG::FESDescriptor::FStreamType::AudioStream)
					{
						ElectraProtronUtils::FCodecInfo::FAudio Audio;
						Audio.SampleRate = Audios[0]->GetSampleRate();
						Audio.NumChannels = Audios[0]->GetChannelCount();
						OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FAudio>(MoveTemp(Audio));
						OutCodecInfo.RFC6381 = TEXT("mp4a.40.34");
						OutCodecInfo.FourCC = Electra::UtilitiesMP4::MakeBoxAtom('m','p','g','a');
						OutCodecInfo.HumanReadableFormatInfo = TEXT("MPEG audio");
						OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Audio;
						AddChildren(Audios[0]);
					}
				}
				break;
			}
			default:
			{
				ElectraProtronUtils::FCodecInfo::FAudio Audio;
				Audio.SampleRate = Audios[0]->GetSampleRate();
				Audio.ChannelConfiguration = 0;
				Audio.NumChannels = Audios[0]->GetChannelCount();
				OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FAudio>(MoveTemp(Audio));
				OutCodecInfo.RFC6381 = Electra::UtilitiesMP4::GetPrintableBoxAtom(Audios[0]->GetType());
				OutCodecInfo.FourCC = Audios[0]->GetType();
				OutCodecInfo.DCR = Audios[0]->GetBoxData();
				OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Audio;
				AddChildren(Audios[0]);
				break;
			}
		}

	}
	else if (SampleType == Electra::UtilitiesMP4::FMP4BoxSampleEntry::ESampleType::QTFFTimecode)
	{
		TArray<TSharedPtr<Electra::UtilitiesMP4::FMP4BoxQTFFTimecodeSampleEntry, ESPMode::ThreadSafe>> Timecodes;
		Stsd->GetSampleDescriptions(Timecodes);
		check(Timecodes.Num() == 1);
		switch(Timecodes[0]->GetType())
		{
			case Electra::UtilitiesMP4::MakeBoxAtom('t','m','c','d'):
			{
				ElectraProtronUtils::FCodecInfo::FTMCDTimecode Timecode;

				Timecode.Flags = Timecodes[0]->GetFlags();
				Timecode.Timescale = Timecodes[0]->GetTimescale();
				Timecode.FrameDuration = Timecodes[0]->GetFrameDuration();
				Timecode.NumberOfFrames = Timecodes[0]->GetNumberOfFrames();
				OutCodecInfo.Properties.Emplace<ElectraProtronUtils::FCodecInfo::FTMCDTimecode>(MoveTemp(Timecode));
				OutCodecInfo.RFC6381 = Electra::UtilitiesMP4::GetPrintableBoxAtom(Timecodes[0]->GetType());
				OutCodecInfo.FourCC = Timecodes[0]->GetType();
				OutCodecInfo.DCR = Timecodes[0]->GetBoxData();
				OutCodecInfo.Type = ElectraProtronUtils::FCodecInfo::EType::Timecode;
				AddChildren(Timecodes[0]);
				break;
			}
		}
	}

	// If the track has not been identified as usable so far, remove it.
	if (OutCodecInfo.Type == ElectraProtronUtils::FCodecInfo::EType::Invalid)
	{
		if (!bIsEncrypted)
		{
			TArray<TSharedPtr<Electra::UtilitiesMP4::FMP4BoxSampleEntry, ESPMode::ThreadSafe>> Entries;
			Stsd->GetSampleDescriptions(Entries);
			check(Entries.Num() == 1);
			UE_LOG(LogElectraProtron, Warning, TEXT("Track of sample type \"%s\" is not supported, ignoring track #%u."), *Electra::UtilitiesMP4::GetPrintableBoxAtom(Entries[0]->GetType()), InTrackID);
		}
		else
		{
			UE_LOG(LogElectraProtron, Warning, TEXT("Track is using encryption, ignoring track #%u."), InTrackID);
		}
	}
}



FVariant FElectraProtronPlayer::FImpl::GetMediaInfo(FName InInfoName)
{
	if (InInfoName == ElectraProtronOptionNames::StartTimecodeValue || InInfoName == ElectraProtronOptionNames::StartTimecodeFrameRate || InInfoName == ElectraProtronOptionNames::KeyframeInterval)
	{
		auto ci = CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video);
		int32 SelectedVideoTrackIndex = TrackSelection.SelectedTrackIndex[ci];
		if (SelectedVideoTrackIndex >= 0 && SelectedVideoTrackIndex < UsableTrackArrayIndicesByType[ci].Num())
		{
			const FTrackInfo& ti(*Tracks[UsableTrackArrayIndicesByType[ci][SelectedVideoTrackIndex]]);

			if (InInfoName == ElectraProtronOptionNames::StartTimecodeValue && ti.FirstSampleTimecode.IsSet())
			{
				return FVariant(ti.FirstSampleTimecode.GetValue().Timecode);
			}
			else if (InInfoName == ElectraProtronOptionNames::StartTimecodeFrameRate && ti.FirstSampleTimecode.IsSet())
			{
				return FVariant(ti.FirstSampleTimecode.GetValue().Framerate);
			}
			else if (InInfoName == ElectraProtronOptionNames::KeyframeInterval)
			{
				return FVariant(ti.bIsKeyframeOnlyFormat ? (int32)1 : (int32)0);
			}
		}
	}
	return FVariant();
}



int32 FElectraProtronPlayer::FImpl::GetNumTracks(EMediaTrackType InTrackType)
{
	switch(InTrackType)
	{
		case EMediaTrackType::Video:
		{
			return UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num();
		}
		case EMediaTrackType::Audio:
		{
			return UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num();
		}
	}
	return 0;
}
int32 FElectraProtronPlayer::FImpl::GetNumTrackFormats(EMediaTrackType InTrackType, int32 InTrackIndex)
{
	if (InTrackIndex >= 0)
	{
		// Every track this player supports, if the track exists, only has a single format.
		switch(InTrackType)
		{
			case EMediaTrackType::Video:
			{
				return InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num() ? 1 : 0;
			}
			case EMediaTrackType::Audio:
			{
				return InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num() ? 1 : 0;
			}
		}
	}
	return 0;
}

int32 FElectraProtronPlayer::FImpl::GetTrackFormat(EMediaTrackType InTrackType, int32 InTrackIndex)
{
	if (InTrackIndex >= 0)
	{
		// Every track this player supports, if the track exists, only has a single format.
		switch(InTrackType)
		{
			case EMediaTrackType::Video:
			{
				return InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num() ? 0 : -1;
			}
			case EMediaTrackType::Audio:
			{
				return InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num() ? 0 : -1;
			}
		}
	}
	return -1;
}

FText FElectraProtronPlayer::FImpl::GetTrackDisplayName(EMediaTrackType InTrackType, int32 InTrackIndex)
{
	if (InTrackIndex >= 0)
	{
		switch(InTrackType)
		{
			case EMediaTrackType::Video:
			{
				if (InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num())
				{
					const FTrackInfo& ti(*Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)][InTrackIndex]]);
					FString Name(ti.MP4Track->GetCommonMetadata().Name);
					if (Name.IsEmpty())
					{
						Name = FString::Printf(TEXT("Video track #%u"), ti.TrackID);
					}
					return FText::FromString(Name);
				}
				break;
			}
			case EMediaTrackType::Audio:
			{
				if (InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num())
				{
					const FTrackInfo& ti(*Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)][InTrackIndex]]);
					FString Name(ti.MP4Track->GetCommonMetadata().Name);
					if (Name.IsEmpty())
					{
						Name = FString::Printf(TEXT("Audio track #%u"), ti.TrackID);
					}
					return FText::FromString(Name);
				}
				break;
			}
		}
	}
	return FText();
}

FString FElectraProtronPlayer::FImpl::GetTrackLanguage(EMediaTrackType InTrackType, int32 InTrackIndex)
{
	if (InTrackIndex >= 0)
	{
		switch(InTrackType)
		{
			case EMediaTrackType::Video:
			{
				if (InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num())
				{
					return Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)][InTrackIndex]]->MP4Track->GetCommonMetadata().LanguageTag.Get();
				}
				break;
			}
			case EMediaTrackType::Audio:
			{
				if (InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num())
				{
					return Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)][InTrackIndex]]->MP4Track->GetCommonMetadata().LanguageTag.Get();
				}
				break;
			}
		}
	}
	return FString();
}

FString FElectraProtronPlayer::FImpl::GetTrackName(EMediaTrackType InTrackType, int32 InTrackIndex)
{
	if (InTrackIndex >= 0)
	{
		switch(InTrackType)
		{
			case EMediaTrackType::Video:
			{
				if (InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num())
				{
					return FString::Printf(TEXT("%u"), Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)][InTrackIndex]]->TrackID);
				}
				break;
			}
			case EMediaTrackType::Audio:
			{
				if (InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num())
				{
					return FString::Printf(TEXT("%u"), Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)][InTrackIndex]]->TrackID);
				}
				break;
			}
		}
	}
	return FString();
}

bool FElectraProtronPlayer::FImpl::GetVideoTrackFormat(int32 InTrackIndex, int32 InFormatIndex, FMediaVideoTrackFormat& OutFormat)
{
	if (InTrackIndex >= 0 && InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)].Num() && InFormatIndex == 0)
	{
		const FTrackInfo& ti(*Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)][InTrackIndex]]);
		const ElectraProtronUtils::FCodecInfo::FVideo& vi(ti.CodecInfo.Properties.Get<ElectraProtronUtils::FCodecInfo::FVideo>());
		OutFormat.Dim.X = (int32) vi.Width;
		OutFormat.Dim.Y = (int32) vi.Height;
		OutFormat.FrameRate = (float) vi.FrameRate.GetAsDouble();
		OutFormat.FrameRates = TRange<float>{ OutFormat.FrameRate };
		OutFormat.TypeName = ti.HumanReadableCodecFormat;
		return true;
	}
	return false;
}

bool FElectraProtronPlayer::FImpl::GetAudioTrackFormat(int32 InTrackIndex, int32 InFormatIndex, FMediaAudioTrackFormat& OutFormat)
{
	if (InTrackIndex >= 0 && InTrackIndex < UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)].Num() && InFormatIndex == 0)
	{
		const FTrackInfo& ti(*Tracks[UsableTrackArrayIndicesByType[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)][InTrackIndex]]);
		const ElectraProtronUtils::FCodecInfo::FAudio& ai(ti.CodecInfo.Properties.Get<ElectraProtronUtils::FCodecInfo::FAudio>());
		OutFormat.BitsPerSample = 16;
		OutFormat.NumChannels = ai.NumChannels;
		OutFormat.SampleRate = ai.SampleRate;
		OutFormat.TypeName = ti.HumanReadableCodecFormat;
		return true;
	}
	return false;
}

int32 FElectraProtronPlayer::FImpl::GetSelectedTrack(EMediaTrackType InTrackType)
{
	switch(InTrackType)
	{
		case EMediaTrackType::Video:
		{
			return TrackSelection.SelectedTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)];
		}
		case EMediaTrackType::Audio:
		{
			return TrackSelection.SelectedTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)];
		}
	}
	return -1;
}


bool FElectraProtronPlayer::FImpl::SelectTrack(EMediaTrackType InTrackType, int32 InTrackIndex)
{
	if (InTrackIndex < -1)
	{
		InTrackIndex = -1;
	}
	switch(InTrackType)
	{
		case EMediaTrackType::Video:
		{
			const auto TypeIndex = CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video);
			if (InTrackIndex < UsableTrackArrayIndicesByType[TypeIndex].Num())
			{
				if (TrackSelection.SelectedTrackIndex[TypeIndex] != InTrackIndex)
				{
					TrackSelection.SelectedTrackIndex[TypeIndex] = InTrackIndex;
					TrackSelection.bChanged = true;
					bAreRatesValid = false;
					UpdateTrackLoader(TypeIndex);
					HandleActiveTrackChanges();
				}
				return true;
			}
			break;
		}
		case EMediaTrackType::Audio:
		{
			const auto TypeIndex = CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio);
			if (InTrackIndex < UsableTrackArrayIndicesByType[TypeIndex].Num())
			{
				if (TrackSelection.SelectedTrackIndex[TypeIndex] != InTrackIndex)
				{
					TrackSelection.SelectedTrackIndex[TypeIndex] = InTrackIndex;
					TrackSelection.bChanged = true;
					bAreRatesValid = false;
					UpdateTrackLoader(TypeIndex);
					HandleActiveTrackChanges();
				}
				return true;
			}
			break;
		}
		default:
		{
			break;
		}
	}
	return false;
}

bool FElectraProtronPlayer::FImpl::SetTrackFormat(EMediaTrackType InTrackType, int32 InTrackIndex, int32 InFormatIndex)
{
	return false;
}

bool FElectraProtronPlayer::FImpl::QueryCacheState(EMediaCacheState InState, TRangeSet<FTimespan>& OutTimeRanges)
{
	if (InState == EMediaCacheState::Loading)
	{
		if (TrackSelection.SelectedTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)] >= 0)
		{
			OutTimeRanges = VideoLoaderThread.GetTimeRangesToLoad();
			return true;
		}
		else if (TrackSelection.SelectedTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)] >= 0)
		{
			OutTimeRanges = AudioLoaderThread.GetTimeRangesToLoad();
			return true;
		}
	}
	else if (InState == EMediaCacheState::Loaded)
	{
		if (TrackSelection.SelectedTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)] >= 0)
		{
			TSharedPtr<FSampleQueueInterface, ESPMode::ThreadSafe> sqi(GetCurrentSampleQueueInterface());
			if (sqi.IsValid())
			{
				sqi->GetVideoCache().QueryCacheState(OutTimeRanges);
				return true;
			}
		}
	}
	return false;
}

int32 FElectraProtronPlayer::FImpl::GetSampleCount(EMediaCacheState InState)
{
	return 0;
}

float FElectraProtronPlayer::FImpl::GetRate()
{
	return CurrentRate;
}

bool FElectraProtronPlayer::FImpl::SetRate(float InRate)
{
	HandleActiveTrackChanges();

	IntendedRate = InRate;
	HandleRateChanges();

	return true;
}

FTimespan FElectraProtronPlayer::FImpl::GetTime()
{
	return CurrentPlayPosTime;
}

bool FElectraProtronPlayer::FImpl::SetLooping(bool bInLooping)
{
	bool bOk = true;
	if (bOk && !VideoDecoderThread.SetLooping(bInLooping))
	{
		bOk = false;
	}
	if (bOk && !AudioDecoderThread.SetLooping(bInLooping))
	{
		bOk = false;
	}
	SharedPlayParams->bShouldLoop = bInLooping && bOk;
	return bOk;
}

bool FElectraProtronPlayer::FImpl::IsLooping()
{
	return SharedPlayParams->bShouldLoop;
}

void FElectraProtronPlayer::FImpl::Seek(const FTimespan& InTime, int32 InNewSequenceIndex, const TOptional<int32>& InNewLoopIndex)
{
	FSeekRequest sr;
	sr.NewTime = InTime;
	sr.NewSequenceIndex = InNewSequenceIndex;
	sr.NewLoopIndex = InNewLoopIndex;
	if (CurrentSampleQueueInterface.IsValid())
	{
		CurrentSampleQueueInterface->SeekIssuedTo(InTime, TOptional<int32>(InNewSequenceIndex));
	}
	SeekRequestLock.Lock();
	PendingSeekRequest = MoveTemp(sr);
	SeekRequestLock.Unlock();
	WorkMessageSignal.Signal();
}

TRange<FTimespan> FElectraProtronPlayer::FImpl::GetPlaybackTimeRange(EMediaTimeRangeType InRangeToGet)
{
	return InRangeToGet == EMediaTimeRangeType::Absolute ? TRange<FTimespan>(FTimespan(0), GetDuration()) : CurrentPlaybackRange;
}

bool FElectraProtronPlayer::FImpl::SetPlaybackTimeRange(const TRange<FTimespan>& InTimeRange)
{
	// For proper validation we need to have the content duration.
	if (Duration <= FTimespan::Zero() || InTimeRange.IsDegenerate() || !InTimeRange.HasLowerBound() || !InTimeRange.HasUpperBound() || InTimeRange.GetLowerBoundValue() > InTimeRange.GetUpperBoundValue())
	{
		return false;
	}
	// If we get an empty range we instead set the range to be the entire movie.
	if (InTimeRange.IsEmpty())
	{
		CurrentPlaybackRange = TRange<FTimespan>(FTimespan(0), GetDuration());
	}
	else
	{
		TRange<FTimespan> r(InTimeRange);
		if (r.GetLowerBoundValue() < FTimespan::Zero())
		{
			UE_LOG(LogElectraProtron, Warning, TEXT("Clamping start of playback range to zero as it was set negative."));
			r.SetLowerBoundValue(FTimespan::Zero());
		}
		if (r.GetUpperBoundValue() > Duration)
		{
			UE_LOG(LogElectraProtron, Warning, TEXT("Clamping end of playback range to movie duration as it was set larger."));
			r.SetUpperBoundValue(Duration);
		}
		CurrentPlaybackRange = MoveTemp(r);
	}
	CurrentSampleQueueInterface->SetPlaybackRange(CurrentPlaybackRange);
	VideoLoaderThread.SetPlaybackRange(CurrentPlaybackRange);
	AudioLoaderThread.SetPlaybackRange(CurrentPlaybackRange);
	VideoDecoderThread.SetPlaybackRange(CurrentPlaybackRange);
	AudioDecoderThread.SetPlaybackRange(CurrentPlaybackRange);
	return true;
}

void FElectraProtronPlayer::FImpl::TickFetch(FTimespan InDeltaTime, FTimespan InTimecode)
{
}

void FElectraProtronPlayer::FImpl::TickInput(FTimespan InDeltaTime, FTimespan InTimecode)
{
	if (LastErrorMessage.IsEmpty())
	{
		if (!VideoDecoderThread.GetLastError().IsEmpty())
		{
			LastErrorMessage = VideoDecoderThread.GetLastError();
		}
		else if (!AudioDecoderThread.GetLastError().IsEmpty())
		{
			LastErrorMessage = AudioDecoderThread.GetLastError();
		}
		else if (!VideoLoaderThread.GetLastError().IsEmpty())
		{
			LastErrorMessage = VideoLoaderThread.GetLastError();
		}
		else if (!AudioLoaderThread.GetLastError().IsEmpty())
		{
			LastErrorMessage = AudioLoaderThread.GetLastError();
		}
	}

	auto sqi = GetCurrentSampleQueueInterface();
	if (sqi.IsValid())
	{
		FMediaTimeStamp ts = sqi->GetLastHandedOutTimestamp();
		const bool bIsVideoActive = TrackSelection.ActiveTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video)] != -1;
		const bool bIsAudioActive = TrackSelection.ActiveTrackIndex[CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio)] != -1;
		if (ts.IsValid())
		{
			FTimespan NewPos = ts.GetTime();
			if (bIsVideoActive)
			{
				VideoDecoderThread.SetEstimatedPlaybackTime(NewPos);
				CurrentPlayPosTime = NewPos;
			}
			if (!bIsAudioActive)
			{
				AudioDecoderThread.SetEstimatedPlaybackTime(NewPos);
			}
		}
		else if (bIsAudioActive)
		{
			FTimespan NewPos = AudioDecoderThread.GetEstimatedPlaybackTime();
			VideoDecoderThread.SetEstimatedPlaybackTime(NewPos);
			CurrentPlayPosTime = NewPos;
		}
	}
}


void FElectraProtronPlayer::FImpl::UpdateTrackLoader(int32 InCodecTypeIndex)
{
	if (TrackSelection.SelectedTrackIndex[InCodecTypeIndex] >= 0)
	{
		auto Track = Tracks[UsableTrackArrayIndicesByType[InCodecTypeIndex][TrackSelection.SelectedTrackIndex[InCodecTypeIndex]]];
		check(Track.IsValid());
		if (Track.IsValid())
		{
			// Do we have a track sample buffer for this track?
			if (!TrackSampleBuffers.Contains(Track->TrackID))
			{
				// No, create it now.
				TSharedPtr<FMP4TrackSampleBuffer, ESPMode::ThreadSafe> tsb = MakeShared<FMP4TrackSampleBuffer, ESPMode::ThreadSafe>();
				tsb->TrackAndCodecInfo = Track;
				tsb->TrackID = Track->TrackID;
				TrackSampleBuffers.Emplace(Track->TrackID, MoveTemp(tsb));
			}

			if (InCodecTypeIndex == CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Video))
			{
				VideoLoaderThread.RequestLoad(TrackSampleBuffers[Track->TrackID], CurrentPlayPosTime);
			}
			else if (InCodecTypeIndex == CodecTypeIndex(ElectraProtronUtils::FCodecInfo::EType::Audio))
			{
				AudioLoaderThread.RequestLoad(TrackSampleBuffers[Track->TrackID], CurrentPlayPosTime);
			}
		}
	}
}



IMediaSamples::EFetchBestSampleResult FElectraProtronPlayer::FImpl::FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp>& InTimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bInReverse, bool bInConsistentResult)
{
	TSharedPtr<FSampleQueueInterface, ESPMode::ThreadSafe> sqi(GetCurrentSampleQueueInterface());
	if (sqi.IsValid())
	{
		FProtronVideoCache::EGetResult gr = sqi->GetVideoCache().GetFrame(OutSample, InTimeRange, IsLooping(), bInReverse, bInConsistentResult);
		if (gr == FProtronVideoCache::EGetResult::Hit)
		{
			sqi->UpdateNextExpectedTimestamp(OutSample, bInReverse, IsLooping());
			sqi->UpdateLastHandedOutTimestamp(OutSample);
			return IMediaSamples::EFetchBestSampleResult::Ok;
		}
		else if (gr == FProtronVideoCache::EGetResult::PurgedEmpty)
		{
			sqi->ResetCurrentTimestamps();
			return IMediaSamples::EFetchBestSampleResult::PurgedToEmpty;
		}
	}
	return IMediaSamples::EFetchBestSampleResult::NoSample;
}
bool FElectraProtronPlayer::FImpl::FetchAudio(TRange<FMediaTimeStamp> InTimeRange, TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& OutSample)
{
	TSharedPtr<FSampleQueueInterface, ESPMode::ThreadSafe> sqi(GetCurrentSampleQueueInterface());
	return sqi.IsValid() ? sqi->GetCurrentSampleQueue()->FetchAudio(InTimeRange, OutSample) : false;
}
bool FElectraProtronPlayer::FImpl::FetchCaption(TRange<FMediaTimeStamp> InTimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
{
	return false;
}
bool FElectraProtronPlayer::FImpl::FetchMetadata(TRange<FMediaTimeStamp> InTimeRange, TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe>& OutSample)
{
	return false;
}
bool FElectraProtronPlayer::FImpl::FetchSubtitle(TRange<FMediaTimeStamp> InTimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
{
	return false;
}
void FElectraProtronPlayer::FImpl::FlushSamples()
{
}
void FElectraProtronPlayer::FImpl::SetMinExpectedNextSequenceIndex(TOptional<int32> InNextSequenceIndex)
{
	TSharedPtr<FSampleQueueInterface, ESPMode::ThreadSafe> sqi(GetCurrentSampleQueueInterface());
	if (sqi.IsValid())
	{
		sqi->GetCurrentSampleQueue()->SetMinExpectedNextSequenceIndex(InNextSequenceIndex);
	}
}
bool FElectraProtronPlayer::FImpl::PeekVideoSampleTime(FMediaTimeStamp& OutTimeStamp)
{
	TSharedPtr<FSampleQueueInterface, ESPMode::ThreadSafe> sqi(GetCurrentSampleQueueInterface());
	return sqi.IsValid() ? sqi->PeekVideoSampleTime(OutTimeStamp) : false;
}
bool FElectraProtronPlayer::FImpl::CanReceiveVideoSamples(uint32 InNum) const
{
	return true;
}
bool FElectraProtronPlayer::FImpl::CanReceiveAudioSamples(uint32 InNum) const
{
	TSharedPtr<FSampleQueueInterface, ESPMode::ThreadSafe> sqi(GetCurrentSampleQueueInterface());
	return sqi.IsValid() ? sqi->CanEnqueueAudioSample() : true;
}
bool FElectraProtronPlayer::FImpl::CanReceiveSubtitleSamples(uint32 InNum) const
{
	return true;
}
bool FElectraProtronPlayer::FImpl::CanReceiveCaptionSamples(uint32 InNum) const
{
	return true;
}
bool FElectraProtronPlayer::FImpl::CanReceiveMetadataSamples(uint32 InNum) const
{
	return true;
}
int32 FElectraProtronPlayer::FImpl::NumAudioSamples() const
{
	return 0;
}
int32 FElectraProtronPlayer::FImpl::NumCaptionSamples() const
{
	return 0;
}
int32 FElectraProtronPlayer::FImpl::NumMetadataSamples() const
{
	return 0;
}
int32 FElectraProtronPlayer::FImpl::NumSubtitleSamples() const
{
	return 0;
}
int32 FElectraProtronPlayer::FImpl::NumVideoSamples() const
{
	return 0;
}
