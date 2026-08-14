// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/MP4Boxes/MP4Track.h"
#include "ElectraBaseModule.h"	// for log category `LogElectraBase`

namespace Electra
{
	namespace UtilitiesMP4
	{

		bool FMP4Track::Prepare(FTimeFraction InFullMovieDuration, FTimeFraction InAdjustedMovieDuration)
		{
			if (!TrakBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `trak` box given."));
				return false;
			}
			TkhdBox = TrakBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxTKHD>(Electra::UtilitiesMP4::MakeBoxAtom('t','k','h','d'), 0);
			if (!TkhdBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `tkhd` box in `trak`."));
				return false;
			}
			Convs.TrackID = TkhdBox->GetTrackID();
			Convs.FullMovieDuration = InFullMovieDuration;
			Convs.DurationFromMvhdBox = InAdjustedMovieDuration;
			Convs.DurationFromTkhdBox.SetFromND(TkhdBox->GetDuration(), Convs.DurationFromMvhdBox.GetDenominator());

			// Check for correct box hierarchy.
			TSharedPtr<FMP4BoxMDIA, ESPMode::ThreadSafe> MdiaBox = TrakBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxMDIA>(Electra::UtilitiesMP4::MakeBoxAtom('m','d','i','a'), 0);
			if (!MdiaBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `mdia` box in `trak`."));
				return false;
			}
			TSharedPtr<FMP4BoxMINF, ESPMode::ThreadSafe> MinfBox = MdiaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxMINF>(Electra::UtilitiesMP4::MakeBoxAtom('m','i','n','f'), 0);
			if (!MinfBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `minf` box in `mdia`."));
				return false;
			}
			TSharedPtr<FMP4BoxSTBL, ESPMode::ThreadSafe> StblBox = MinfBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTBL>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','b','l'), 0);
			if (!StblBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `stbl` box in `minf`."));
				return false;
			}

			MdhdBox = MdiaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxMDHD>(Electra::UtilitiesMP4::MakeBoxAtom('m','d','h','d'), 0);
			if (!MdhdBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `mdhd` box in `mdia`."));
				return false;
			}
			Convs.DurationFromMdhdBox = MdhdBox->GetDuration();
			if (Convs.DurationFromMdhdBox.GetDenominator() == 0)
			{
				LastErrorMessage = FString::Printf(TEXT("Timescale in `mdhd` box is zero, which is not supported."));
				return false;
			}

			// Required sample information boxes:
			SttsBox = StblBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTTS>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','t','s'), 0);
			if (!SttsBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `stts` box in `stbl`."));
				return false;
			}
			StscBox = StblBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTSC>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','s','c'), 0);
			if (!StscBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `stsc` box in `stbl`."));
				return false;
			}
			StszBox = StblBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTSZ>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','s','z'), 0);
			if (!StszBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `stsz` box in `stbl`."));
				return false;
			}
			StcoBox = StblBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTCO>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','c','o'), 0);
			if (!StcoBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("No `stco` ot `co64` box in `stbl`."));
				return false;
			}

			// Validity check
			Convs.NumTotalSamples = StszBox->GetNumberOfSamples();
			if (Convs.NumTotalSamples != SttsBox->GetNumTotalSamples())
			{
				LastErrorMessage = FString::Printf(TEXT("Mismatching number of samples in `stts` and `stsz` boxes."));
				return false;
			}

			// Optional sample information boxes:
			CttsBox = StblBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxCTTS>(Electra::UtilitiesMP4::MakeBoxAtom('c','t','t','s'), 0);
			StssBox = StblBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxSTSS>(Electra::UtilitiesMP4::MakeBoxAtom('s','t','s','s'), 0);
			StblBox->GetAllBoxInstances(SgpdBoxes, Electra::UtilitiesMP4::MakeBoxAtom('s','g','p','d'));
			StblBox->GetAllBoxInstances(SbgpBoxes, Electra::UtilitiesMP4::MakeBoxAtom('s','b','g','p'));

