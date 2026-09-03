// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::IoStore::HTTP
{

// {{{1 activity ...............................................................

#if IAS_HTTP_WITH_PERF

////////////////////////////////////////////////////////////////////////////////
class FStopwatch
{
public:
	uint64	GetInterval(uint32 i) const;
	void	SendStart()		{ Impl(0); }
	void	SendEnd()		{ Impl(1); }
	void	RecvStart()		{ Impl(2); }
	void	RecvEnd()		{ Impl(3); }

private:
	void	Impl(uint32 Index);
	uint64	Samples[4] = {};
	uint32	Counts[2] = {};
};

////////////////////////////////////////////////////////////////////////////////
uint64 FStopwatch::GetInterval(uint32 i) const
{
	if (i >= UE_ARRAY_COUNT(Samples) - 1)
	{
		return 0;
	}
	return Samples[i + 1] - Samples[i];
}

////////////////////////////////////////////////////////////////////////////////
void FStopwatch::Impl(uint32 Index)
{
	if (uint64& Out = Samples[Index]; Out == 0)
	{
		Out = FPlatformTime::Cycles64();
	}
	Counts[Index >> 1] += !(Index & 1);
}

#endif // IAS_HTTP_WITH_PERF

////////////////////////////////////////////////////////////////////////////////
static FLaneEstate* GActivityTraceEstate = LaneEstate_New({
	.Name = "Iax/Activity",
	.Group = "Iax",
	.Channel = GetIaxTraceChannel(),
	.Weight = 11,
});


////////////////////////////////////////////////////////////////////////////////
struct FResponseInternal
{
	FMessageOffsets Offsets;
	int32			ContentLength = 0;
	uint16			MessageLength;
	mutable int16	Code;
};

////////////////////////////////////////////////////////////////////////////////
struct alignas(16) FActivity
{
	enum class EState : uint8
	{
		None,
		Build,
		Send,
		RecvMessage,
		RecvStream,
		RecvContent,
		RecvDone,
		Completed,
		Cancelled,
		Failed,
		_Num,
	};

	FActivity*			Next = nullptr;
	int8				Slot = -1;
	EState				State = EState::None;
	uint8				IsKeepAlive : 1;
	uint8				NoContent : 1;
	uint8				bFollow30x : 1;
	uint8				bAllowChunked : 1;
	uint8				LengthScore : 3;
	uint8				_Unused : 1;
	uint32				StateParam = 0;
#if IAS_HTTP_WITH_PERF
	FStopwatch			Stopwatch;
#endif
	union {
		FHost*			Host;
		FIoBuffer*		Dest;
		const char*		ErrorReason;
	};
	UPTRINT				SinkParam;
	FTicketSink			Sink;
	FResponseInternal	Response;
	FBuffer				Buffer;
};

////////////////////////////////////////////////////////////////////////////////
static void Activity_ChangeState(FActivity* Activity, FActivity::EState InState, uint32 Param=0)
{
	Trace(Activity, ETrace::StateChange, uint32(InState));

	check(Activity->State != InState);
	Activity->State = InState;
	Activity->StateParam = Param;
}

////////////////////////////////////////////////////////////////////////////////
static int32 Activity_Rewind(FActivity* Activity)
{
	using EState = FActivity::EState;

	if (Activity->State == EState::Send)
	{
		Activity->StateParam = 0;
		return 0;
	}

	if (Activity->State == EState::RecvMessage)
	{
		Activity->Buffer.Resize(Activity->StateParam);
		Activity_ChangeState(Activity, EState::Send);
		return 1;
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////
static uint32 Activity_RemainingKiB(FActivity* Activity)
{
	if (Activity->State <= FActivity::EState::RecvStream)  return MAX_uint32;
	if (Activity->State >  FActivity::EState::RecvContent) return 0;

	uint32 ContentLength = uint32(Activity->Response.ContentLength);
	check(Activity->StateParam <= ContentLength);
	return (ContentLength - Activity->StateParam) >> 10;
}

////////////////////////////////////////////////////////////////////////////////
static void Activity_CallSink(FActivity* Activity)
{
	static uint32 Scope = LaneTrace_NewScope("Iax/Sink");
	FLaneTrace* Lane = LaneEstate_Lookup(GActivityTraceEstate, Activity);
	FLaneTraceScope _(Lane, Scope);

	FTicketStatus& SinkArg = *(FTicketStatus*)Activity;
	Activity->Sink(SinkArg);
}

////////////////////////////////////////////////////////////////////////////////
static FActivity* Activity_Alloc(uint32 BufferSize)
{
	BufferSize = (BufferSize + 15) & ~15;

	uint32 Size = BufferSize + sizeof(FActivity);
	auto* Activity = (FActivity*)FMemory::Malloc(Size, alignof(FActivity));

	new (Activity) FActivity();

	auto* Scratch = (char*)(Activity + 1);
	uint32 ScratchSize = BufferSize;
	Activity->Buffer = FBuffer(Scratch, ScratchSize);

	return Activity;
}

////////////////////////////////////////////////////////////////////////////////
static void Activity_Free(FActivity* Activity)
{
	Trace(Activity, ETrace::ActivityDestroy, 0);

	Activity->~FActivity();
	FMemory::Free(Activity);
}

////////////////////////////////////////////////////////////////////////////////
static void Activity_SetError(FActivity* Activity, const char* Reason, int32 Code=-1)
{
	Activity->IsKeepAlive = 0;
	Activity->ErrorReason = Reason;

	Code = (Code < 0) ? LastSocketResult() : Code;
	Activity_ChangeState(Activity, FActivity::EState::Failed, Code);
}

////////////////////////////////////////////////////////////////////////////////
static void Activity_SetError(FActivity* Activity, const FOutcome& Outcome)
{
	Activity_SetError(Activity, Outcome.GetMessage().GetData(), Outcome.GetErrorCode());
}

////////////////////////////////////////////////////////////////////////////////
static void Activity_SetScore(FActivity* Activity, uint32 ContentSizeEst)
{
	if (ContentSizeEst == 0)
	{
		Activity->LengthScore = 0;
		return;
	}

	uint32 ContentEstKiB = (ContentSizeEst + 1023) >> 10;
	ContentEstKiB |= 2;
	uint32 Pow2 = FMath::FloorLog2(uint32(ContentEstKiB));
	Pow2 = FMath::Min(Pow2, 7u);
	Activity->LengthScore = uint8(Pow2);
}



////////////////////////////////////////////////////////////////////////////////
static void Trace(const struct FActivity* Activity, ETrace Action, uint32 Param)
{
	if (Action == ETrace::ActivityCreate)
	{
		static uint32 ActScopes[8] = {};
		if (ActScopes[0] == 0)
		{
			ActScopes[0] = LaneTrace_NewScope("Iax/Activity");
			ActScopes[1] = LaneTrace_NewScope("Iax/Activity_2");
			ActScopes[2] = LaneTrace_NewScope("Iax/Activity_4");
			ActScopes[3] = LaneTrace_NewScope("Iax/Activity_8");
			ActScopes[4] = LaneTrace_NewScope("Iax/Activity_16");
			ActScopes[5] = LaneTrace_NewScope("Iax/Activity_32");
			ActScopes[6] = LaneTrace_NewScope("Iax/Activity_128");
			ActScopes[7] = LaneTrace_NewScope("Iax/Activity_256");
		}

		FLaneTrace* Lane = LaneEstate_Build(GActivityTraceEstate, Activity);
		LaneTrace_Enter(Lane, ActScopes[Activity->LengthScore]);
		return;
	}

	if (Action == ETrace::ActivityDestroy)
	{
		LaneEstate_Demolish(GActivityTraceEstate, Activity);
		return;
	}

	FLaneTrace* Lane = LaneEstate_Lookup(GActivityTraceEstate, Activity);

	if (Action == ETrace::StateChange)
	{
		static constexpr FAnsiStringView StateNames[] = {
			"Iax/None",
			"Iax/Build",
			"Iax/WaitForSocket",
			"Iax/WaitResponse",
			"Iax/RecvStream",
			"Iax/RecvContent",
			"Iax/RecvDone",
			"Iax/Completed",
			"Iax/Cancelled",
			"Iax/Failed",
		};
		static_assert(UE_ARRAY_COUNT(StateNames) == uint32(FActivity::EState::_Num));
		static uint32 StateScopes[UE_ARRAY_COUNT(StateNames)] = {};
		if (StateScopes[0] == 0)
		{
			for (int32 i = 0; FAnsiStringView Name : StateNames)
			{
				StateScopes[i++] = LaneTrace_NewScope(Name);
			}
		}

		uint32 Scope = StateScopes[Param];
		if (Param == uint32(FActivity::EState::Build))
		{
			LaneTrace_Enter(Lane, Scope);
		}
		else
		{
			LaneTrace_Change(Lane, Scope);
		}

		return;
	}
}

// }}}

} // namespace UE::IoStore::HTTP
