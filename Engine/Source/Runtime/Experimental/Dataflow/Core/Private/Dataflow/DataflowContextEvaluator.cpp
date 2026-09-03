// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowContextEvaluator.h"
#include "Dataflow/DataflowNode.h"

namespace UE::Dataflow
{
	void FContextEvaluator::ScheduleNodeEvaluation(const FDataflowNode& Node, FOnPostEvaluationFunction OnPostEvaluation)
	{
		if (Node.NumOutputs() == 0)
		{
			// likely a terminal node or no output set the output to default Guid
			FEvaluationEntry Entry
			{
				.WeakNode = Node.AsWeak(),
				.Id = FNodeOutputId{ Node.GetGuid(), FGuid() },
				.OnPostEvaluation = OnPostEvaluation,
			};
			ScheduleEvaluation(Entry);
		}
		else
		{
			// schedule all available outputs
			for (const FDataflowOutput* Output : Node.GetOutputs())
			{
				if (Output)
				{
					ScheduleEvaluation(*Output, OnPostEvaluation);
				}
			}
		}
		Process();
	}

	void FContextEvaluator::ScheduleOutputEvaluation(const FDataflowOutput& Output, FOnPostEvaluationFunction OnPostEvaluation)
	{
		ScheduleEvaluation(Output, OnPostEvaluation);
		Process();
	}

	void FContextEvaluator::ScheduleEvaluation(const FDataflowOutput& Output, FOnPostEvaluationFunction OnPostEvaluation)
	{
		if (const FDataflowNode* Node = Output.GetOwningNode())
		{
			FEvaluationEntry Entry
			{
				.WeakNode = Node->AsWeak(),
				.Id = FNodeOutputId{ Node->GetGuid(), Output.GetGuid() },
				.OnPostEvaluation = OnPostEvaluation,
			};
			ScheduleEvaluation(Entry);
		}
	}

	void FContextEvaluator::ScheduleEvaluation(const FEvaluationEntry& Entry)
	{
		if (RunningTasks.Contains(Entry.Id) || PendingEvaluationEntries.Contains(Entry.Id))
		{
			UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::ScheduleEvaluation : skipped [%s]"), *Entry.ToString());
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::ScheduleEvaluation : [%s]"), *Entry.ToString());
		PendingEvaluationEntries.Add(Entry.Id, Entry);

		// add upstream outputs
		TArray<const FDataflowOutput*> InvalidUpstreamOutputs;
		if (TSharedPtr<const FDataflowNode> Node = Entry.WeakNode.Pin())
		{
			InvalidUpstreamOutputs.Reset();
			FindInvalidUpstreamOutputs(*Node, InvalidUpstreamOutputs);
			for (const FDataflowOutput* UpstreamOutput : InvalidUpstreamOutputs)
			{
				UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::ScheduleEvaluation :  [%s] -- Invalid Upstream output [%s]"), *Entry.ToString(), *UpstreamOutput->GetName().ToString());
			}
			for (const FDataflowOutput* UpstreamOutput : InvalidUpstreamOutputs)
			{
				ScheduleEvaluation(*UpstreamOutput, {});
			}
		}
	}

	void FContextEvaluator::Cancel()
	{
		PendingEvaluationEntries.Reset();
		CompletedTasks.Reset();
	}

	void FContextEvaluator::FindInvalidUpstreamOutputs(const FDataflowNode& Node, TArray<const FDataflowOutput*>& OutInvalidUpstreamOutputs)
	{
		for (const FDataflowInput* Input : Node.GetInputs())
		{
			if (Input)
			{
				if (const FDataflowOutput* UpstreamOutput = Input->GetConnection())
				{
					UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::FindInvalidUpstreamOutputs :  [%s] input[%s] -> output [%s]"), 
						*Node.GetName().ToString(), 
						*Input->GetName().ToString(),
						*UpstreamOutput->GetName().ToString()
					);

					if (!UpstreamOutput->HasValidData(OwningContext))
					{
						OutInvalidUpstreamOutputs.Add(UpstreamOutput);
					}
				}
			}
		}
	}

	bool FContextEvaluator::ShouldRunOnGameThread(const FDataflowNode& Node)
	{
		for (const FDataflowInput* Input : Node.GetInputs())
		{
			if (Input)
			{
				const FString InputTypeName = Input->GetType().ToString();
				// skeletal mesh and static mesh do support asynchronous loading and do not allow for accessing 
				// their property from elsewhere than the gamethread
				if (InputTypeName.Contains("UStaticMesh") || InputTypeName.Contains("USkeletalMesh"))
				{
					return true;
				}
			}
		}
		return Node.EvaluateOnGameThreadOnly();
	}

