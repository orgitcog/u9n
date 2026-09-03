// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePlaybackManager.h"

#include "Channels/MovieSceneTimeWarpChannel.h"
#include "Misc/CoreMiscDefines.h"
#include "MovieScene.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSequence.h"

FMovieScenePlaybackManager::FMovieScenePlaybackManager()
{
}

FMovieScenePlaybackManager::FMovieScenePlaybackManager(UMovieSceneSequence* InSequence)
{
	Initialize(InSequence);
}

void FMovieScenePlaybackManager::Initialize(UMovieSceneSequence* InSequence)
{
	if (!ensure(InSequence))
	{
		return;
	}

	UMovieScene* MovieScene = InSequence->GetMovieScene();
	const EMovieSceneEvaluationType EvaluationType = MovieScene->GetEvaluationType();

	DisplayRate = MovieScene->GetDisplayRate();

	// Make our playback position work *only* in ticks. We will handle conversion to/from frames ourselves.
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	PlaybackPosition.SetTimeBase(TickResolution, TickResolution, EvaluationType);

	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	SequenceStartTick = UE::MovieScene::DiscreteInclusiveLower(PlaybackRange);
	SequenceEndTick = UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange);

	ResetPlaybackSettings();

	PlaybackPosition.Reset(SequenceStartTick);
}

void FMovieScenePlaybackManager::ResetPlaybackSettings()
{
	StartOffsetTicks = EndOffsetTicks = FFrameNumber(0);
	NumLoopsToPlay = 1;
	NumLoopsCompleted = 0;
	PlayRate = 1.0;
	PlayDirection = EPlayDirection::Forwards;
}

void FMovieScenePlaybackManager::Update(float InDeltaSeconds, FContexts& OutContexts)
{
	using namespace UE::MovieScene;

	if (PlaybackStatus != EMovieScenePlayerStatus::Playing)
	{
		return;
	}
	
	// Get the new time, advanced by InDeltaSeconds.
	const FFrameTime PreviousTick = PlaybackPosition.GetCurrentPosition();
	const FFrameTime DeltaTicks = PlaybackPosition.GetInputRate().AsFrameTime(
			(IsPlayingForward() ? InDeltaSeconds : (-InDeltaSeconds)) * PlayRate);
	const FFrameTime NextTick = PreviousTick + DeltaTicks;

	FFrameTime WarpedNextTick = NextTick;
	if (bTransformPlaybackTime)
	{
		if (TimeTransform.FindFirstWarpDomain() == ETimeWarpChannelDomain::PlayRate)
		{
			WarpedNextTick = TimeTransform.TransformTime(WarpedNextTick);
		}
	}

	InternalUpdateToTick(WarpedNextTick.RoundToFrame(), OutContexts);
}

void FMovieScenePlaybackManager::UpdateTo(const FFrameTime NextTime, FContexts& OutContexts)
{
	const FFrameTime NextTick = ConvertFrameTime(NextTime, DisplayRate, PlaybackPosition.GetInputRate());
	InternalUpdateToTick(NextTick.RoundToFrame(), OutContexts);
}

