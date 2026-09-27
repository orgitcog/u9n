// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

namespace UE::Learning
{
	struct FFrameSet;

	/**
	 * A FFrameRangeSet represents a set of ranges within a set of sequences. This can be useful for encoding things like a collection of tags or 
	 * labels for different parts of some database of animations, replays, or recordings.
	 *
	 * This data structure stores that information in a way that makes performing operations such as union, intersection, difference etc, efficient.
	 * 
	 * The way it works is by storing a sorted array of "entries", where each entry has a corresponding sequence, the number of ranges in the set for
	 * that sequence, and an index into three large arrays of range starts, lengths, and offsets. The sub-ranges of the large arrays of range 
	 * starts and lengths are also sorted, while the offsets arrays stores the offset that would be used if you had to take a slice of a large flat 
	 * array of frame data.
	 *
	 * Having the entries sorted by sequence, and the ranges sorted, allows for efficient set operations via tape-merge algorithms.
	 *
	 * This data-structure is related to the FrameSet data-structure, which essentially acts the same way but stores individual frames instead of
	 * frame ranges.
	 */
	struct LEARNING_API FFrameRangeSet
	{
	public:

		/** Check if the FrameRangeSet is well-formed (i.e. correctly sorted without duplicate entries) */
		void Check() const;

		/**
		 * Adds the given ranges associated with the given sequence to the set. Assumes this sequence (and no sequences with a larger index)
		 * are already added to the set.
		 */
		void AddEntry(
			const int32 InSequence,
			const TLearningArrayView<1, const int32> InStarts,
			const TLearningArrayView<1, const int32> InLengths);

		/** True if the FrameRangeSet is Empty, otherwise false */
		bool IsEmpty() const;

		/** Empties the FrameRangeSet */
		void Empty();

		/** Gets the number of entries in the FrameRangeSet */
		int32 GetEntryNum() const;

		/** Returns an array of sequences for each entry */
		TLearningArrayView<1, const int32> GetEntrySequences() const;

		/** Returns an array for the number of ranges in each entry */
		TLearningArrayView<1, const int32> GetEntryRangeNums() const;

		/** Gets the sequence associated with a given entry */
		int32 GetEntrySequence(const int32 EntryIdx) const;

		/** Gets the number of ranges for a given entry */
		int32 GetEntryRangeNum(const int32 EntryIdx) const;

		/** Computes the total number of frames given by all ranges in an entry */
		int32 GetEntryTotalFrameNum(const int32 EntryIdx) const;

		/** Gets all the range starts associated with a given entry */
		TLearningArrayView<1, const int32> GetEntryRangeStarts(const int32 EntryIdx) const;

		/** Gets all the range lengths associated with a given entry */
		TLearningArrayView<1, const int32> GetEntryRangeLengths(const int32 EntryIdx) const;
		
		/** Gets all the range offsets associated with a given entry */
		TLearningArrayView<1, const int32> GetEntryRangeOffsets(const int32 EntryIdx) const;

		/** Gets the range start associated with a given entry and range index */
		int32 GetEntryRangeStart(const int32 EntryIdx, const int32 RangeIdx) const;

		/** Gets the range length associated with a given entry and range index */
		int32 GetEntryRangeLength(const int32 EntryIdx, const int32 RangeIdx) const;

		/** Computes the offset associated with a given entry and range index */
		int32 GetEntryRangeOffset(const int32 EntryIdx, const int32 RangeIdx) const;

		/** Gets the range start time associated with a given entry and range index */
		float GetEntryRangeStartTime(const int32 EntryIdx, const int32 RangeIdx, const float FrameDeltaTime) const;

		/** Gets the range end time associated with a given entry and range index */
		float GetEntryRangeEndTime(const int32 EntryIdx, const int32 RangeIdx, const float FrameDeltaTime) const;

		/** Gets the range duration associated with a given entry and range index */
		float GetEntryRangeDuration(const int32 EntryIdx, const int32 RangeIdx, const float FrameDeltaTime) const;

		/** Computes the total number of ranges across all entries */
		int32 GetTotalRangeNum() const;

		/** Gets an array of the range starts for all ranges */
		TLearningArrayView<1, const int32> GetAllRangeStarts() const;

		/** Gets an array of the range lengths for all ranges */
		TLearningArrayView<1, const int32> GetAllRangeLengths() const;

		/** Gets an array of the range offsets for all ranges */
		TLearningArrayView<1, const int32> GetAllRangeOffsets() const;

		/** Computes the total number of frames across all entries and ranges */
		int32 GetTotalFrameNum() const;

		/** Checks if this FrameRangeSet contains a given sequence */
		bool ContainsSequence(const int32 Sequence) const;