	bool FContextEvaluator::TryScheduleTask(const FEvaluationEntry& Entry)
	{
		TArray<const FDataflowOutput*> InvalidUpstreamOutputs;
		if (TSharedPtr<const FDataflowNode> Node = Entry.WeakNode.Pin())
		{
			InvalidUpstreamOutputs.Reset();
			FindInvalidUpstreamOutputs(*Node, InvalidUpstreamOutputs);
			if (InvalidUpstreamOutputs.IsEmpty())
			{
				ScheduleTask(Entry);
				return true;
			}
		}
		return false;
	}

	void FContextEvaluator::GetStats(int32& OutNumPendingTasks, int32& OutNumRunningTasks, int32& OutNumCompletedTasks) const
	{
		// ugly but we need to make sure the stats are up to date 
		const_cast<FContextEvaluator*>(this)->ClearCompletedTasks();

		OutNumPendingTasks = PendingEvaluationEntries.Num();
		OutNumRunningTasks = RunningTasks.Num();
		OutNumCompletedTasks = CompletedTasks.Num();
	}

	void FContextEvaluator::Process()
	{
		int32 NumScheduleTasks = 0;
		for (auto Iter = PendingEvaluationEntries.CreateIterator(); Iter; ++Iter)
		{
			if (TryScheduleTask(Iter->Value))
			{
				NumScheduleTasks++;
				Iter.RemoveCurrent();
			}
		}

		if (NumScheduleTasks == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::Process : No Task Scheduled NumPendingTasks=[%d]"), PendingEvaluationEntries.Num());
			for (const TPair<FNodeOutputId, FEvaluationEntry>& Entry : PendingEvaluationEntries)
			{
				UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::Process : \t -[%s]"), *Entry.Value.ToString());
			}
		}

		ClearCompletedTasks();
	}

	void FContextEvaluator::ClearCompletedTasks()
	{
		for (auto Iter = RunningTasks.CreateIterator(); Iter; ++Iter)
		{
			const FGraphEventRef& Task = (*Iter).Value;
			if (Task->IsCompleted())
			{
				Iter.RemoveCurrent();
				CompletedTasks.Add((*Iter).Key);
			}
		}
	}

	void FContextEvaluator::ScheduleTask(const FEvaluationEntry& Entry)
	{
		if (TSharedPtr<const FDataflowNode> Node = Entry.WeakNode.Pin())
		{
			FGraphEventArray ExistingTasks;
			if (FGraphEventRef* ExistingTask = RunningTasks.Find(Entry.Id))
			{ 
				ExistingTasks.Add(*ExistingTask);
			}

			const bool bUseGameThread = ShouldRunOnGameThread(*Node);

			UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::ScheduleTask : [%s] GameThread=[%d] previousTasks=[%d]"),
				*Entry.ToString(),
				(int32)bUseGameThread,
				(int32)ExistingTasks.Num()
			);

			FContext* ContextPtr = &OwningContext;
			FGraphEventRef NewTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
				[ContextPtr, Entry]
				{
					if (ContextPtr)
					{
						if (TSharedPtr<const FDataflowNode> Node = Entry.WeakNode.Pin())
						{
							Node->SetAsyncEvaluating(true);
							if (const FDataflowOutput* Output = Node->FindOutput(Entry.Id.OutputId))
							{
								Node->Evaluate(*ContextPtr, Output);
							}
							else if (Node->NumOutputs() == 0)
							{
								Node->Evaluate(*ContextPtr, nullptr);
							}
							Node->SetAsyncEvaluating(false);

							UE_LOG(LogTemp, Warning, TEXT("FContextEvaluator::EndTask : [%s]"), *Entry.ToString());
						}
					}
				},
				TStatId(),
				&ExistingTasks, /* prerequisites - make sure we wait on the previous one if any */
				bUseGameThread ? ENamedThreads::GameThread : ENamedThreads::AnyThread
			);

			auto OnFinishEvaluating = 
				[Evaluator = this, ContextPtr, OnPostEvaluation = Entry.OnPostEvaluation]()
				{
					Evaluator->Process();
					if (OnPostEvaluation.IsSet() && ContextPtr)
					{
						OnPostEvaluation(*ContextPtr);
					}
				};

			// handle post evaluation and run it on the game thread 
			NewTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
				OnFinishEvaluating,
				TStatId(),
				NewTask, /* prerequisites */
				ENamedThreads::GameThread
			);

			RunningTasks.Add(Entry.Id, NewTask);
		}
	}

	FString FContextEvaluator::FEvaluationEntry::ToString() const
	{
		static FName UnknownName("-Unknown-");
		FName NodeName = UnknownName;
		FName OutputName = UnknownName;
		if (TSharedPtr<const FDataflowNode> Node = WeakNode.Pin())
		{
			NodeName = Node->GetName();
			if (const FDataflowOutput* Output = Node->FindOutput(Id.OutputId))
			{
				OutputName = Output->GetName();
			}
		}
		return FString::Format(TEXT("{0}.{1}"), { NodeName.ToString(), OutputName.ToString() });
	}
};