void FMovieScenePlaybackManager::InternalUpdateToTick(const FFrameNumber NextTick, FContexts& OutContexts)
{
	using namespace UE::MovieScene;

	// If we are stopped, just move the playhead to the given time, without generating evaluation contexts.
	if (PlaybackStatus == EMovieScenePlayerStatus::Stopped)
	{
		PlaybackPosition.Reset(NextTick);
		return;
	}

	// Check we have some loop counters that make sense.
	if (NumLoopsToPlay > 0 && !ensure(NumLoopsCompleted < NumLoopsToPlay))
	{
		PlaybackStatus = EMovieScenePlayerStatus::Stopped;
		PlaybackPosition.Reset(NextTick);
	}

	// Gather some information about this update.
	const bool bShouldJump = 
		(PlaybackStatus != EMovieScenePlayerStatus::Playing && PlaybackStatus != EMovieScenePlayerStatus::Scrubbing);

	const FFrameNumber EffectiveStartTick = SequenceStartTick + StartOffsetTicks;
	const FFrameNumber EffectiveEndTick = SequenceEndTick - EndOffsetTicks;
	const FFrameNumber EffectiveDurationTicks = FMath::Max(FFrameNumber(0), EffectiveEndTick - EffectiveStartTick);
	const FFrameNumber LastValidTick = GetLastValidTick();
	// IMPORTANT: we assume that LastValidTick is less than the duration (current implementation is 
	//			  duration minus one tick).

	const bool bIsPlayingForward = IsPlayingForward();
	const FFrameNumber LoopStartTick = bIsPlayingForward ? EffectiveStartTick : LastValidTick;
	const FFrameNumber LoopLastTick = bIsPlayingForward ? LastValidTick : EffectiveStartTick;
	// If the start/end offsets make the duration 0, we treat each loop as one frame long.
	const FFrameNumber LoopDurationTicks = FMath::Max(FFrameNumber(1), EffectiveDurationTicks);

	// Figure out if we crossed the loop-end boundary.
	if ((bIsPlayingForward && NextTick > LoopLastTick) ||
			((!bIsPlayingForward && NextTick < LoopLastTick)))
	{
		// Compute how many times we crossed the loop-end boundary.
		const FFrameNumber LoopRelativeTick = NextTick - LoopStartTick;
		const int32 NumLoopingsOver = FMath::Abs(LoopRelativeTick.Value) / LoopDurationTicks.Value;
		ensure(NumLoopingsOver > 0);

		// Massage this a bit with the following rules:
		//
		// 1) Don't go over the number of loops to play unless we're playing indefinitely.
		//
		// 2) Add an extra completed loop if we reached the end (as opposed to crossing it) because
		//    that should count as "completing a loop".
		//
		const int32 NumLoopsNewlyCompleted = ((NumLoopsToPlay > 0) ?
				FMath::Min(NumLoopingsOver, NumLoopsToPlay - NumLoopsCompleted) :
				NumLoopingsOver);

		// Play the last bit of the loop if we are looping and doing any sort of dissections.
		if (DissectLooping != EMovieSceneLoopDissection::None)
		{
			OutContexts.Add(
					FMovieSceneContext(PlaybackPosition.PlayTo(LoopLastTick), PlaybackStatus)
					.SetHasJumped(bShouldJump)
					);
		}

		// See if we need to generate more update ranges for the loops. This can happen if we had a
		// large delta-time, and the duration of a loop is pretty short (i.e. we could have looped 
		// several times in one update).
		if (NumLoopingsOver > 1)
		{
			const int32 ExtraLoops = NumLoopingsOver - 1;

			if (DissectLooping == EMovieSceneLoopDissection::DissectAll)
			{
				// If we dissect the looping, we add an explicit update for each loop, from start to end.
				if (!bPingPongPlayback)
				{
					for (int32 Index = 0; Index < ExtraLoops; ++Index)
					{
						PlaybackPosition.Reset(LoopStartTick);
						OutContexts.Add(
								FMovieSceneContext(PlaybackPosition.PlayTo(LoopLastTick), PlaybackStatus)
								.SetHasJumped(true)
								);
					}
				}
				else
				{
					ReversePlayDirection();

					for (int32 Index = 0; Index < ExtraLoops; ++Index)
					{
						if (IsPlayingForward())
						{
							PlaybackPosition.Reset(EffectiveStartTick);
							OutContexts.Add(
									FMovieSceneContext(PlaybackPosition.PlayTo(LastValidTick), PlaybackStatus)
									.SetHasJumped(true)
									);
						}
						else
						{
							PlaybackPosition.Reset(LastValidTick);
							OutContexts.Add(
									FMovieSceneContext(PlaybackPosition.PlayTo(EffectiveStartTick), PlaybackStatus)
									.SetHasJumped(true)
									);
						}

						ReversePlayDirection();
					}
				}
			}
			else
			{
				// We are not dissecting loops so don't emit extra evaluation contexts.
				// If we are ping-pong'ing, we at least need to keep track of which way we are now going.
				if (bPingPongPlayback && (ExtraLoops + 1) % 2 != 0)
				{
					ReversePlayDirection();
				}
			}
		}
		else if (bPingPongPlayback)
		{
			ReversePlayDirection();
		}

		// Complete the loops we said we completed.
		NumLoopsCompleted += NumLoopsNewlyCompleted;

		if (NumLoopsToPlay > 0 && NumLoopsCompleted >= NumLoopsToPlay)
		{
			// If we have played all the loops we needed to play, we can stop playback.
			// However, if we don't dissect looping, we didn't finish the loop, so do it now.
			if (DissectLooping == EMovieSceneLoopDissection::None)
			{
				OutContexts.Add(
						FMovieSceneContext(PlaybackPosition.PlayTo(LoopLastTick), PlaybackStatus)
						.SetHasJumped(bShouldJump)
						);
			}

			PlaybackStatus = EMovieScenePlayerStatus::Stopped;
		}
		else
		{
			// Start the next loop with any overplay from the update.
			//
			// When playing forward, we have, e.g.:
			// loop = [-30, -10], next time = -5, relative time = 25, mod(25, 20) = 5
			//
			// When playing backwards, we have, e.g.:
			// loop = [-30, -10], next time = -35, relative time = -25, mod(-25, 20) = -5
			const FFrameNumber OverplayTicks = LoopRelativeTick.Value % LoopDurationTicks.Value;

			// Don't use LoopStartTick/LoopLastTick here because if we are ping-pong'ing, we would
			// be going the other way. Use the most recent value of PlayDirection (via IsPlayingForward())
			// for the same reason.
			// Also, reverse OverplayTicks when ping-pong'in since we're playing this overplay in the
			// reverse direction.
			const FFrameTime EffectiveOverplayTicks = 
				(IsPlayingForward() ? EffectiveStartTick : LastValidTick) + 
				((!bPingPongPlayback) ? OverplayTicks : (-OverplayTicks));
			PlaybackPosition.Reset(IsPlayingForward() ? EffectiveStartTick : LastValidTick);
			OutContexts.Add(
					FMovieSceneContext(PlaybackPosition.PlayTo(EffectiveOverplayTicks), PlaybackStatus)
					.SetHasJumped(true)
					);

			// If the overplay leads us exactly to the last tick of the loop, let's count that
			// as a completed loop... but only if that finishes playback. Otherwise, we'll wait
			// for the next update to loop over in order to avoid counting that loop twice.
			if (EffectiveOverplayTicks == LoopLastTick 
					&& NumLoopsToPlay > 0
					&& NumLoopsCompleted == NumLoopsToPlay - 1)
			{
				++NumLoopsCompleted;

				PlaybackStatus = EMovieScenePlayerStatus::Stopped;
			}
		}
	}
	else
	{
		// We haven't crossed a loop-end boundary... just chug along.
		OutContexts.Add(
				FMovieSceneContext(PlaybackPosition.PlayTo(NextTick), PlaybackStatus)
				.SetHasJumped(bShouldJump)
				);

		// If we were updated right up to the last tick of the loop, let's count that as a completed
		// loop... but only if that finishes playback. Otherwise, we'll wait for the next update to
		// loop over in order to avoid counting that loop twice.
		if (NextTick == LoopLastTick
				&& NumLoopsToPlay > 0
				&& NumLoopsCompleted == NumLoopsToPlay - 1)
		{
			++NumLoopsCompleted;

			PlaybackStatus = EMovieScenePlayerStatus::Stopped;
		}
	}

	ensure(OutContexts.Num() > 0);
	ensure(OutContexts.Num() == 1 || (DissectLooping != EMovieSceneLoopDissection::None));
}