			// Start with default values for what is mapped onto the timeline.
			Convs.CompositionTimeAtZeroPoint = CttsBox.IsValid() && CttsBox->GetEntries().Num() ? CttsBox->GetEntries()[0].sample_offset : 0;
			Convs.MappedDurationFromElstBox = Convs.DurationFromMvhdBox;

			// Optional edit list
			ElstBox = TrakBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxELST>(Electra::UtilitiesMP4::MakeBoxAtom('e','l','s','t'), 1);
			// If there is an edit list it needs to be simple and only contain a composition time mapping.
			if (ElstBox.IsValid())
			{
				if (ElstBox->RepeatEdits())
				{
					LastErrorMessage = FString::Printf(TEXT("Repeating `elst` box ist not supported."));
					return false;
				}
				// If there is more than a single entry things are bound to be complicated, so we don't even
				// want to know if the entry we are interested in is there.
				const TArray<Electra::UtilitiesMP4::FMP4BoxELST::FEntry>& ElstEntries = ElstBox->GetEntries();
				if (ElstEntries.Num() == 0)
				{
					LastErrorMessage = FString::Printf(TEXT("Edit list is empty."));
					return false;
				}
				if (ElstEntries.Num() > 1)
				{
					LastErrorMessage = FString::Printf(TEXT("Edit list with more than one entry is not supported."));
					return false;
				}
				if (!(ElstEntries[0].media_rate_integer == 1 && ElstEntries[0].media_rate_fraction == 0))
				{
					LastErrorMessage = FString::Printf(TEXT("Edit list entries with playback rates other than 1.0 are not supported."));
					return false;
				}
				if (ElstEntries[0].media_time < 0)
				{
					LastErrorMessage = FString::Printf(TEXT("Edit list specifies an empty edit, which is not supported."));
					return false;
				}

				Convs.CompositionTimeAtZeroPoint = ElstEntries[0].media_time;
				// The mapped duration may be different from the media duration itself, in which case the track is either truncated
				// or the last sample has a longer duration to be repeated until the end.
				if (ElstEntries[0].edit_duration == 0)
				{
					// The value of 0 is reserved for fragmented files with no `mehd` box.
					// We do not handle this case.
					LastErrorMessage = FString::Printf(TEXT("Edit list specifies zero edit duration, which is not supported."));
					return false;
				}
				Convs.MappedDurationFromElstBox.SetFromND(ElstEntries[0].edit_duration, Convs.DurationFromMvhdBox.GetDenominator());
			}
			// For convenience sake convert the mapped duration from `mvhd` timescale into `mdhd` timescale.
			Convs.MappedDurationFromElstBox.SetFromND(Convs.MappedDurationFromElstBox.GetAsTimebase(Convs.DurationFromMdhdBox.GetDenominator()), Convs.DurationFromMdhdBox.GetDenominator());


			// Set up common track meta data
			UdtaBox = TrakBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxUDTA>(Electra::UtilitiesMP4::MakeBoxAtom('u','d','t','a'), 0);
			if (UdtaBox.IsValid())
			{
				// Is there a `name` box?
				TSharedPtr<FMP4BoxBase, ESPMode::ThreadSafe> NameBox = UdtaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxBase>(Electra::UtilitiesMP4::MakeBoxAtom('n','a','m','e'), 0);
				if (NameBox.IsValid() && NameBox->GetBoxData().Num())
				{
					CommonMetadata.Name = FString::ConstructFromPtrSize(reinterpret_cast<const ANSICHAR*>(NameBox->GetBoxData().GetData()), NameBox->GetBoxData().Num());
				}
			}
			TSharedPtr<FMP4BoxHDLR, ESPMode::ThreadSafe> HdlrBox = MdiaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxHDLR>(Electra::UtilitiesMP4::MakeBoxAtom('h','d','l','r'), 0);
			check(HdlrBox.IsValid())
			if (HdlrBox.IsValid())
			{
				CommonMetadata.HandlerName = HdlrBox->GetHandlerName();
			}
			// Get language from `mdhd` box first.
			CommonMetadata.LanguageTag = MdhdBox->GetLanguageTag();
			// Then, if there is an `elng` box that gives better information get it from there.
			TSharedPtr<FMP4BoxELNG, ESPMode::ThreadSafe> ElngBox = MdiaBox->FindBoxRecursive<Electra::UtilitiesMP4::FMP4BoxELNG>(Electra::UtilitiesMP4::MakeBoxAtom('e','l','n','g'), 0);
			if (ElngBox.IsValid())
			{
				CommonMetadata.LanguageTag = ElngBox->GetLanguageTag();
			}

