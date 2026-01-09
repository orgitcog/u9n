// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utilities/UtilitiesMP4.h"
#include "Utilities/MP4Boxes/MP4Boxes.h"
#include "Utilities/MP4Boxes/MP4BoxIterators.h"
#include "Utilities/MP4Boxes/MP4BoxMetadata.h"

namespace Electra
{
	namespace UtilitiesMP4
	{
		class FMP4Track : public TSharedFromThis<FMP4Track>
		{
		public:
			struct FFirstSample
			{
				int64 SamplePTS = 0;
				int64 StartPTS = 0;
				uint32 SampleNumber = 0;
				uint32 SyncSampleNumber = 0;
			};

			struct FLastSample
			{
				int64 SamplePTS = 0;
				int64 EndPTS = 0;
				uint32 SampleNumber = 0;
				uint32 LastSampleNumber = 0;
			};

		protected:
			struct FConvenience
			{
				FFirstSample FirstSample;
				FLastSample LastSample;
				FTimeFraction FullMovieDuration;
				FTimeFraction DurationFromMvhdBox;
				FTimeFraction DurationFromTkhdBox;
				FTimeFraction DurationFromMdhdBox;
				FTimeFraction MappedDurationFromElstBox;
				int64 BaseMediaDecodeTime = 0;
				int64 CompositionTimeAtZeroPoint = 0;
				int64 DTSShiftAtZeroPoint = 0;
				uint32 TrackID = 0;
				uint32 NumTotalSamples = 0;
			};

		public:
			static TSharedPtr<FMP4Track, ESPMode::ThreadSafe> Create(const TSharedPtr<FMP4BoxTRAK, ESPMode::ThreadSafe>& InTrakBox)
			{ return MakeShareable<FMP4Track>(new FMP4Track(InTrakBox)); }
			virtual ~FMP4Track() = default;

			ELECTRABASE_API bool Prepare(FTimeFraction InFullMovieDuration, FTimeFraction InAdjustedMovieDuration);
			ELECTRABASE_API FString GetLastError();
			ELECTRABASE_API const FMP4TrackMetadataCommon& GetCommonMetadata();

			class FIterator
			{
			public:
				virtual ~FIterator() = default;

				bool IsValid() const
				{ return Convs.NumTotalSamples != 0; }
				TWeakPtr<FMP4Track, ESPMode::ThreadSafe> GetTrack() const
				{ return Track; }
				uint32 GetTrackID() const
				{ return Convs.TrackID; }
				uint32 GetSampleNumber() const
				{ return SampleNumber; }
				// Returns the DTS without mapping to the timeline.
				FTimeFraction GetDTS() const
				{ return CurrentDTS; }
				// Returns the effective DTS, which has the timeline mapping applied. This may result in a negative value.
				FTimeFraction GetEffectiveDTS() const
				{ return CurrentEffectiveDTS; }
				// Returns the PTS as the sum of the DTS and the composition time offset, without mapping to the timeline.
				FTimeFraction GetPTS() const
				{ return CurrentPTS; }
				// Returns the effective PTS, which is the media time mapped into the 0-based timeline.
				FTimeFraction GetEffectivePTS() const
				{ return CurrentEffectivePTS; }
				FTimeFraction GetDuration() const
				{ return CurrentDuration; }
				// Returns the duration as an FTimespan, which may be slightly more accurate than as a fraction.
				FTimespan GetDurationAsTimespan() const
				{ return CurrentDurationTS; }
				bool IsSyncOrRAPSample() const
				{ return bCurrentIsSyncOrRAP; }
				int64 GetSampleSize() const
				{ return CurrentSampleSize; }
				int64 GetSampleFileOffset() const
				{ return CurrentSampleFileOffset; }
				uint32 GetTimescale() const
				{ return Convs.DurationFromMdhdBox.GetDenominator(); }
				uint32 GetNumSamples() const
				{ return Convs.NumTotalSamples; }
				// Returns the track's entire media duration, not affected by an edit list. Timescale comes from `mdhd` box.
				FTimeFraction GetTrackDuration() const
				{ return Convs.DurationFromMdhdBox; }
				// Returns the effective track's duration, as specified by an edit list. Timescale has been converted into `mdhd` timescale!
				FTimeFraction GetEffectiveTrackDuration() const
				{ return Convs.MappedDurationFromElstBox; }

				// Returns information about the first sample that is mapped to the 0-based timeline via `elst` box.
				const FFirstSample& GetFirstSampleInfo() const
				{ return Convs.FirstSample; }
				// Returns information about the last sample that is mapped to the 0-based timeline via `elst` box.
				const FLastSample& GetLastSampleInfo() const
				{ return Convs.LastSample; }

				// Advances this iterator to the next sample. Returns true if there is one, false if not.
				// This iterates over the entire track, ignoring timeline mapping.
				ELECTRABASE_API bool Next();

				// Recedes this iterator to the previous sample. Returns true if there is one, false if not.
				// This iterates over the entire track, ignoring timeline mapping.
				ELECTRABASE_API bool Prev();

				// Returns whether the iterator points to the first overall sample, ignoring mapping to the timeline.
				bool IsFirst() const
				{ return SampleNumber == 0; }
				// Returns whether the iterator points to the last overall sample, ignoring mapping to the timeline.
				bool IsLast() const
				{ return SampleNumber+1 >= Convs.NumTotalSamples; }