		/** Checks if this FrameRangeSet contains a given sequence and frame in that sequence */
		bool Contains(const int32 Sequence, const int32 Frame) const;

		/** Checks if this FrameRangeSet contains a given sequence and time in that sequence */
		bool ContainsTime(const int32 Sequence, const float Time, const float FrameDeltaTime) const;

		/** Finds the entry index associated with a given sequence */
		int32 FindSequenceEntry(const int32 Sequence) const;

		/** Finds the entry index and range index associated with a given sequence and frame in that sequence */
		bool Find(int32& OutEntryIdx, int32& OutRangeIdx, int32& OutRangeFrame, const int32 Sequence, const int32 Frame) const;

		/** Finds the entry index and range index associated with a range index into all ranges */
		bool FindTotalRange(int32& OutEntryIdx, int32& OutRangeIdx, const int32 TotalRangeIdx) const;

		/** Finds the entry index and range index associated with a given sequence and time in that sequence */
		bool FindTime(int32& OutEntryIdx, int32& OutRangeIdx, float& OutRangeTime, const int32 Sequence, const float Time, const float FrameDeltaTime) const;

		/** Finds entry index, frame index, and frame in that range, associated with some offset  */
		bool FindOffset(int32& OutEntryIdx, int32& OutRangeIdx, int32& OutRangeFrame, const int32 Offset) const;

	public:

		/** Array of sequences associated with each entry */
		TLearningArray<1, int32> EntrySequences;

		/** Array of offsets into the RangeStarts, RangeLengths, RangeOffsets arrays associated with each entry */
		TLearningArray<1, int32> EntryRangeOffsets;

		/** Array of the number of ranges associated with each entry */
		TLearningArray<1, int32> EntryRangeNums;

		/** Large array of all range starts for all entries, indexed using offsets from EntryRangeOffsets */
		TLearningArray<1, int32> RangeStarts;

		/** Large array of all range lengths for all entries, indexed using offsets from EntryRangeOffsets */
		TLearningArray<1, int32> RangeLengths;

		/** Large array of all range frame offsets for all entries, indexed using offsets from EntryRangeOffsets */
		TLearningArray<1, int32> RangeOffsets;
	};

	namespace FrameRangeSet
	{
		/** Checks if two frame range sets are equal */
		LEARNING_API bool Equal(const FFrameRangeSet& Lhs, const FFrameRangeSet& Rhs);

		/** Computes the union of a frame set and frame range set */
		LEARNING_API void Union(FFrameRangeSet& OutFrameRangeSet, const FFrameSet& FrameSet, const FFrameRangeSet& FrameRangeSet);

		/** Computes the intersection of a frame set and frame range set */
		LEARNING_API void Intersection(FFrameSet& OutFrameSet, const FFrameSet& FrameSet, const FFrameRangeSet& FrameRangeSet);

		/** Computes the difference of a frame set and frame range set */
		LEARNING_API void Difference(FFrameSet& OutFrameSet, const FFrameSet& FrameSet, const FFrameRangeSet& FrameRangeSet);

		/** Computes the difference of a frame range set and a frame set */
		LEARNING_API void Difference(FFrameRangeSet& OutFrameRangeSet, const FFrameRangeSet& FrameRangeSet, const FFrameSet& FrameSet);

		/** Computes the union of two frame range sets */
		LEARNING_API void Union(FFrameRangeSet& Out, const FFrameRangeSet& Lhs, const FFrameRangeSet& Rhs);

		/** Computes the intersection of two frame range sets */
		LEARNING_API void Intersection(FFrameRangeSet& Out, const FFrameRangeSet& Lhs, const FFrameRangeSet& Rhs);

		/** Computes the difference of two frame range sets */
		LEARNING_API void Difference(FFrameRangeSet& Out, const FFrameRangeSet& Lhs, const FFrameRangeSet& Rhs);

		/**
		 * Computes the intersection of two frame range sets while recording the flat frame offsets in the lhs and rhs, associated with each range
		 * that gets added to the output. The OutLhsOffsets and OutRhsOffsets outputs should be pre-allocated to at least the maximum of the
		 * number of ranges in Lhs and Rhs. Returns the number of ranges in the added to the output frame range set.
		 */
		LEARNING_API int32 IntersectionWithOffsets(
			FFrameRangeSet& Out, 
			TLearningArrayView<1, int32> OutLhsOffsets,
			TLearningArrayView<1, int32> OutRhsOffsets,
			const FFrameRangeSet& Lhs,
			const FFrameRangeSet& Rhs);

		/** Creates a new frame range set with the start of all ranges trimmed by the given number of frames */
		LEARNING_API void TrimStart(FFrameRangeSet& Out, const FFrameRangeSet& FrameRangeSet, const int32 TrimFrameNum);

