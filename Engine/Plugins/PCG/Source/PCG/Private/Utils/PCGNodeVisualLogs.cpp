// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGNodeVisualLogs.h"

#include "PCGComponent.h"
#include "PCGModule.h"

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGNode"

#if WITH_EDITOR

namespace PCGNodeVisualLogsConstants
{
	static constexpr int MaxLogsInSummary = 8;
	static const FText Warning = LOCTEXT("PCGLogWarning", "Warning");
	static const FText Error = LOCTEXT("PCGLogError", "Error");
}

void FPCGNodeVisualLogs::Log(const FPCGStack& InPCGStack, ELogVerbosity::Type InVerbosity, const FText& InMessage)
{
	bool bAdded = false;

	{
		FWriteScopeLock ScopedWriteLock(LogsLock);

		FPCGPerNodeVisualLogs& NodeLogs = StackToLogs.FindOrAdd(InPCGStack);

		constexpr int32 MaxLogged = 1024;
		if (StackToLogs.Num() < MaxLogged)
		{
			NodeLogs.Emplace(InMessage, InVerbosity);

			bAdded = true;
		}
	}

	// Broadcast outside of write scope lock
	if (bAdded && !InPCGStack.GetStackFrames().IsEmpty())
	{
		const bool bIsInGameThread = IsInGameThread();

		TArray<TWeakObjectPtr<const UPCGNode>> NodeWeakPtrs;
		{
			FGCScopeGuard Guard;
			for (const FPCGStackFrame& Frame : InPCGStack.GetStackFrames())
			{
				if (const UPCGNode* Node = Frame.GetObject_NoGuard<UPCGNode>())
				{
					if (bIsInGameThread)
					{
						Node->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(Node), EPCGChangeType::Cosmetic);
					}
					else
					{
						NodeWeakPtrs.Add(Node);
					}
				}
			}
		}

		if(!NodeWeakPtrs.IsEmpty())
		{
			ExecuteOnGameThread(UE_SOURCE_LOCATION, [NodeWeakPtrs]()
			{
				for(const TWeakObjectPtr<const UPCGNode>& NodeWeakPtr : NodeWeakPtrs)
				{
					if (const UPCGNode* Node = NodeWeakPtr.Get())
					{
						Node->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(Node), EPCGChangeType::Cosmetic);
					}
				}
			});
		}
	}
}

bool FPCGNodeVisualLogs::HasLogs(const FPCGStack& InPCGStack) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack) && !Entry.Value.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

bool FPCGNodeVisualLogs::HasLogs(const FPCGStack& InPCGStack, ELogVerbosity::Type& OutMinVerbosity) const
{
	OutMinVerbosity = ELogVerbosity::All;
	
	FReadScopeLock ScopedReadLock(LogsLock);

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack))
		{
			for (const FPCGNodeLogEntry& Log : Entry.Value)
			{
				OutMinVerbosity = FMath::Min(OutMinVerbosity, Log.Verbosity);
			}
		}
	}

	return OutMinVerbosity != ELogVerbosity::All;
}

bool FPCGNodeVisualLogs::HasLogsOfVerbosity(const FPCGStack& InPCGStack, ELogVerbosity::Type InVerbosity) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack))
		{
			if (Algo::FindByPredicate(Entry.Value, [InVerbosity](const FPCGNodeLogEntry& Log) { return Log.Verbosity == InVerbosity; }))
			{
				return true;
			}
		}
	}

	return false;
}

FPCGPerNodeVisualLogs FPCGNodeVisualLogs::GetLogs(const FPCGStack& InPCGStack) const
{
	FPCGPerNodeVisualLogs Logs;

	ForAllMatchingLogs(InPCGStack, [&Logs](const FPCGStack&, const FPCGPerNodeVisualLogs& InLogs)
	{
		Logs.Append(InLogs);
		return true;
	});
	
	return Logs;
}

void FPCGNodeVisualLogs::ForAllMatchingLogs(const FPCGStack& InPCGStack, TFunctionRef<bool(const FPCGStack&, const FPCGPerNodeVisualLogs&)> InFunc) const
{
	FReadScopeLock ScopedReadLock(LogsLock);
	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack))
		{
			if (!InFunc(Entry.Key, Entry.Value))
			{
				break;
			}
		}
	}
}

void FPCGNodeVisualLogs::GetLogs(const UPCGNode* InNode, FPCGPerNodeVisualLogs& OutLogs, TArray<const UPCGComponent*>& OutComponents) const
{
	OutLogs.Reset();
	OutComponents.Reset();

	FReadScopeLock ScopedReadLock(LogsLock);
	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		const FPCGStack& Stack = Entry.Key;
		if (Stack.HasObject(InNode))
		{
			OutLogs.Append(Entry.Value);
			const UPCGComponent* RootComponent = Stack.GetRootComponent();
			for (int LogIndex = 0; LogIndex < Entry.Value.Num(); ++LogIndex)
			{
				OutComponents.Add(RootComponent);
			}
		}
	}
}