			bHasBeenPrepared = true;

			// Given the timeline mapping, locate the sample number that falls onto the start of the timeline
			// and last one falling onto the end of the timeline.
			TSharedPtr<FIterator, ESPMode::ThreadSafe> StartIt = CreateIterator();

			// If we could not create an iterator then this file is most likely a fragmented mp4
			// which is not handled.
			if (!StartIt.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("Could not locate first media sample. Is this an empty or a fragmented mp4?"));
				return false;
			}
			const int64 pts0 = Convs.CompositionTimeAtZeroPoint;
			uint32 SyncSampleNum = 0;
			bool bFound = false;
			while(1)
			{
				if (StartIt->IsSyncOrRAPSample())
				{
					SyncSampleNum = StartIt->GetSampleNumber();
				}
				int64 s = StartIt->GetPTS().GetNumerator();
				int64 e = s + StartIt->GetDuration().GetNumerator();
				if (pts0 >= s && pts0 < e)
				{
					Convs.FirstSample.SampleNumber = StartIt->GetSampleNumber();
					Convs.FirstSample.SamplePTS = s;
					Convs.FirstSample.StartPTS = pts0;
					Convs.FirstSample.SyncSampleNumber = SyncSampleNum;
					Convs.DTSShiftAtZeroPoint = StartIt->GetDTS().GetNumerator();
					bFound = true;
					break;
				}
				if (!StartIt->Next())
				{
					break;
				}
			}
			check(bFound);

			TSharedPtr<FIterator, ESPMode::ThreadSafe> EndIt = CreateIteratorAtLastFrame();
			check(EndIt.IsValid());

			// Find the highest PTS
			uint32 HighestPTSIndex = ~0U;
			int64 HighestPTS = TNumericLimits<int64>::Min();
			int64 HighestEndPTS = 0;
			while(1)
			{
				int64 pts = EndIt->GetPTS().GetNumerator();
				if (pts > HighestPTS)
				{
					HighestPTS = pts;
					HighestEndPTS = pts + EndIt->GetDuration().GetNumerator();
					HighestPTSIndex = EndIt->GetSampleNumber();
				}
				if (EndIt->IsSyncOrRAPSample() || !EndIt->Prev())
				{
					break;
				}
			}
			check(HighestPTSIndex != ~0U);

