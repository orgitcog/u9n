// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTest.h"
#include "StateTreeTestBase.h"
#include "StateTreeTestTypes.h"

#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeCompiler.h"
#include "Conditions/StateTreeCommonConditions.h"

#define LOCTEXT_NAMESPACE "AITestSuite_StateTreeTest"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::StateTree::Tests
{

struct FStateTreeTest_TransitionPriority : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

		/*
			- Root
				- State1 : Task1 -> Succeeded
					- State1A : Task1A -> Next
					- State1B : Task1B -> Next
					- State1C : Task1C
		
			Task1A completed first, transitioning to State1B.
			Task1, Task1B, and Task1C complete at the same time, we should take the transition on the first completed state (State1).
		*/
		
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UStateTreeState& State1A = State1.AddChildState(FName(TEXT("State1A")));
		UStateTreeState& State1B = State1.AddChildState(FName(TEXT("State1B")));
		UStateTreeState& State1C = State1.AddChildState(FName(TEXT("State1C")));

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 2;
		State1.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
		
		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		Task1A.GetNode().TicksToCompletion = 1;
		State1A.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::NextState);

		auto& Task1B = State1B.AddTask<FTestTask_Stand>(FName(TEXT("Task1B")));
		Task1B.GetNode().TicksToCompletion = 2;
		State1B.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::NextState);

		auto& Task1C = State1C.AddTask<FTestTask_Stand>(FName(TEXT("Task1C")));
		Task1C.GetNode().TicksToCompletion = 2;
		
		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));
		const FString StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE("StateTree Task1 should enter state", Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE("StateTree Task1A should enter state", Exec.Expect(Task1A.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from Task1A to Task1B
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task1A should complete", Exec.Expect(Task1A.GetName(), StateCompletedStr));
		AITEST_TRUE("StateTree Task1B should enter state", Exec.Expect(Task1B.GetName(), EnterStateStr));
		Exec.LogClear();

		// Task1 completes, and we should take State1 transition. 
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task1 should complete", Exec.Expect(Task1.GetName(), StateCompletedStr));
		AITEST_EQUAL("Tree execution should stop on success", Status, EStateTreeRunStatus::Succeeded);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionPriority, "System.StateTree.Transition.Priority");

struct FStateTreeTest_TransitionPriorityEnterState : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		
		UStateTreeState& Root =	EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& State0 = Root.AddChildState(FName(TEXT("State0")));
		UStateTreeState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UStateTreeState& State1A = State1.AddChildState(FName(TEXT("State1A")));
		UStateTreeState& State2 = Root.AddChildState(FName(TEXT("State2")));
		UStateTreeState& State3 = Root.AddChildState(FName(TEXT("State3")));

		auto& Task0 = State0.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		State0.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &State1);

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().EnterStateResult = EStateTreeRunStatus::Failed;
		State1.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &State2);
		
		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		State1A.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &State3);

		auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		State2.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		auto& Task3 = State3.AddTask<FTestTask_Stand>(FName(TEXT("Task3")));
		State3.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		
		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));
		const FString StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE("StateTree Task0 should enter state", Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from State0 to State1, it should fail (Task1), and the transition on State1->State2 (and not State1A->State3)
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should complete", Exec.Expect(Task0.GetName(), StateCompletedStr));
		AITEST_TRUE("StateTree Task2 should enter state", Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_FALSE("StateTree Task3 should not enter state", Exec.Expect(Task3.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionPriorityEnterState, "System.StateTree.Transition.PriorityEnterState");

struct FStateTreeTest_TransitionNextSelectableState : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

		UStateTreeState& Root =	EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& State0 = Root.AddChildState(FName(TEXT("State0")));
		UStateTreeState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UStateTreeState& State2 = Root.AddChildState(FName(TEXT("State2")));

		auto& EvalA = EditorData.AddEvaluator<FTestEval_A>();
		EvalA.GetInstanceData().bBoolA = true;

		auto& Task0 = State0.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		State0.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::NextSelectableState);

		// Add Task 1 with Condition that will always fail
		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		auto& BoolCond1 = State1.AddEnterCondition<FStateTreeCompareBoolCondition>();

		EditorData.AddPropertyBinding(EvalA, TEXT("bBoolA"), BoolCond1, TEXT("bLeft"));
		BoolCond1.GetInstanceData().bRight = !EvalA.GetInstanceData().bBoolA;

		// Add Task 2 with Condition that will always succeed
		auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		auto& BoolCond2 = State2.AddEnterCondition<FStateTreeCompareBoolCondition>();
		State2.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

		EditorData.AddPropertyBinding(EvalA, TEXT("bBoolA"), BoolCond2, TEXT("bLeft"));
		BoolCond2.GetInstanceData().bRight = EvalA.GetInstanceData().bBoolA;

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));
		const FString StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Exec.Start();
		AITEST_TRUE("StateTree Task0 should enter state", Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from State0 and tries to select State1. It should fail (Task1) and because transition is set to "Next Selectable", it should now select Task 2 and Enter State
		Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should complete", Exec.Expect(Task0.GetName(), StateCompletedStr));
		AITEST_FALSE("StateTree Task1 should not enter state", Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE("StateTree Task2 should enter state", Exec.Expect(Task2.GetName(), EnterStateStr));
		Exec.LogClear();

		// Complete Task2
		Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task2 should complete", Exec.Expect(Task2.GetName(), StateCompletedStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionNextSelectableState, "System.StateTree.Transition.NextSelectableState");

struct FStateTreeTest_TransitionNextWithParentData : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

		UStateTreeState& Root =	EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& State0 = Root.AddChildState(FName(TEXT("State0")));
		UStateTreeState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UStateTreeState& State1A = State1.AddChildState(FName(TEXT("State1A")));

		auto& RootTask = Root.AddTask<FTestTask_B>(FName(TEXT("RootTask")));
		RootTask.GetInstanceData().bBoolB = true;

		auto& Task0 = State0.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		State0.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::NextState);

		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		auto& BoolCond1 = State1A.AddEnterCondition<FStateTreeCompareBoolCondition>();

		EditorData.AddPropertyBinding(RootTask, TEXT("bBoolB"), BoolCond1, TEXT("bLeft"));
		BoolCond1.GetInstanceData().bRight = true;

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));
		const FString StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Exec.Start();
		AITEST_TRUE("StateTree Task0 should enter state", Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from State0 and tries to select State1.
		// This tests that data from current shared active states (Root) is available during state selection.
		Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should complete", Exec.Expect(Task0.GetName(), StateCompletedStr));
		AITEST_TRUE("StateTree Task1A should enter state", Exec.Expect(Task1A.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionNextWithParentData, "System.StateTree.Transition.NextWithParentData");

struct FStateTreeTest_TransitionGlobalDataView : FStateTreeTestBase
{
	// Tests that the global eval and task dataviews are kept up to date when transitioning from  
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& StateA = Root.AddChildState(FName(TEXT("A")));
		UStateTreeState& StateB = Root.AddChildState(FName(TEXT("B")));

		auto& EvalA = EditorData.AddEvaluator<FTestEval_A>(FName(TEXT("Eval")));
		EvalA.GetInstanceData().IntA = 42;
		auto& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>(FName(TEXT("Global")));
		GlobalTask.GetInstanceData().Value = 123;
		
		// State A
		auto& Task0 = StateA.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		StateA.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &StateB);

		// State B
		auto& Task1 = StateB.AddTask<FTestTask_PrintValue>(FName(TEXT("Task1")));
		EditorData.AddPropertyBinding(EvalA, TEXT("IntA"), Task1, TEXT("Value"));
		auto& Task2 = StateB.AddTask<FTestTask_PrintValue>(FName(TEXT("Task2")));
		EditorData.AddPropertyBinding(GlobalTask, TEXT("Value"), Task2, TEXT("Value"));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);

		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString EnterStateStr(TEXT("EnterState"));
		const FString EnterState42Str(TEXT("EnterState42"));
		const FString EnterState123Str(TEXT("EnterState123"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE("StateTree Task0 should enter state", Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from StateA to StateB, Task0 should enter state with evaluator value copied.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should enter state with value 42", Exec.Expect(Task1.GetName(), EnterState42Str));
		AITEST_TRUE("StateTree Task1 should enter state with value 123", Exec.Expect(Task2.GetName(), EnterState123Str));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionGlobalDataView, "System.StateTree.Transition.GlobalDataView");

struct FStateTreeTest_TransitionDelay : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		const FGameplayTag Tag = GetTestTag1();

		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& StateA = Root.AddChildState(FName(TEXT("A")));
		UStateTreeState& StateB = Root.AddChildState(FName(TEXT("B")));

		// State A
		auto& Task0 = StateA.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		Task0.GetNode().TicksToCompletion = 100;
		
		FStateTreeTransition& Transition = StateA.AddTransition(EStateTreeTransitionTrigger::OnEvent, EStateTreeTransitionType::GotoState, &StateB);
		Transition.bDelayTransition = true;
		Transition.DelayDuration = 0.15f;
		Transition.DelayRandomVariance = 0.0f;
		Transition.RequiredEvent.Tag = Tag;

		// State B
		auto& Task1 = StateB.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 100;

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);

		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));
		const FString StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE("StateTree Task0 should enter state", Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// This should cause delayed transition.
		Exec.SendEvent(Tag);
		
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should tick", Exec.Expect(Task0.GetName(), TickStr));
		Exec.LogClear();

		// Should have execution frames
		AITEST_TRUE("Should have active frames", InstanceData.GetExecutionState()->ActiveFrames.Num() > 0);

		// Should have delayed transitions
		const int32 NumDelayedTransitions0 = InstanceData.GetExecutionState()->DelayedTransitions.Num();
		AITEST_EQUAL("Should have a delayed transition", NumDelayedTransitions0, 1);

		// Tick and expect a delayed transition. 
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should tick", Exec.Expect(Task0.GetName(), TickStr));
		Exec.LogClear();

		const int32 NumDelayedTransitions1 = InstanceData.GetExecutionState()->DelayedTransitions.Num();
		AITEST_EQUAL("Should have a delayed transition", NumDelayedTransitions1, 1);

		// Should complete delayed transition.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should exit state", Exec.Expect(Task0.GetName(), ExitStateStr));
		AITEST_TRUE("StateTree Task1 should enter state", Exec.Expect(Task1.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionDelay, "System.StateTree.Transition.Delay");

struct FStateTreeTest_TransitionDelayZero : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		const FGameplayTag Tag = GetTestTag1();

		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& StateA = Root.AddChildState(FName(TEXT("A")));
		UStateTreeState& StateB = Root.AddChildState(FName(TEXT("B")));

		// State A
		auto& Task0 = StateA.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		Task0.GetNode().TicksToCompletion = 100;
		
		FStateTreeTransition& Transition = StateA.AddTransition(EStateTreeTransitionTrigger::OnEvent, EStateTreeTransitionType::GotoState, &StateB);
		Transition.bDelayTransition = true;
		Transition.DelayDuration = 0.0f;
		Transition.DelayRandomVariance = 0.0f;
		Transition.RequiredEvent.Tag = Tag;

		// State B
		auto& Task1 = StateB.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 100;

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);

		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));
		const FString StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE("StateTree Task0 should enter state", Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// This should cause delayed transition. Because the time is 0, it should happen immediately.
		Exec.SendEvent(Tag);
		
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Task0 should exit state", Exec.Expect(Task0.GetName(), ExitStateStr));
		AITEST_TRUE("StateTree Task1 should enter state", Exec.Expect(Task1.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TransitionDelayZero, "System.StateTree.Transition.DelayZero");

struct FStateTreeTest_PassingTransitionEventToStateSelection : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		FPropertyBindingPath PathToPayloadMember;
		{
			const bool bParseResult = PathToPayloadMember.FromString(TEXT("Payload.A"));

			AITEST_TRUE("Parsing path should succeeed", bParseResult);

			FStateTreeEvent EventWithPayload;
			EventWithPayload.Payload = FInstancedStruct::Make<FStateTreeTest_PropertyStructA>();
			const bool bUpdateSegments = PathToPayloadMember.UpdateSegmentsFromValue(FStateTreeDataView(FStructView::Make(EventWithPayload)));
			AITEST_TRUE("Updating segments should succeeed", bUpdateSegments);
		}

		// This state shouldn't be selected, because transition's condition and state's enter condition exlude each other.
		UStateTreeState& StateA = Root.AddChildState(FName(TEXT("A")));
		StateA.bHasRequiredEventToEnter  = true;
		StateA.RequiredEventToEnter.PayloadStruct = FStateTreeTest_PropertyStructA::StaticStruct();
		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		TStateTreeEditorNode<FStateTreeCompareIntCondition>& AIntCond = StateA.AddEnterCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
		AIntCond.GetInstanceData().Right = 0;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(StateA.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(AIntCond.ID, TEXT("Left")));

		// This state should be selected as the sent event fullfils both transition's condition and state's enter condition.
		UStateTreeState& StateB = Root.AddChildState(FName(TEXT("B")));
		StateB.bHasRequiredEventToEnter  = true;
		StateB.RequiredEventToEnter.PayloadStruct = FStateTreeTest_PropertyStructA::StaticStruct();
		auto& TaskB = StateB.AddTask<FTestTask_PrintValue>(FName(TEXT("TaskB")));
		// Test copying data from the state event. The condition properties are copied from temp instance data during selection, this gets copied from active instance data.
		TaskB.GetInstanceData().Value = -1; // Initially -1, expected to be overridden by property binding below. 
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(StateB.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(TaskB.ID, TEXT("Value")));
		
		TStateTreeEditorNode<FStateTreeCompareIntCondition>& BIntCond = StateB.AddEnterCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
		BIntCond.GetInstanceData().Right = 1;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(StateB.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(BIntCond.ID, TEXT("Left")));

		// This state should be selected only initially when there's not event in the queue.
		UStateTreeState& StateInitial = Root.AddChildState(FName(TEXT("Initial")));
		auto& TaskInitial = StateInitial.AddTask<FTestTask_Stand>(FName(TEXT("TaskInitial")));
		// Transition from Initial -> StateA
		FStateTreeTransition& TransA = StateInitial.AddTransition(EStateTreeTransitionTrigger::OnEvent, FGameplayTag(), EStateTreeTransitionType::GotoState, &StateA);
		TransA.RequiredEvent.PayloadStruct = FStateTreeTest_PropertyStructA::StaticStruct();
		TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransAIntCond = TransA.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
		TransAIntCond.GetInstanceData().Right = 1;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(TransA.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(TransAIntCond.ID, TEXT("Left")));
		// Transition from Initial -> StateB
		FStateTreeTransition& TransB = StateInitial.AddTransition(EStateTreeTransitionTrigger::OnEvent, FGameplayTag(), EStateTreeTransitionType::GotoState, &StateB);
		TransB.RequiredEvent.PayloadStruct = FStateTreeTest_PropertyStructA::StaticStruct();
		TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransBIntCond = TransB.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
		TransBIntCond.GetInstanceData().Right = 1;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(TransB.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(TransBIntCond.ID, TEXT("Left")));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);

		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString EnterStateStr(TEXT("EnterState"));

		Status = Exec.Start();
		AITEST_TRUE("StateTree TaskInitial should enter state", Exec.Expect(TaskInitial.GetName(), EnterStateStr));
		Exec.LogClear();

		// The conditions test for payload Value=1, the first event should not trigger transition. 
		Exec.SendEvent(GetTestTag1(), FConstStructView::Make(FStateTreeTest_PropertyStructA{0}));
		Exec.SendEvent(GetTestTag1(), FConstStructView::Make(FStateTreeTest_PropertyStructA{1}));
		Status = Exec.Tick(0.1f);

		AITEST_FALSE("StateTree TaskA should not enter state", Exec.Expect(TaskA.GetName(), EnterStateStr));
		AITEST_TRUE("StateTree TaskB should enter state", Exec.Expect(TaskB.GetName(), TEXT("EnterState1"))); // TaskB decorates "EnterState" with value from the payload.
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_PassingTransitionEventToStateSelection, "System.StateTree.Transition.PassingTransitionEventToStateSelection");