FText FPCGNodeVisualLogs::GetSummaryText(const FPCGPerNodeVisualLogs& InLogs, const TArray<const UPCGComponent*>* InComponents, ELogVerbosity::Type* OutMinimumVerbosity)
{
	check(!InComponents || InLogs.Num() == InComponents->Num());

	FText Summary = FText::GetEmpty();
	ELogVerbosity::Type MinVerbosity = ELogVerbosity::All;

	const int32 NumLogs = FMath::Min(InLogs.Num(), PCGNodeVisualLogsConstants::MaxLogsInSummary);

	for (int32 LogIndex = 0; LogIndex < NumLogs; ++LogIndex)
	{
		const FPCGNodeLogEntry& LogEntry = InLogs[LogIndex];

		const FText& MessageVerbosity = (LogEntry.Verbosity == ELogVerbosity::Warning ? PCGNodeVisualLogsConstants::Warning : PCGNodeVisualLogsConstants::Error);
		if (InComponents)
		{
			const UPCGComponent* Component = (*InComponents)[LogIndex];
			const FText ActorName = (Component && Component->GetOwner()) ? FText::FromString(Component->GetOwner()->GetActorLabel()) : LOCTEXT("PCGLogMissingComponent", "MissingComponent");

			if (Summary.IsEmpty())
			{
				Summary = FText::Format(LOCTEXT("NodeTooltipLogWithActorEmpty", "[{0}] {1}: {2}"), ActorName, MessageVerbosity, LogEntry.Message);
			}
			else
			{
				Summary = FText::Format(LOCTEXT("NodeTooltipLogWithActor", "{0}\n[{1}] {2}: {3}"), Summary, ActorName, MessageVerbosity, LogEntry.Message);
			}
		}
		else
		{
			if (Summary.IsEmpty())
			{
				Summary = FText::Format(LOCTEXT("NodeTooltipLogEmpty", "{0}: {1}"), MessageVerbosity, LogEntry.Message);
			}
			else
			{
				Summary = FText::Format(LOCTEXT("NodeTooltipLog", "{0}\n{1}: {2}"), Summary, MessageVerbosity, LogEntry.Message);
			}
		}

		MinVerbosity = FMath::Min(MinVerbosity, LogEntry.Verbosity);
	}

	// Check log level for all entries, not only the first entries
	for (int32 LogIndex = NumLogs; LogIndex < InLogs.Num(); ++LogIndex)
	{
		const FPCGNodeLogEntry& LogEntry = InLogs[LogIndex];
		MinVerbosity = FMath::Min(MinVerbosity, LogEntry.Verbosity);
	}

	// Finally, if we had at most the limit of entries, we'll add an ellipsis.
	if (InLogs.Num() > PCGNodeVisualLogsConstants::MaxLogsInSummary)
	{
		Summary = FText::Format(LOCTEXT("NodeTooltipEllipsis", "{0}\n..."), Summary);
	}

	if (OutMinimumVerbosity)
	{
		*OutMinimumVerbosity = MinVerbosity;
	}

	return Summary;
}


FText FPCGNodeVisualLogs::GetLogsSummaryText(const UPCGNode* InNode, ELogVerbosity::Type& OutMinimumVerbosity) const
{
	FPCGPerNodeVisualLogs Logs;
	TArray<const UPCGComponent*> Components;
	GetLogs(InNode, Logs, Components);

	return GetSummaryText(Logs, &Components, &OutMinimumVerbosity);
}

FText FPCGNodeVisualLogs::GetLogsSummaryText(const FPCGStack& InBaseStack, ELogVerbosity::Type* OutMinimumVerbosity) const
{
	return GetSummaryText(GetLogs(InBaseStack), nullptr, OutMinimumVerbosity);
}

void FPCGNodeVisualLogs::ClearLogs(const FPCGStack& InPCGStack)
{
	TSet<const UPCGNode*> TouchedNodes;

	{
		FGCScopeGuard Guard;
		FWriteScopeLock ScopedWriteLock(LogsLock);

		TArray<FPCGStack> StacksToRemove;
		for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
		{
			// Always take every opportunity to flush messages logged against invalid/dead components.
			const bool bComponentValid = IsValid(Entry.Key.GetRootComponent());

			if (!bComponentValid || Entry.Key.BeginsWith(InPCGStack))
			{
				if (!bComponentValid)
				{
					UE_LOG(LogPCG, Verbose, TEXT("Cleared out logs for null component."));
				}

				StacksToRemove.Add(Entry.Key);

				for (const FPCGStackFrame& Frame : Entry.Key.GetStackFrames())
				{
					if (const UPCGNode* Node = Frame.GetObject_NoGuard<UPCGNode>())
					{
						TouchedNodes.Add(Node);
					}
				}
			}
		}

		for (const FPCGStack& StackToRemove : StacksToRemove)
		{
			StackToLogs.Remove(StackToRemove);
		}
	}

	// Broadcast change notification outside of write scope lock
	if (IsInGameThread())
	{
		for (const UPCGNode* Node : TouchedNodes)
		{
			if (Node)
			{
				Node->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(Node), EPCGChangeType::Cosmetic);
			}
		}
	}
	else if(!TouchedNodes.IsEmpty())
	{
		TArray<TWeakObjectPtr<const UPCGNode>> NodeWeakPtrs;
		NodeWeakPtrs.Reserve(TouchedNodes.Num());
		Algo::Transform(TouchedNodes, NodeWeakPtrs, [](const UPCGNode* Node) { return Node; });

		ExecuteOnGameThread(UE_SOURCE_LOCATION, [NodeWeakPtrs]()
		{
			for (const TWeakObjectPtr<const UPCGNode>& NodeWeakPtr : NodeWeakPtrs)
			{
				if (const UPCGNode* Node = NodeWeakPtr.Get())
				{
					Node->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(Node), EPCGChangeType::Cosmetic);
				}
			}
		});
	}
}

void FPCGNodeVisualLogs::ClearLogs(const UPCGComponent* InComponent)
{
	FPCGStack Stack;
	Stack.PushFrame(InComponent);
	ClearLogs(Stack);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
