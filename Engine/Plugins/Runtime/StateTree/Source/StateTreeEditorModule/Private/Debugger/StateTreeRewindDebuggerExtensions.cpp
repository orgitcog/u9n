// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_TRACE_DEBUGGER

#include "Debugger/StateTreeRewindDebuggerExtensions.h"

#include "Debugger/StateTreeTrace.h"
#include "Debugger/StateTreeTraceProvider.h"
#include "StateTreeDelegates.h"
#include "Trace/Trace.h"

namespace UE::StateTreeDebugger
{

void FRewindDebuggerExtension::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	
	const IStateTreeTraceProvider* TraceProvider = AnalysisSession->ReadProvider<IStateTreeTraceProvider>(FStateTreeTraceProvider::ProviderName);

	const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*RewindDebugger->GetAnalysisSession());
	TraceServices::FFrame Frame;

	// Require some debug frame to exist before doing any processing, currently the frame itself is not used
	if (FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, RewindDebugger->CurrentTraceTime(), Frame))
	{
		auto SetScrubberTimeline = [&]()
		{
			if (RewindDebugger->IsPIESimulating())
			{
				return;
			}

			const double CurrentScrubTime = RewindDebugger->GetScrubTime();
			if (LastScrubTime != CurrentScrubTime)
			{
				UE::StateTree::Delegates::OnTracingTimelineScrubbed.Broadcast(CurrentScrubTime);
				LastScrubTime = CurrentScrubTime;
			}
		};
		SetScrubberTimeline();
	}
}

void FRewindDebuggerRuntimeExtension::RecordingStarted()
{ 
	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel))
	{
		UE::Trace::ToggleChannel(TEXT("StateTreeDebugChannel"), true);
		UE::StateTree::Delegates::OnTracingStateChanged.Broadcast(EStateTreeTraceStatus::TracesStarted);
	}
}

void FRewindDebuggerRuntimeExtension::RecordingStopped()
{
	// Shouldn't fire as channel will already be disabled when rewind debugger stops, but just as safeguard
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel))
	{
		UE::StateTree::Delegates::OnTracingStateChanged.Broadcast(EStateTreeTraceStatus::StoppingTrace);
		UE::Trace::ToggleChannel(TEXT("StateTreeDebugChannel"), false);
	}
}


} // namespace UE::StateTreeDebugger

#endif // WITH_STATETREE_TRACE_DEBUGGER