struct FStateTreeTest_FollowTransitions : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);
		RootPropertyBag.AddProperty(FName(TEXT("Int")), EPropertyBagPropertyType::Int32);
		RootPropertyBag.SetValueInt32(FName(TEXT("Int")), 1);

		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& StateTrans = Root.AddChildState(FName(TEXT("Trans")));
		UStateTreeState& StateA = Root.AddChildState(FName(TEXT("A")));
		UStateTreeState& StateB = Root.AddChildState(FName(TEXT("B")));
		UStateTreeState& StateC = Root.AddChildState(FName(TEXT("C")));

		// Root

		// Trans
		{
			StateTrans.SelectionBehavior = EStateTreeStateSelectionBehavior::TryFollowTransitions;

			{
				// This transition should be skipped due to the condition
				FStateTreeTransition& TransA = StateTrans.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateA);
				TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransIntCond = TransA.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 0;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}

			{
				// This transition leads to selection, but will be overridden.
				FStateTreeTransition& TransB = StateTrans.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateB);
				TransB.Priority = EStateTreeTransitionPriority::Normal;
				TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransIntCond = TransB.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}

			{
				// This transition is selected, should override previous one due to priority.
				FStateTreeTransition& TransC = StateTrans.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateC);
				TransC.Priority = EStateTreeTransitionPriority::High;
				TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransIntCond = TransC.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}
		}

		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		auto& TaskB = StateB.AddTask<FTestTask_Stand>(FName(TEXT("TaskB")));
		auto& TaskC = StateC.AddTask<FTestTask_Stand>(FName(TEXT("TaskC")));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);

		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));

		Status = Exec.Start();
		AITEST_FALSE("StateTree TaskA should not enter state", Exec.Expect(TaskA.GetName(), EnterStateStr));
		AITEST_FALSE("StateTree TaskB should not enter state", Exec.Expect(TaskB.GetName(), EnterStateStr));
		AITEST_TRUE("StateTree TaskC should enter state", Exec.Expect(TaskC.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_FollowTransitions, "System.StateTree.Transition.FollowTransitions");

struct FStateTreeTest_InfiniteLoop : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);
		RootPropertyBag.AddProperty(FName(TEXT("Int")), EPropertyBagPropertyType::Int32);
		RootPropertyBag.SetValueInt32(FName(TEXT("Int")), 1);
		
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UStateTreeState& StateA = Root.AddChildState(FName(TEXT("A")));
		UStateTreeState& StateB = StateA.AddChildState(FName(TEXT("B")));

		// Root

		// State A
		{
			StateA.SelectionBehavior = EStateTreeStateSelectionBehavior::TryFollowTransitions;
			{
				// A -> B
				FStateTreeTransition& Trans = StateA.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateB);
				TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransIntCond = Trans.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}
		}

		// State B
		{
			StateB.SelectionBehavior = EStateTreeStateSelectionBehavior::TryFollowTransitions;
			{
				// B -> A
				FStateTreeTransition& Trans = StateB.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateA);
				TStateTreeEditorNode<FStateTreeCompareIntCondition>& TransIntCond = Trans.AddCondition<FStateTreeCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}
		}
		
		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		auto& TaskB = StateB.AddTask<FTestTask_Stand>(FName(TEXT("TaskB")));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);

		AITEST_TRUE("StateTree should get compiled", bResult);

		EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		const FString TickStr(TEXT("Tick"));
		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));

		GetTestRunner().AddExpectedError(TEXT("Loop detected when trying to select state"), EAutomationExpectedErrorFlags::Contains, 1);
		GetTestRunner().AddExpectedError(TEXT("Failed to select initial state"), EAutomationExpectedErrorFlags::Contains, 1);
		
		Status = Exec.Start();
		AITEST_EQUAL("Start should fail", Status, EStateTreeRunStatus::Failed);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_InfiniteLoop, "System.StateTree.Transition.InfiniteLoop");