FFrameNumber FMovieScenePlaybackManager::GetLastValidTick() const
{
	// TODO: handle precision problems with float SubFrame, or change SubFrame to double.
	return SequenceEndTick - EndOffsetTicks - 1;  // minus one tick for exclusive end frame.
}

FMovieSceneContext FMovieScenePlaybackManager::UpdateAtCurrentTime() const
{
	return FMovieSceneContext(PlaybackPosition.GetCurrentPositionAsRange(), PlaybackStatus);
}

FFrameTime FMovieScenePlaybackManager::GetCurrentTime() const
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	return ConvertFrameTime(PlaybackPosition.GetCurrentPosition(), TickResolution, DisplayRate);
}

void FMovieScenePlaybackManager::SetCurrentTime(const FFrameTime& InFrameTime)
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	const FFrameNumber CurrentTick = ConvertFrameTime(InFrameTime, DisplayRate, TickResolution).RoundToFrame();
	PlaybackPosition.Reset(CurrentTick);
}

TRange<FFrameTime> FMovieScenePlaybackManager::GetEffectivePlaybackRange() const
{
	const FFrameNumber StartTick = SequenceStartTick + StartOffsetTicks;
	const FFrameNumber EndTick = SequenceEndTick - EndOffsetTicks;

	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();

	const FFrameTime StartFrame = ConvertFrameTime(StartTick, TickResolution, DisplayRate);
	const FFrameTime EndFrame = ConvertFrameTime(EndTick, TickResolution, DisplayRate);

	return TRange<FFrameTime>(TRangeBound<FFrameTime>::Inclusive(StartFrame), TRangeBound<FFrameTime>::Exclusive(EndFrame));
}

