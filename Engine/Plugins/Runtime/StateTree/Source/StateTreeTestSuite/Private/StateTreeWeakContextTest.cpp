// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTest.h"
#include "StateTreeTestBase.h"
#include "StateTreeTestTypes.h"

#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeCompiler.h"

#define LOCTEXT_NAMESPACE "AITestSuite_StateTreeTestWeakContext"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::StateTree::Tests
{
struct FStateTreeTest_WeakContext_FinishTask : FStateTreeTestBase
{
	//Tree 1 : Global Task
		//	Root : Task 
		//		State1 : Task -> Root
		//			State2 : Task -> Root
	virtual bool InstantTest() override
	{
		struct FWeakContext
		{
			FStateTreeWeakExecutionContext ContextTree1GlobalTask;
			FStateTreeWeakExecutionContext ContextTree1RootTask;
			FStateTreeWeakExecutionContext ContextTree1State1Task;
			FStateTreeWeakExecutionContext ContextTree1State2Task;

			bool bGlobalFinishTaskSuccessOnTick = false;
			bool bState1lFinishTaskFailOnTick = false;
		};

		FWeakContext WeakContext;

		// Building up the State Tree
		UStateTree& StateTree1 = NewStateTree();
		{
			UStateTreeEditorData& EditorData1 = *Cast<UStateTreeEditorData>(StateTree1.EditorData);
			{
				// Global Task
				TStateTreeEditorNode<FTestTask_PrintValue>& Tree1GlobalTask = EditorData1.AddGlobalTask<FTestTask_PrintValue>(FName(TEXT("Tree1GlobalTask")));
				{
					Tree1GlobalTask.GetNode().CustomEnterStateFunc = [&WeakContext](FStateTreeExecutionContext& Context, const FTestTask_PrintValue* Task)
						{
							WeakContext.ContextTree1GlobalTask = Context.MakeWeakExecutionContext();
						};
					Tree1GlobalTask.GetNode().CustomTickFunc = [&WeakContext](FStateTreeExecutionContext& Context, const FTestTask_PrintValue* Task)
						{
							if (WeakContext.bGlobalFinishTaskSuccessOnTick)
							{
								WeakContext.ContextTree1GlobalTask.FinishTask(EStateTreeFinishTaskType::Succeeded);
							}
						};
				}

				// Root State
				UStateTreeState& Root = EditorData1.AddSubTree("Tree1StateRoot");
				{
					TStateTreeEditorNode<FTestTask_PrintValue>& Tree1RootTask = Root.AddTask<FTestTask_PrintValue>(FName(TEXT("Tree1RootTask")));
					Tree1RootTask.GetNode().CustomEnterStateFunc = [&WeakContext](FStateTreeExecutionContext& Context, const FTestTask_PrintValue* Task)
						{
							WeakContext.ContextTree1RootTask = Context.MakeWeakExecutionContext();
						};

				}

				// State 1
				UStateTreeState& State1 = Root.AddChildState("Tree1State1");
				{
					TStateTreeEditorNode<FTestTask_PrintValue>& Tree1State1Task = State1.AddTask<FTestTask_PrintValue>(FName(TEXT("Tree1State1Task")));
					Tree1State1Task.GetNode().CustomEnterStateFunc = [&WeakContext](FStateTreeExecutionContext& Context, const FTestTask_PrintValue* Task)
						{
							WeakContext.ContextTree1State1Task = Context.MakeWeakExecutionContext();
						};
					Tree1State1Task.GetNode().CustomTickFunc = [&WeakContext](FStateTreeExecutionContext& Context, const FTestTask_PrintValue* Task)
						{
							if (WeakContext.bState1lFinishTaskFailOnTick)
							{
								WeakContext.ContextTree1State1Task.FinishTask(EStateTreeFinishTaskType::Failed);
							}
						};
				}

				// State 2
				UStateTreeState& State2 = State1.AddChildState("Tree1State2");
				{
					TStateTreeEditorNode<FTestTask_PrintValue>& Tree1State2Task = State2.AddTask<FTestTask_PrintValue>(FName(TEXT("Tree1State2Task")));
					Tree1State2Task.GetNode().CustomEnterStateFunc = [&WeakContext](FStateTreeExecutionContext& Context, const FTestTask_PrintValue* Task)
						{
							WeakContext.ContextTree1State2Task = Context.MakeWeakExecutionContext();
						};
				}
			};
		}

		// Compile tree
		{
			FStateTreeCompilerLog Log;
			FStateTreeCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(StateTree1);
			AITEST_TRUE("StateTree1 should get compiled", bResult);
		}

		// Create context
		FStateTreeInstanceData InstanceData;
		{
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE("StateTree should init", bInitSucceeded);
			}
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				EStateTreeRunStatus Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1", "Tree1State2"));

				AITEST_TRUE(TEXT("Start should EnterState"), Exec.Expect("Tree1GlobalTask", TEXT("EnterState0"))
							.Then("Tree1RootTask", TEXT("EnterState0"))
							.Then("Tree1State1Task", TEXT("EnterState0"))
							.Then("Tree1State2Task", TEXT("EnterState0")));
				Exec.LogClear();
			}

			// Test that everything tick and there are no transitions.
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				EStateTreeRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1", "Tree1State2"));

				AITEST_TRUE(TEXT("Tick should Tick"), Exec.Expect("Tree1GlobalTask", TEXT("Tick0"))
							.Then("Tree1RootTask", TEXT("Tick0"))
							.Then("Tree1State1Task", TEXT("Tick0"))
							.Then("Tree1State2Task", TEXT("Tick0")));
				Exec.LogClear();
			}

			// Test Finish GlobalTask inside the tick
			{
				WeakContext.bGlobalFinishTaskSuccessOnTick = true;
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				EStateTreeRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Succeeded", Status, EStateTreeRunStatus::Succeeded);
				AITEST_TRUE(TEXT("Tick should Tick"), Exec.Expect("Tree1GlobalTask", TEXT("Tick0"))
							.Then("Tree1State2Task", TEXT("ExitState0"))
							.Then("Tree1State1Task", TEXT("ExitState0"))
							.Then("Tree1RootTask", TEXT("ExitState0")));
				WeakContext.bGlobalFinishTaskSuccessOnTick = false;
			}

			// Finished global task stop the execution. Reset the execution.
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				Exec.Start();
				Exec.LogClear();
			}

			// Test Finish GlobalTask outside the tick
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				WeakContext.ContextTree1GlobalTask.FinishTask(EStateTreeFinishTaskType::Failed);
				EStateTreeRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Failed", Status, EStateTreeRunStatus::Failed);
				AITEST_FALSE(TEXT("Tick should not Tick"), Exec.Expect("Tree1GlobalTask", TEXT("Tick0")));
				AITEST_FALSE(TEXT("Tick should not Tick"), Exec.Expect("Tree1RootTask", TEXT("Tick0")));
				AITEST_TRUE(TEXT("Tick should Tick"), Exec.Expect("Tree1State2Task", TEXT("ExitState0"))
							.Then("Tree1State1Task", TEXT("ExitState0"))
							.Then("Tree1RootTask", TEXT("ExitState0")));
			}

			// Finished global task stop the execution. Reset the execution.
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				Exec.Start();
				Exec.LogClear();
			}

			// Test Finish StateTask inside the tick
			{
				WeakContext.bState1lFinishTaskFailOnTick = true;
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				EStateTreeRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("Tick should Tick"), Exec.Expect("Tree1GlobalTask", TEXT("Tick0"))
							.Then("Tree1RootTask", TEXT("Tick0"))
							.Then("Tree1State1Task", TEXT("Tick0"))
							.Then("Tree1State2Task", TEXT("ExitState0"))
							.Then("Tree1State1Task", TEXT("ExitState0"))
							.Then("Tree1RootTask", TEXT("ExitState0")));

				WeakContext.bGlobalFinishTaskSuccessOnTick = false;
			}

			// Test Finish StateTask outside the tick
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				WeakContext.ContextTree1State1Task.FinishTask(EStateTreeFinishTaskType::Succeeded);
				EStateTreeRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("Tick should Tick"), Exec.Expect("Tree1GlobalTask", TEXT("Tick0"))
							.Then("Tree1RootTask", TEXT("Tick0"))
							.Then("Tree1State2Task", TEXT("Tick0"))
							.Then("Tree1State2Task", TEXT("ExitState0"))
							.Then("Tree1State1Task", TEXT("ExitState0"))
							.Then("Tree1RootTask", TEXT("ExitState0")));
			}

			// Stop the Exec
			{
				FTestStateTreeExecutionContext Exec(StateTree1, StateTree1, InstanceData);
				Exec.Stop();
			}
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_WeakContext_FinishTask, "System.StateTree.WeakContext.FinishTask");
} // namespace UE::StateTree::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