struct FStateTreeTest_RegularTransitions : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//  Global task and parameter
		//	RootA
		//		StateB -> Next
		//		StateC -> Next
		//		StateD -> Next
		//		StateE -> Next
		//		StateF -> Next
		//		StateG -> Succeeded

		FStateTreeCompilerLog Log;

		// Main asset
		UStateTree& StateTree = NewStateTree();
		{
			UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
			FGuid RootParameter_ValueID;
			{
				// Parameters
				FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);
				RootPropertyBag.AddProperty("Value", EPropertyBagPropertyType::Int32);
				RootPropertyBag.SetValueInt32("Value", -111);
				RootParameter_ValueID = RootPropertyBag.FindPropertyDescByName("Value")->ID;

				TStateTreeEditorNode<FTestTask_PrintValue>& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>("GlobalTask");
				GlobalTask.GetInstanceData().Value = -1;
				EditorData.AddPropertyBinding(FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(GlobalTask.ID, TEXT("Value")));
			}

			UStateTreeState& Root = EditorData.AddSubTree("RootA");
			{
				TStateTreeEditorNode<FTestTask_PrintValue>& Task = Root.AddTask<FTestTask_PrintValue>("TaskA");
				Task.GetInstanceData().Value = -1;
				EditorData.AddPropertyBinding(FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(Task.ID, TEXT("Value")));
			}
			{
				UStateTreeState& StateB = Root.AddChildState("StateB", EStateTreeStateType::State);
				TStateTreeEditorNode<FTestTask_PrintValue>& Task = StateB.AddTask<FTestTask_PrintValue>("TaskB");
				Task.GetInstanceData().Value = 1;
				FStateTreeTransition& Transition = StateB.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UStateTreeState& StateB = Root.AddChildState("StateC", EStateTreeStateType::State);
				TStateTreeEditorNode<FTestTask_PrintValue>& Task = StateB.AddTask<FTestTask_PrintValue>("TaskC");
				Task.GetInstanceData().Value = 2;
				FStateTreeTransition& Transition = StateB.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UStateTreeState& StateD = Root.AddChildState("StateD", EStateTreeStateType::State);
				TStateTreeEditorNode<FTestTask_PrintValue>& Task = StateD.AddTask<FTestTask_PrintValue>("TaskD");
				Task.GetInstanceData().Value = 3;
				FStateTreeTransition& Transition = StateD.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Root);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}

			FStateTreeCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(StateTree);
			AITEST_TRUE("StateTree should get compiled", bResult);
		}

		const FString EnterStateStr(TEXT("EnterState"));
		const FString ExitStateStr(TEXT("ExitState"));

		{
			EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
			FStateTreeInstanceData InstanceData;
			FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("StateTree should init", bInitSucceeded);

			FInstancedPropertyBag Parameters;
			Parameters.MigrateToNewBagInstance(StateTree.GetDefaultParameters());
			Parameters.SetValueInt32("Value", 111);

			Status = Exec.Start(&Parameters);
			AITEST_EQUAL("Start should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("Start should enter Global tasks", Exec.Expect("GlobalTask", TEXT("EnterState111")));
			AITEST_TRUE("Start should enter StateA", Exec.Expect("TaskA", TEXT("EnterState111")));
			AITEST_TRUE("Start should enter StateB", Exec.Expect("TaskB", TEXT("EnterState1")));
			Exec.LogClear();

			Status = Exec.Tick(1.5f); // over tick, should trigger
			AITEST_EQUAL("1st Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("1st Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("1st Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("1st Tick should tick StateB", Exec.Expect("TaskB", TEXT("Tick1")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL("2nd Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("2nd Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("2nd Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("2nd Tick should tick the StateB", Exec.Expect("TaskB", TEXT("Tick1")));
			AITEST_TRUE("2nd Tick should exit the StateB", Exec.Expect("TaskB", TEXT("ExitState1")));
			AITEST_TRUE("2nd Tick should enter the StateC", Exec.Expect("TaskC", TEXT("EnterState2")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL("3rd Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("3rd Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("3rd Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("3rd Tick should tick StateC", Exec.Expect("TaskC", TEXT("Tick2")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL("4th Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("4th Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("244ththnd Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("4th Tick should tick the StateC", Exec.Expect("TaskC", TEXT("Tick2")));
			AITEST_TRUE("4th Tick should exit the StateC", Exec.Expect("TaskC", TEXT("ExitState2")));
			AITEST_TRUE("4th Tick should enter the StateD", Exec.Expect("TaskD", TEXT("EnterState3")));
			Exec.LogClear();

			Status = Exec.Tick(0.001f);
			AITEST_EQUAL("5th Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("5th Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("5th Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("5th Tick should tick StateD", Exec.Expect("TaskD", TEXT("Tick3")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL("6th Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("6th Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("6th Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("6th Tick should tick StateD", Exec.Expect("TaskD", TEXT("Tick3")));
			AITEST_TRUE("6th Tick should exit the StateD", Exec.Expect("TaskD", TEXT("ExitState3")));
			//AITEST_FALSE("6th Tick should not exit the Global tasks", Exec.Expect("GlobalTask", TEXT("ExitState111")));
			//AITEST_FALSE("6th Tick should not enter the Global tasks", Exec.Expect("GlobalTask", TEXT("EnterState111")));
			//AITEST_FALSE("6th Tick should not exit the StateA", Exec.Expect("TaskA", TEXT("ExitState111")));
			//AITEST_FALSE("6th Tick should not enter the StateA", Exec.Expect("TaskA", TEXT("EnterState111")));
			AITEST_TRUE("6th Tick should enter the StateB", Exec.Expect("TaskB", TEXT("EnterState1")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL("7th Tick should complete with Running", Status, EStateTreeRunStatus::Running);
			AITEST_TRUE("7th Tick should tick Global tasks", Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE("7th Tick should tick StateA", Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE("7th Tick should tick StateB", Exec.Expect("TaskB", TEXT("Tick1")));
			Exec.LogClear();

			Exec.Stop();
			AITEST_TRUE("Stop Tick should exit the StateB", Exec.Expect("TaskB", TEXT("ExitState1")));
			AITEST_TRUE("Stop Tick should exit the StateA", Exec.Expect("TaskA", TEXT("ExitState111")));
			AITEST_TRUE("Stop should tick Global tasks", Exec.Expect("GlobalTask", TEXT("ExitState111")));
			Exec.LogClear();
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_RegularTransitions, "System.StateTree.Transition.RegularTransitions");

} // namespace UE::StateTree::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
