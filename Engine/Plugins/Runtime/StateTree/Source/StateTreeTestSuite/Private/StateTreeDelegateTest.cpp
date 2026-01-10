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
struct FStateTreeTest_Delegate_ConcurrentListeners : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName("Root"));

		auto& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName("DispatcherTask"));
		auto& ListenerTaskA = Root.AddTask<FTestTask_ListenDelegate>(FName("ListenerTaskA"));
		auto& ListenerTaskB = Root.AddTask<FTestTask_ListenDelegate>(FName("ListenerTaskB"));

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskB, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskA, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree ListenerTaskA should be triggered once."), Exec.Expect(ListenerTaskA.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("StateTree ListenerTaskB should be triggered once."), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));

		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree ListenerTaskA should be triggered twice."), Exec.Expect(ListenerTaskA.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		AITEST_TRUE(TEXT("StateTree ListenerTaskB should be triggered twice."), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_ConcurrentListeners, "System.StateTree.Delegate.ConcurrentListeners");

struct FStateTreeTest_Delegate_MutuallyExclusiveListeners : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UStateTreeState& StateA = Root.AddChildState("A");
		UStateTreeState& StateB = Root.AddChildState("B");

		StateA.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateB);
		StateB.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateA);

		auto& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		auto& ListenerTaskA0 = StateA.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTaskA0")));
		auto& ListenerTaskA1 = StateA.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTaskA1")));
		auto& ListenerTaskB = StateB.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTaskB")));

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskA0, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskA1, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskB, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		AITEST_TRUE("StateTree Active States should be in Root/A", Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree ListenerTaskA0 should be triggered once"), Exec.Expect(ListenerTaskA0.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("StateTree ListenerTaskA1 should be triggered once"), Exec.Expect(ListenerTaskA1.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("StateTree ListenerTaskB shouldn't be triggered."), !Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE("StateTree Active States should be in Root/B", Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree ListenerTaskB should be triggered once"), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("StateTree ListenerTaskA0 shouldn't be triggered."), !Exec.Expect(ListenerTaskA0.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("StateTree ListenerTaskA1 shouldn't be triggered."), !Exec.Expect(ListenerTaskA1.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE("StateTree Active States should be in Root/A", Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_MutuallyExclusiveListeners, "System.StateTree.Delegate.MutuallyExclusiveListeners");

struct FStateTreeTest_Delegate_Transitions : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UStateTreeState& StateA = Root.AddChildState("A");
		UStateTreeState& StateB = Root.AddChildState("B");

		FStateTreeTransition& TransitionAToB = StateA.AddTransition(EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &StateB);
		FStateTreeTransition& TransitionBToA = StateB.AddTransition(EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &StateA);

		auto& DispatcherTask0 = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask0")));
		auto& DispatcherTask1 = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask1")));
		
		EditorData.AddPropertyBinding(FPropertyBindingPath(DispatcherTask0.ID, GET_MEMBER_NAME_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate)),  FPropertyBindingPath(TransitionAToB.ID, GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelegateListener)));
		EditorData.AddPropertyBinding(FPropertyBindingPath(DispatcherTask1.ID, GET_MEMBER_NAME_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate)), FPropertyBindingPath(TransitionBToA.ID, GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelegateListener)));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		AITEST_TRUE("StateTree Active States should be in Root/A", Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Active States should be in Root/B", Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE("StateTree Active States should be in Root/A", Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_Transitions, "System.StateTree.Delegate.Transitions");

struct FStateTreeTest_Delegate_Rebroadcasting : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		auto& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		auto& RedispatcherTask = Root.AddTask<FTestTask_RebroadcastDelegate>(FName(TEXT("RedispatcherTask")));
		auto& ListenerTask = Root.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), RedispatcherTask, TEXT("Listener"));
		EditorData.AddPropertyBinding(RedispatcherTask, TEXT("Dispatcher"), ListenerTask, TEXT("Listener"));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree ListenerTask should be triggered once."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree ListenerTask should be triggered twice."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_Rebroadcasting, "System.StateTree.Delegate.Rebroadcasting");

struct FStateTreeTest_Delegate_SelfRemoval : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		auto& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		auto& CustomFuncTask = Root.AddTask<FTestTask_CustomFuncOnDelegate>(FName(TEXT("CustomFuncTask")));

		uint32 TriggersCounter = 0;

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), CustomFuncTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));
		CustomFuncTask.GetNode().CustomFunc = [&TriggersCounter](const FStateTreeWeakExecutionContext& WeakContext, FStateTreeDelegateListener Listener)
			{
				++TriggersCounter;
				WeakContext.UnbindDelegate(Listener);
			};

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("StateTree Delegate should be triggered once"), TriggersCounter, 1);
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("StateTree Delegate should be triggered once"), TriggersCounter, 1);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_SelfRemoval, "System.StateTree.Delegate.SelfRemoval");

struct FStateTreeTest_Delegate_WithoutRemoval : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UStateTreeState& StateA = Root.AddChildState("A");
		UStateTreeState& StateB = Root.AddChildState("B");

		FStateTreeTransition& TransitionAToB = StateA.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &StateB);

		auto& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		auto& ListenerTask = StateA.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		ListenerTask.GetNode().bRemoveOnExit = false;

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		Exec.LogClear();
		AITEST_TRUE("StateTree Active States should be in Root/A", Exec.ExpectInActiveStates(Root.Name, StateA.Name));

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree Delegate should be triggered once."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE("StateTree Active States should be in Root/B", Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_FALSE(TEXT("StateTree Delegate shouldn't be triggered again."), Exec.Expect(ListenerTask.GetName()));
		AITEST_TRUE("StateTree Active States should be in Root/B", Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_WithoutRemoval, "System.StateTree.Delegate.WithoutRemoval");

struct FStateTreeTest_Delegate_GlobalDispatcherAndListener : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		auto& RootTask = Root.AddTask<FTestTask_Stand>();
		RootTask.GetNode().TicksToCompletion = 100;

		auto& DispatcherTask = EditorData.AddGlobalTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		auto& ListenerTask = EditorData.AddGlobalTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
		ListenerTask.GetNode().bRemoveOnExit = false;

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree Delegate should be triggered once."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("StateTree Delegate should be triggered twice."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_GlobalDispatcherAndListener, "System.StateTree.Delegate.GlobalDispatcherAndListener");

struct FStateTreeTest_Delegate_ListeningToDelegateOnExit : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		UStateTree& StateTree = NewStateTree();
		UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);
		UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		auto& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		auto& ListenerTask = Root.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
		ListenerTask.GetNode().bRemoveOnExit = false;

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnExitDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(StateTree);
		AITEST_TRUE("StateTree should get compiled", bResult);

		FStateTreeInstanceData InstanceData;
		FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("StateTree should init", bInitSucceeded);

		Exec.Start();
		AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates(Root.Name));
		Exec.LogClear();

		Exec.Stop();
		AITEST_FALSE(TEXT("StateTree Delegate shouldn't be triggered"), Exec.Expect(ListenerTask.GetName()));
		Exec.LogClear();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_Delegate_ListeningToDelegateOnExit, "System.StateTree.Delegate.ListeningToDelegateOnExit");

} // namespace UE::StateTree::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
