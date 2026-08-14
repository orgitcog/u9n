// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "AutoRTFM.h"
#include "Containers/StringConv.h"
#include "Containers/Utf8String.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h"
#include "UObject/UnrealType.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMNativeConstructorWrapperInline.h"
#include "VerseVM/Inline/VVMScopeInline.h"
#include "VerseVM/Inline/VVMUniqueStringInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/Inline/VVMValueObjectInline.h"
#include "VerseVM/Inline/VVMVarInline.h"
#include "VerseVM/Inline/VVMVerseClassInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMArrayBase.h"
#include "VerseVM/VVMBytecode.h"
#include "VerseVM/VVMBytecodeOps.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMDebugger.h"
#include "VerseVM/VVMFailureContext.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMFrame.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMGlobalHeapPtr.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMOption.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMProfilingLibrary.h"
#include "VerseVM/VVMPropertyInlineCache.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMSamplingProfiler.h"
#include "VerseVM/VVMSuspension.h"
#include "VerseVM/VVMTask.h"
#include "VerseVM/VVMUniqueString.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMValuePrinting.h"
#include "VerseVM/VVMVar.h"
#include "VerseVM/VVMVerseException.h"
#include <stdio.h>

static_assert(UE_AUTORTFM, "New VM depends on AutoRTFM.");

namespace Verse
{

// The Interpreter is organized into two main execution loops: the main loop and the suspension loop.
// The main loop works like a normal interpreter loop. Control flow falls through from one bytecode
// to the next. We also have jump instructions which can divert control flow. However, since Verse
// also has failure, the bytecode has support for any bytecode that fails jumping to the current
// failure context's "on fail" bytecode destination. The way this works is that the BeginFailureContext
// and EndFailureContext bytecodes form a pair. The BeginFailureContext specifies where to jump to in
// the event of failure. Notably, if failure doesn't happen, the EndFailureContext bytecode must execute.
// This means that BeginFailureContext and EndFailureContext should be control equivalent -- we can't
// have jumps that jump over an EndFailureContext bytecode from within the failure context range.
//
// The bytecode also has builtin support for Verse's lenient execution model. This support is fundamental
// to the execution model of the bytecode. Bytecode instructions can suspend when a needed input
// operand is not concrete -- it's a placeholder -- and then resume execution when the input operand
// becomes concrete. Bytecode suspensions will capture their input operands and use the captured operands
// when they resume execution. When a placeholder becomes concrete unlocking a suspension, that suspension
// will execute in the suspension interpreter loop. The reason bytecode suspensions capture their input
// operands is so that those bytecode frame slots can be reused by the rest of the bytecode program.
// Because the operands aren't reloaded from the frame, and instead from the suspension, our bytecode
// generator can have a virtual register allocation algorithm that doesn't need to take into account
// liveness constraints dictated by leniency. This invariant has interesting implications executing a
// failure context leniently. In that scenario, we need to capture everything that's used both in the
// then/else branch. (For now, we implement this by just cloning the entire frame.) It's a goal to
// share as much code as we can between the main and suspension interpreter loops. That's why there
// are overloaded functions and interpreter-loop-specific macros that can handle both bytecode
// structs and suspension captures.
//
// Because of leniency, the interpreter needs to be careful about executing effects in program order. For
// example, if you have two effectful bytecodes one after the other, and the first one suspends, then the
// second one can't execute until the first one finishes. To handle this, we track an effect token that we
// thread through the program. Effectful operations will require the effect token to be concrete. They only
// execute after the token is concrete. Effectful operations always define a new non-concrete effect token.
// Only after the operation executes will it set the effect token to be concrete.
//
// Slots in the bytecode are all unification variables in support of Verse's general unification variable
// semantics. In our runtime, a unification variable is either a normal concrete value or a placeholder.
// A placeholder is used to support leniency. A placeholder can be used to unify two non-concrete variables.
// A placeholder can also point at a list of suspensions to fire when it becomes concrete. And finally, a
// placeholder can be mutated to point at a concrete value. When the runtime mutates a placeholder to
// point at a concrete value, it will fire its list of suspensions.
//
// Logically, a bytecode frame is initialized with empty placeholders. Every local variable in Verse is a
// unification variable. However, we really want to avoid this placeholder allocation for every local. After
// all, most locals will be defined before they're used. We optimize this by making these slots VRestValue
// instead of VPlaceholder. A VRestValue can be thought of a promise to produce a VPlaceholder if it's used
// before it has a concretely defined value. However, if we define a value in a bytecode slot before it's
// used, we can elide the allocation of the VPlaceholder altogether.

/*
  # Object archetype construction semantics
  ## Basic terminology

  A class **constructor** contains the bytecode of its body (including field initializers, `block`s, `let`s, etc.).  A
  **constructor** represents a similar thing for the body of constructor functions. These are also referred to as **body
  worker functions**.

  An **archetype** is a data structure that just represents the fields that can be initialized by a constructor/body
  worker function, along with storing the type of each field. We use this for determining the shape of an object and
  which fields' data will live in the object versus living in the shape; this tells us how to allocate the memory for
  said object.


  ## Constructors, delegating constructors, and side effects

  Constructors can forward to other constructors (this is also referred to as a _delegating constructor_).
  The semantics here are illustrated in the following examples:

  A basic example:

  ```
  c1 := class { A:int = 1 }

  MakeC1_1<constructor>():= c1:
	  block{SideEffectB()}
	  A := 3 # This doesn't unify because `MakeC1` already initializes `A`
	  block{SideEffectC()}}

  MakeC1<constructor>():= c1:
	  A := 2
	  block{SideEffectA()}
	  MakeC1_1<constructor>() # This is to a call of `c1`, which executes as per-normal.

  O:= MakeC1()
  O.A = 2
  # The order of the side effects are as indicated by the function names in lexicographical order.
  ```
  The side effects execute in the order `SideEffectA`, then `SideEffectB`, and then finally `SideEffectC`.

  However, in the case of a delegating constructor to a base class, the semantics are slightly different. In such a
  case, before the call to the delegating constructor, the current class's body is run first, along with any side
  effects that it produces, _then_ the call to the delegating constructor is made. The following example illustrates:

  ```
  c1 := class { A:int = 1 }
  c2 := class(c1):
	  block {SideEffectB()}
	  A<override>:int = 2 # This is what actually initializes `A`
	  block {SideEffectC()}

  MakeC1_1<constructor>():= c1:
	  block{SideEffectE()}
	  A := 3
	  block{SideEffectF()}}

  MakeC1<constructor>():= c1:
	  A := 4
	  block{SideEffectD()}
	  MakeC1_1<constructor>() # This is to a call of `c1`, which executes as per-normal.

  MakeC2<constructor>():= c2:
	  block{SideEffectA()}
	  # Before `MakeC1` is called, we call `c2`'s constructor.
	  # Note that we skip calling `c1`'s constructor from `c2`'s constructor in this case.
	  MakeC1<constructor>()

  O:= MakeC2()
  O.A = 2
  # The order of the side effects are as indicated by the function names in lexicographical order.
  ```

  Similarly, the side effects here execute in the order `SideEffectA`, then `SideEffectB`, then `SideEffectC`, and so on.

  In order to implement these semantics correctly, we keep track of fields that we've already initialized using the
  `CreateField` instruction, relying on the invariant that an uninitialized `VValue` represents an uninitialized field.

  In the archetypes, we set them to either point to the delegating archetype representing the nested constructor,
  or, if none exists, we set it to the class body constructor (since an archetype may not initialize all fields in the class).
  The base class body archetype will, naturally, point to nothing. When we construct a new object, we walk the archetype
  linked list and determine the entries that will be initialized in the object/shape, which is how we determine the emergent
  type to create/vend for the object.
 */

// This is used as a special PC to get the interpreter to break out of its loop.
COREUOBJECT_API FOpErr StopInterpreterSentry;
// This is used as a special PC to get the interpreter to throw a runtime error from the watchdog.
FOpErr ThrowRuntimeErrorSentry;

namespace
{
struct FExecutionState
{
	FOp* PC{nullptr};
	VFrame* Frame{nullptr};

	const TWriteBarrier<VValue>* Constants{nullptr};
	VRestValue* Registers{nullptr};
	FValueOperand* Operands{nullptr};
	FLabelOffset* Labels{nullptr};

	FExecutionState(FOp* PC, VFrame* Frame)
		: PC(PC)
		, Frame(Frame)
		, Constants(Frame->Procedure->GetConstantsBegin())
		, Registers(Frame->Registers)
		, Operands(Frame->Procedure->GetOperandsBegin())
		, Labels(Frame->Procedure->GetLabelsBegin())
	{
	}

	FExecutionState() = default;
	FExecutionState(const FExecutionState&) = default;
	FExecutionState(FExecutionState&&) = default;
	FExecutionState& operator=(const FExecutionState&) = default;
};

// In Verse, all functions conceptually take a single argument tuple
// To avoid unnecessary boxing and unboxing of VValues, we add an optimization where we try to avoid boxing/unboxing as much as possible
// This function reconciles the number of expected parameters with the number of provided arguments and boxes/unboxes only as needed
template <typename ArgFunction, typename StoreFunction, typename NamedArgFunction, typename NamedStoreFunction>
static void UnboxArguments(FAllocationContext Context, uint32 NumParams, uint32 NumNamedParams, uint32 NumArgs, FNamedParam* NamedParams, TArrayView<TWriteBarrier<VUniqueString>>* NamedArgs, ArgFunction GetArg, StoreFunction StoreArg, NamedArgFunction GetNamedArg, NamedStoreFunction StoreNamedArg)
{
	// --- Unnamed parameters -------------------------------
	if (NumArgs == NumParams)
	{
		/* direct passing */
		for (uint32 Arg = 0; Arg < NumArgs; ++Arg)
		{
			StoreArg(Arg, GetArg(Arg));
		}
	}
	else if (NumArgs == 1)
	{
		// Function wants loose arguments but a tuple is provided - unbox them
		VValue IncomingArg = GetArg(0);
		VArrayBase& Args = IncomingArg.StaticCast<VArrayBase>();

		V_DIE_UNLESS(NumParams == Args.Num());
		for (uint32 Param = 0; Param < NumParams; ++Param)
		{
			StoreArg(Param, Args.GetValue(Param));
		}
	}
	else if (NumParams == 1)
	{
		// Function wants loose arguments in a box, ie:
		// F(X:tuple(int, int)):int = X(0) + X(1)
		// F(3, 5) = 8 <-- we need to box these
		VArray& ArgArray = VArray::New(Context, NumArgs, GetArg);
		StoreArg(0, ArgArray);
	}
	else
	{
		V_DIE("Unexpected parameter/argument count mismatch");
	}

	// --- Named parameters ---------------------------------
	const uint32 NumNamedArgs = NamedArgs ? NamedArgs->Num() : 0;
	for (uint32 NamedParamIdx = 0; NamedParamIdx < NumNamedParams; ++NamedParamIdx)
	{
		VValue ValueToStore;
		for (uint32 NamedArgIdx = 0; NamedArgIdx < NumNamedArgs; ++NamedArgIdx)
		{
			if (NamedParams[NamedParamIdx].Name.Get() == (*NamedArgs)[NamedArgIdx].Get())
			{
				ValueToStore = GetNamedArg(NamedArgIdx);
				break;
			}
		}
		StoreNamedArg(NamedParamIdx, ValueToStore);
	}
}

template <typename ReturnSlotType, typename ArgFunction, typename NamedArgFunction>
static VFrame& MakeFrameForCallee(
	FRunningContext Context,
	FOp* CallerPC,
	VFrame* CallerFrame,
	ReturnSlotType ReturnSlot,
	VProcedure& Procedure,
	TWriteBarrier<VValue> Self,
	TWriteBarrier<VScope> Scope,
	const uint32 NumArgs,
	TArrayView<TWriteBarrier<VUniqueString>>* NamedArgs,
	ArgFunction GetArg,
	NamedArgFunction GetNamedArg)
{
	VFrame& Frame = VFrame::New(Context, CallerPC, CallerFrame, ReturnSlot, Procedure);

	check(FRegisterIndex::PARAMETER_START + Procedure.NumPositionalParameters + Procedure.NumNamedParameters <= Procedure.NumRegisters);

	Frame.Registers[FRegisterIndex::SELF].Set(Context, Self.Get());
	if (Scope)
	{
		Frame.Registers[FRegisterIndex::SCOPE].Set(Context, *Scope.Get());
	}

	UnboxArguments(
		Context, Procedure.NumPositionalParameters, Procedure.NumNamedParameters, NumArgs, Procedure.GetNamedParamsBegin(), NamedArgs,
		GetArg,
		[&](uint32 Param, VValue Value) {
			Frame.Registers[FRegisterIndex::PARAMETER_START + Param].Set(Context, Value);
		},
		GetNamedArg,
		[&](uint32 NamedParam, VValue Value) {
			Frame.Registers[Procedure.GetNamedParamsBegin()[NamedParam].Index.Index].Set(Context, Value);
		});

	return Frame;
}
} // namespace

static constexpr bool DoStats = false;
static double NumReuses;
static double TotalNumFailureContexts;

class FInterpreter
{
	FRunningContext Context;

	FExecutionState State;

	VTask* Task;
	VRestValue EffectToken{0};
	/// This represents the current queue that newly-unblocked suspensions get enqueued on.
	VSuspension* UnblockedSuspensionQueue{nullptr};

	VFailureContext* const OutermostFailureContext;
	VTask* OutermostTask;
	FOp* OutermostStartPC;
	FOp* OutermostEndPC;

	FString ExecutionTrace;
	FExecutionState SavedStateForTracing;

	static constexpr uint32 CachedFailureContextsCapacity = 32;
	uint32 NumCachedFailureContexts = 0; // This represents how many elements are in CachedFailureContexts.
	VFailureContext* CachedFailureContexts[CachedFailureContextsCapacity];

	// These fields are in service of the dynamic escape analysis we do of failure contexts.
	// At a high level, failure contexts escape during leniency and when we call into native.
	// If a failure context doesn't escape, we cache it for reuse. An unescaped failure context
	// is put back in the cache if we finish executing inside that failure context or if we fail.
	uint32 NumUnescapedFailureContexts = 0; // This represents the number of failure contexts at the top of the failure context stack that have not escaped yet.
	VFailureContext* _FailureContext;

	void PushReusableFailureContext()
	{
		checkSlow(NumUnescapedFailureContexts > 0);
		NumUnescapedFailureContexts -= 1;

		if (NumCachedFailureContexts < CachedFailureContextsCapacity)
		{
			CachedFailureContexts[NumCachedFailureContexts] = _FailureContext;
			++NumCachedFailureContexts;
		}
	}

	VFailureContext* PopReusableFailureContext()
	{
		if (!NumCachedFailureContexts)
		{
			return nullptr;
		}

		if constexpr (DoStats)
		{
			NumReuses += 1.0;
		}

		--NumCachedFailureContexts;
		return CachedFailureContexts[NumCachedFailureContexts];
	}

	void EscapeFailureContext()
	{
		NumUnescapedFailureContexts = 0;
	}

	VFailureContext* FailureContext()
	{
		EscapeFailureContext();
		return _FailureContext;
	}

	FORCEINLINE VValue GetOperand(FValueOperand Operand)
	{
		if (Operand.IsRegister())
		{
			return State.Registers[Operand.AsRegister().Index].Get(Context);
		}
		else if (Operand.IsConstant())
		{
			return State.Constants[Operand.AsConstant().Index].Get().Follow();
		}
		else
		{
			return VValue();
		}
	}

	static VValue GetOperand(const TWriteBarrier<VValue>& Value)
	{
		return Value.Get().Follow();
	}

	TArrayView<FValueOperand> GetOperands(TOperandRange<FValueOperand> Operands)
	{
		return TArrayView<FValueOperand>(State.Operands + Operands.Index, Operands.Num);
	}

	template <typename CellType>
	TArrayView<TWriteBarrier<CellType>> GetOperands(TOperandRange<TWriteBarrier<CellType>> Immediates)
	{
		TWriteBarrier<CellType>* Constants = BitCast<TWriteBarrier<CellType>*>(State.Constants);
		return TArrayView<TWriteBarrier<CellType>>{Constants + Immediates.Index, Immediates.Num};
	}

	static TArrayView<TWriteBarrier<VValue>> GetOperands(TArray<TWriteBarrier<VValue>>& Operands)
	{
		return TArrayView<TWriteBarrier<VValue>>(Operands);
	}

	TArrayView<FLabelOffset> GetConstants(TOperandRange<FLabelOffset> Constants)
	{
		return TArrayView<FLabelOffset>(State.Labels + Constants.Index, Constants.Num);
	}