FFrameTime FMovieScenePlaybackManager::GetEffectiveStartTime() const
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	return ConvertFrameTime(SequenceStartTick + StartOffsetTicks, TickResolution, DisplayRate);
}

FFrameTime FMovieScenePlaybackManager::GetEffectiveEndTime() const
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	return ConvertFrameTime(SequenceEndTick + EndOffsetTicks, TickResolution, DisplayRate);
}

void FMovieScenePlaybackManager::SetStartOffset(const FFrameTime& InStartOffset)
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	const FFrameNumber InStartOffsetTicks = ConvertFrameTime(InStartOffset, DisplayRate, TickResolution).RoundToFrame();

	SetStartAndEndOffsetTicks(InStartOffsetTicks, EndOffsetTicks);
}

void FMovieScenePlaybackManager::SetEndOffset(const FFrameTime& InEndOffset)
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	const FFrameNumber InEndOffsetTicks = ConvertFrameTime(InEndOffset, DisplayRate, TickResolution).RoundToFrame();

	SetStartAndEndOffsetTicks(StartOffsetTicks, InEndOffsetTicks);
}

void FMovieScenePlaybackManager::SetEndOffsetAsTime(const FFrameTime& InEndTime)
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	const FFrameNumber InEndTick = ConvertFrameTime(InEndTime, DisplayRate, TickResolution).RoundToFrame();

	const FFrameNumber InEndOffsetTicks = SequenceEndTick - InEndTick;
	SetStartAndEndOffsetTicks(StartOffsetTicks, InEndOffsetTicks);
}

void FMovieScenePlaybackManager::SetStartAndEndOffsetTicks(FFrameNumber InStartOffsetTicks, FFrameNumber InEndOffsetTicks)
{
	const FFrameNumber SequenceDurationTicks = SequenceEndTick - SequenceStartTick;

	StartOffsetTicks = FMath::Min(
			FMath::Max(InStartOffsetTicks, FFrameNumber(0)),
			SequenceDurationTicks);

	EndOffsetTicks = FMath::Min(
			FMath::Max(InEndOffsetTicks, FFrameNumber(0)),
			SequenceDurationTicks - StartOffsetTicks);
}

FFrameTime FMovieScenePlaybackManager::GetStartOffset() const
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	return ConvertFrameTime(StartOffsetTicks, TickResolution, DisplayRate);
}

FFrameTime FMovieScenePlaybackManager::GetEndOffset() const
{
	const FFrameRate TickResolution = PlaybackPosition.GetOutputRate();
	return ConvertFrameTime(EndOffsetTicks, TickResolution, DisplayRate);
}

void FMovieScenePlaybackManager::SetNumLoopsToPlay(int32 InNumLoopsToPlay)
{
	NumLoopsToPlay = InNumLoopsToPlay;
}

void FMovieScenePlaybackManager::ReversePlayDirection()
{
	if (PlayDirection == EPlayDirection::Forwards)
	{
		PlayDirection = EPlayDirection::Backwards;
	}
	else
	{
		PlayDirection = EPlayDirection::Forwards;
	}
}

void FMovieScenePlaybackManager::SetPingPongPlayback(bool bInPingPongPlayback)
{
	bPingPongPlayback = bInPingPongPlayback;
}

