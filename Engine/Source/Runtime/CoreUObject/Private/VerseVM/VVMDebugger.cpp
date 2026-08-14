// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMDebugger.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMFrame.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMNativeFunction.h"

static Verse::FDebugger* GDebugger = nullptr;

Verse::FDebugger* Verse::GetDebugger()
{
	return GDebugger;
}

void Verse::SetDebugger(FDebugger* Arg)
{
	StoreStoreFence();
	GDebugger = Arg;
	if (Arg)
	{
		FContext::AttachedDebugger();
	}
	else
	{
		FContext::DetachedDebugger();
	}
}

namespace Verse
{
COREUOBJECT_API extern FOpErr StopInterpreterSentry;

namespace
{
bool IsFalse(VValue Arg)
{
	return Arg.IsCell() && &Arg.AsCell() == GlobalFalsePtr.Get();
}
} // namespace

void Debugger::ForEachStackFrame(
	FRunningContext Context,
	const FOp& PC,
	VFrame& Frame,
	VTask& Task,
	const FNativeFrame* NativeFrame,
	TFunctionRef<void(const FLocation*, FFrame)> F)
{
	TWriteBarrier<VUniqueString> SelfName{Context, VUniqueString::New(Context, "Self")};
	auto Loop = [&](const FOp& PC, VFrame& Frame) {
		const FOp* J = &PC;
		for (VFrame* I = &Frame; I; I = I->CallerFrame.Get())
		{
			VUniqueString& FilePath = *I->Procedure->FilePath;
			if (FilePath.Num() == 0)
			{
				continue;
			}
			TArray<TTuple<TWriteBarrier<VUniqueString>, VValue>> Registers;
			VValue SelfValue = I->Registers[FRegisterIndex::SELF].Get(Context);
			V_DIE_IF_MSG(
				SelfValue.IsUninitialized(),
				"`Self` should have been bound by now for methods, and set to `GlobalFalse()` for functions. "
				"This indicates either a codegen issue, or a failure in `CallWithSelf`!");
			if (IsFalse(SelfValue))
			{
				Registers.Reserve(I->Procedure->NumRegisterNames);
			}
			else
			{
				Registers.Reserve(I->Procedure->NumRegisterNames + 1);
				Registers.Emplace(SelfName, I->Registers[FRegisterIndex::SELF].Get(Context));
			}
			for (auto K = I->Procedure->GetRegisterNamesBegin(), Last = I->Procedure->GetRegisterNamesEnd(); K != Last; ++K)
			{
				Registers.Emplace(K->Name, I->Registers[K->Index.Index].Get(Context));
			}
			FFrame DebuggerFrame{Context, *I->Procedure->Name, FilePath, ::MoveTemp(Registers)};
			if (J == &StopInterpreterSentry)
			{
				F(nullptr, ::MoveTemp(DebuggerFrame));
			}
			else
			{
				const FLocation* Location = I->Procedure->GetLocation(*J);
				F(Location, ::MoveTemp(DebuggerFrame));
			}
			J = I->CallerPC;
		}
	};
	Loop(PC, Frame);
	NativeFrame->WalkTaskFrames(&Task, [&Context, &Loop, &F](const FNativeFrame& Frame) {
		if (const VNativeFunction* Callee = Frame.Callee)
		{
			FFrame DebuggerFrame{Context, *Callee->Name};
			F(nullptr, ::MoveTemp(DebuggerFrame));
		}
		Loop(*Frame.CallerPC, *Frame.CallerFrame);
	});
}
} // namespace Verse

#endif