		/** Creates a new frame range set with the end of all ranges trimmed by the given number of frames */
		LEARNING_API void TrimEnd(FFrameRangeSet& Out, const FFrameRangeSet& FrameRangeSet, const int32 TrimFrameNum);

		/** Creates a new frame range set with both the start and the end of all ranges trimmed by the given numbers of frames */
		LEARNING_API void Trim(FFrameRangeSet& Out, const FFrameRangeSet& FrameRangeSet, const int32 TrimStartFrameNum, const int32 TrimEndFrameNum);

		/** Creates a new frame range set with the start of all ranges padded by the given number of frames */
		LEARNING_API void PadStart(FFrameRangeSet& Out, const FFrameRangeSet& FrameRangeSet, const int32 PadFrameNum);

		/** Creates a new frame range set with the end of all ranges padded by the given number of frames */
		LEARNING_API void PadEnd(FFrameRangeSet& Out, const FFrameRangeSet& FrameRangeSet, const int32 PadFrameNum);

		/** Creates a new frame range set with both the start and the end of all ranges padded by the given numbers of frames */
		LEARNING_API void Pad(FFrameRangeSet& Out, const FFrameRangeSet& FrameRangeSet, const int32 PadStartFrameNum, int32 PadEndFrameNum);

		/** Makes a Frame Range Set consisting of single-frame ranges from a frame set */
		LEARNING_API void MakeFromFrameSet(FFrameRangeSet& OutFrameRangeSet, const FFrameSet& FrameSet);

		/** Create a frame set from all of the range starts of a given frame range set */
		LEARNING_API void MakeFrameSetFromRangeStarts(FFrameSet& OutFrameSet, const FFrameRangeSet& FrameRangeSet);

		/** Create a frame set from all of the range ends of a given frame range set */
		LEARNING_API void MakeFrameSetFromRangeEnds(FFrameSet& OutFrameSet, const FFrameRangeSet& FrameRangeSet);

		/** Trim a frame range set to just the period of time before the first occurrence of a frame from a given frame set */
		LEARNING_API void RangesBeforeFrameSet(FFrameRangeSet& OutFrameRangeSet, const FFrameRangeSet& FrameRangeSet, const FFrameSet& FrameSet);

		/** Trim a frame range set to just the period of time after the first occurrence of a frame from a given frame set */
		LEARNING_API void RangesAfterFrameSet(FFrameRangeSet& OutFrameRangeSet, const FFrameRangeSet& FrameRangeSet, const FFrameSet& FrameSet);

		/** Computes the entry indices associated with every range in the set */
		LEARNING_API void AllRangeEntries(
			TLearningArrayView<1, int32> OutRangeEntries,
			const FFrameRangeSet& FrameRangeSet);

		/** Computes the range indices associated with every range in the set */
		LEARNING_API void AllRangeIndices(
			TLearningArrayView<1, int32> OutRangeIndices,
			const FFrameRangeSet& FrameRangeSet);

		/** Computes the sequences associated with every range in the set */
		LEARNING_API void AllRangeSequences(
			TLearningArrayView<1, int32> OutRangeSequences,
			const FFrameRangeSet& FrameRangeSet);

		/** Computes the start times associated with every range in the set */
		LEARNING_API void AllRangeStartTimes(
			TLearningArrayView<1, float> OutRangeStartTimes,
			const FFrameRangeSet& FrameRangeSet,
			const float FrameDeltaTime);

		/** Computes the end times associated with every range in the set */
		LEARNING_API void AllRangeEndTimes(
			TLearningArrayView<1, float> OutRangeEndTimes,
			const FFrameRangeSet& FrameRangeSet,
			const float FrameDeltaTime);

		/** Computes the durations associated with every range in the set */
		LEARNING_API void AllRangeDurations(
			TLearningArrayView<1, float> OutRangeDurations,
			const FFrameRangeSet& FrameRangeSet,
			const float FrameDeltaTime);

		/** Iterates over every range in the set and calls the provided callback */
		LEARNING_API void ForEachRange(
			const FFrameRangeSet& FrameRangeSet,
			const TFunctionRef<void(
				const int32 TotalRangeIdx,
				const int32 EntryIdx,
				const int32 RangeIdx)> Body);

		/** Iterates over every range in the set in parallel and calls the provided callback */
		LEARNING_API void ParallelForEachRange(
			const FFrameRangeSet& FrameRangeSet,
			const TFunctionRef<void(
				const int32 TotalRangeIdx,
				const int32 EntryIdx,
				const int32 RangeIdx)> Body);
	}
}