// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMFailureContext.h"
#include "VerseVM/VVMFrame.h"
#include "VerseVM/VVMSamplingProfiler.h"
#include "VerseVM/VVMTask.h"

namespace Verse
{
// The entry point that all `C++` calls into Verse should go through
// Returns false if a runtime error occured
//
// Style note: Try not to split this function out into other calls too much...
// we want anyone who reads this to easily be able to grasp the logic-flow.
template <typename TFunctor>
bool FRunningContext::EnterVM_Internal(TFunctor F, EEnterVMMode Mode)
{
	V_DIE_UNLESS(!AutoRTFM::IsClosed());
	FContextImpl* Impl = GetImpl();
	const bool bTopLevel = Impl->_NativeFrame == nullptr;

	if (bTopLevel)
	{
		Impl->StartComputationWatchdog();
		if (FSamplingProfiler* Sampler = GetSamplingProfiler())
		{
			Sampler->SetMutatorContext(this);
			Sampler->Start();
		}
	}

	auto SetupAndRun = [&]() {
		COREUOBJECT_API extern FOpErr StopInterpreterSentry;
		if (bTopLevel)
		{
			// We need to create a 'root' frame as this call into the interpreter could either from the top level or from
			// some C++ that Verse called into higher up the stack. So, this frame represents that top level C++
			VTask* Task = &VTask::New(*this, &StopInterpreterSentry, VFrame::GlobalEmptyFrame.Get(), /*YieldTask*/ nullptr, /*Parent*/ nullptr);
			VFailureContext* FailureContext = &VFailureContext::New(*this, Task, nullptr, *VFrame::GlobalEmptyFrame.Get(), VValue(), &StopInterpreterSentry);
			FNativeFrame NewNativeFrame{.FailureContext = FailureContext, .Task = Task, .CallerPC = nullptr, .CallerFrame = nullptr, .PreviousNativeFrame = nullptr};
			TGuardValue<FNativeFrame*> NativeFrameGuard(Impl->_NativeFrame, &NewNativeFrame);

			NewNativeFrame.Start(*this);
			F();
			NewNativeFrame.CommitIfNoAbort(*this);
		}
		else if (Mode == EEnterVMMode::NewTransaction)
		{
			// Push a new transaction onto our frame to match the new AutoRTFM transaction which this enum tells us we've pushed already
			const FNativeFrame* NativeFrame = GetImpl()->NativeFrame();
			VFailureContext* NewFailureContext = &VFailureContext::New(*this, NativeFrame->Task, NativeFrame->FailureContext, *VFrame::GlobalEmptyFrame.Get(), VValue(), &StopInterpreterSentry);
			TGuardValue<VFailureContext*> FailureContextGuard(Impl->_NativeFrame->FailureContext, NewFailureContext);

			Impl->_NativeFrame->Start(*this);
			F();
			Impl->_NativeFrame->CommitIfNoAbort(*this);
		}
		else
		{
			F();
		}
	};

	if (AutoRTFM::IsTransactional())
	{
		SetupAndRun();
	}
	else
	{
		// `NativeContext.Start` calls `AutoRTFM::ForTheRuntime::StartTransaction()`
		// which can't start a new transaction stack, so we need to make one first.
		AutoRTFM::TransactThenOpen([&] { SetupAndRun(); });
	}

	if (bTopLevel)
	{
		Impl->PauseComputationWatchdog();
		if (FSamplingProfiler* Sampler = GetSamplingProfiler())
		{
			Sampler->Pause();
		}
	}

	return AutoRTFM::ForTheRuntime::GetContextStatus() != AutoRTFM::EContextStatus::AbortedByCascadingAbort;
}

} // namespace Verse
#endif // WITH_VERSE_VM