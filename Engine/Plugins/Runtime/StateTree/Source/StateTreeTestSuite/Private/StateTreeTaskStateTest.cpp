// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTest.h"
#include "StateTreeTestBase.h"
#include "StateTreeTestTypes.h"

#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeCompiler.h"
#include "Conditions/StateTreeCommonConditions.h"

#define LOCTEXT_NAMESPACE "AITestSuite_StateTreeTask"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::StateTree::Tests
{

namespace Private
{
} // namespace Private

struct FStateTreeTest_TasksCompletion_AllAny : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		// Main asset
		UStateTree& StateTree = NewStateTree();
		UStateTreeState* RootState = nullptr;
		{
			UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

			UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Tree1StateRoot")));
			RootState = &Root;
			Root.TasksCompletion = EStateTreeTaskCompletionType::All;
			for (int32 TaskIndex = 0; TaskIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TaskIndex)
			{
				TStateTreeEditorNode<FTestTask_Stand>& Task = Root.AddTask<FTestTask_Stand>(FName(*FString::Printf(TEXT("Tree1StateRootTask_%d"), TaskIndex)));
				Task.GetNode().TicksToCompletion = TaskIndex + 1;
				if (TaskIndex == 10)
				{
					Task.GetNode().bConsideredForCompletion = false;
					Task.GetNode().TickCompletionResult = EStateTreeRunStatus::Failed;
				}
			}
			UStateTreeState& Tree1StateA = Root.AddChildState("Tree1StateA");
			Tree1StateA.TasksCompletion = EStateTreeTaskCompletionType::All;
			for (int32 TaskIndex = 0; TaskIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TaskIndex)
			{
				TStateTreeEditorNode<FTestTask_Stand>& Task = Tree1StateA.AddTask<FTestTask_Stand>(FName(*FString::Printf(TEXT("Tree1StateATask_%d"), TaskIndex)));
				Task.GetNode().TicksToCompletion = TaskIndex + 1;

				if (TaskIndex == 22)
				{
					Task.GetNode().bConsideredForCompletion = false;
					Task.GetNode().EnterStateResult = EStateTreeRunStatus::Failed;
				}
			}

			{
				FStateTreeCompilerLog Log;
				FStateTreeCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(StateTree);
				AITEST_TRUE("StateTree should get compiled", bResult);
			}
		}

		// Test All
		{
			EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
			FStateTreeInstanceData InstanceData;
			FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("StateTree should init", bInitSucceeded);

			{
				Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EStateTreeRunStatus::Running);
				Exec.LogClear();
			}

			for (int32 TickIndex = 0; TickIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TickIndex)
			{
				Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));

				for (int32 TaskIndex = 0; TaskIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TaskIndex)
				{
					bool bTickedRoot = Exec.Expect(*FString::Printf(TEXT("Tree1StateRootTask_%d"), TaskIndex), TEXT("Tick"));
					if (TaskIndex < TickIndex)
					{
						AITEST_FALSE(*FString::Printf(TEXT("Should not tick Task %d, %d"), TickIndex, TaskIndex), bTickedRoot);
					}
					else
					{
						AITEST_TRUE(*FString::Printf(TEXT("Should tick Task %d, %d"), TickIndex, TaskIndex), bTickedRoot);
					}
					bool bTickedA = Exec.Expect(*FString::Printf(TEXT("Tree1StateATask_%d"), TaskIndex), TEXT("Tick"));
					if (TaskIndex < TickIndex || TaskIndex == 22) // task 22 fails on enter
					{
						AITEST_FALSE(*FString::Printf(TEXT("Should not tick Task A %d, %d"), TickIndex, TaskIndex), bTickedA);
					}
					else
					{
						AITEST_TRUE(*FString::Printf(TEXT("Should tick Task A %d, %d"), TickIndex, TaskIndex), bTickedA);
					}
				}
				const bool bStateSucceeded = Exec.Expect("Tree1StateRootTask_0", "StateCompleted");
				const bool bLastTick = TickIndex == FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup - 1;
				AITEST_EQUAL(TEXT("Tick should not complete the task."), bStateSucceeded, bLastTick);
				Exec.LogClear();
			}
			{
				Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				bool bTicked = Exec.Expect("Tree1StateRootTask_0", TEXT("Tick"));
				AITEST_TRUE(TEXT("Reset should allow new tick."), bTicked);
			}

			Exec.Stop();
		}

		// Test any
		RootState->TasksCompletion = EStateTreeTaskCompletionType::Any;
		{
			FStateTreeCompilerLog Log;
			FStateTreeCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(StateTree);
			AITEST_TRUE("StateTree should get compiled", bResult);
		}
		{
			EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
			FStateTreeInstanceData InstanceData;
			FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("StateTree should init", bInitSucceeded);

			{
				Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EStateTreeRunStatus::Running);
				Exec.LogClear();
			}

			{
				Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				for (int32 TaskIndex = 0; TaskIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TaskIndex)
				{
					bool bTicked = Exec.Expect(*FString::Printf(TEXT("Tree1StateRootTask_%d"), TaskIndex), TEXT("Tick"));
					AITEST_TRUE(*FString::Printf(TEXT("Should tick Task %d"), TaskIndex), bTicked);
				}
				const bool bStateSucceeded = Exec.Expect("Tree1StateRootTask_0", "StateCompleted");
				AITEST_TRUE(TEXT("Tick should not complete the task."), bStateSucceeded);
				Exec.LogClear();
			}
			{
				Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				bool bTicked = Exec.Expect("Tree1StateRootTask_0", TEXT("Tick"));
				AITEST_TRUE(TEXT("Reset should allow new tick."), bTicked);
			}

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TasksCompletion_AllAny, "System.StateTree.TasksCompletion.AllAny");