	template <typename OpType, typename = void>
	struct HasDest : std::false_type
	{
	};
	template <typename OpType>
	struct HasDest<OpType, std::void_t<decltype(OpType::Dest)>> : std::true_type
	{
	};

	// Construct a return slot for the "Dest" field of "Op" if it has one.
	template <typename OpType>
	auto MakeReturnSlot(OpType& Op)
	{
		return MakeReturnSlot(Op, HasDest<OpType>{});
	}

	template <typename OpType>
	VRestValue* MakeReturnSlot(OpType& Op, std::false_type)
	{
		return nullptr;
	}

	template <typename OpType>
	auto MakeReturnSlot(OpType& Op, std::true_type)
	{
		return MakeOperandReturnSlot(Op.Dest);
	}

	VRestValue* MakeOperandReturnSlot(FRegisterIndex Dest)
	{
		return &State.Frame->Registers[Dest.Index];
	}

	VValue MakeOperandReturnSlot(const TWriteBarrier<VValue>& Dest)
	{
		return GetOperand(Dest);
	}

	// Include autogenerated functions to create captures
#include "VVMMakeCapturesFuncs.gen.h"

	void PrintOperandOrValue(FString& String, FRegisterIndex Operand)
	{
		if (Operand.Index == FRegisterIndex::UNINITIALIZED)
		{
			String += "(UNINITIALIZED)";
		}
		else
		{
			String += State.Frame->Registers[Operand.Index].ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
	}

	void PrintOperandOrValue(FString& String, FValueOperand Operand)
	{
		if (Operand.IsRegister())
		{
			String += State.Frame->Registers[Operand.AsRegister().Index].ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
		else if (Operand.IsConstant())
		{
			String += State.Constants[Operand.AsConstant().Index].Get().ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
		else
		{
			String += "Empty";
		}
	}

	template <typename T>
	void PrintOperandOrValue(FString& String, TWriteBarrier<T>& Operand)
	{
		if constexpr (std::is_same_v<T, VValue>)
		{
			String += Operand.Get().ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
		else
		{
			if (Operand)
			{
				String += Operand->ToString(Context, EValueStringFormat::CellsWithAddresses);
			}
			else
			{
				String += "(NULL)";
			}
		}
	}

	void PrintOperandOrValue(FString& String, TOperandRange<FValueOperand> Operands)
	{
		String += TEXT("(");
		const TCHAR* Separator = TEXT("");
		for (int32 Index = 0; Index < Operands.Num; ++Index)
		{
			String += Separator;
			Separator = TEXT(", ");
			PrintOperandOrValue(String, State.Operands[Operands.Index + Index]);
		}
		String += TEXT(")");
	}

	template <typename T>
	void PrintOperandOrValue(FString& String, TOperandRange<TWriteBarrier<T>> Operands)
	{
		TWriteBarrier<T>* Constants = BitCast<TWriteBarrier<T>*>(State.Constants);
		String += TEXT("(");
		const TCHAR* Separator = TEXT("");
		for (int32 Index = 0; Index < Operands.Num; ++Index)
		{
			String += Separator;
			Separator = TEXT(", ");
			PrintOperandOrValue(String, Constants[Operands.Index + Index]);
		}
		String += TEXT(")");
	}

	template <typename T>
	void PrintOperandOrValue(FString& String, TArray<TWriteBarrier<T>>& Operands)
	{
		String += TEXT("(");
		const TCHAR* Separator = TEXT("");
		for (TWriteBarrier<T>& Operand : Operands)
		{
			String += Separator;
			Separator = TEXT(", ");
			PrintOperandOrValue(String, Operand);
		}
		String += TEXT(")");
	}

	template <typename OpOrCaptures>
	FString TraceOperandsImpl(OpOrCaptures& Op, TArray<EOperandRole> RolesToPrint)
	{
		FString String;
		const TCHAR* Separator = TEXT("");
		Op.ForEachOperand([&](EOperandRole Role, auto& OperandOrValue, const TCHAR* Name) {
			if (RolesToPrint.Find(Role) != INDEX_NONE)
			{
				String += Separator;
				Separator = TEXT(", ");
				String.Append(Name).Append("=");
				PrintOperandOrValue(String, OperandOrValue);
			}
		});
		return String;
	}

	template <typename OpOrCaptures>
	FString TraceInputs(OpOrCaptures& Op)
	{
		return TraceOperandsImpl(Op, {EOperandRole::Use, EOperandRole::Immediate});
	}

	template <typename OpOrCaptures>
	FString TraceOutputs(OpOrCaptures& Op)
	{
		return TraceOperandsImpl(Op, {EOperandRole::UnifyDef, EOperandRole::ClobberDef});
	}

	FString TracePrefix(VProcedure* Procedure, VRestValue* CurrentEffectToken, EOpcode Opcode, uint32 BytecodeOffset, bool bLenient)
	{
		FString String;
		String += FString::Printf(TEXT("%p"), Procedure);
		String += FString::Printf(TEXT("#%u|"), BytecodeOffset);
		if (CurrentEffectToken)
		{
			String += TEXT("EffectToken=");
			String += CurrentEffectToken->ToString(Context, EValueStringFormat::CellsWithAddresses);
			String += TEXT("|");
		}
		if (bLenient)
		{
			String += TEXT("Lenient|");
		}
		String += ToString(Opcode);
		String += TEXT("(");
		return String;
	}

	void BeginTrace()
	{
		if (CVarSingleStepTraceExecution.GetValueOnAnyThread())
		{
			getchar();
		}

		SavedStateForTracing = State;
		if (State.PC == &StopInterpreterSentry)
		{
			UE_LOG(LogVerseVM, Display, TEXT("StoppingExecution, encountered StopInterpreterSentry"));
			return;
		}

		if (State.PC == &ThrowRuntimeErrorSentry)
		{
			UE_LOG(LogVerseVM, Display, TEXT("StoppingExecution, encountered ThrowRuntimeErrorSentry"));
			return;
		}

		ExecutionTrace = TracePrefix(State.Frame->Procedure.Get(), &EffectToken, State.PC->Opcode, State.Frame->Procedure->BytecodeOffset(*State.PC), false);

#define VISIT_OP(Name)                                                     \
	case EOpcode::Name:                                                    \
	{                                                                      \
		ExecutionTrace += TraceInputs(*static_cast<FOp##Name*>(State.PC)); \
		break;                                                             \
	}

		switch (State.PC->Opcode)
		{
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
		}

		ExecutionTrace += TEXT(")");
	}

	template <typename CaptureType>
	void BeginTrace(CaptureType& Captures, VBytecodeSuspension& Suspension)
	{
		if (CVarSingleStepTraceExecution.GetValueOnAnyThread())
		{
			getchar();
		}

		ExecutionTrace = TracePrefix(Suspension.Procedure.Get(), nullptr, Suspension.Opcode, Suspension.BytecodeOffset, true);
		ExecutionTrace += TraceInputs(Captures);
		ExecutionTrace += TEXT(")");
	}

	void EndTrace(bool bSuspended, bool bFailed)
	{
		FExecutionState CurrentState = State;
		State = SavedStateForTracing;

		FString Temp;

#define VISIT_OP(Name)                                           \
	case EOpcode::Name:                                          \
	{                                                            \
		Temp = TraceOutputs(*static_cast<FOp##Name*>(State.PC)); \
		break;                                                   \
	}

		switch (State.PC->Opcode)
		{
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
		}

		if (!Temp.IsEmpty())
		{
			ExecutionTrace += TEXT("|");
			ExecutionTrace += Temp;
		}

		if (bSuspended)
		{
			ExecutionTrace += TEXT("|Suspending");
		}

		if (bFailed)
		{
			ExecutionTrace += TEXT("|Failed");
		}

		UE_LOG(LogVerseVM, Display, TEXT("%s"), *ExecutionTrace);

		State = CurrentState;
	}

	template <typename CaptureType>
	void EndTraceWithCaptures(CaptureType& Captures, bool bSuspended, bool bFailed)
	{
		ExecutionTrace += TEXT("|");
		ExecutionTrace += TraceOutputs(Captures);
		if (bSuspended)
		{
			ExecutionTrace += TEXT("|Suspending");
		}

		if (bFailed)
		{
			ExecutionTrace += TEXT("|Failed");
		}
		UE_LOG(LogVerseVM, Display, TEXT("%s"), *ExecutionTrace);
	}

	static bool Def(FRunningContext Context, VValue ResultSlot, VValue Value, VSuspension*& SuspensionsToFire)
	{
		// The comparison returns equal if we encounter a placeholder
		ECompares Cmp = VValue::Equal(Context, ResultSlot, Value, [Context, &SuspensionsToFire](VValue Left, VValue Right) {
			// Given how the interpreter is structured, we know these must be resolved
			// to placeholders. They can't be pointing to values or we should be using
			// the value they point to.
			checkSlow(!Left.IsPlaceholder() || Left.Follow().IsPlaceholder());
			checkSlow(!Right.IsPlaceholder() || Right.Follow().IsPlaceholder());

			if (Left.IsPlaceholder() && Right.IsPlaceholder())
			{
				Left.GetRootPlaceholder().Unify(Context, Right.GetRootPlaceholder());
				return;
			}

			VSuspension* NewSuspensionToFire;
			if (Left.IsPlaceholder())
			{
				NewSuspensionToFire = Left.GetRootPlaceholder().SetValue(Context, Right);
			}
			else
			{
				NewSuspensionToFire = Right.GetRootPlaceholder().SetValue(Context, Left);
			}

			if (!SuspensionsToFire)
			{
				SuspensionsToFire = NewSuspensionToFire;
			}
			else
			{
				SuspensionsToFire->Tail().Next.Set(Context, NewSuspensionToFire);
			}
		});
		return Cmp == ECompares::Eq;
	}

	bool Def(VValue ResultSlot, VValue Value)
	{
		return Def(Context, ResultSlot, Value, UnblockedSuspensionQueue);
	}

	bool Def(const TWriteBarrier<VValue>& ResultSlot, VValue Value)
	{
		return Def(GetOperand(ResultSlot), Value);
	}

	static bool Def(FRunningContext Context, VRestValue& ResultSlot, VValue Value, VSuspension*& SuspensionsToFire)
	{
		// TODO: This needs to consider split depth eventually.
		if (LIKELY(ResultSlot.CanDefQuickly()))
		{
			ResultSlot.Set(Context, Value);
			return true;
		}
		return Def(Context, ResultSlot.Get(Context), Value, SuspensionsToFire);
	}

	bool Def(VRestValue& ResultSlot, VValue Value)
	{
		return Def(Context, ResultSlot, Value, UnblockedSuspensionQueue);
	}

	bool Def(FRegisterIndex ResultSlot, VValue Value)
	{
		return Def(State.Frame->Registers[ResultSlot.Index], Value);
	}

	static bool Def(FRunningContext Context, VReturnSlot& ReturnSlot, VValue Value, VSuspension*& SuspensionsToFire)
	{
		if (ReturnSlot.Kind == VReturnSlot::EReturnKind::RestValue)
		{
			if (ReturnSlot.RestValue)
			{
				return Def(Context, *ReturnSlot.RestValue, Value, SuspensionsToFire);
			}
			else
			{
				return true;
			}
		}
		else
		{
			checkSlow(ReturnSlot.Kind == VReturnSlot::EReturnKind::Value);
			return Def(Context, ReturnSlot.Value.Get(), Value, SuspensionsToFire);
		}
	}

	bool Def(VReturnSlot& ReturnSlot, VValue Value)
	{
		return Def(Context, ReturnSlot, Value, UnblockedSuspensionQueue);
	}

	void BumpEffectEpoch()
	{
		EffectToken.Reset(0);
	}

	FOpResult::EKind FinishedExecutingFailureContextLeniently(VFailureContext& FailureContext, FOp* StartPC, FOp* EndPC, VValue NextEffectToken)
	{
		VFailureContext* ParentFailure = FailureContext.Parent.Get();
		VTask* ParentTask = FailureContext.Task.Get();

		if (StartPC < EndPC)
		{
			VFrame* Frame = FailureContext.Frame.Get();
			// When we cloned the frame for lenient execution, we guarantee the caller info
			// isn't set because when this is done executing, it should not return to the
			// caller at the time of creation of the failure context. It should return back here.
			V_DIE_IF(Frame->CallerFrame || Frame->CallerPC);

			FInterpreter Interpreter(
				Context,
				FExecutionState(StartPC, Frame),
				ParentFailure,
				ParentTask,
				NextEffectToken,
				StartPC, EndPC);
			FOpResult::EKind Result = Interpreter.Execute();
			if (Result == FOpResult::Error)
			{
				return Result;
			}

			// TODO: We need to think through exactly what control flow inside
			// of the then/else of a failure context means. For example, then/else
			// can contain a break/return, but we might already be executing past
			// that then/else leniently. So we need to somehow find a way to transfer
			// control of the non-lenient execution. This likely means the below
			// def of the effect token isn't always right.

			// This can't fail.
			Def(FailureContext.DoneEffectToken, Interpreter.EffectToken.Get(Context));
		}
		else
		{
			// This can't fail.
			Def(FailureContext.DoneEffectToken, NextEffectToken);
		}

		if (ParentFailure && !ParentFailure->bFailed)
		{
			// We increment the suspension count for our parent failure
			// context when this failure context sees lenient execution.
			// So this is the decrement to balance that out that increment.
			return FinishedExecutingSuspensionIn(*ParentFailure);
		}
		return FOpResult::Return;
	}

	FOpResult::EKind FinishedExecutingSuspensionIn(VFailureContext& FailureContext)
	{
		V_DIE_IF(FailureContext.bFailed);

		V_DIE_UNLESS(FailureContext.SuspensionCount);
		uint32 RemainingCount = --FailureContext.SuspensionCount;
		if (RemainingCount)
		{
			return FOpResult::Return;
		}

		if (LIKELY(!FailureContext.bExecutedEndFailureContextOpcode))
		{
			return FOpResult::Return;
		}

		FailureContext.FinishedExecuting(Context);
		FOp* StartPC = FailureContext.ThenPC;
		FOp* EndPC = FailureContext.FailurePC;
		// Since we finished executing all suspensions in this failure context without failure, we can now commit the transaction
		VValue NextEffectToken = FailureContext.BeforeThenEffectToken.Get(Context);
		if (NextEffectToken.IsPlaceholder())
		{
			VValue NewNextEffectToken = VValue::Placeholder(VPlaceholder::New(Context, 0));
			DoTransactionActionWhenEffectTokenIsConcrete<TransactAction::Commit>(FailureContext, *FailureContext.Task, NextEffectToken, NewNextEffectToken);
			NextEffectToken = NewNextEffectToken;
		}
		else
		{
			FailureContext.Transaction.Commit(Context);
		}

		return FinishedExecutingFailureContextLeniently(FailureContext, StartPC, EndPC, NextEffectToken);
	}

	FOpResult::EKind Fail()
	{
#if DO_GUARD_SLOW
		if (NumUnescapedFailureContexts)
		{
			V_DIE_IF(_FailureContext->SuspensionCount);
			V_DIE_IF(_FailureContext->bExecutedEndFailureContextOpcode);
		}
#endif
		// This doesn't escape the failure context.
		return Fail(*_FailureContext);
	}

	FOpResult::EKind Fail(VFailureContext& FailureContext)
	{
		V_DIE_IF(FailureContext.bFailed);
		V_DIE_UNLESS(Task == FailureContext.Task.Get());

		FailureContext.Fail(Context);
		FailureContext.FinishedExecuting(Context);

		if (LIKELY(!FailureContext.bExecutedEndFailureContextOpcode))
		{
			return FOpResult::Return;
		}

		FOp* StartPC = FailureContext.FailurePC;
		FOp* EndPC = FailureContext.DonePC;
		VValue NextEffectToken = FailureContext.IncomingEffectToken.Get();

		return FinishedExecutingFailureContextLeniently(FailureContext, StartPC, EndPC, NextEffectToken);
	}

	// Returns true if unwinding succeeded. False if we are trying to unwind past
	// the outermost frame of this Interpreter instance.
	bool UnwindIfNeeded()
	{
		if (NumUnescapedFailureContexts)
		{
			// When we suspend in a failure context, we escape that failure context.
			// When we unblock a suspension, we also escape all unescaped failure contexts
			// at the top of the stack.
			//
			// So, if we make it here after encountering failure, it means we could only
			// have failed in a non-lenient context, so therefore, we could only have failed
			// at the top-most failure context.
#if DO_GUARD_SLOW
			{
				VFailureContext* FailureContext = _FailureContext->Parent.Get();
				for (uint32 I = 0; I < NumUnescapedFailureContexts - 1; ++I)
				{
					V_DIE_IF(FailureContext->bFailed);
					FailureContext = FailureContext->Parent.Get();
				}
			}
#endif

			if (_FailureContext->bFailed)
			{
				PushReusableFailureContext();
				State = FExecutionState(_FailureContext->FailurePC, _FailureContext->Frame.Get());
				EffectToken.Set(Context, _FailureContext->IncomingEffectToken.Get());
				_FailureContext = _FailureContext->Parent.Get();
			}

			return true;
		}

		if (!FailureContext()->bFailed)
		{
			return true;
		}

		VFailureContext* FailedContext = FailureContext();
		while (true)
		{
			if (FailedContext == OutermostFailureContext)
			{
				return false;
			}

			VFailureContext* Parent = FailedContext->Parent.Get();
			if (!Parent->bFailed)
			{
				break;
			}
			FailedContext = Parent;
		}

		State = FExecutionState(FailedContext->FailurePC, FailedContext->Frame.Get());
		_FailureContext = FailedContext->Parent.Get();
		EffectToken.Set(Context, FailedContext->IncomingEffectToken.Get());

		return true;
	}

	template <typename ReturnSlotType>
	void Suspend(VFailureContext& FailureContext, VTask& SuspendingTask, ReturnSlotType ResumeSlot)
	{
		V_DIE_UNLESS(&FailureContext == OutermostFailureContext);

		SuspendingTask.Suspend(Context);
		SuspendingTask.ResumeSlot.Set(Context, ResumeSlot);
	}

	// Returns true if yielding succeeded. False if we are trying to yield past
	// the outermost frame of this Interpreter instance.
	bool YieldIfNeeded(FOp* NextPC)
	{
		V_DIE_UNLESS(FailureContext() == OutermostFailureContext);

		while (true)
		{
			if (Task->bRunning)
			{
				// The task is still active or already unwinding.
				if (Task->Phase != VTask::EPhase::CancelStarted)
				{
					return true;
				}

				if (Task->CancelChildren(Context))
				{
					BeginUnwind(NextPC);
					return true;
				}

				Task->Suspend(Context);
			}
			else
			{
				if (Task->Phase == VTask::EPhase::CancelRequested)
				{
					Task->Phase = VTask::EPhase::CancelStarted;
					if (Task->CancelChildren(Context))
					{
						Task->Resume(Context);
						BeginUnwind(NextPC);
						return true;
					}
				}
			}

			VTask* SuspendedTask = Task;

			// Save the current state for when the task is resumed.
			SuspendedTask->ResumePC = NextPC;
			SuspendedTask->ResumeFrame.Set(Context, State.Frame);

			// Switch back to the task that started or resumed this one.
			State = FExecutionState(SuspendedTask->YieldPC, SuspendedTask->YieldFrame.Get());
			Task = SuspendedTask->YieldTask.Get();

			// Detach the task from the stack.
			SuspendedTask->YieldPC = &StopInterpreterSentry;
			SuspendedTask->YieldTask.Reset();

			if (SuspendedTask == OutermostTask)
			{
				return false;
			}

			NextPC = State.PC;
		}
	}

	// Jump from PC to its associated unwind label, in the current function or some transitive caller.
	// There must always be some unwind label, because unwinding always terminates at EndTask.
	void BeginUnwind(FOp* PC)
	{
		V_DIE_UNLESS(Task->bRunning);

		Task->Phase = VTask::EPhase::CancelUnwind;
		Task->ExecNativeDefer(Context);

		for (VFrame* Frame = State.Frame; Frame != nullptr; PC = Frame->CallerPC, Frame = Frame->CallerFrame.Get())
		{
			VProcedure* Procedure = Frame->Procedure.Get();
			int32 Offset = Procedure->BytecodeOffset(PC);

			for (
				FUnwindEdge* UnwindEdge = Procedure->GetUnwindEdgesBegin();
				UnwindEdge != Procedure->GetUnwindEdgesEnd() && UnwindEdge->Begin < Offset;
				UnwindEdge++)
			{
				if (Offset <= UnwindEdge->End)
				{
					State = FExecutionState(UnwindEdge->OnUnwind.GetLabeledPC(), Frame);
					return;
				}
			}
		}

		VERSE_UNREACHABLE();
	}

	enum class TransactAction
	{
		Start,
		Commit
	};

	template <TransactAction Action>
	void DoTransactionActionWhenEffectTokenIsConcrete(VFailureContext& FailureContext, VTask& TaskContext, VValue IncomingEffectToken, VValue NextEffectToken)
	{
		VLambdaSuspension& Suspension = VLambdaSuspension::New(
			Context, FailureContext, TaskContext,
			[](FRunningContext TheContext, VLambdaSuspension& LambdaSuspension, VSuspension*& SuspensionsToFire) {
				if constexpr (Action == TransactAction::Start)
				{
					LambdaSuspension.FailureContext->Transaction.Start(TheContext);
				}
				else
				{
					LambdaSuspension.FailureContext->Transaction.Commit(TheContext);
				}
				VValue NextEffectToken = LambdaSuspension.Args()[0].Get();
				FInterpreter::Def(TheContext, NextEffectToken, VValue::EffectDoneMarker(), SuspensionsToFire);
			},
			NextEffectToken);

		IncomingEffectToken.EnqueueSuspension(Context, Suspension);
	}

	// Macros to be used both directly in the interpreter loops and impl functions.
	// Parameterized over the implementation of ENQUEUE_SUSPENSION, FAIL, and YIELD.

#define REQUIRE_CONCRETE(Value)          \
	if (UNLIKELY(Value.IsPlaceholder())) \
	{                                    \
		ENQUEUE_SUSPENSION(Value);       \
	}

#define DEF(Result, Value)   \
	if (!Def(Result, Value)) \
	{                        \
		FAIL();              \
	}

#define OP_RESULT_HELPER(Result)                  \
	if (!Result.IsReturn())                       \
	{                                             \
		if (Result.Kind == FOpResult::Block)      \
		{                                         \
			check(Result.Value.IsPlaceholder());  \
			ENQUEUE_SUSPENSION(Result.Value);     \
		}                                         \
		else if (Result.Kind == FOpResult::Fail)  \
		{                                         \
			FAIL();                               \
		}                                         \
		else if (Result.Kind == FOpResult::Yield) \
		{                                         \
			YIELD();                              \
		}                                         \
		else if (Result.Kind == FOpResult::Error) \
		{                                         \
			RUNTIME_ERROR(Result.Value);          \
		}                                         \
		else                                      \
		{                                         \
			VERSE_UNREACHABLE();                  \
		}                                         \
	}

	// Macro definitions to be used in impl functions.

#define ENQUEUE_SUSPENSION(Value) \
	return                        \
	{                             \
		FOpResult::Block, Value   \
	}

#define FAIL()          \
	return              \
	{                   \
		FOpResult::Fail \
	}

#define YIELD()          \
	return               \
	{                    \
		FOpResult::Yield \
	}

#define RUNTIME_ERROR(Value)    \
	return                      \
	{                           \
		FOpResult::Error, Value \
	}

#define RAISE_RUNTIME_ERROR_CODE(Context, Diagnostic)                                           \
	const Verse::SRuntimeDiagnosticInfo& DiagnosticInfo = GetRuntimeDiagnosticInfo(Diagnostic); \
	Context.RaiseVerseRuntimeError(Diagnostic, FText::FromString(DiagnosticInfo.Description));

#define RAISE_RUNTIME_ERROR(Context, Diagnostic, Message) \
	Context.RaiseVerseRuntimeError(Diagnostic, FText::FromString(DiagnosticInfo.Description));

#define RAISE_RUNTIME_ERROR_FORMAT(Context, Diagnostic, FormatString, ...) \
	Context.RaiseVerseRuntimeError(Diagnostic, FText::FromString(FString::Printf(FormatString, ##__VA_ARGS__)));

	VRational& PrepareRationalSourceHelper(VValue& Source)
	{
		if (VRational* RationalSource = Source.DynamicCast<VRational>())
		{
			return *RationalSource;
		}

		V_DIE_UNLESS_MSG(Source.IsInt(), "Unsupported operands were passed to a Rational operation!");

		return VRational::New(Context, Source.AsInt(), VInt(Context, 1));
	}

	template <typename OpType>
	FOpResult AddImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			DEF(Op.Dest, VInt::Add(Context, LeftSource.AsInt(), RightSource.AsInt()));
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			DEF(Op.Dest, LeftSource.AsFloat() + RightSource.AsFloat());
		}
		else if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);

			DEF(Op.Dest, VRational::Add(Context, LeftRational, RightRational).StaticCast<VCell>());
		}
		else if (LeftSource.IsCellOfType<VArrayBase>() && RightSource.IsCellOfType<VArrayBase>())
		{
			// Array concatenation.
			VArrayBase& LeftArray = LeftSource.StaticCast<VArrayBase>();
			VArrayBase& RightArray = RightSource.StaticCast<VArrayBase>();

			DEF(Op.Dest, VArray::Concat(Context, LeftArray, RightArray));
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Add` operation!");
		}

		return {FOpResult::Return};
	}

	// TODO: Add the ability for bytecode instructions to have optional arguments so instead of having this bytecode
	//		 we can just have 'Add' which can take a boolean telling it whether the result should be mutable.
	template <typename OpType>
	FOpResult MutableAddImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsCellOfType<VArrayBase>() && RightSource.IsCellOfType<VArrayBase>())
		{
			// Array concatenation.
			VArrayBase& LeftArray = LeftSource.StaticCast<VArrayBase>();
			VArrayBase& RightArray = RightSource.StaticCast<VArrayBase>();

			DEF(Op.Dest, VMutableArray::Concat(Context, LeftArray, RightArray));
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `MutableAdd` operation!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult SubImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			DEF(Op.Dest, VInt::Sub(Context, LeftSource.AsInt(), RightSource.AsInt()));
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			DEF(Op.Dest, LeftSource.AsFloat() - RightSource.AsFloat());
		}
		else if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);

			DEF(Op.Dest, VRational::Sub(Context, LeftRational, RightRational).StaticCast<VCell>());
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Sub` operation!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult MulImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt())
		{
			if (RightSource.IsInt())
			{
				DEF(Op.Dest, VInt::Mul(Context, LeftSource.AsInt(), RightSource.AsInt()));
				return {FOpResult::Return};
			}
			else if (RightSource.IsFloat())
			{
				DEF(Op.Dest, LeftSource.AsInt().ConvertToFloat() * RightSource.AsFloat());
				return {FOpResult::Return};
			}
		}
		else if (LeftSource.IsFloat())
		{
			if (RightSource.IsInt())
			{
				DEF(Op.Dest, LeftSource.AsFloat() * RightSource.AsInt().ConvertToFloat());
				return {FOpResult::Return};
			}
			else if (RightSource.IsFloat())
			{
				DEF(Op.Dest, LeftSource.AsFloat() * RightSource.AsFloat());
				return {FOpResult::Return};
			}
		}

		if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);

			DEF(Op.Dest, VRational::Mul(Context, LeftRational, RightRational).StaticCast<VCell>());
			return {FOpResult::Return};
		}

		V_DIE("Unsupported operands were passed to a `Mul` operation!");
	}

	template <typename OpType>
	FOpResult DivImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (RightSource.AsInt().IsZero())
			{
				FAIL();
			}

			DEF(Op.Dest, VRational::New(Context, LeftSource.AsInt(), RightSource.AsInt()).StaticCast<VCell>());
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			DEF(Op.Dest, LeftSource.AsFloat() / RightSource.AsFloat());
		}
		else if (LeftSource.IsCellOfType<VRational>() || RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = PrepareRationalSourceHelper(LeftSource);
			VRational& RightRational = PrepareRationalSourceHelper(RightSource);
			if (RightRational.IsZero())
			{
				FAIL();
			}

			DEF(Op.Dest, VRational::Div(Context, LeftRational, RightRational).StaticCast<VCell>());
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Div` operation!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult ModImpl(OpType& Op)
	{
		VValue LeftSource = GetOperand(Op.LeftSource);
		VValue RightSource = GetOperand(Op.RightSource);
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (RightSource.AsInt().IsZero())
			{
				FAIL();
			}

			DEF(Op.Dest, VInt::Mod(Context, LeftSource.AsInt(), RightSource.AsInt()));
		}
		// TODO: VRational could support Mod in limited circumstances
		else
		{
			V_DIE("Unsupported operands were passed to a `Mod` operation!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NegImpl(OpType& Op)
	{
		VValue Source = GetOperand(Op.Source);
		REQUIRE_CONCRETE(Source);

		if (Source.IsInt())
		{
			DEF(Op.Dest, VInt::Neg(Context, Source.AsInt()));
		}
		else if (Source.IsFloat())
		{
			DEF(Op.Dest, -(Source.AsFloat()));
		}
		else if (Source.IsCellOfType<VRational>())
		{
			DEF(Op.Dest, VRational::Neg(Context, Source.StaticCast<VRational>()));
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `Neg` operation");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult QueryImpl(OpType& Op)
	{
		VValue Source = GetOperand(Op.Source);
		REQUIRE_CONCRETE(Source);

		if (Source.ExtractCell() == GlobalFalsePtr.Get())
		{
			FAIL();
		}
		else if (VOption* Option = Source.DynamicCast<VOption>()) // True = VOption(VFalse), which is handled by this case
		{
			DEF(Op.Dest, Option->GetValue());
		}
		else if (!Source.IsUObject())
		{
			V_DIE("Unimplemented type passed to VM `Query` operation");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult MapKeyImpl(OpType& Op)
	{
		VValue Map = GetOperand(Op.Map);
		VValue Index = GetOperand(Op.Index);
		REQUIRE_CONCRETE(Map);
		REQUIRE_CONCRETE(Index);

		if (Map.IsCellOfType<VMapBase>() && Index.IsInt())
		{
			DEF(Op.Dest, Map.StaticCast<VMapBase>().GetKey(Index.AsInt32()));
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `MapKey` operation!");
		}
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult MapValueImpl(OpType& Op)
	{
		VValue Map = GetOperand(Op.Map);
		VValue Index = GetOperand(Op.Index);
		REQUIRE_CONCRETE(Map);
		REQUIRE_CONCRETE(Index);

		if (Map.IsCellOfType<VMapBase>() && Index.IsInt())
		{
			DEF(Op.Dest, Map.StaticCast<VMapBase>().GetValue(Index.AsInt32()));
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `MapValue` operation!");
		}
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult LengthImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		// We need this to be concrete before we can attempt to get its size, even if the values in the container
		// might be placeholders.
		REQUIRE_CONCRETE(Container);
		if (const VArrayBase* Array = Container.DynamicCast<VArrayBase>())
		{
			DEF(Op.Dest, VInt{static_cast<int32>(Array->Num())});
		}
		else if (const VMapBase* Map = Container.DynamicCast<VMapBase>())
		{
			DEF(Op.Dest, VInt{static_cast<int32>(Map->Num())});
		}
		else
		{
			V_DIE("Unsupported container type passed!");
		}

		return {FOpResult::Return};
	}

	// TODO (SOL-5813) : Optimize melt to start at the value it suspended on rather
	// than re-doing the entire melt Op again which is what we do currently.
	template <typename OpType>
	FOpResult MeltImpl(OpType& Op)
	{
		VValue Value = GetOperand(Op.Value);
		VValue Result = VValue::Melt(Context, Value);
		REQUIRE_CONCRETE(Result);
		DEF(Op.Dest, Result);
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult FreezeImpl(OpType& Op)
	{
		VValue Value = GetOperand(Op.Value);
		FOpResult Result = VValue::Freeze(Context, Value);
		if (Result.IsReturn())
		{
			DEF(Op.Dest, Result.Value);
		}
		return Result.Kind;
	}

	template <typename OpType>
	FOpResult VarGetImpl(OpType& Op)
	{
		VValue Var = GetOperand(Op.Var);
		REQUIRE_CONCRETE(Var);
		VValue Result;
		if (VVar* Ref = Var.DynamicCast<VVar>())
		{
			Result = Ref->Get(Context);
		}
		else if (VNativeRef* NativeRef = Var.DynamicCast<VNativeRef>())
		{
			Result = *NativeRef;
		}
		else
		{
			V_DIE("Unexpected ref type %s", *Var.AsCell().DebugName());
		}
		DEF(Op.Dest, Result);
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult VarSetImpl(OpType& Op)
	{
		VValue Var = GetOperand(Op.Var);
		VValue Value = GetOperand(Op.Value);
		REQUIRE_CONCRETE(Var);
		if (VVar* VarPtr = Var.DynamicCast<VVar>())
		{
			VarPtr->Set(Context, Value);
		}
		else if (VNativeRef* Ref = Var.DynamicCast<VNativeRef>())
		{
			FOpResult Result = Ref->Set(Context, Value);
			OP_RESULT_HELPER(Result);
		}
		else
		{
			V_DIE("Unexpected ref type %s", *Value.AsCell().DebugName());
		}
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult CallSetImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		const VValue Index = GetOperand(Op.Index);
		const VValue ValueToSet = GetOperand(Op.ValueToSet);
		REQUIRE_CONCRETE(Container);
		REQUIRE_CONCRETE(Index); // Must be an Int32 (although UInt32 is better)
		if (VMutableArray* Array = Container.DynamicCast<VMutableArray>())
		{
			// Bounds check since this index access in Verse is failable.
			if (Index.IsInt32() && Index.AsInt32() >= 0 && Array->IsInBounds(Index.AsInt32()))
			{
				Array->SetValueTransactionally(Context, static_cast<uint32>(Index.AsInt32()), ValueToSet);
			}
			else
			{
				FAIL();
			}
		}
		else if (VMutableMap* Map = Container.DynamicCast<VMutableMap>())
		{
			Map->AddTransactionally(Context, Index, ValueToSet);
		}
		else
		{
			V_DIE("Unsupported container type passed!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult CallImpl(OpType& Op, VValue Callee, VTask* TaskContext, VValue IncomingEffectToken)
	{
		// Handles FOpCall for all non-VFunction calls
		check(!Callee.IsPlaceholder());

		auto Arguments = GetOperands(Op.Arguments);
		if (VNativeFunction* NativeFunction = Callee.DynamicCast<VNativeFunction>())
		{
			// With leniency, the active failure contexts aren't 1:1 with the active transactions.
			// The active failure contexts form a tree. The active transactions form a path in that tree.
			// Right now, an active VM transaction is 1:1 with an RTFM transaction.
			// So, this begs the question: when calling a native function that has effects <= <computes>,
			// what do we do if that native call is inside a failure context that isn't part of the active transaction path.
			// What transaction do we run it in?
			// If we make it so that native functions suspend on the effect token, we never find ourselves in the
			// "what do we do if that native call is inside a failure context that isn't part of the active transaction path" problem.
			// But also, long term, this will make more programs stuck than we want.
			REQUIRE_CONCRETE(IncomingEffectToken);

			VValue Self;
			if constexpr (std::is_same_v<OpType, FOpCallWithSelf>)
			{
				V_DIE_IF(NativeFunction->HasSelf()); // Since we are passing `Self` explicitly, this shouldn't have been previously-bound
				Self = GetOperand(Op.Self);
			}
			else
			{
				Self = NativeFunction->Self.Get();
			}

			VFunction::Args Args;
			Args.AddUninitialized(NativeFunction->NumPositionalParameters);
			UnboxArguments(
				Context, NativeFunction->NumPositionalParameters, 0, Arguments.Num(), nullptr, nullptr,
				[&](uint32 Arg) {
					return GetOperand(Arguments[Arg]);
				},
				[&](uint32 Param, VValue Value) {
					Args[Param] = Value;
				},
				[](uint32 NamedArg) -> VValue { VERSE_UNREACHABLE(); },
				[](uint32 NamedParam, VValue Value) -> VValue { VERSE_UNREACHABLE(); });

			FNativeCallResult Result{FNativeCallResult::Error};
			Context.PushNativeFrame(FailureContext(), NativeFunction, State.PC, State.Frame, TaskContext, [&] {
				Context.CheckForHandshake([&] {
					if (FSamplingProfiler* Sampler = GetSamplingProfiler())
					{
						// We have sample here to know when we are in a native func
						Sampler->Sample(Context, State.PC, State.Frame, Task);
					}
				});
				Result = (*NativeFunction->Thunk)(Context, Self, Args);
			});
			OP_RESULT_HELPER(Result);
			DEF(Op.Dest, Result.Value);
			return {FOpResult::Return};
		}
		else
		{
			V_DIE_UNLESS(Arguments.Num() == 1);

			VValue Argument = GetOperand(Arguments[0]);
			if (VArrayBase* Array = Callee.DynamicCast<VArrayBase>())
			{
				REQUIRE_CONCRETE(Argument);
				// Bounds check since this index access in Verse is fallible.
				if (Argument.IsUint32() && Array->IsInBounds(Argument.AsUint32()))
				{
					DEF(Op.Dest, Array->GetValue(Argument.AsUint32()));
				}
				else
				{
					FAIL();
				}
			}
			else if (VMapBase* Map = Callee.DynamicCast<VMapBase>())
			{
				// TODO SOL-5621: We need to ensure the entire Key structure is concrete, not just the top-level.
				REQUIRE_CONCRETE(Argument);
				if (VValue Result = Map->Find(Context, Argument))
				{
					DEF(Op.Dest, Result);
				}
				else
				{
					FAIL();
				}
			}
			else if (VType* Type = Callee.DynamicCast<VType>())
			{
				REQUIRE_CONCRETE(Argument);
				if (Type->Subsumes(Context, Argument))
				{
					DEF(Op.Dest, Argument);
				}
				else
				{
					FAIL();
				}
			}
			else
			{
				V_DIE("Unknown callee");
			}
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewArrayImpl(OpType& Op)
	{
		auto Values = GetOperands(Op.Values);
		const uint32 NumValues = Values.Num();
		VArray& NewArray = VArray::New(Context, NumValues, [this, &Values](uint32 Index) { return GetOperand(Values[Index]); });
		DEF(Op.Dest, NewArray);
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewMutableArrayImpl(OpType& Op)
	{
		auto Values = GetOperands(Op.Values);
		const uint32 NumValues = Values.Num();
		VMutableArray& NewArray = VMutableArray::New(Context, NumValues, [this, &Values](uint32 Index) { return GetOperand(Values[Index]); });
		DEF(Op.Dest, NewArray);
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewMutableArrayWithCapacityImpl(OpType& Op)
	{
		const VValue Size = GetOperand(Op.Size);
		REQUIRE_CONCRETE(Size); // Must be an Int32 (although UInt32 is better)
		// TODO: We should kill this opcode until we actually have a use for it.
		// Allocating this with None array type means we're not actually reserving a
		// capacity. The way to do this right in the future is to use profiling to
		// guide what array type we pick. This opcode is currently only being
		// used in our bytecode tests.
		DEF(Op.Dest, VMutableArray::New(Context, 0, static_cast<uint32>(Size.AsInt32()), EArrayType::None));

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult ArrayAddImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		const VValue ValueToAdd = GetOperand(Op.ValueToAdd);
		REQUIRE_CONCRETE(Container);
		if (VMutableArray* Array = Container.DynamicCast<VMutableArray>())
		{
			Array->AddValue(Context, ValueToAdd);
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `ArrayAdd` operation!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult InPlaceMakeImmutableImpl(OpType& Op)
	{
		const VValue Container = GetOperand(Op.Container);
		REQUIRE_CONCRETE(Container);
		if (Container.IsCellOfType<VMutableArray>())
		{
			Container.StaticCast<VMutableArray>().InPlaceMakeImmutable(Context);
			checkSlow(Container.IsCellOfType<VArray>() && !Container.IsCellOfType<VMutableArray>());
		}
		else
		{
			V_DIE("Unimplemented type passed to VM `InPlaceMakeImmutable` operation!");
		}

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewOptionImpl(OpType& Op)
	{
		VValue Value = GetOperand(Op.Value);

		DEF(Op.Dest, VOption::New(Context, Value));

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewMapImpl(OpType& Op)
	{
		auto Keys = GetOperands(Op.Keys);
		auto Values = GetOperands(Op.Values);

		const uint32 NumKeys = Keys.Num();
		V_DIE_UNLESS(NumKeys == static_cast<uint32>(Values.Num()));

		VMapBase& NewMap = VMapBase::New<VMap>(Context, NumKeys, [this, &Keys, &Values](uint32 Index) {
			return TPair<VValue, VValue>(GetOperand(Keys[Index]), GetOperand(Values[Index]));
		});

		DEF(Op.Dest, NewMap);

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewClassImpl(OpType& Op)
	{
		auto AttrIndices = GetOperands(Op.AttributeIndices);
		auto Attrs = GetOperands(Op.Attributes);
		VArray* AttrIndicesValue = nullptr;
		VArray* AttrsValue = nullptr;
		if (Attrs.Num() > 0)
		{
			AttrIndicesValue = &VArray::New(Context, AttrIndices.Num(), [this, &AttrIndices](uint32 Index) {
				return AttrIndices[Index].Get();
			});
			AttrsValue = &VArray::New(Context, Attrs.Num(), [this, &Attrs](uint32 Index) {
				return GetOperand(Attrs[Index]);
			});
		}
		UStruct* ImportStruct = Cast<UStruct>(Op.ImportStruct.Get().ExtractUObject());

		auto Inherited = GetOperands(Op.Inherited);
		TArray<VClass*> InheritedClasses = {};
		int32 NumInherited = Inherited.Num();
		InheritedClasses.Reserve(NumInherited);
		for (int32 Index = 0; Index < NumInherited; ++Index)
		{
			const VValue CurrentArg = GetOperand(Inherited[Index]);
			REQUIRE_CONCRETE(CurrentArg);
			InheritedClasses.Add(&CurrentArg.StaticCast<VClass>());
		}

		VClass::EFlags Flags = Op.Flags;
		if (Op.bNativeBound || Attrs.Num() > 0 || InheritedClasses.ContainsByPredicate([](VClass* Class) { return Class->IsNativeRepresentation(); }))
		{
			EnumAddFlags(Flags, VClass::EFlags::NativeRepresentation);
		}

		// We're doing this because the placeholder during codegen time isn't yet concrete.
		REQUIRE_CONCRETE(Op.Archetype->NextArchetype.Get(Context));
		VClass& NewClass = VClass::New(
			Context,
			Op.Package.Get(),
			Op.RelativePath.Get(),
			Op.ClassName.Get(),
			AttrIndicesValue,
			AttrsValue,
			ImportStruct,
			Op.bNativeBound,
			Op.ClassKind,
			Flags,
			InheritedClasses,
			*Op.Archetype,
			*Op.ConstructorBody);
		if (ImportStruct)
		{
			GlobalProgram->AddImport(Context, NewClass, ImportStruct);
		}

		DEF(Op.ClassDest, NewClass);
		DEF(Op.ArchetypeDest, NewClass.GetArchetype());
		DEF(Op.ConstructorDest, NewClass.GetConstructor());
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult BindNativeClassImpl(OpType& Op)
	{
		VValue ClassValue = GetOperand(Op.Class);
		REQUIRE_CONCRETE(ClassValue);

		VClass& Class = ClassValue.StaticCast<VClass>();
		if (!Class.IsNativeRepresentation())
		{
			return {FOpResult::Return};
		}

		TArray<VClass*> ClassesVisited;
		FOpResult Result = RequireClassConcrete(Class, ClassesVisited);
		if (!Result.IsReturn())
		{
			return Result;
		}

		if (Class.Attributes)
		{
			const uint32 NumAttributes = Class.Attributes->Num();
			for (uint32 Index = 0; Index < NumAttributes; ++Index)
			{
				VValue AttributeValue = Class.Attributes->GetValue(Index);
				REQUIRE_CONCRETE(AttributeValue);
			}
		}

		// TODO: Allow native functions to require a concrete UClass before being called.
		Class.GetOrCreateUEType<UStruct>(Context);
		return {FOpResult::Return};
	}

	static FOpResult RequireClassConcrete(VClass& Class, TArray<VClass*>& ClassesVisited)
	{
		ClassesVisited.Add(&Class);

		// Require concrete field types.
		const uint32 NumArchetypeEntries = Class.GetArchetype().NumEntries;
		for (uint32 Index = 0; Index < NumArchetypeEntries; ++Index)
		{
			VArchetype::VEntry& Entry = Class.GetArchetype().Entries[Index];
			if (VValue FieldType = Entry.Type.Follow(); !FieldType.IsUninitialized())
			{
				FOpResult Result = RequireTypeConcrete(FieldType, ClassesVisited);
				if (!Result.IsReturn())
				{
					return Result;
				}
			}
		}

		return {FOpResult::Return};
	}

	static FOpResult RequireTypeConcrete(VValue Type, TArray<VClass*>& ClassesVisited)
	{
		REQUIRE_CONCRETE(Type);
		if (VTypeType* TypeType = Type.DynamicCast<VTypeType>())
		{
			return RequireTypeConcrete(TypeType->PositiveType.Follow(), ClassesVisited);
		}
		else if (VClass* ClassType = Type.DynamicCast<VClass>())
		{
			if (!ClassesVisited.Contains(ClassType))
			{
				return RequireClassConcrete(*ClassType, ClassesVisited);
			}
		}
		else if (VArrayType* ArrayType = Type.DynamicCast<VArrayType>())
		{
			return RequireTypeConcrete(ArrayType->ElementType.Follow(), ClassesVisited);
		}
		else if (VGeneratorType* GeneratorType = Type.DynamicCast<VGeneratorType>())
		{
			return RequireTypeConcrete(GeneratorType->ElementType.Follow(), ClassesVisited);
		}
		else if (VMapType* MapType = Type.DynamicCast<VMapType>())
		{
			FOpResult KeyResult = RequireTypeConcrete(MapType->KeyType.Follow(), ClassesVisited);
			if (!KeyResult.IsReturn())
			{
				return KeyResult;
			}
			FOpResult ValueResult = RequireTypeConcrete(MapType->ValueType.Follow(), ClassesVisited);
			if (!ValueResult.IsReturn())
			{
				return ValueResult;
			}
		}
		else if (VPointerType* PointerType = Type.DynamicCast<VPointerType>())
		{
			return RequireTypeConcrete(PointerType->ValueType.Follow(), ClassesVisited);
		}
		else if (VOptionType* OptionType = Type.DynamicCast<VOptionType>())
		{
			return RequireTypeConcrete(OptionType->ValueType.Follow(), ClassesVisited);
		}
		else if (VTupleType* Tuple = Type.DynamicCast<VTupleType>())
		{
			for (uint32 Index = 0; Index < Tuple->NumElements; Index++)
			{
				FOpResult Result = RequireTypeConcrete(Tuple->GetElementTypes()[Index].Follow(), ClassesVisited);
				if (!Result.IsReturn())
				{
					return Result;
				}
			}
		}
		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult NewObjectImpl(OpType& Op)
	{
		VValue ArchetypeOperand = GetOperand(Op.Archetype);
		REQUIRE_CONCRETE(ArchetypeOperand);
		VArchetype& Archetype = ArchetypeOperand.StaticCast<VArchetype>();

		// This should have been set previously when the `VClass` constructor was run.
		REQUIRE_CONCRETE(Archetype.Class.Get(Context));
		VClass& ObjectClass = Archetype.Class.Get(Context).StaticCast<VClass>();

		// TODO: (yiliang.siew) We also need the delegating archetype to be concrete here, but we'll get
		// into a suspension loop if we do so because the class isn't yet concrete.
		REQUIRE_CONCRETE(Archetype.NextArchetype.Get(Context));

		// UObject/VNativeStruct or VObject?
		bool bNativeRepresentation = ObjectClass.IsNativeRepresentation();
		if (!bNativeRepresentation && !ObjectClass.IsStruct())
		{
			// Debugging functionality. This lets us test that both paths work as expected and not just with the smaller
			// subset of code that uses native Verse interop.
			const float UObjectProbability = CVarUObjectProbability.GetValueOnAnyThread();
			bNativeRepresentation = UObjectProbability > 0.0f && (UObjectProbability > RandomUObjectProbability.FRand());
		}

		// In the non-native case, the `VObject` isn't actually wrapped, but the bytecode assumes it is and
		// `UnwrapNativeConstructorWrapper` internally just no-ops in that case.
		VValue NewObject;
		if (bNativeRepresentation)
		{
			if (!ObjectClass.IsStruct())
			{
				if (!verse::CanAllocateUObjects())
				{
					RAISE_RUNTIME_ERROR_FORMAT(Context, ERuntimeDiagnostic::ErrRuntime_MemoryLimitExceeded, TEXT("Ran out of memory for allocating `UObject`s while attempting to construct a Verse object of type %s!"), *ObjectClass.GetBaseName().AsString());
					return {FOpResult::Error};
				}

				NewObject = ObjectClass.NewUObject(Context); // `NewUObject` wraps the newly-allocated `UObject` in a `VNativeConstructorWrapper` internally and returns it.
			}
			else
			{
				NewObject = ObjectClass.NewNativeStruct(Context);
			}
		}
		else
		{
			NewObject = ObjectClass.NewVObject(Context, Archetype);
		}
		DEF(Op.Dest, NewObject);

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult LoadFieldImpl(OpType& Op)
	{
		VValue ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);

		// NOTE: (yiliang.siew) We handle both the case where the native operand may be wrapped or not.
		// It would be wrapped if we tried to load a field during construction. (i.e. in a `block` of a constructor.)
		if (VNativeConstructorWrapper* ObjectWrapper = ObjectOperand.DynamicCast<VNativeConstructorWrapper>(); ObjectWrapper)
		{
			ObjectOperand = ObjectWrapper->WrappedObject();
		}

		VUniqueString& FieldName = *Op.Name.Get();
		if (VObject* Object = ObjectOperand.DynamicCast<VObject>(); Object) // Handles both `VValueObject`s and `VNativeStruct`s.
		{
			VCell& Cell = ObjectOperand.AsCell();

			if constexpr (
				std::is_same_v<OpType, FOpLoadFieldICOffset>
				|| std::is_same_v<OpType, FOpLoadFieldICConstant>
				|| std::is_same_v<OpType, FOpLoadFieldICFunction>
				|| std::is_same_v<OpType, FOpLoadFieldICNativeFunction>)
			{
				if (Cell.EmergentTypeOffset == Op.EmergentTypeOffset)
				{
					VValue Result;
					if constexpr (std::is_same_v<OpType, FOpLoadFieldICOffset>)
					{
						Result = BitCast<VRestValue*>(BitCast<char*>(&Cell) + Op.ICPayload)->Get(Context);
					}
					if constexpr (std::is_same_v<OpType, FOpLoadFieldICConstant>)
					{
						Result = VValue::Decode(Op.ICPayload);
					}
					if constexpr (std::is_same_v<OpType, FOpLoadFieldICFunction>)
					{
						Result = BitCast<VFunction*>(Op.ICPayload)->Bind(Context, Cell.StaticCast<VObject>());
					}
					if constexpr (std::is_same_v<OpType, FOpLoadFieldICNativeFunction>)
					{
						Result = BitCast<VNativeFunction*>(Op.ICPayload)->Bind(Context, Cell.StaticCast<VObject>());
					}

					DEF(Op.Dest, Result);
					return {FOpResult::Return};
				}
			}

			FCacheCase CacheCase;
			FOpResult FieldResult = Object->LoadField(Context, FieldName, &CacheCase);
			if (!FieldResult.IsReturn())
			{
				V_DIE_UNLESS(FieldResult.IsError());
				return {FOpResult::Error};
			}

			if constexpr (std::is_same_v<OpType, FOpLoadField>)
			{
				if (CacheCase)
				{
					Op.EmergentTypeOffset = CacheCase.EmergentTypeOffset;
					EOpcode NewOpcode;
					switch (CacheCase.Kind)
					{
						case FCacheCase::EKind::Offset:
							Op.ICPayload = CacheCase.U.Offset;
							NewOpcode = EOpcode::LoadFieldICOffset;
							break;
						case FCacheCase::EKind::ConstantValue:
							Op.ICPayload = CacheCase.U.Value.Encode();
							NewOpcode = EOpcode::LoadFieldICConstant;
							break;
						case FCacheCase::EKind::ConstantFunction:
							Op.ICPayload = BitCast<uint64>(CacheCase.U.Function);
							NewOpcode = EOpcode::LoadFieldICFunction;
							break;
						case FCacheCase::EKind::ConstantNativeFunction:
							Op.ICPayload = BitCast<uint64>(CacheCase.U.NativeFunction);
							NewOpcode = EOpcode::LoadFieldICNativeFunction;
							break;
						default:
							VERSE_UNREACHABLE();
					}
					StoreStoreFence();
					Op.Opcode = NewOpcode;
				}
			}

			DEF(Op.Dest, FieldResult.Value);
			return {FOpResult::Return};
		}

		if (UObject* UEObject = ObjectOperand.ExtractUObject())
		{
			FOpResult FieldResult = UVerseClass::LoadField(Context, UEObject, FieldName);
			if (FieldResult.IsReturn())
			{
				DEF(Op.Dest, FieldResult.Value);
				return {FOpResult::Return};
			}
			else
			{
				V_DIE_UNLESS(FieldResult.IsError());
				return {FOpResult::Error};
			}
		}

		V_DIE("Unsupported operand to a `LoadField` operation when loading: %s!", *FieldName.AsString());
	}

	template <typename OpType>
	FOpResult LoadFieldFromSuperImpl(OpType& Op)
	{
		const VValue ScopeOperand = GetOperand(Op.Scope);
		REQUIRE_CONCRETE(ScopeOperand);

		const VValue SelfOperand = GetOperand(Op.Self);
		REQUIRE_CONCRETE(SelfOperand);

		VUniqueString& FieldName = *Op.Name.Get();

		// Currently, we only allow object instances (of classes) to be referred to by `Self`.
		V_DIE_UNLESS(SelfOperand.IsCellOfType<VValueObject>() || SelfOperand.IsUObject());
		if (VValueObject* SelfValueObject = SelfOperand.DynamicCast<VValueObject>())
		{
			V_DIE_IF(SelfValueObject->IsStruct()); // Structs don't support inheritance or methods.
		}

		// NOTE: (yiliang.siew) We need to allocate a new function here for now in order to support passing methods around
		// as first-class values, since the method for each caller can't just be shared as the function from the
		// shape/constructor.
		VScope& Scope = ScopeOperand.StaticCast<VScope>();
		V_DIE_UNLESS(Scope.SuperClass);

		VFunction* FunctionWithSelf = nullptr;
		VArchetype* CurrentArchetype = &Scope.SuperClass->GetArchetype();
		while (CurrentArchetype && !FunctionWithSelf)
		{
			FunctionWithSelf = CurrentArchetype->LoadFunction(Context, FieldName, SelfOperand);
			CurrentArchetype = CurrentArchetype->NextArchetype.Get(Context).DynamicCast<VArchetype>();
		}
		V_DIE_UNLESS(FunctionWithSelf);
		DEF(Op.Dest, *FunctionWithSelf);

		return {FOpResult::Return};
	}

	template <typename OpType>
	FOpResult UnifyFieldImpl(OpType& Op)
	{
		const VValue ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);
		VValue ValueOperand = GetOperand(Op.Value);
		REQUIRE_CONCRETE(ValueOperand);
		VUniqueString& FieldName = *Op.Name.Get();

		bool bSucceeded = false;

		VValue UnwrappedObject;
		if (VNativeConstructorWrapper* Wrapper = ObjectOperand.DynamicCast<VNativeConstructorWrapper>())
		{
			VValue WrappedObject = Wrapper->WrappedObject();
			if (UObject* UEObject = WrappedObject.ExtractUObject())
			{
				UnwrappedObject = UEObject;
			}
			else if (VNativeStruct* NativeStruct = WrappedObject.DynamicCast<VNativeStruct>())
			{
				UnwrappedObject = *NativeStruct;
			}
			else
			{
				V_DIE("Currently, only wrapped `UObject`s and `VNativeStruct`s are supported for native objects!");
			}
		}
		else
		{
			UnwrappedObject = ObjectOperand;
		}

		if (VObject* Object = UnwrappedObject.DynamicCast<VObject>())
		{
			const VEmergentType* EmergentType = Object->GetEmergentType();
			VShape* Shape = EmergentType->Shape.Get();
			V_DIE_UNLESS(Shape != nullptr);
			const VShape::VEntry* Field = Shape->GetField(FieldName);
			V_DIE_UNLESS(Field != nullptr);
			switch (Field->Type)
			{
				case EFieldType::Offset:
					checkSlow(Object->IsA<VValueObject>()); // Offset fields should only exist on non-native objects
					bSucceeded = Def(Object->GetFieldData(*EmergentType->CppClassInfo)[Field->Index], ValueOperand);
					break;

				case EFieldType::Constant:
					bSucceeded = Def(Field->Value.Get(), ValueOperand);
					break;

				// NOTE: VNativeRef::Set only makes sense here because UnifyField is only used for initialization.
				// These cases should only exist for when the object is a `VNativeStruct`, since how we wrap objects is
				// enforced by convention.
				case EFieldType::FProperty:
				{
					checkSlow(Object->IsA<VNativeStruct>());
					FOpResult Result = VNativeRef::Set<false>(Context, nullptr, Object->GetData(*EmergentType->CppClassInfo), Field->UProperty, ValueOperand);
					OP_RESULT_HELPER(Result);
					bSucceeded = true;
					break;
				}
				case EFieldType::FPropertyVar:
				{
					checkSlow(Object->IsA<VNativeStruct>());
					FOpResult Result = VNativeRef::Set<false>(Context, nullptr, Object->GetData(*EmergentType->CppClassInfo), Field->UProperty, ValueOperand.StaticCast<VVar>().Get(Context));
					OP_RESULT_HELPER(Result);
					bSucceeded = true;
					break;
				}
				case EFieldType::FVerseProperty:
					checkSlow(Object->IsA<VNativeStruct>());
					bSucceeded = Def(*Field->UProperty->ContainerPtrToValuePtr<VRestValue>(Object->GetData(*EmergentType->CppClassInfo)), ValueOperand);
					break;
				default:
					V_DIE("Field: %s has an unsupported type; cannot unify!", *Op.Name.Get()->AsString());
					break;
			}
		}
		else if (UnwrappedObject.IsUObject())
		{
			UObject* UEObject = UnwrappedObject.ExtractUObject();
			UVerseClass* Class = CastChecked<UVerseClass>(UEObject->GetClass());
			VShape* Shape = Class->Shape.Get();
			V_DIE_UNLESS(Shape);
			const VShape::VEntry* Field = Shape->GetField(FieldName);
			V_DIE_UNLESS(Field);
			switch (Field->Type)
			{
				// NOTE: VNativeRef::Set only makes sense here because UnifyField is only used for initialization.
				case EFieldType::FProperty:
				{
					FOpResult Result = VNativeRef::Set<false>(Context, nullptr, UEObject, Field->UProperty, ValueOperand);
					OP_RESULT_HELPER(Result);
					bSucceeded = true;
					break;
				}
				case EFieldType::FPropertyVar:
				{
					FOpResult Result = VNativeRef::Set<false>(Context, nullptr, UEObject, Field->UProperty, ValueOperand.StaticCast<VVar>().Get(Context));
					OP_RESULT_HELPER(Result);
					bSucceeded = true;
					break;
				}
				case EFieldType::FVerseProperty:
					bSucceeded = Def(*Field->UProperty->ContainerPtrToValuePtr<VRestValue>(UEObject), ValueOperand);
					break;
				case EFieldType::Constant:
					bSucceeded = Def(Field->Value.Get(), ValueOperand);
					break;
				default:
					V_DIE("Field: %s has an unsupported type; cannot unify!", *FieldName.AsString());
					break;
			}
		}
		else
		{
			V_DIE("Unsupported operand to a `UnifyField` operation when attempting to unify %s!", *FieldName.AsString());
		}

		return bSucceeded ? FOpResult{FOpResult::Return} : FOpResult{FOpResult::Fail};
	}

	template <typename OpType>
	FOpResult BeginProfileBlockImpl(OpType& Op)
	{
		DEF(Op.Dest, VInt(Context, BitCast<int64>(FPlatformTime::Cycles64())));

		FVerseProfilingDelegates::RaiseBeginProfilingEvent();

		return FOpResult{FOpResult::Return};
	}

	template <typename OpType>
	FOpResult EndProfileBlockImpl(OpType& Op)
	{
		const uint64_t WallTimeEnd = FPlatformTime::Cycles64();

		const uint64_t WallTimeStart = BitCast<uint64_t>(GetOperand(Op.WallTimeStart).AsInt().AsInt64());
		const double WallTimeTotal = FPlatformTime::ToMilliseconds64(WallTimeEnd - WallTimeStart);

		// Build the locus
		const VUniqueString* SnippetPath = Op.SnippetPath.Get();
		TOptional<FUtf8String> SnippetPathStr = SnippetPath->AsOptionalUtf8String();

		const FProfileLocus Locus = {
			.BeginRow = GetOperand(Op.BeginRow).AsUint32(),
			.BeginColumn = GetOperand(Op.BeginColumn).AsUint32(),
			.EndRow = GetOperand(Op.EndRow).AsUint32(),
			.EndColumn = GetOperand(Op.EndColumn).AsUint32(),
			.SnippetPath = SnippetPathStr ? SnippetPathStr.GetValue() : "",
		};

		const VValue UserTag = GetOperand(Op.UserTag);
		const VCell& UserTagCell = UserTag.AsCell();
		const FUtf8StringView UserTagStr = UserTag.AsCell().StaticCast<VArray>().AsStringView();

		FVerseProfilingDelegates::RaiseEndProfilingEvent(UserTagStr.Len() ? reinterpret_cast<const char*>(UserTagStr.GetData()) : "", WallTimeTotal, Locus);

		return FOpResult{FOpResult::Return};
	}

	template <typename OpType>
	FOpResult SetFieldImpl(OpType& Op)
	{
		VValue ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);
		VValue Value = GetOperand(Op.Value);
		VUniqueString& FieldName = *Op.Name.Get();

		// This is only used for setting into a deeply mutable struct.
		// However, this code should just work for setting fields var
		// fields in a class when we stop boxing those fields in a VVar.
		if (VNativeConstructorWrapper* WrappedObject = ObjectOperand.DynamicCast<VNativeConstructorWrapper>(); WrappedObject)
		{
			ObjectOperand = WrappedObject->WrappedObject();
		}
		if (VObject* Object = ObjectOperand.DynamicCast<VObject>(); Object)
		{
			const VEmergentType* EmergentType = Object->GetEmergentType();
			VShape* Shape = EmergentType->Shape.Get();
			const VShape::VEntry* Field = Shape->GetField(FieldName);
			if (Field->Type == EFieldType::Offset)
			{
				Object->GetFieldData(*EmergentType->CppClassInfo)[Field->Index].SetTransactionally(Context, Value);
			}
			else
			{
				VNativeStruct& NativeStruct = ObjectOperand.StaticCast<VNativeStruct>();
				if (Field->Type == EFieldType::FProperty)
				{
					FOpResult Result = VNativeRef::Set<true>(Context, &NativeStruct, NativeStruct.GetData(*EmergentType->CppClassInfo), Field->UProperty, Value);
					OP_RESULT_HELPER(Result);
				}
				else if (Field->Type == EFieldType::FVerseProperty)
				{
					Field->UProperty->ContainerPtrToValuePtr<VRestValue>(Object->GetData(*EmergentType->CppClassInfo))->SetTransactionally(Context, Value);
				}
				else
				{
					V_DIE("Field %s has an unsupported type; cannot set!", *FieldName.AsString());
				}
			}
		}
		else if (ObjectOperand.IsUObject())
		{
			// TODO: Implement this when we stop boxing fields in VVars.
			VERSE_UNREACHABLE();
		}
		else
		{
			V_DIE("Unsupported operand to a `SetField` operation!");
		}

		return FOpResult{FOpResult::Return};
	}

	template <typename OpType>
	FOpResult CreateFieldImpl(OpType& Op)
	{
		VValue ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);

		// The result of CreateField indicates whether the field has already been created, either
		// by a previous initializer or as a constant entry in the object's shape.
		//
		// For VValueObjects, this state is currently tracked in the fields themselves, using the
		// uninitialized `VValue()`. Native types don't have sentinel values like this, so they are
		// wrapped in VNativeConstructorWrapper which uses a separate map.
		//
		// Constructors and class bodies use JumpIfInitialized on this result to skip overridden
		// initializers, so the uninitialized `VValue()` indicates that the field is new.
		VUniqueString& FieldName = *Op.Name.Get();
		if (VNativeConstructorWrapper* WrappedObject = ObjectOperand.DynamicCast<VNativeConstructorWrapper>())
		{
			if (WrappedObject->CreateField(Context, FieldName))
			{
				DEF(Op.Dest, VValue());
			}
			else
			{
				DEF(Op.Dest, GlobalFalse());
			}
		}
		else if (VValueObject* Object = ObjectOperand.DynamicCast<VValueObject>())
		{
			if (Object->CreateField(FieldName))
			{
				DEF(Op.Dest, VValue());
			}
			else
			{
				DEF(Op.Dest, GlobalFalse());
			}
		}
		else
		{
			V_DIE("Unsupported object operand to a `CreateField` operation!");
		}

		return FOpResult{FOpResult::Return};
	}

	template <typename OpType>
	FOpResult UnwrapNativeConstructorWrapperImpl(OpType& Op)
	{
		// Unwrap the native object and return it, while throwing away the wrapper object.
		const VValue ObjectOperand = GetOperand(Op.Object);
		REQUIRE_CONCRETE(ObjectOperand);

		if (VNativeConstructorWrapper* Wrapper = ObjectOperand.DynamicCast<VNativeConstructorWrapper>())
		{
			DEF(Op.Dest, Wrapper->WrappedObject());
		}
		else if (VObject* VerseObject = ObjectOperand.DynamicCast<VObject>())
		{
			DEF(Op.Dest, *VerseObject);
		}
		else if (UObject* UEObject = ObjectOperand.ExtractUObject())
		{
			DEF(Op.Dest, UEObject);
		}
		else
		{
			V_DIE("The `UnwrapNativeConstructorWrapper` opcode only wrapped/unwrapped objects; unrecognized operand type indicates a problem in the codegen!");
		}
		// The wrapper object should naturally get GC'ed after this in the next cycle, since it's only referenced when
		// we first create the native object.

		return FOpResult{FOpResult::Return};
	}

	FOpResult NeqImplHelper(VValue LeftSource, VValue RightSource)
	{
		VValue ToSuspendOn;
		// This returns true for placeholders, so if we see any placeholders,
		// we're not yet done checking for inequality because we need to
		// check the concrete values.
		ECompares Cmp = VValue::Equal(Context, LeftSource, RightSource, [&](VValue Left, VValue Right) {
			checkSlow(Left.IsPlaceholder() || Right.IsPlaceholder());
			if (!ToSuspendOn)
			{
				ToSuspendOn = Left.IsPlaceholder() ? Left : Right;
			}
		});

		if (Cmp == ECompares::Neq)
		{
			return {FOpResult::Return};
		}
		REQUIRE_CONCRETE(ToSuspendOn);
		FAIL();
	}

	FOpResult LtImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Lt(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() < RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Lt(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Lt` operation!");
		}

		return {FOpResult::Return};
	}

	FOpResult LteImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Lte(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() <= RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Lte(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Lte` operation!");
		}

		return {FOpResult::Return};
	}

	FOpResult GtImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Gt(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() > RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Gt(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Gt` operation!");
		}

		return {FOpResult::Return};
	}

	FOpResult GteImplHelper(VValue LeftSource, VValue RightSource)
	{
		REQUIRE_CONCRETE(LeftSource);
		REQUIRE_CONCRETE(RightSource);

		if (LeftSource.IsInt() && RightSource.IsInt())
		{
			if (!VInt::Gte(Context, LeftSource.AsInt(), RightSource.AsInt()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsFloat() && RightSource.IsFloat())
		{
			if (!(LeftSource.AsFloat() >= RightSource.AsFloat()))
			{
				FAIL();
			}
		}
		else if (LeftSource.IsCellOfType<VRational>() && RightSource.IsCellOfType<VRational>())
		{
			VRational& LeftRational = LeftSource.StaticCast<VRational>();
			VRational& RightRational = RightSource.StaticCast<VRational>();
			if (!VRational::Gte(Context, LeftRational, RightRational))
			{
				FAIL();
			}
		}
		else
		{
			V_DIE("Unsupported operands were passed to a `Gte` operation!");
		}

		return {FOpResult::Return};
	}

	FORCENOINLINE void HandleHandshakeSlowpath()
	{
		if (Context.IsRuntimeErrorRequested())
		{
			Context.ClearRuntimeErrorRequest();
			State.PC = &ThrowRuntimeErrorSentry;
			return;
		}

		if (FDebugger* Debugger = GetDebugger(); Debugger && State.PC != &StopInterpreterSentry)
		{
			Debugger->Notify(Context, *State.PC, *State.Frame, *Task);
		}

		if (FSamplingProfiler* Sampler = GetSamplingProfiler())
		{
			Sampler->Sample(Context, State.PC, State.Frame, Task);
		}
	}

#define DECLARE_COMPARISON_OP_IMPL(OpName)                              \
	template <typename OpType>                                          \
	FOpResult OpName##Impl(OpType& Op)                                  \
	{                                                                   \
		VValue LeftSource = GetOperand(Op.LeftSource);                  \
		VValue RightSource = GetOperand(Op.RightSource);                \
		FOpResult Result = OpName##ImplHelper(LeftSource, RightSource); \
		if (Result.IsReturn())                                          \
		{                                                               \
			/* success returns the left - value */                      \
			Def(Op.Dest, LeftSource);                                   \
		}                                                               \
		return Result;                                                  \
	}

	DECLARE_COMPARISON_OP_IMPL(Neq)
	DECLARE_COMPARISON_OP_IMPL(Lt)
	DECLARE_COMPARISON_OP_IMPL(Lte)
	DECLARE_COMPARISON_OP_IMPL(Gt)
	DECLARE_COMPARISON_OP_IMPL(Gte)

#undef ENQUEUE_SUSPENSION
#undef FAIL
#undef YIELD
#undef RUNTIME_ERROR

	// NOTE: (yiliang.siew) We don't templat-ize `bHasOutermostPCBounds` since it would mean duplicating the codegen
	// where `ExecuteImpl` gets called. Since it's the interpreter loop and a really big function, it bloats compile times.
	template <bool bPrintTrace>
	FORCENOINLINE FOpResult::EKind ExecuteImpl(const bool bHasOutermostPCBounds)
	{
		// Macros to be used in both the interpreter loops.
		// Parameterized over the implementation of BEGIN/END_OP_CASE as well as ENQUEUE_SUSPENSION, FAIL, YIELD, and RUNTIME_ERROR

#define OP_IMPL_HELPER(OpImpl, ...)               \
	FOpResult Result = OpImpl(Op, ##__VA_ARGS__); \
	OP_RESULT_HELPER(Result)

/// Define an opcode implementation that may suspend as part of execution.
#define OP(OpName)         \
	BEGIN_OP_CASE(OpName){ \
		OP_IMPL_HELPER(OpName##Impl)} END_OP_CASE()

#define OP_WITH_IMPL(OpName, OpImpl) \
	BEGIN_OP_CASE(OpName){           \
		OP_IMPL_HELPER(OpImpl)} END_OP_CASE()

// Macro definitions to be used in the main interpreter loop.

// We REQUIRE_CONCRETE on the effect token first because it obviates the need to capture
// the incoming effect token. If the incoming effect token is a placeholder, we will
// suspend, and we'll only resume after it becomes concrete.
#define OP_IMPL_THREAD_EFFECTS(OpName)                         \
	BEGIN_OP_CASE(OpName)                                      \
	{                                                          \
		VValue IncomingEffectToken = EffectToken.Get(Context); \
		BumpEffectEpoch();                                     \
		REQUIRE_CONCRETE(IncomingEffectToken);                 \
		OP_IMPL_HELPER(OpName##Impl)                           \
		DEF(EffectToken, VValue::EffectDoneMarker());          \
	}                                                          \
	END_OP_CASE()

#define NEXT_OP(bSuspended, bFailed)   \
	if constexpr (bPrintTrace)         \
	{                                  \
		EndTrace(bSuspended, bFailed); \
	}                                  \
	NextOp();                          \
	break

#define BEGIN_OP_CASE(Name)                                 \
	case EOpcode::Name:                                     \
	{                                                       \
		if constexpr (bPrintTrace)                          \
		{                                                   \
			BeginTrace();                                   \
		}                                                   \
		FOp##Name& Op = *static_cast<FOp##Name*>(State.PC); \
		NextPC = BitCast<FOp*>(&Op + 1);

#define END_OP_CASE()      \
	NEXT_OP(false, false); \
	}

#define ENQUEUE_SUSPENSION(Value)                                                                                                                       \
	VBytecodeSuspension& Suspension = VBytecodeSuspension::New(Context, *FailureContext(), *Task, *State.Frame->Procedure, State.PC, MakeCaptures(Op)); \
	Value.EnqueueSuspension(Context, Suspension);                                                                                                       \
	++FailureContext()->SuspensionCount;                                                                                                                \
	NEXT_OP(true, false)

#define FAIL()                      \
	if (Fail() == FOpResult::Error) \
	{                               \
		return FOpResult::Error;    \
	}                               \
	if (!UnwindIfNeeded())          \
	{                               \
		return FOpResult::Fail;     \
	}                               \
	NextPC = State.PC;              \
	NEXT_OP(false, true)

#define YIELD()                                            \
	Suspend(*FailureContext(), *Task, MakeReturnSlot(Op)); \
	if (!YieldIfNeeded(NextPC))                            \
	{                                                      \
		return FOpResult::Yield;                           \
	}                                                      \
	NextPC = State.PC;                                     \
	NEXT_OP(false, false)

#define RUNTIME_ERROR(Value) return FOpResult::Error

		if (UnblockedSuspensionQueue)
		{
			goto SuspensionInterpreterLoop;
		}

	MainInterpreterLoop:
		while (true)
		{
			FOp* NextPC = nullptr;

			auto UpdateExecutionState = [&](FOp* PC, VFrame* Frame) {
				State = FExecutionState(PC, Frame);
				NextPC = PC;
			};

			auto ReturnTo = [&](FOp* PC, VFrame* Frame) {
				if (Frame)
				{
					UpdateExecutionState(PC, Frame);
				}
				else
				{
					NextPC = &StopInterpreterSentry;
				}
			};

			auto NextOp = [&] {
				if (bHasOutermostPCBounds)
				{
					if (UNLIKELY(!State.Frame->CallerFrame
								 && (NextPC < OutermostStartPC || NextPC >= OutermostEndPC)))
					{
						NextPC = &StopInterpreterSentry;
					}
				}

				State.PC = NextPC;
			};

			Context.CheckForHandshake([&] {
				HandleHandshakeSlowpath();
			});

			switch (State.PC->Opcode)
			{
				OP(Add)
				OP(Sub)
				OP(Mul)
				OP(Div)
				OP(Mod)
				OP(Neg)

				OP(MutableAdd)

				OP(Neq)
				OP(Lt)
				OP(Lte)
				OP(Gt)
				OP(Gte)

				OP(Query)

				OP_IMPL_THREAD_EFFECTS(Melt)
				OP_IMPL_THREAD_EFFECTS(Freeze)

				OP_IMPL_THREAD_EFFECTS(VarGet)
				OP_IMPL_THREAD_EFFECTS(VarSet)
				OP_IMPL_THREAD_EFFECTS(SetField)
				OP(CreateField)
				OP(UnwrapNativeConstructorWrapper)
				OP_IMPL_THREAD_EFFECTS(CallSet)

				OP(NewOption)
				OP(Length)
				OP(NewArray)
				OP(NewMutableArray)
				OP(NewMutableArrayWithCapacity)
				OP_IMPL_THREAD_EFFECTS(ArrayAdd)
				OP(InPlaceMakeImmutable)
				OP(NewMap)
				OP(MapKey)
				OP(MapValue)
				OP(NewClass)
				OP(BindNativeClass)
				OP(UnifyField)

				OP_WITH_IMPL(LoadField, LoadFieldImpl)
				OP_WITH_IMPL(LoadFieldICOffset, LoadFieldImpl)
				OP_WITH_IMPL(LoadFieldICConstant, LoadFieldImpl)
				OP_WITH_IMPL(LoadFieldICFunction, LoadFieldImpl)
				OP_WITH_IMPL(LoadFieldICNativeFunction, LoadFieldImpl)
				OP(LoadFieldFromSuper)

				OP(BeginProfileBlock)
				OP(EndProfileBlock)

				BEGIN_OP_CASE(Err)
				{
					// If this is the stop interpreter sentry op, return.
					if (&Op == &StopInterpreterSentry)
					{
						return FOpResult::Return;
					}

					RAISE_RUNTIME_ERROR_CODE(Context, ERuntimeDiagnostic::ErrRuntime_Internal);
					return FOpResult::Error;
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Tracepoint)
				{
					VUniqueString& Name = *Op.Name.Get();
					UE_LOG(LogVerseVM, Display, TEXT("Hit tracepoint: %s"), *Name.AsString());
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Move)
				{
					// TODO SOL-4459: This doesn't work with leniency and failure. For example,
					// if both Dest/Source are placeholders, failure will never be associated
					// to this Move, but that can't be right.
					DEF(Op.Dest, GetOperand(Op.Source));
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Jump)
				{
					NextPC = Op.JumpOffset.GetLabeledPC();
				}
				END_OP_CASE()

				BEGIN_OP_CASE(JumpIfInitialized)
				{
					VValue Val = GetOperand(Op.Source);
					if (!Val.IsUninitialized())
					{
						NextPC = Op.JumpOffset.GetLabeledPC();
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(Switch)
				{
					VValue Which = GetOperand(Op.Which);
					TArrayView<FLabelOffset> Offsets = GetConstants(Op.JumpOffsets);
					NextPC = Offsets[Which.AsInt32()].GetLabeledPC();
				}
				END_OP_CASE()

				// TODO(SOL-7928): Remove this instruction. It is a hack for BPVM compatibility.
				BEGIN_OP_CASE(JumpIfArchetype)
				{
					VValue Object = GetOperand(Op.Object);
					if (VNativeConstructorWrapper* Wrapper = Object.DynamicCast<VNativeConstructorWrapper>())
					{
						if (UObject* NativeObject = Wrapper->WrappedObject().ExtractUObject())
						{
							if (NativeObject->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
							{
								NextPC = Op.JumpOffset.GetLabeledPC();
							}
						}
					}
					else if (VValueObject* VerseObject = Object.DynamicCast<VValueObject>())
					{
						if (VerseObject->Misc2 & VCell::ArchetypeTag)
						{
							NextPC = Op.JumpOffset.GetLabeledPC();
						}
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(BeginFailureContext)
				{
					if constexpr (DoStats)
					{
						TotalNumFailureContexts += 1.0;
					}

					VValue IncomingEffectToken = EffectToken.Get(Context);

					static_assert(std::is_trivially_destructible_v<VFailureContext>);
					void* Allocation = PopReusableFailureContext();
					if (!Allocation)
					{
						Allocation = FAllocationContext(Context).AllocateFastCell(sizeof(VFailureContext));
					}
					_FailureContext = new (Allocation) VFailureContext(Context, Task, _FailureContext, *State.Frame, IncomingEffectToken, Op.OnFailure.GetLabeledPC());

					if (IncomingEffectToken.IsPlaceholder())
					{
						BumpEffectEpoch();
						// This purposefully escapes the failure context.
						DoTransactionActionWhenEffectTokenIsConcrete<TransactAction::Start>(*FailureContext(), *Task, IncomingEffectToken, EffectToken.Get(Context));
					}
					else
					{
						_FailureContext->Transaction.Start(Context);
						++NumUnescapedFailureContexts;
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(EndFailureContext)
				{
					V_DIE_IF(_FailureContext->bFailed);   // We shouldn't have failed and still made it here.
					V_DIE_UNLESS(_FailureContext->Frame); // A null Frame indicates an artificial context from task resumption.

					if (_FailureContext->SuspensionCount)
					{
						// When we suspend inside of a failure context, we escape that failure context.
						V_DIE_UNLESS(NumUnescapedFailureContexts == 0);

						_FailureContext->bExecutedEndFailureContextOpcode = true;
						_FailureContext->ThenPC = NextPC;
						_FailureContext->DonePC = Op.Done.GetLabeledPC();

						if (_FailureContext->Parent)
						{
							++_FailureContext->Parent->SuspensionCount;
						}
						_FailureContext->BeforeThenEffectToken.Set(Context, EffectToken.Get(Context));
						EffectToken.Set(Context, _FailureContext->DoneEffectToken.Get(Context));
						NextPC = Op.Done.GetLabeledPC();
						_FailureContext->Frame.Set(Context, _FailureContext->Frame->CloneWithoutCallerInfo(Context));
					}
					else
					{
						_FailureContext->FinishedExecuting(Context);

						if (VValue IncomingEffectToken = EffectToken.Get(Context); IncomingEffectToken.IsPlaceholder())
						{
							// This is the case where an effect token wasn't concrete when the failure context started.
							// We shouldn't have created an unescaped failure context to begin with in this case. See
							// code in BeginFailureContext.
							checkSlow(NumUnescapedFailureContexts == 0);
							BumpEffectEpoch();
							DoTransactionActionWhenEffectTokenIsConcrete<TransactAction::Commit>(*FailureContext(), *Task, IncomingEffectToken, EffectToken.Get(Context));
						}
						else
						{
							if (NumUnescapedFailureContexts)
							{
								// We didn't escape the current failure context: we didn't suspend and the effect token is concrete.
								// Therefore, we can put it into our cache for reuse.
								PushReusableFailureContext();
							}
							_FailureContext->Transaction.Commit(Context);
						}
					}

					_FailureContext = _FailureContext->Parent.Get();
				}
				END_OP_CASE()

				BEGIN_OP_CASE(BeginTask)
				{
					V_DIE_UNLESS(FailureContext() == OutermostFailureContext);

					VTask* Parent = Op.bAttached ? Task : nullptr;
					Task = &VTask::New(Context, Op.OnYield.GetLabeledPC(), State.Frame, Task, Parent);

					DEF(Op.Dest, *Task);
				}
				END_OP_CASE()

				BEGIN_OP_CASE(EndTask)
				{
					V_DIE_UNLESS(Task->bRunning);
					V_DIE_UNLESS(FailureContext() == OutermostFailureContext);

					if (Task->Phase == VTask::EPhase::CancelRequested)
					{
						Task->Phase = VTask::EPhase::CancelStarted;
					}

					VValue Result;
					VTask* Awaiter;
					VTask* SignaledTask = nullptr;
					if (Task->Phase == VTask::EPhase::Active)
					{
						if (!Task->CancelChildren(Context))
						{
							VTask* Child = Task->LastChild.Get();
							Task->Park(Context, Child->LastCancel);
							Task->Defer(Context, [Child](FAccessContext Context, VTask* Task) {
								AutoRTFM::Open([&] { Task->Unpark(Context, Child->LastCancel); });
							});

							NextPC = &Op;
							YIELD();
						}

						Result = GetOperand(Op.Value);
						Task->Result.Set(Context, Result);

						// Communicate the result to the parent task, if there is one.
						if (Op.Write.Index < FRegisterIndex::UNINITIALIZED)
						{
							if (State.Frame->Registers[Op.Write.Index].Get(Context).IsUninitialized())
							{
								State.Frame->Registers[Op.Write.Index].Set(Context, Result);
							}
						}
						if (Op.Signal.IsRegister())
						{
							VSemaphore& Semaphore = GetOperand(Op.Signal).StaticCast<VSemaphore>();
							Semaphore.Count += 1;

							if (Semaphore.Count == 0)
							{
								V_DIE_UNLESS(Semaphore.Await.Get());
								SignaledTask = Semaphore.Await.Get();
								Semaphore.Await.Reset();
							}
						}

						Awaiter = Task->LastAwait.Get();
						Task->LastAwait.Reset();
					}
					else
					{
						V_DIE_UNLESS(VTask::EPhase::CancelStarted <= Task->Phase && Task->Phase < VTask::EPhase::Canceled);

						if (!Task->CancelChildren(Context))
						{
							V_DIE_UNLESS(Task->Phase == VTask::EPhase::CancelStarted);

							NextPC = &Op;
							YIELD();
						}

						Task->Phase = VTask::EPhase::Canceled;
						Result = GlobalFalse();

						Awaiter = Task->LastCancel.Get();
						Task->LastCancel.Reset();

						if (VTask* Parent = Task->Parent.Get())
						{
							// A canceling parent is implicitly awaiting its last child.
							if (Parent->Phase == VTask::EPhase::CancelStarted && Parent->LastChild.Get() == Task)
							{
								SignaledTask = Parent;
							}
						}
					}

					Task->ExecNativeAwaits(Context);
					Task->Suspend(Context);
					Task->Detach(Context);

					// This task may be resumed to run unblocked suspensions, but nothing remains to run after them.
					Task->ResumePC = &StopInterpreterSentry;
					Task->ResumeFrame.Set(Context, State.Frame);

					UpdateExecutionState(Task->YieldPC, Task->YieldFrame.Get());
					Task = Task->YieldTask.Get();

					auto ResumeAwaiter = [&](VTask* Awaiter) {
						Awaiter->YieldPC = NextPC;
						Awaiter->YieldFrame.Set(Context, State.Frame);
						Awaiter->YieldTask.Set(Context, Task);
						Awaiter->Resume(Context);

						UpdateExecutionState(Awaiter->ResumePC, Awaiter->ResumeFrame.Get());
						if (Task == nullptr)
						{
							OutermostTask = Awaiter;
						}
						Task = Awaiter;
					};

					// Resume any awaiting (or cancelling) tasks in the order they arrived.
					// The front of the list is the most recently-awaiting task, which should run last.
					if (SignaledTask && !SignaledTask->bRunning)
					{
						ResumeAwaiter(SignaledTask);
					}
					for (VTask* PrevTask; Awaiter != nullptr; Awaiter = PrevTask)
					{
						PrevTask = Awaiter->PrevTask.Get();

						// Normal resumption of a canceling task is a no-op.
						if (Awaiter->Phase != VTask::EPhase::Active)
						{
							continue;
						}

						ResumeAwaiter(Awaiter);
						Task->ExecNativeDefer(Context);
						if (!Def(Task->ResumeSlot, Result))
						{
							V_DIE("Failed unifying the result of `Await` or `Cancel`");
						}
					}

					// A resumed task may already have been re-suspended or canceled.
					if (Task == nullptr || !YieldIfNeeded(NextPC))
					{
						return FOpResult::Yield;
					}
					NextPC = State.PC;
				}
				END_OP_CASE()

				BEGIN_OP_CASE(NewSemaphore)
				{
					VSemaphore& Semaphore = VSemaphore::New(Context);
					DEF(Op.Dest, Semaphore);
				}
				END_OP_CASE()

				BEGIN_OP_CASE(WaitSemaphore)
				{
					VSemaphore& Semaphore = GetOperand(Op.Source).StaticCast<VSemaphore>();
					Semaphore.Count -= Op.Count;

					if (Semaphore.Count < 0)
					{
						V_DIE_IF(Semaphore.Await.Get());
						Semaphore.Await.Set(Context, Task);
						YIELD();
					}
				}
				END_OP_CASE()

				// An indexed access (i.e. `B := A[10]`) is just the same as `Call(B, A, 10)`.
				BEGIN_OP_CASE(Call)
				{
					VValue Callee = GetOperand(Op.Callee);
					REQUIRE_CONCRETE(Callee);

					if (VFunction* Function = Callee.DynamicCast<VFunction>())
					{
						VRestValue* ReturnSlot = MakeReturnSlot(Op);
						TArrayView<FValueOperand> Arguments = GetOperands(Op.Arguments);
						TArrayView<TWriteBarrier<VUniqueString>> NamedArguments = GetOperands(Op.NamedArguments);
						TArrayView<FValueOperand> NamedArgumentValues = GetOperands(Op.NamedArgumentValues);
						VFrame& NewFrame = MakeFrameForCallee(
							Context,
							NextPC,
							State.Frame,
							ReturnSlot,
							*Function->Procedure,
							Function->Self,
							Function->ParentScope,
							Arguments.Num(),
							&NamedArguments,
							[&](uint32 Arg) {
								return GetOperand(Arguments[Arg]);
							},
							[&](uint32 NamedArg) {
								return GetOperand(NamedArgumentValues[NamedArg]);
							});
						UpdateExecutionState(Function->GetProcedure().GetOpsBegin(), &NewFrame);
					}
					else
					{
						OP_IMPL_HELPER(CallImpl, Callee, Task, EffectToken.Get(Context));
					}
				}
				END_OP_CASE()

				BEGIN_OP_CASE(CallWithSelf) // non-suspension version
				{
					// TODO: (yiliang.siew) Find a way to share the code between the suspension/non-suspension version.
					VValue Callee = GetOperand(Op.Callee);
					REQUIRE_CONCRETE(Callee);

					VValue Self = GetOperand(Op.Self);
					REQUIRE_CONCRETE(Self);

					V_DIE_UNLESS(Callee.IsCell());
					V_DIE_IF_MSG(Callee.IsCellOfType<VProcedure>(), "`CallWithSelf` should be passed a `VFunction`-without-`Self` set, not a `VProcedure`! This indicates an issue with the codegen.");
					if (VFunction* Function = Callee.DynamicCast<VFunction>(); Function)
					{
						checkSlow(!Function->HasSelf());
						VRestValue* ReturnSlot = &State.Frame->Registers[Op.Dest.Index];
						TArrayView<FValueOperand> Arguments = GetOperands(Op.Arguments);
						TArrayView<TWriteBarrier<VUniqueString>> NamedArguments = GetOperands(Op.NamedArguments);
						TArrayView<FValueOperand> NamedArgumentValues = GetOperands(Op.NamedArgumentValues);
						VFrame& NewFrame = MakeFrameForCallee(
							Context,
							NextPC,
							State.Frame,
							ReturnSlot,
							Function->GetProcedure(),
							{Context, Self},
							Function->ParentScope,
							Arguments.Num(),
							&NamedArguments,
							[&](uint32 Arg) {
								return GetOperand(Arguments[Arg]);
							},
							[&](uint32 NamedArg) {
								return GetOperand(NamedArgumentValues[NamedArg]);
							});
						UpdateExecutionState(Function->GetProcedure().GetOpsBegin(), &NewFrame);
					}
					else if (VNativeFunction* NativeFunction = Callee.DynamicCast<VNativeFunction>(); NativeFunction)
					{
						// `Self` binding is handled internally within `CallImpl`.
						OP_IMPL_HELPER(CallImpl, Callee, Task, EffectToken.Get(Context));
					}
					else
					{
						V_DIE("Unsupported callee operand type: %s passed to `CallWithSelf`!", *Callee.AsCell().GetEmergentType()->Type->DebugName());
					}
				}
				END_OP_CASE() // CallWithSelf

				BEGIN_OP_CASE(Return)
				{
					// TODO SOL-4461: Return should work with lenient execution of failure contexts.
					// We can't just logically execute the first Return we encounter during lenient
					// execution if the then/else when executed would've returned.
					//
					// We also need to figure out how to properly pop a frame off if the
					// failure context we're leniently executing returns. We could continue
					// to execute the current frame and just not thread through the effect
					// token, so no effects could happen. But that's inefficient.

					VValue IncomingEffectToken = EffectToken.Get(Context);
					DEF(State.Frame->ReturnSlot.EffectToken, IncomingEffectToken); // This can't fail.

					VValue Value = GetOperand(Op.Value);
					VFrame& Frame = *State.Frame;

					ReturnTo(Frame.CallerPC, Frame.CallerFrame.Get());

					// TODO: Add a test where this unification fails at the top level with no return continuation.
					DEF(Frame.ReturnSlot, Value);
				}
				END_OP_CASE()

				BEGIN_OP_CASE(ResumeUnwind)
				{
					BeginUnwind(NextPC);
					NextPC = State.PC;
				}
				END_OP_CASE()

				OP(NewObject)

				BEGIN_OP_CASE(Reset)
				{
					State.Frame->Registers[Op.Dest.Index].Reset(0);
				}
				END_OP_CASE()

				BEGIN_OP_CASE(NewVar)
				{
					DEF(Op.Dest, VVar::New(Context));
				}
				END_OP_CASE()

				default:
					V_DIE("Invalid opcode: %u", static_cast<FOpcodeInt>(State.PC->Opcode));
			}

			if (UnblockedSuspensionQueue)
			{
				goto SuspensionInterpreterLoop;
			}
		}

#undef OP_IMPL_THREAD_EFFECTS
#undef BEGIN_OP_CASE
#undef END_OP_CASE
#undef ENQUEUE_SUSPENSION
#undef FAIL
#undef YIELD

		// Macro definitions to be used in the suspension interpreter loop.

#define OP_IMPL_THREAD_EFFECTS(OpName)                   \
	BEGIN_OP_CASE(OpName)                                \
	{                                                    \
		OP_IMPL_HELPER(OpName##Impl)                     \
		DEF(Op.EffectToken, VValue::EffectDoneMarker()); \
	}                                                    \
	END_OP_CASE()

#define BEGIN_OP_CASE(Name)                                                                              \
	case EOpcode::Name:                                                                                  \
	{                                                                                                    \
		F##Name##SuspensionCaptures& Op = BytecodeSuspension.GetCaptures<F##Name##SuspensionCaptures>(); \
		if constexpr (bPrintTrace)                                                                       \
		{                                                                                                \
			BeginTrace(Op, BytecodeSuspension);                                                          \
		}

#define END_OP_CASE()                                                  \
	FinishedExecutingSuspensionIn(*BytecodeSuspension.FailureContext); \
	if constexpr (bPrintTrace)                                         \
	{                                                                  \
		EndTraceWithCaptures(Op, false, false);                        \
	}                                                                  \
	break;                                                             \
	}

#define ENQUEUE_SUSPENSION(Value)                         \
	Value.EnqueueSuspension(Context, *CurrentSuspension); \
	if constexpr (bPrintTrace)                            \
	{                                                     \
		EndTraceWithCaptures(Op, true, false);            \
	}                                                     \
	break

#define FAIL()                                                        \
	if constexpr (bPrintTrace)                                        \
	{                                                                 \
		EndTraceWithCaptures(Op, false, true);                        \
	}                                                                 \
	if (Fail(*BytecodeSuspension.FailureContext) == FOpResult::Error) \
	{                                                                 \
		return FOpResult::Error;                                      \
	}                                                                 \
	break

#define YIELD()                                                                                \
	FinishedExecutingSuspensionIn(*BytecodeSuspension.FailureContext);                         \
	if constexpr (bPrintTrace)                                                                 \
	{                                                                                          \
		EndTraceWithCaptures(Op, false, false);                                                \
	}                                                                                          \
	Suspend(*BytecodeSuspension.FailureContext, *BytecodeSuspension.Task, MakeReturnSlot(Op)); \
	break

	SuspensionInterpreterLoop:
		EscapeFailureContext();
		do
		{
			check(!!UnblockedSuspensionQueue);

			// We want the enqueueing of newly-unblocked suspensions to go onto the unblocked suspension
			// queue, while also allowing newly-blocked suspensions to be enqueued on a different suspension queue instead.
			// This allows us to avoid linking both suspension queues together, which would form an execution cycle.
			VSuspension* CurrentSuspension = UnblockedSuspensionQueue;
			UnblockedSuspensionQueue = UnblockedSuspensionQueue->Next.Get();
			CurrentSuspension->Next.Set(Context, nullptr);

			if (!CurrentSuspension->FailureContext->bFailed)
			{
#if WITH_EDITORONLY_DATA
				FPackageScope PackageScope = Context.SetCurrentPackage(CurrentSuspension->CurrentPackage.Get());
#endif

				if (VLambdaSuspension* LambdaSuspension = CurrentSuspension->DynamicCast<VLambdaSuspension>())
				{
					LambdaSuspension->Callback(Context, *LambdaSuspension, UnblockedSuspensionQueue);
				}
				else
				{
					VBytecodeSuspension& BytecodeSuspension = CurrentSuspension->StaticCast<VBytecodeSuspension>();

					switch (BytecodeSuspension.Opcode)
					{
						OP(Add)
						OP(Sub)
						OP(Mul)
						OP(Div)
						OP(Mod)
						OP(Neg)

						OP(MutableAdd)

						OP(Neq)
						OP(Lt)
						OP(Lte)
						OP(Gt)
						OP(Gte)

						OP(Query)

						OP_IMPL_THREAD_EFFECTS(Melt)
						OP_IMPL_THREAD_EFFECTS(Freeze)

						OP_IMPL_THREAD_EFFECTS(VarGet)
						OP_IMPL_THREAD_EFFECTS(VarSet)
						OP_IMPL_THREAD_EFFECTS(SetField)
						OP(CreateField)
						OP(UnwrapNativeConstructorWrapper)
						OP_IMPL_THREAD_EFFECTS(CallSet)

						OP(Length)
						OP(NewMutableArrayWithCapacity)
						OP_IMPL_THREAD_EFFECTS(ArrayAdd)
						OP(InPlaceMakeImmutable)
						OP(MapKey)
						OP(MapValue)
						OP(NewClass)
						OP(BindNativeClass)
						OP(UnifyField)

						OP_WITH_IMPL(LoadField, LoadFieldImpl)
						OP_WITH_IMPL(LoadFieldICOffset, LoadFieldImpl)
						OP_WITH_IMPL(LoadFieldICConstant, LoadFieldImpl)
						OP_WITH_IMPL(LoadFieldICFunction, LoadFieldImpl)
						OP_WITH_IMPL(LoadFieldICNativeFunction, LoadFieldImpl)
						OP(LoadFieldFromSuper)

						// An indexed access (i.e. `B := A[10]`) is just the same as `Call(B, A, 10)`.
						BEGIN_OP_CASE(Call)
						{
							VValue Callee = GetOperand(Op.Callee);
							REQUIRE_CONCRETE(Callee);

							if (VFunction* Function = Callee.DynamicCast<VFunction>())
							{
								FOp* CallerPC = nullptr;
								VFrame* CallerFrame = nullptr;
								VValue ReturnSlot = MakeReturnSlot(Op);
								TArrayView<TWriteBarrier<VValue>> Arguments = GetOperands(Op.Arguments);
								TArrayView<TWriteBarrier<VUniqueString>> NamedArguments(Op.NamedArguments);
								TArrayView<TWriteBarrier<VValue>> NamedArgumentValues = GetOperands(Op.NamedArgumentValues);
								VFrame& NewFrame = MakeFrameForCallee(
									Context,
									CallerPC,
									CallerFrame,
									ReturnSlot,
									*Function->Procedure,
									Function->Self,
									Function->ParentScope,
									Arguments.Num(),
									&NamedArguments,
									[&](uint32 Arg) {
										return GetOperand(Arguments[Arg]);
									},
									[&](uint32 NamedArg) {
										return GetOperand(NamedArgumentValues[NamedArg]);
									});
								NewFrame.ReturnSlot.EffectToken.Set(Context, GetOperand(Op.ReturnEffectToken));
								VFailureContext& FailureContext = *BytecodeSuspension.FailureContext;
								VTask& TaskContext = *BytecodeSuspension.Task;
								FInterpreter Interpreter(
									Context,
									FExecutionState(Function->GetProcedure().GetOpsBegin(), &NewFrame),
									&FailureContext,
									Task,
									GetOperand(Op.EffectToken));
								FOpResult::EKind Result = Interpreter.Execute();
								if (Result == FOpResult::Error)
								{
									return Result;
								}
								V_DIE_UNLESS(Result == FOpResult::Return);
							}
							else
							{
								FOpResult Result = CallImpl(Op, Callee, BytecodeSuspension.Task.Get(), GetOperand(Op.EffectToken));
								switch (Result.Kind)
								{
									case FOpResult::Return:
									case FOpResult::Yield:
										DEF(Op.ReturnEffectToken, GetOperand(Op.EffectToken));
										break;

									case FOpResult::Block:
									case FOpResult::Fail:
									case FOpResult::Error:
										break;
								}
								OP_RESULT_HELPER(Result);
							}
						}
						END_OP_CASE()

						BEGIN_OP_CASE(CallWithSelf) // suspension version
						{
							VValue Callee = GetOperand(Op.Callee);
							REQUIRE_CONCRETE(Callee);

							VValue Self = GetOperand(Op.Self);
							REQUIRE_CONCRETE(Self);

							V_DIE_IF_MSG(Callee.IsCellOfType<VProcedure>(), "`CallWithSelf` should be passed a `VFunction`-without-`Self` set, not a `VProcedure`! This indicates an issue with the codegen.");
							if (VFunction* Function = Callee.DynamicCast<VFunction>())
							{
								checkSlow(!Function->HasSelf());
								FOp* CallerPC = nullptr;
								VFrame* CallerFrame = nullptr;
								VValue ReturnSlot = MakeReturnSlot(Op);
								TArrayView<TWriteBarrier<VValue>> Arguments = GetOperands(Op.Arguments);
								TArrayView<TWriteBarrier<VUniqueString>> NamedArguments(Op.NamedArguments);
								TArrayView<TWriteBarrier<VValue>> NamedArgumentValues = GetOperands(Op.NamedArgumentValues);
								VFrame& NewFrame = MakeFrameForCallee(
									Context,
									CallerPC,
									CallerFrame,
									ReturnSlot,
									Function->GetProcedure(),
									{Context, Self},
									// TODO: (yiliang.siew) `(super:)` can't be referenced in a constructor yet, but when it can,
									// this can't just be an empty scope. It'll need the class's scope that contains `(super:)`
									/* Scope*/ {},
									Arguments.Num(),
									&NamedArguments,
									[&](uint32 Arg) {
										return GetOperand(Arguments[Arg]);
									},
									[&](uint32 NamedArg) {
										return GetOperand(NamedArgumentValues[NamedArg]);
									});
								NewFrame.ReturnSlot.EffectToken.Set(Context, GetOperand(Op.ReturnEffectToken));
								VFailureContext& FailureContext = *BytecodeSuspension.FailureContext;
								VTask& TaskContext = *BytecodeSuspension.Task;
								FInterpreter Interpreter(
									Context,
									FExecutionState(Function->GetProcedure().GetOpsBegin(), &NewFrame),
									&FailureContext,
									Task,
									GetOperand(Op.EffectToken));
								FOpResult::EKind Result = Interpreter.Execute();
								if (Result == FOpResult::Error)
								{
									return Result;
								}
								V_DIE_UNLESS(Result == FOpResult::Return);
							}
							else if (VNativeFunction* NativeFunction = Callee.DynamicCast<VNativeFunction>(); NativeFunction)
							{
								// `Self` binding is handled internally within `CallImpl`.
								FOpResult Result = CallImpl(Op, Callee, BytecodeSuspension.Task.Get(), GetOperand(Op.EffectToken));
								switch (Result.Kind)
								{
									case FOpResult::Return:
									case FOpResult::Yield:
										DEF(Op.ReturnEffectToken, GetOperand(Op.EffectToken));
										break;

									case FOpResult::Block:
									case FOpResult::Fail:
									case FOpResult::Error:
										break;
								}
								OP_RESULT_HELPER(Result);
							}
							else
							{
								V_DIE("Unsupported operand passed to `CallWithSelf`!");
							}
						}
						END_OP_CASE() // CallWithSelf

						OP(NewObject)

						default:
							V_DIE("Invalid opcode: %u", static_cast<FOpcodeInt>(State.PC->Opcode));
					}
				}
			}
		}
		while (UnblockedSuspensionQueue);

		if (!UnwindIfNeeded())
		{
			return FOpResult::Return;
		}
		if (!YieldIfNeeded(State.PC))
		{
			return FOpResult::Return;
		}

		goto MainInterpreterLoop;

#undef OP_IMPL_THREAD_EFFECTS
#undef BEGIN_OP_CASE
#undef END_OP_CASE
#undef ENQUEUE_SUSPENSION
#undef FAIL
#undef YIELD
#undef RUNTIME_ERROR
#undef RAISE_RUNTIME_ERROR_CODE
#undef RAISE_RUNTIME_ERROR
#undef RAISE_RUNTIME_ERROR_FORMAT
	}

#undef OP_RESULT_HELPER

public:
	FInterpreter(FRunningContext Context, FExecutionState State, VFailureContext* FailureContext, VTask* Task, VValue IncomingEffectToken, FOp* StartPC = nullptr, FOp* EndPC = nullptr)
		: Context(Context)
		, State(State)
		, Task(Task)
		, OutermostFailureContext(FailureContext)
		, OutermostTask(Task)
		, OutermostStartPC(StartPC)
		, OutermostEndPC(EndPC)
		, _FailureContext(FailureContext)
	{
		V_DIE_UNLESS(OutermostFailureContext);
		V_DIE_UNLESS(!!OutermostStartPC == !!OutermostEndPC);
		EffectToken.Set(Context, IncomingEffectToken);
	}

	FOpResult::EKind Execute()
	{
		V_DIE_UNLESS(AutoRTFM::ForTheRuntime::GetContextStatus() == AutoRTFM::EContextStatus::OnTrack);

		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			if (OutermostStartPC)
			{
				return ExecuteImpl<true>(true);
			}
			else
			{
				return ExecuteImpl<true>(false);
			}
		}
		else
		{
			if (OutermostStartPC)
			{
				return ExecuteImpl<false>(true);
			}
			else
			{
				return ExecuteImpl<false>(false);
			}
		}
	}

	static FOpResult InvokeWithSelf(FRunningContext Context, VFunction& Function, VValue Self, VFunction::Args&& IncomingArguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs, VFunction::Args* NamedArgVals)
	{
		// This function expects to be run in the open
		check(!AutoRTFM::IsClosed());

		VRestValue ReturnSlot(0);

		VFunction::Args Arguments = MoveTemp(IncomingArguments);

		FOp* CallerPC = &StopInterpreterSentry;
		VFrame* CallerFrame = nullptr;
		TArrayView<TWriteBarrier<VUniqueString>> NamedArgsViewStorage;
		TArrayView<TWriteBarrier<VUniqueString>>* NamedArgsView = nullptr;
		if (NamedArgs)
		{
			NamedArgsViewStorage = *NamedArgs;
			NamedArgsView = &NamedArgsViewStorage;
		}
		VFrame& Frame = MakeFrameForCallee(
			Context, CallerPC, CallerFrame, &ReturnSlot, *Function.Procedure, {Context, Self}, Function.ParentScope, Arguments.Num(), NamedArgsView,
			[&](uint32 Arg) {
				return Arguments[Arg];
			},
			[&](uint32 NamedArg) {
				return (*NamedArgVals)[NamedArg];
			});

		// Check if we're inside native C++ code that was invoked by Verse
		const FNativeFrame* NativeFrame = Context.NativeFrame();
		V_DIE_UNLESS(NativeFrame);

		FInterpreter Interpreter(
			Context,
			FExecutionState(Function.GetProcedure().GetOpsBegin(), &Frame),
			NativeFrame->FailureContext,
			NativeFrame->Task,
			VValue::EffectDoneMarker());

		FOpResult::EKind Result = Interpreter.Execute();

		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			UE_LOG(LogVerseVM, Display, TEXT("\n"));
		}

		if constexpr (DoStats)
		{
			UE_LOG(LogVerseVM, Display, TEXT("Num Transactions: %lf"), TotalNumFailureContexts);
			UE_LOG(LogVerseVM, Display, TEXT("Num Reuses: %lf"), NumReuses);
			UE_LOG(LogVerseVM, Display, TEXT("Hit rate: %lf"), NumReuses / TotalNumFailureContexts);
		}

		return {Result, Result == FOpResult::Return ? ReturnSlot.Get(Context) : VValue()};
	}

	static FOpResult Spawn(FRunningContext Context, VValue CalleeValue, VFunction::Args&& IncomingArguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs, VFunction::Args* NamedArgVals)
	{
		// This function expects to be run in the open
		check(!AutoRTFM::IsClosed());

		const FNativeFrame* NativeFrame = Context.NativeFrame();
		V_DIE_UNLESS(NativeFrame);

		VTask* Task = &VTask::New(Context, &StopInterpreterSentry, VFrame::GlobalEmptyFrame.Get(), /*YieldTask*/ nullptr, /*Parent*/ nullptr);

		VTask::FCallerSpec CallerSpec = VTask::MakeFrameForSpawn(Context);

		VFunction::Args Arguments = MoveTemp(IncomingArguments);

		FOpResult::EKind Result = FOpResult::Return;
		if (VFunction* Callee = CalleeValue.DynamicCast<VFunction>())
		{
			TArrayView<TWriteBarrier<VUniqueString>> NamedArgsViewStorage;
			TArrayView<TWriteBarrier<VUniqueString>>* NamedArgsView = nullptr;
			if (NamedArgs)
			{
				NamedArgsViewStorage = *NamedArgs;
				NamedArgsView = &NamedArgsViewStorage;
			}
			VFrame& Frame = MakeFrameForCallee(
				Context,
				CallerSpec.PC,
				CallerSpec.Frame,
				CallerSpec.ReturnSlot,
				*Callee->Procedure,
				Callee->Self,
				Callee->ParentScope,
				Arguments.Num(),
				NamedArgsView,
				[&](uint32 Arg) {
					return Arguments[Arg];
				},
				[&](uint32 NamedArg) {
					return (*NamedArgVals)[NamedArg];
				});

			FInterpreter Interpreter(
				Context,
				FExecutionState(Callee->GetProcedure().GetOpsBegin(), &Frame),
				NativeFrame->FailureContext,
				Task,
				VValue::EffectDoneMarker());

			Result = Interpreter.Execute();
		}
		else if (VNativeFunction* NativeCallee = CalleeValue.DynamicCast<VNativeFunction>())
		{
			V_DIE_IF(NamedArgs);

			Task->Suspend(Context); // So that Call.Return invokes Task->Resume()
			Task->ResumePC = CallerSpec.PC;
			Task->ResumeFrame.Set(Context, CallerSpec.Frame);
			Task->ResumeSlot.Set(Context, CallerSpec.ReturnSlot);

			FAccessContext(Context).PushNativeFrame(
				NativeFrame->FailureContext,
				NativeCallee,
				CallerSpec.PC,
				CallerSpec.Frame,
				Task, [&] {
					Result = NativeCallee->Thunk(Context, NativeCallee->Self.Get(), Arguments).Kind;
				});
		}

		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			UE_LOG(LogVerseVM, Display, TEXT("\n"));
		}

		// TODO: `spawn->native function` calls are not filling in the 'native return value' which causes failure to be returned from the VNI glue.
		//		 This should be fixed then we can enable this check again for soundness. For now we just return the task regardless.
		//
		// We expect Result here to be either Return (the callee completed), Yield (the callee suspended), or Error (a runtime error occurred)
		// V_DIE_IF(Result == FOpResult::Fail || Result == FOpResult::Block);

		return {Result, *Task};
	}

	static FOpResult::EKind Resume(FRunningContext Context, VValue ResumeArgument, VTask& Task)
	{
		// This function expects to be run in the open + inside native C++ code that was invoked by Verse
		V_DIE_UNLESS(!AutoRTFM::IsClosed() && Context.NativeFrame());

		if (Task.Phase != VTask::EPhase::Active)
		{
			return FOpResult::Return;
		}

		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			UE_LOG(LogVerseVM, Display, TEXT(""));
			UE_LOG(LogVerseVM, Display, TEXT("Resuming:"));
		}

		Task.Resume(Context);

		FInterpreter Interpreter(
			Context,
			FExecutionState(Task.ResumePC, Task.ResumeFrame.Get()),
			Context.NativeFrame()->FailureContext,
			&Task,
			VValue::EffectDoneMarker());

		Task.ExecNativeDefer(Context);

		bool bExecute = true;
		if (!FInterpreter::Def(Context, Task.ResumeSlot, ResumeArgument, Interpreter.UnblockedSuspensionQueue))
		{
			Interpreter.Fail(*Interpreter.FailureContext());
			bExecute = Interpreter.UnwindIfNeeded();
		}

		FOpResult::EKind Result = FOpResult::Return;
		if (bExecute)
		{
			Result = Interpreter.Execute();
		}

		V_DIE_IF(Result == FOpResult::Fail);
		return Result;
	}

	static FOpResult::EKind Unwind(FRunningContext Context, VTask& Task)
	{
		// This function expects to be run in the open + inside native C++ code that was invoked by Verse
		V_DIE_UNLESS(!AutoRTFM::IsClosed() && Context.NativeFrame());

		V_DIE_UNLESS(Task.Phase == VTask::EPhase::CancelStarted && !Task.LastChild);

		if (CVarTraceExecution.GetValueOnAnyThread())
		{
			UE_LOG(LogVerseVM, Display, TEXT(""));
			UE_LOG(LogVerseVM, Display, TEXT("Unwinding:"));
		}

		Task.Resume(Context);

		FInterpreter Interpreter(
			Context,
			FExecutionState(Task.ResumePC, Task.ResumeFrame.Get()),
			Context.NativeFrame()->FailureContext,
			&Task,
			VValue::EffectDoneMarker());

		Interpreter.BeginUnwind(Interpreter.State.PC);
		FOpResult::EKind Result = Interpreter.Execute();

		V_DIE_IF(Result == FOpResult::Fail);
		return Result;
	}
}; // namespace Verse

FOpResult VFunction::InvokeWithSelf(FRunningContext Context, VValue InSelf, Args&& Arguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs, Args* NamedArgVals)
{
	FOpResult Result = FInterpreter::InvokeWithSelf(Context, *this, InSelf, MoveTemp(Arguments), NamedArgs, NamedArgVals);
	check(!Result.IsReturn() || !Result.Value.IsPlaceholder());
	return Result;
}

FOpResult VFunction::InvokeWithSelf(FRunningContext Context, VValue InSelf, VValue Argument, TWriteBarrier<VUniqueString>* NamedArg)
{
	if (NamedArg)
	{
		TArray<TWriteBarrier<VUniqueString>> NamedArgs{*NamedArg};
		Args NamedArgVals{Argument};
		FOpResult Result = FInterpreter::InvokeWithSelf(Context, *this, InSelf, VFunction::Args{Argument}, &NamedArgs, &NamedArgVals);
		return Result;
	}
	FOpResult Result = FInterpreter::InvokeWithSelf(Context, *this, InSelf, VFunction::Args{Argument}, nullptr, nullptr);
	check(!Result.IsReturn() || !Result.Value.IsPlaceholder());
	return Result;
}

FOpResult VFunction::Spawn(FRunningContext Context, VValue Callee, Args&& Arguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs /*= nullptr*/, Args* NamedArgVals /*= nullptr*/)
{
	return FInterpreter::Spawn(Context, Callee, MoveTemp(Arguments), NamedArgs, NamedArgVals);
}

FOpResult::EKind VTask::Resume(FRunningContext Context, VValue ResumeArgument)
{
	return FInterpreter::Resume(Context, ResumeArgument, *this);
}

FOpResult::EKind VTask::Unwind(FRunningContext Context)
{
	return FInterpreter::Unwind(Context, *this);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