			const int64 pts1 = Convs.CompositionTimeAtZeroPoint + Convs.MappedDurationFromElstBox.GetNumerator();
			// Is the mapped duration is greater or equal to what the media duration is?
			if (pts1 >= HighestEndPTS)
			{
				Convs.LastSample.SampleNumber = HighestPTSIndex;
				Convs.LastSample.LastSampleNumber = Convs.NumTotalSamples - 1;
				Convs.LastSample.SamplePTS = HighestPTS;
				Convs.LastSample.EndPTS = pts1;
				double PaddingDuration = (pts1 - HighestEndPTS) / (double)Convs.DurationFromMdhdBox.GetDenominator();
				if (PaddingDuration >= 0.001)
				{
					UE_LOG(LogElectraBase, Verbose, TEXT("Last sample duration in track #%u will be extended by %#.5f seconds to align with the movie duration in the `mvhd` box."), Convs.TrackID, PaddingDuration);
				}
			}
			// The mapping truncates the media. Find where that is.
			else
			{
				EndIt = CreateIteratorAtLastFrame();
				bFound = false;
				while(!bFound)
				{
					int64 s = EndIt->GetPTS().GetNumerator();
					int64 e = s + EndIt->GetDuration().GetNumerator();
					if (pts1 > s && pts1 <= e)
					{
						// This sample contains the end of the mapped duration.
						Convs.LastSample.SamplePTS = s;
						Convs.LastSample.EndPTS = pts1;
						Convs.LastSample.SampleNumber = EndIt->GetSampleNumber();
						// If it is a sync or rap sample then this is also the last sample we need to look at
						// as no later sample (in decode order) may be needed to decode this one.
						if (EndIt->IsSyncOrRAPSample() || EndIt->IsLast())
						{
							Convs.LastSample.LastSampleNumber = EndIt->GetSampleNumber();
						}
						else
						{
							// We need decode frames up to this PTS, meaning that everything that comes
							// earlier in decode order we need to decode as well.
							Convs.LastSample.LastSampleNumber = EndIt->GetSampleNumber();
							while(EndIt->Next())
							{
								int64 NextS = EndIt->GetDTS().GetNumerator();
								if (NextS > pts1)
								{
									break;
								}
								Convs.LastSample.LastSampleNumber = EndIt->GetSampleNumber();
							}
						}
						bFound = true;
					}
					else if (!EndIt->Prev())
					{
						break;
					}
				}
				check(bFound);
			}
			return true;
		}

		const FMP4TrackMetadataCommon& FMP4Track::GetCommonMetadata()
		{
			return CommonMetadata;
		}


		FString FMP4Track::GetLastError()
		{
			return LastErrorMessage;
		}


		// Returns the number of samples in this track.
		uint32 FMP4Track::GetNumberOfSamples()
		{
			check(bHasBeenPrepared);
			return bHasBeenPrepared ? Convs.NumTotalSamples : 0;
		}

		// Create an interator starting at the first sample.
		TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> FMP4Track::CreateIterator()
		{
			check(bHasBeenPrepared);
			if (!bHasBeenPrepared)
			{
				LastErrorMessage = FString::Printf(TEXT("Track has not been prepared, cannot create an iterator."));
				return nullptr;
			}
			if (Convs.NumTotalSamples == 0)
			{
				LastErrorMessage = FString::Printf(TEXT("There are no samples in this track, cannot create an iterator."));
				return nullptr;
			}
			TUniquePtr<FIterator> It(new FIterator);
			It->Track = AsWeak();
			It->SampleNumber = 0;
			// Copy convenience values into the iterator
			It->Convs = Convs;
			// Set up the box iterators.
			It->stszIt.SetBox(StszBox);
			It->sttsIt.SetBox(SttsBox);
			It->cttsIt.SetBox(CttsBox, Convs.NumTotalSamples);
			It->stcoIt.SetBox(StcoBox);
			It->stscIt.SetBox(StscBox, Convs.NumTotalSamples);
			It->stssIt.SetBox(StssBox, Convs.NumTotalSamples);
			// Do we have a `rap ` group?
			TSharedPtr<FMP4BoxSGPD, ESPMode::ThreadSafe>* rapSGPD = SgpdBoxes.FindByPredicate([](const TSharedPtr<FMP4BoxSGPD, ESPMode::ThreadSafe>& e){ return e->GetGroupingType() == Electra::UtilitiesMP4::MakeBoxAtom('r','a','p',' ');});
			TSharedPtr<FMP4BoxSBGP, ESPMode::ThreadSafe>* rapSBGP = SbgpBoxes.FindByPredicate([](const TSharedPtr<FMP4BoxSBGP, ESPMode::ThreadSafe>& e){ return e->GetGroupingType() == Electra::UtilitiesMP4::MakeBoxAtom('r','a','p',' ');});
			if (rapSGPD && rapSBGP)
			{
				It->rapIt.SetBox(*rapSBGP, (*rapSGPD)->GetDefaultGroupDescriptionIndex(), Convs.NumTotalSamples);
			}
			else
			{
				// Initialize the iterator such that it can be used to return "not a RAP" for every sample.
				It->rapIt.SetBox(nullptr, 0, Convs.NumTotalSamples);
			}
			It->Update();
			return MakeShareable(It.Release());
		}

		// Create an interator starting at the last sample (used when iterating in reverse, crossing back from the beginning to the end)
		TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> FMP4Track::CreateIteratorAtLastFrame()
		{
			check(bHasBeenPrepared);
			if (!bHasBeenPrepared)
			{
				LastErrorMessage = FString::Printf(TEXT("Track has not been prepared, cannot create an iterator."));
				return nullptr;
			}
			if (Convs.NumTotalSamples == 0)
			{
				LastErrorMessage = FString::Printf(TEXT("There are no samples in this track, cannot create an iterator."));
				return nullptr;
			}
			return CreateIterator(Convs.NumTotalSamples - 1);
		}

		TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> FMP4Track::CreateIterator(uint32 InAtSampleNumber)
		{
			TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> It(CreateIterator());
			if (It.IsValid() && InAtSampleNumber)
			{
				InAtSampleNumber = InAtSampleNumber < Convs.NumTotalSamples ? InAtSampleNumber : Convs.NumTotalSamples-1;
				It->SampleNumber = InAtSampleNumber;
				It->stszIt.SetToSampleNumber(InAtSampleNumber);
				It->sttsIt.SetToSampleNumber(InAtSampleNumber);
				It->cttsIt.SetToSampleNumber(InAtSampleNumber);
				It->stcoIt.SetToSampleNumber(InAtSampleNumber);
				It->stscIt.SetToSampleNumber(InAtSampleNumber);
				It->stssIt.SetToSampleNumber(InAtSampleNumber);
				It->rapIt.SetToSampleNumber(InAtSampleNumber);
				It->Update();
			}
			return It;
		}


		TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> FMP4Track::CreateIteratorAtKeyframe(FTimeValue InForTime, FTimeValue InLaterTimeThreshold)
		{
			check(bHasBeenPrepared);
			if (!bHasBeenPrepared || !SttsBox.IsValid() || SttsBox->GetTotalDuration()==0)
			{
				LastErrorMessage = FString::Printf(TEXT("Track has not been prepared, cannot create an iterator."));
				return nullptr;
			}
			if (!InForTime.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("Invalid time, cannot create an iterator."));
				return nullptr;
			}
			if (!Convs.DurationFromMdhdBox.IsValid() || !Convs.MappedDurationFromElstBox.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("Invalid track duration, cannot create an iterator."));
				return nullptr;
			}
			if (InForTime < FTimeValue::GetZero())
			{
				InForTime.SetToZero();
			}
			if (InLaterTimeThreshold < FTimeValue::GetZero())
			{
				InForTime.SetToZero();
			}
			const uint32 TrackTimescale = Convs.DurationFromMdhdBox.GetDenominator();
			int64 LocalTrackTime = InForTime.GetAsTimebase(TrackTimescale);
			// Clamp the time into the media time. The input may be larger than the media time, which is possible due to an
			// edit list mapping more content into the timeline than the media has. We need to find the frame in the media
			// though, so we clamp the time into the media time.
			LocalTrackTime = LocalTrackTime > Convs.DurationFromMdhdBox.GetNumerator() ? Convs.DurationFromMdhdBox.GetNumerator() : LocalTrackTime;
			int64 MaxLocalTrackTime = (InForTime + InLaterTimeThreshold).GetAsTimebase(TrackTimescale);

			// Shift the search time into the media timeline.
			LocalTrackTime += Convs.CompositionTimeAtZeroPoint;
			MaxLocalTrackTime += Convs.CompositionTimeAtZeroPoint;

			int64 ApproxSampleNumber = Convs.NumTotalSamples ? LocalTrackTime * (Convs.NumTotalSamples-1) / SttsBox->GetTotalDuration() : 0;
			check(ApproxSampleNumber <= 0xffffffff);


			TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> ApproxFrameIt(CreateIterator((uint32)ApproxSampleNumber));
			if (!ApproxFrameIt.IsValid())
			{
				LastErrorMessage = FString::Printf(TEXT("Failed to create track iterator for sample #%lld with %u samples in track"), (long long int)ApproxSampleNumber, Convs.NumTotalSamples);
				return nullptr;
			}
			// Move the approximate iterator backwards or forwards towards the target time.
			// This should not be off by much unless variable frame rate is used with greatly varying durations
			// or an edit list cuts off significant amounts of the media.
			if (ApproxFrameIt->GetPTS().GetNumerator() > LocalTrackTime)
			{
				for(; !ApproxFrameIt->IsFirst() && ApproxFrameIt->GetPTS().GetNumerator() > LocalTrackTime; ApproxFrameIt->Prev())
				{ }
			}
			else if (ApproxFrameIt->GetPTS().GetNumerator()+ApproxFrameIt->GetDuration().GetNumerator() <= LocalTrackTime)
			{
				for(; !ApproxFrameIt->IsLast() && ApproxFrameIt->GetPTS().GetNumerator()+ApproxFrameIt->GetDuration().GetNumerator() <= LocalTrackTime; ApproxFrameIt->Next())
				{ }
			}
			// Locate the nearest earlier sync sample, which might be the current one already.
			TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> SyncFrameIt(ApproxFrameIt->Clone());
			for(; !SyncFrameIt->IsFirst() && (SyncFrameIt->GetPTS().GetNumerator() > LocalTrackTime || !SyncFrameIt->IsSyncOrRAPSample()); SyncFrameIt->Prev())
			{ }
			TSharedPtr<FMP4Track::FIterator, ESPMode::ThreadSafe> NextSyncFrameIt(ApproxFrameIt->Clone());
			bool bLaterOneIsPossible = false;
			if (MaxLocalTrackTime > LocalTrackTime)
			{
				// Due to possible frame reordering we need to look at the DTS here with the composition offset applied
				// to be sure to find the correct sample. If we were to look at the PTS we could leave the loop to early.
				for(int64 MaxDtsWithComp=MaxLocalTrackTime+Convs.CompositionTimeAtZeroPoint; !NextSyncFrameIt->IsLast() && (NextSyncFrameIt->GetDTS().GetNumerator() <= MaxDtsWithComp && !NextSyncFrameIt->IsSyncOrRAPSample()); NextSyncFrameIt->Next())
				{ }
				bLaterOneIsPossible = NextSyncFrameIt->IsSyncOrRAPSample() && NextSyncFrameIt->GetPTS().GetNumerator() <= MaxLocalTrackTime;
			}
			// Did we even find any sync sample?
			if (!SyncFrameIt->IsSyncOrRAPSample() && !NextSyncFrameIt->IsSyncOrRAPSample())
			{
				LastErrorMessage = FString::Printf(TEXT("No sync sample found, cannot create an iterator."));
				return nullptr;
			}
			// If there is a possible later one to use we need to check if the earlier one is outside the threshold.
			if (bLaterOneIsPossible && LocalTrackTime - SyncFrameIt->GetPTS().GetNumerator() > MaxLocalTrackTime - LocalTrackTime)
			{
				return NextSyncFrameIt;
			}
			return SyncFrameIt;
		}