struct FStateTreeTest_TasksCompletion_FailureTasks : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 BadTask = 2;
		// Main asset
		UStateTree& StateTree = NewStateTree();
		{
			UStateTreeEditorData& EditorData = *Cast<UStateTreeEditorData>(StateTree.EditorData);

			UStateTreeState& Root = EditorData.AddSubTree(FName(TEXT("Tree1StateRoot")));
			Root.TasksCompletion = EStateTreeTaskCompletionType::All;
			for (int32 TaskIndex = 0; TaskIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TaskIndex)
			{
				TStateTreeEditorNode<FTestTask_Stand>& Task = Root.AddTask<FTestTask_Stand>(FName(*FString::Printf(TEXT("Tree1StateRootTask_%d"), TaskIndex)));
				Task.GetNode().TicksToCompletion = TaskIndex + 1;
				if (TaskIndex == BadTask)
				{
					Task.GetNode().TickCompletionResult = EStateTreeRunStatus::Failed;
				}
			}

			{
				FStateTreeCompilerLog Log;
				FStateTreeCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(StateTree);
				AITEST_TRUE("StateTree should get compiled", bResult);
			}
		}

		{
			EStateTreeRunStatus Status = EStateTreeRunStatus::Unset;
			FStateTreeInstanceData InstanceData;
			FTestStateTreeExecutionContext Exec(StateTree, StateTree, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("StateTree should init", bInitSucceeded);

			{
				Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EStateTreeRunStatus::Running);
				Exec.LogClear();
			}

			for (int32 TickIndex = 0; TickIndex <= BadTask; ++TickIndex)
			{
				Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot"));
				const bool bLastTick = TickIndex == BadTask;
				for (int32 TaskIndex = 0; TaskIndex < FStateTreeTasksCompletionStatus::MaxNumberOfTasksPerGroup; ++TaskIndex)
				{
					bool bTicked = Exec.Expect(*FString::Printf(TEXT("Tree1StateRootTask_%d"), TaskIndex), TEXT("Tick"));
					if (TaskIndex < TickIndex || (bLastTick && TaskIndex > BadTask))
					{
						AITEST_FALSE(*FString::Printf(TEXT("Should not tick Task %d, %d"), TickIndex, TaskIndex), bTicked);
					}
					else
					{
						AITEST_TRUE(*FString::Printf(TEXT("Should tick Task %d, %d"), TickIndex, TaskIndex), bTicked);
					}
				}
				const bool bStateSucceeded = Exec.Expect("Tree1StateRootTask_0", "StateCompleted");
				AITEST_EQUAL(TEXT("Tick should not complete the task."), bStateSucceeded, bLastTick);
				Exec.LogClear();
			}
			{
				Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EStateTreeRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot"));
				bool bTicked = Exec.Expect("Tree1StateRootTask_0", TEXT("Tick"));
				AITEST_TRUE(TEXT("Reset should allow new tick."), bTicked);
			}

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TasksCompletion_FailureTasks, "System.StateTree.TasksCompletion.Failure");