				// Same as above, but obeying the timeline mapping and taking into consideration
				// any required earlier sync frame and later frames due to reordering.
				ELECTRABASE_API bool NextEffective();
				ELECTRABASE_API bool PrevEffective();
				bool IsFirstEffective() const
				{ return SampleNumber <= Convs.FirstSample.SyncSampleNumber; }
				bool IsLastEffective() const
				{ return SampleNumber >= Convs.LastSample.LastSampleNumber; }

				// Creates a copy of this iterator.
				TSharedPtr<FIterator, ESPMode::ThreadSafe> Clone()
				{ return MakeShareable(new FIterator(*this)); }
			protected:
				FIterator() = default;
				FIterator(const FIterator& InOther) = default;
				friend class FMP4Track;

				void Update();

				TWeakPtr<FMP4Track, ESPMode::ThreadSafe> Track;
				FConvenience Convs;
				FSTSZBoxIterator stszIt;
				FSTTSBoxIterator sttsIt;
				FCTTSBoxIterator cttsIt;
				FSTSCBoxIterator stscIt;
				FSTSSBoxIterator stssIt;
				FSTCOBoxIterator stcoIt;
				FSBGPBoxIterator rapIt;
				uint32 SampleNumber = 0;

				FTimeFraction CurrentDTS;
				FTimeFraction CurrentPTS;
				FTimeFraction CurrentEffectiveDTS;
				FTimeFraction CurrentEffectivePTS;
				FTimeFraction CurrentDuration;
				FTimespan CurrentDurationTS;
				int64 CurrentSampleFileOffset = 0;
				int64 CurrentSampleSize = 0;
				bool bCurrentIsSyncOrRAP = false;
			};

			// Create an interator starting at the first sample.
			ELECTRABASE_API TSharedPtr<FIterator, ESPMode::ThreadSafe> CreateIterator();
			// Create an interator starting at the last sample (used when iterating in reverse, crossing back from the beginning to the end)
			ELECTRABASE_API TSharedPtr<FIterator, ESPMode::ThreadSafe> CreateIteratorAtLastFrame();
			// Create an iterator starting at a keyframe on or before the given time, or at a later time within the given
			// threshold should one be right after the given time and would not be selected due to timescale rounding issues.
			ELECTRABASE_API TSharedPtr<FIterator, ESPMode::ThreadSafe> CreateIteratorAtKeyframe(FTimeValue InForTime, FTimeValue InLaterTimeThreshold=FTimeValue::GetZero());
			// Create an interator starting at a given sample number.
			ELECTRABASE_API TSharedPtr<FIterator, ESPMode::ThreadSafe> CreateIterator(uint32 InAtSampleNumber);

			// Returns the number of samples in this track.
			ELECTRABASE_API uint32 GetNumberOfSamples();

			// Returns information about the first sample that is mapped to the 0-based timeline via `elst` box.
			const FFirstSample& GetFirstSampleInfo() const
			{ return Convs.FirstSample; }
			// Returns information about the last sample that is mapped to the 0-based timeline via `elst` box.
			const FLastSample& GetLastSampleInfo() const
			{ return Convs.LastSample; }

			// Returns the duration of the movie as a whole, which is set from the longest track.
			const FTimeFraction& GetFullMovieDuration() const
			{ return Convs.FullMovieDuration; }

		protected:
			FMP4Track(const TSharedPtr<FMP4BoxTRAK, ESPMode::ThreadSafe>& InTrakBox) : TrakBox(InTrakBox)
			{ }

			TSharedPtr<FMP4BoxTRAK, ESPMode::ThreadSafe> TrakBox;

			TSharedPtr<FMP4BoxTKHD, ESPMode::ThreadSafe> TkhdBox;
			TSharedPtr<FMP4BoxELST, ESPMode::ThreadSafe> ElstBox;
			TSharedPtr<FMP4BoxMDHD, ESPMode::ThreadSafe> MdhdBox;
			TSharedPtr<FMP4BoxSTTS, ESPMode::ThreadSafe> SttsBox;
			TSharedPtr<FMP4BoxCTTS, ESPMode::ThreadSafe> CttsBox;
			TSharedPtr<FMP4BoxSTSC, ESPMode::ThreadSafe> StscBox;
			TSharedPtr<FMP4BoxSTSZ, ESPMode::ThreadSafe> StszBox;
			TSharedPtr<FMP4BoxSTCO, ESPMode::ThreadSafe> StcoBox;
			TSharedPtr<FMP4BoxSTSS, ESPMode::ThreadSafe> StssBox;
			TSharedPtr<FMP4BoxUDTA, ESPMode::ThreadSafe> UdtaBox;

			TArray<TSharedPtr<FMP4BoxSGPD, ESPMode::ThreadSafe>> SgpdBoxes;
			TArray<TSharedPtr<FMP4BoxSBGP, ESPMode::ThreadSafe>> SbgpBoxes;

			FConvenience Convs;

			FMP4TrackMetadataCommon CommonMetadata;

			bool bHasBeenPrepared = false;

			FString LastErrorMessage;
		};

	} // namespace UtilitiesMP4

} // namespace Electra