/****************************************************************************************************************************************************/
/****************************************************************************************************************************************************/
/****************************************************************************************************************************************************/

		void FMP4Track::FIterator::Update()
		{
			if (!IsValid())
			{
				return;
			}
			int64 DTS = sttsIt.GetCurrentTime() + Convs.BaseMediaDecodeTime;
			uint32 Duration = sttsIt.GetCurrentDuration();
			int64 CompositionTimeOffset = cttsIt.GetCurrentOffset();
			int64 PTS = DTS + CompositionTimeOffset;

			const uint32 Timescale = Convs.DurationFromMdhdBox.GetDenominator();
			CurrentDTS.SetFromND(DTS, Timescale);
			CurrentPTS.SetFromND(PTS, Timescale);
			CurrentEffectiveDTS.SetFromND(DTS - Convs.DTSShiftAtZeroPoint, Timescale);
			CurrentEffectivePTS.SetFromND(PTS - Convs.CompositionTimeAtZeroPoint, Timescale);
			// Set the duration as the fraction of the duration and the timescale.
			CurrentDuration.SetFromND(Duration, Timescale);
			// Also set the duration as the delta of the DTS of this sample and the next
			// in timespan units. This is to avoid transformation issues from media local
			// time into the timescale used in engine.
			CurrentDurationTS = FTimeFraction(DTS + Duration, Timescale).GetAsTimespan() - CurrentDTS.GetAsTimespan();
			CurrentSampleSize = stszIt.GetCurrentSampleSize();
			bCurrentIsSyncOrRAP = stssIt.IsSyncSample() || rapIt.GetCurrentGroupDescriptionIndex() != 0;

			// Which chunk is this sample in?
			uint32 chunkIndex = stscIt.GetCurrentChunkIndex();
			check(chunkIndex);
			uint64 chunkOffset = stcoIt.GetOffsetForChunkIndex(chunkIndex - 1);
			check(chunkOffset);
			// Which sample position within the current chunk run are we at?
			uint32 samplePosInChunk = stscIt.GetSampleIndexInCurrentChunk();
			// Giving us which sample number at the start of the chunk?
			uint32 sampleNumAtChunkStart = SampleNumber - samplePosInChunk;
			for(uint32 i=0; i<samplePosInChunk; ++i)
			{
				chunkOffset += stszIt.GetSampleSizeForSampleNum(sampleNumAtChunkStart + i);
			}
			CurrentSampleFileOffset = (int64) chunkOffset;
		}


		bool FMP4Track::FIterator::Next()
		{
			if (SampleNumber+1 < Convs.NumTotalSamples)
			{
				// Note: stcoIt is not an iterator, so there's nothing to call on it.
				verify(stszIt.Next());
				verify(sttsIt.Next());
				verify(cttsIt.Next());
				verify(stscIt.Next());
				verify(stssIt.Next());
				verify(rapIt.Next());
				++SampleNumber;
				Update();
				return true;
			}
			return false;
		}
		bool FMP4Track::FIterator::Prev()
		{
			if (SampleNumber > 0)
			{
				// Note: stcoIt is not an iterator, so there's nothing to call on it.
				verify(stszIt.Prev());
				verify(sttsIt.Prev());
				verify(cttsIt.Prev());
				verify(stscIt.Prev());
				verify(stssIt.Prev());
				verify(rapIt.Prev());
				--SampleNumber;
				Update();
				return true;
			}
			return false;
		}


		bool FMP4Track::FIterator::NextEffective()
		{
			// The last sample number is inclusive, that is, that sample is needed.
			if (SampleNumber+1 <= Convs.LastSample.LastSampleNumber)
			{
				// Note: stcoIt is not an iterator, so there's nothing to call on it.
				verify(stszIt.Next());
				verify(sttsIt.Next());
				verify(cttsIt.Next());
				verify(stscIt.Next());
				verify(stssIt.Next());
				verify(rapIt.Next());
				++SampleNumber;
				Update();
				return true;
			}
			return false;
		}
		bool FMP4Track::FIterator::PrevEffective()
		{
			if (SampleNumber > Convs.FirstSample.SyncSampleNumber)
			{
				// Note: stcoIt is not an iterator, so there's nothing to call on it.
				verify(stszIt.Prev());
				verify(sttsIt.Prev());
				verify(cttsIt.Prev());
				verify(stscIt.Prev());
				verify(stssIt.Prev());
				verify(rapIt.Prev());
				--SampleNumber;
				Update();
				return true;
			}
			return false;
		}

	} // namespace UtilitiesMP4

} // namespace Electra