// test set status with priority
struct FStateTreeTest_TasksCompletion_Status : FStateTreeTestBase
{
	virtual bool InstantTest() override
	{
		FCompactStateTreeFrame Frame;
		Frame.NumberOfTasksStatusMasks = 1;
		FStateTreeTasksCompletionStatus Status = FStateTreeTasksCompletionStatus(Frame);

		constexpr int32 NumberOfTasks = 4;
		FCompactStateTreeState State;
		State.CompletionTasksControl = EStateTreeTaskCompletionType::All;
		State.CompletionTasksMaskBitsOffset = 3;
		State.CompletionTasksMaskBufferIndex = 0;
		State.CompletionTasksMask = 1 << NumberOfTasks;
		State.CompletionTasksMask -= 1;
		State.CompletionTasksMask <<= State.CompletionTasksMaskBitsOffset;

		UE::StateTree::FTasksCompletionStatus TestingStatus = Status.GetStatus(State);

		auto TestEmpty = [this, &TestingStatus]()
			{
				AITEST_FALSE(TEXT("Empty is not completed."), TestingStatus.IsCompleted());
				AITEST_FALSE(TEXT("Empty has not failure."), TestingStatus.HasAnyFailed());
				AITEST_FALSE(TEXT("Empty is not completed."), TestingStatus.HasAnyCompleted());
				AITEST_FALSE(TEXT("Empty is not completed."), TestingStatus.HasAllCompleted());
				AITEST_EQUAL(TEXT("The completion status is running."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Running);
				for (int32 Index = 0; Index < NumberOfTasks; ++Index)
				{
					AITEST_FALSE(TEXT("Empty is not completed."), TestingStatus.HasFailed(Index));
					AITEST_TRUE(TEXT("Empty is running."), TestingStatus.IsRunning(Index));
					AITEST_EQUAL(TEXT("Empty is running."), TestingStatus.GetStatus(Index), UE::StateTree::ETaskCompletionStatus::Running);
				}
				return true;
			};

		// Test new/empty completion status
		{
			if (!TestEmpty())
			{
				return false;
			}
		}

		// Set tasks (1) to running. Does nothing.
		{
			TestingStatus.SetStatus(1, UE::StateTree::ETaskCompletionStatus::Running);
			if (!TestEmpty())
			{
				return false;
			}
		}

		auto Test1Expected = [this, &TestingStatus](UE::StateTree::ETaskCompletionStatus Expected)
			{
				AITEST_FALSE(TEXT("1 doesn't completed the state."), TestingStatus.IsCompleted());
				AITEST_FALSE(TEXT("1 doesn't completed the failed."), TestingStatus.HasAnyFailed());
				AITEST_TRUE(TEXT("1 does completed any state."), TestingStatus.HasAnyCompleted());
				AITEST_FALSE(TEXT("1 doesn't completed all state."), TestingStatus.HasAllCompleted());
				AITEST_EQUAL(TEXT("States running."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Running);
				for (int32 Index = 0; Index < NumberOfTasks; ++Index)
				{
					AITEST_FALSE(TEXT("1 is not completed."), TestingStatus.HasFailed(Index));
					if (1 == Index)
					{
						AITEST_FALSE(TEXT("1 is Stopped."), TestingStatus.IsRunning(Index));
						AITEST_EQUAL(TEXT("1 is Stopped."), TestingStatus.GetStatus(Index), Expected);
					}
					else
					{
						AITEST_TRUE(TEXT("1 is Stopped."), TestingStatus.IsRunning(Index));
						AITEST_EQUAL(TEXT("1 is Stopped."), TestingStatus.GetStatus(Index), UE::StateTree::ETaskCompletionStatus::Running);
					}
				}
				return true;
			};
		// Set tasks (1) to stop.
		{
			TestingStatus.SetStatus(1, UE::StateTree::ETaskCompletionStatus::Stopped);
			if (!Test1Expected(UE::StateTree::ETaskCompletionStatus::Stopped))
			{
				return false;
			}
		}
		// Set tasks (1) to running. Does nothing.
		{
			ETaskCompletionStatus NewStatus = TestingStatus.SetStatusWithPriority(1, UE::StateTree::ETaskCompletionStatus::Running);
			AITEST_EQUAL(TEXT("1 make the state Stopped."), NewStatus, UE::StateTree::ETaskCompletionStatus::Stopped);
			if (!Test1Expected(UE::StateTree::ETaskCompletionStatus::Stopped))
			{
				return false;
			}
		}
		// Set tasks (1) to Succeeded. Does nothing.
		{
			ETaskCompletionStatus NewStatus = TestingStatus.SetStatusWithPriority(1, UE::StateTree::ETaskCompletionStatus::Succeeded);
			AITEST_EQUAL(TEXT("1 make the state Succeed."), NewStatus, UE::StateTree::ETaskCompletionStatus::Succeeded);
			if (!Test1Expected(UE::StateTree::ETaskCompletionStatus::Succeeded))
			{
				return false;
			}
		}
		// Set tasks (1) to stop. Does nothing.
		{
			ETaskCompletionStatus NewStatus = TestingStatus.SetStatusWithPriority(1, UE::StateTree::ETaskCompletionStatus::Stopped);
			AITEST_EQUAL(TEXT("1 make the state Succeed."), NewStatus, UE::StateTree::ETaskCompletionStatus::Succeeded);
			if (!Test1Expected(UE::StateTree::ETaskCompletionStatus::Succeeded))
			{
				return false;
			}
		}
		// Set tasks (1) to running. Does nothing.
		{
			ETaskCompletionStatus NewStatus = TestingStatus.SetStatusWithPriority(1, UE::StateTree::ETaskCompletionStatus::Stopped);
			AITEST_EQUAL(TEXT("1 make the state Succeed."), NewStatus, UE::StateTree::ETaskCompletionStatus::Succeeded);
			if (!Test1Expected(UE::StateTree::ETaskCompletionStatus::Succeeded))
			{
				return false;
			}
		}
		// Set tasks (1) to fail. Does nothing.
		{
			ETaskCompletionStatus NewStatus = TestingStatus.SetStatusWithPriority(1, UE::StateTree::ETaskCompletionStatus::Failed);
			AITEST_EQUAL(TEXT("1 make the state Succeed."), NewStatus, UE::StateTree::ETaskCompletionStatus::Failed);
			AITEST_TRUE(TEXT("1 completes the state."), TestingStatus.IsCompleted());
			AITEST_TRUE(TEXT("1 completes the failed."), TestingStatus.HasAnyFailed());
			AITEST_TRUE(TEXT("1 completes any state."), TestingStatus.HasAnyCompleted());
			AITEST_TRUE(TEXT("1 completes all state."), TestingStatus.HasAllCompleted());
			AITEST_EQUAL(TEXT("States failed."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Failed);
			for (int32 Index = 0; Index < NumberOfTasks; ++Index)
			{
				if (1 == Index)
				{
					AITEST_TRUE(TEXT("1 is not completed."), TestingStatus.HasFailed(Index));
					AITEST_FALSE(TEXT("1 is failed."), TestingStatus.IsRunning(Index));
					AITEST_EQUAL(TEXT("1 is failed."), TestingStatus.GetStatus(Index), UE::StateTree::ETaskCompletionStatus::Failed);
				}
				else
				{
					AITEST_FALSE(TEXT("1 is not completed."), TestingStatus.HasFailed(Index));
					AITEST_TRUE(TEXT("1 is running."), TestingStatus.IsRunning(Index));
					AITEST_EQUAL(TEXT("1 is running."), TestingStatus.GetStatus(Index), UE::StateTree::ETaskCompletionStatus::Running);
				}
			}
		}
		// Set completion status
		{
			TestingStatus.SetCompletionStatus(UE::StateTree::ETaskCompletionStatus::Stopped);
			AITEST_TRUE(TEXT("All complete the state."), TestingStatus.IsCompleted());
			AITEST_FALSE(TEXT("All complete the failed."), TestingStatus.HasAnyFailed());
			AITEST_TRUE(TEXT("All complete any state."), TestingStatus.HasAnyCompleted());
			AITEST_TRUE(TEXT("All complete all state."), TestingStatus.HasAllCompleted());
			AITEST_EQUAL(TEXT("States stopped."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Stopped);
			for (int32 Index = 0; Index < NumberOfTasks; ++Index)
			{
				AITEST_FALSE(TEXT("1 is not completed."), TestingStatus.HasFailed(Index));
				AITEST_FALSE(TEXT("1 is stopped."), TestingStatus.IsRunning(Index));
				AITEST_EQUAL(TEXT("1 is stopped."), TestingStatus.GetStatus(Index), UE::StateTree::ETaskCompletionStatus::Stopped);
			}
		}
		// Test GetCompletionStatus
		{
			TestingStatus.SetCompletionStatus(UE::StateTree::ETaskCompletionStatus::Running);
			AITEST_EQUAL(TEXT("States stopped."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Running);
		}
		// Test GetCompletionStatus
		{
			TestingStatus.SetCompletionStatus(UE::StateTree::ETaskCompletionStatus::Succeeded);
			AITEST_EQUAL(TEXT("States succeeded."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Succeeded);
		}
		// Test GetCompletionStatus
		{
			TestingStatus.SetCompletionStatus(UE::StateTree::ETaskCompletionStatus::Failed);
			AITEST_EQUAL(TEXT("States failed."), TestingStatus.GetCompletionStatus(), UE::StateTree::ETaskCompletionStatus::Failed);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStateTreeTest_TasksCompletion_Status, "System.StateTree.TasksCompletion.Status");

} // namespace UE::StateTree::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
