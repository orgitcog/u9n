// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/RemoteExecutor.h"
#include "AutoRTFM.h"
#include "UObject/RemoteObject.h"

DEFINE_LOG_CATEGORY(LogRemoteExec);

namespace UE::RemoteExecutor
{
TDelegate<void()> TickNetworkDelegate;
TDelegate<void(FRemoteTransactionId, FRemoteWorkPriority, const TArray<FRemoteServerId>& /*MultiServerCommitRemoteServers*/)> BeginMultiServerCommitDelegate;
TDelegate<void(FRemoteTransactionId, const TArray<FRemoteServerId>& /*MultiServerCommitRemoteServers*/)> ReadyMultiServerCommitDelegate;
TDelegate<void(FRemoteTransactionId, const TArray<FRemoteServerId>& /*MultiServerCommitRemoteServers*/)> AbandonMultiServerCommitDelegate;
TDelegate<void(FRemoteTransactionId, const TArray<FRemoteServerId>& /*MultiServerCommitRemoteServers*/)> EndMultiServerCommitDelegate;
TDelegate<void(FRemoteTransactionId, FRemoteServerId /*ServerId*/)> ReadyRemoteMultiServerCommitDelegate;
TDelegate<void(FRemoteTransactionId, FRemoteServerId /*ServerId*/)> AbortRemoteMultiServerCommitDelegate;
TDelegate<TOptional<TTuple<FName, FRemoteWorkPriority, bool, TFunction<void(void)>>>(void)> FetchNextDeferredRPCDelegate;

TMulticastDelegate<void(FRemoteTransactionId, FName)> OnTransactionStartingDelegate;
TMulticastDelegate<void(FRemoteTransactionId)> OnTransactionCompletedDelegate;
TMulticastDelegate<void(FRemoteTransactionId)> OnTransactionAbortedDelegate;
}

enum class ERemoteExecutorAbortReason : uint8
{
	Unspecified,
	RequiresDependencies,
	AbandonWork
};

FArchive& operator<<(FArchive& Ar, FRemoteTransactionId& Id)
{
	Ar << Id.Id;
	return Ar;
}

struct FRemoteExecutorWork
{
	FName Name;
	FRemoteTransactionId RequestId;
	FRemoteWorkPriority Priority;
	bool bIsTransactional = false;

	TFunction<void(void)> Work;

	TArray<void*> SubsystemRequests;
	bool bRequestCreated = false;
	
	uint32 ExecutionAttempts = 0;
	
	bool bRequiresMultiServerCommit = false;
	FString RequiresMultiServerCommitReason;
};

class FRemoteExecutor
{
	TArray<FRemoteSubsystemBase*> Subsystems;

	uint32 NextTransactionRequestId = 0;

	TArray<FRemoteExecutorWork> PendingWorks;

public:
	FRemoteExecutorWork* ExecutingWork = nullptr;

	ERemoteExecutorAbortReason AbortReason = ERemoteExecutorAbortReason::Unspecified;
	FString AbortReasonDescription;

	// tracking data for servicing a remote multi-server commit
	FRemoteServerId ActiveRemoteMultiServerCommitServerId;
	FRemoteTransactionId ActiveRemoteMultiServerCommitRequestId;
	FRemoteWorkPriority ActiveRemoteMultiServerCommitPriority;
	TArray<TFunction<void()>> ActiveRemoteMultiServerCommitDeferredActions;
	bool bActiveRemoteMultiServerCommitReady = false;

	// tracking data for executing our local multi-server commit
	FRemoteTransactionId MultiServerCommitRequestId;
	TArray<FRemoteServerId> MultiServerCommitReadyServers;
	bool bMultiServerCommitRequiresAbort = false;

	void RegisterSubsystem(FRemoteSubsystemBase* Subsystem)
	{
		Subsystems.Add(Subsystem);
	}

	FRemoteTransactionId GenerateNextTransactionId()
	{
		uint32 RawRequestId = NextTransactionRequestId++;
		return FRemoteTransactionId((RawRequestId++ % 0x800000u) + 1000u);
	}

	void EnqueueWork(FName WorkName, FRemoteWorkPriority InWorkPriority, bool bIsTransactional, const TFunction<void(void)>& InWork)
	{
		FRemoteExecutorWork& NewWork = PendingWorks.Emplace_GetRef();
		NewWork.Name = WorkName;
		NewWork.Priority = InWorkPriority;
		NewWork.Work = InWork;
		NewWork.bIsTransactional = bIsTransactional;
		NewWork.RequestId = GenerateNextTransactionId();
	}

	void ExecutePendingWork()
	{
		check(ExecutingWork == nullptr);

		double LastStallPrintTime = FPlatformTime::Seconds();

		for (int32 LocalIterationNumber = 0; ; LocalIterationNumber++)
		{
			double Now = FPlatformTime::Seconds();
			bool bPrintStall = false;

			if ((Now - LastStallPrintTime) > 1.0)
			{
				LastStallPrintTime = Now;
				bPrintStall = true;
			}

			// pump the network
			UE::RemoteExecutor::TickNetworkDelegate.ExecuteIfBound();

			// fetch and enqueue all RPCs to execute from the network
			for (;;)
			{
				TOptional<TTuple<FName, FRemoteWorkPriority, bool, TFunction<void(void)>>> NewWorkOpt = UE::RemoteExecutor::FetchNextDeferredRPCDelegate.Execute();
				if (NewWorkOpt.IsSet())
				{
					TTuple<FName, FRemoteWorkPriority, bool, TFunction<void(void)>>& NewWork = NewWorkOpt.GetValue();
					EnqueueWork(NewWork.Get<0>(), NewWork.Get<1>(), NewWork.Get<2>(), NewWork.Get<3>());

					UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d] enqueued deferred RPC '%s' %s %s"),
						LocalIterationNumber,
						*NewWork.Get<0>().ToString(),
						*NewWork.Get<1>().ToString(),
						NewWork.Get<2>() ? TEXT("(transactional)") :  TEXT("(non-transactional)"));
				}
				else
				{
					break;
				}
			}

			// check if we're actively servicing a remote multi-server commit
			if (ActiveRemoteMultiServerCommitRequestId.IsValid())
			{
				check(ActiveRemoteMultiServerCommitServerId.IsValid());
				check(ActiveRemoteMultiServerCommitPriority.IsValid());

				if (bPrintStall)
				{
					UE_LOG(LogRemoteExec, Verbose, TEXT("ExecutePendingWork[%d] Waiting on handling remote multi server commit %s %s"),
						LocalIterationNumber,
						*ActiveRemoteMultiServerCommitServerId.ToString(),
						*ActiveRemoteMultiServerCommitRequestId.ToString());
				}

				// if we have an active remote multi-server commit that we are servicing, we pause executing any local work
				// until that is complete
				continue;
			}

			// subsystem ticking
			for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
			{
				FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
				Subsystem->TickSubsystem();
			}

			//
			// if we get here and there is no work pending, break out and finish
			//
			if (PendingWorks.Num() == 0)
			{
				break;
			}

			for (int32 PendingWorkIndex = 0; PendingWorkIndex < PendingWorks.Num(); PendingWorkIndex++)
			{
				FRemoteExecutorWork& PendingWork = PendingWorks[PendingWorkIndex];

				UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d] : PendingWork[%d] '%s' RequestId %s %s"),
					LocalIterationNumber,
					PendingWorkIndex,
					*PendingWork.Name.ToString(),
					*PendingWork.RequestId.ToString(),
					*PendingWork.Priority.ToString());
			}

			// round robin through executing all pending work
			for(int32 PendingWorkIndex = 0; PendingWorkIndex < PendingWorks.Num(); )
			{
				check(ExecutingWork == nullptr);
				TGuardValue<FRemoteExecutorWork*> ExecutingWorkSave(ExecutingWork, &PendingWorks[PendingWorkIndex]);
				check(ExecutingWork);

				UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : Executing request %s %s --"),
					LocalIterationNumber,
					*ExecutingWork->RequestId.ToString(),
					*ExecutingWork->Name.ToString(),
					*ExecutingWork->Priority.ToString()); // -V522

				if (ExecutingWork->bIsTransactional)
				{
					if (!ExecutingWork->bRequestCreated)
					{
						// new request
						ExecutingWork->bRequestCreated = true;

						for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
						{
							FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
							UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : Creating request[%s]"),
								LocalIterationNumber,
								*ExecutingWork->RequestId.ToString(),
								Subsystem->NameForDebug());
							Subsystem->CreateRequest(ExecutingWork->RequestId, ExecutingWork->Priority);

							Subsystem->SetActiveRequest(ExecutingWork->RequestId);

							Subsystem->BeginRequest();
						}
					}
					else
					{
						for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
						{
							FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
							Subsystem->SetActiveRequest(ExecutingWork->RequestId);
						}
					}

					Now = FPlatformTime::Seconds();
					if ((Now - LastStallPrintTime) > 1.0)
					{
						LastStallPrintTime = Now;
						bPrintStall = true;
					}

					// tick the subsystems
					for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
					{
						FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
						Subsystem->TickRequest();
					}

					int32 SubsystemReadyCount = 0;
					for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
					{
						FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
						if (Subsystem->AreDependenciesSatisfied())
						{
							SubsystemReadyCount++;
						}
						else
						{
							if (bPrintStall)
							{
								UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : subsystem %s not ready..."),
									LocalIterationNumber,
									*ExecutingWork->RequestId.ToString(),
									Subsystem->NameForDebug());
							}
						}
					}

					if (SubsystemReadyCount != Subsystems.Num())
					{
						if (bPrintStall)
						{
							UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : %d of %d subsystems not ready..."),
								LocalIterationNumber,
								*ExecutingWork->RequestId.ToString(),
								Subsystems.Num() - SubsystemReadyCount, Subsystems.Num());
						}

						PendingWorkIndex++;
						continue;
					}

					// all of the subsystems are ready, try to perform the work
					AbortReason = ERemoteExecutorAbortReason::Unspecified;
					AbortReasonDescription = TEXT("");

					ExecutingWork->ExecutionAttempts++;

					FRemoteTransactionId RequestId = ExecutingWork->RequestId;
					UE::RemoteExecutor::OnTransactionStartingDelegate.Broadcast(RequestId, ExecutingWork->Name);
					AutoRTFM::ETransactionResult WorkTransactResult =
						AutoRTFM::Transact(
							[this, LocalIterationNumber]()
							{
								// You can set a breakpoint inside the abort handler below to see what caused the abort,
								// or you can rely on the log output which only catches livelocks
								const uint32 ExecutionAttempts = ExecutingWork->ExecutionAttempts;
								UE_AUTORTFM_ONABORT(ExecutionAttempts)
								{
									constexpr uint32 MaxExecutionAttemptsBeforeLivelockWarning = 200;
									if ((ExecutionAttempts >= MaxExecutionAttemptsBeforeLivelockWarning) && (ExecutionAttempts % MaxExecutionAttemptsBeforeLivelockWarning) == 0)
									{
										UE_LOG(LogRemoteExec, Display, TEXT("vvv Transaction Aborted %u Times. Dumping Callstack. vvv"), ExecutionAttempts);
										FDebug::DumpStackTraceToLog(ELogVerbosity::Display);
										UE_LOG(LogRemoteExec, Display, TEXT("^^^ Transaction Aborted %u Times ^^^"), ExecutionAttempts);
									}
								};

								ExecutingWork->Work();

								if (ExecutingWork->bRequiresMultiServerCommit)
								{
									// we're done with the work and about to commit - 
									// first we need to send borrowed objects back
									// to their owner and ask them if they are ready
									// to commit or not...
									UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : STARTING multi-server commit because %s"),
										LocalIterationNumber,
										*ExecutingWork->RequestId.ToString(),
										*ExecutingWork->RequiresMultiServerCommitReason);

									check (!MultiServerCommitRequestId.IsValid());
									check (MultiServerCommitReadyServers.Num() == 0);
									check (bMultiServerCommitRequiresAbort == false);

									UE_AUTORTFM_OPEN
									{
										MultiServerCommitRequestId = ExecutingWork->RequestId;
										bMultiServerCommitRequiresAbort = false;
									};

									// first collect the list of servers that need to be involved
									TArray<FRemoteServerId> MultiServerCommitRemoteServers;
									for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
									{
										UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : BeginMultiServerCommit subsystem %d..."),
											LocalIterationNumber,
											*ExecutingWork->RequestId.ToString(),
											SubsystemIndex);

										FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
										Subsystem->BeginMultiServerCommit(MultiServerCommitRemoteServers);
									}

									UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : BeginMultiServerCommit DONE (%d servers)"),
										LocalIterationNumber,
										*ExecutingWork->RequestId.ToString(),
										MultiServerCommitRemoteServers.Num());

									for (FRemoteServerId ServerId : MultiServerCommitRemoteServers)
									{
										UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : BeginMultiServerCommit server %s"),
											LocalIterationNumber,
											*ExecutingWork->RequestId.ToString(),
											*ServerId.ToString());
									}

									// signal to the relevant servers that they need to help us do a multi-server commit
									UE_AUTORTFM_OPEN
									{
										UE::RemoteExecutor::BeginMultiServerCommitDelegate.ExecuteIfBound(ExecutingWork->RequestId, ExecutingWork->Priority, MultiServerCommitRemoteServers);
									};

									// ask each subsystem to send any necessary data as part of this commit
									for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
									{
										UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : ExecuteMultiServerCommit subsystem %d..."),
											LocalIterationNumber,
											*ExecutingWork->RequestId.ToString(),
											SubsystemIndex);

										FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
										Subsystem->ExecuteMultiServerCommit();
									}

									UE_AUTORTFM_OPEN
									{
										// tell each server that we're done sending commit data and we are waiting for them
										// to respond with whether they are ready or we need to abort and retry
										UE::RemoteExecutor::ReadyMultiServerCommitDelegate.ExecuteIfBound(ExecutingWork->RequestId, MultiServerCommitRemoteServers);
									};

									// tick the network until everything is ready
									double LastPrintTime = FPlatformTime::Seconds();

									for (;;)
									{
										double Now = FPlatformTime::Seconds();
										if ((Now - LastPrintTime) > 1.0)
										{
											LastPrintTime = Now;
											UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : WAITING for multi-server commit..."),
												LocalIterationNumber,
												*ExecutingWork->RequestId.ToString());
										}
										
										if (MultiServerCommitRemoteServers.Num() == MultiServerCommitReadyServers.Num())
										{
											// we got a response from each server, do a sanity check to ensure the list of ready
											// servers is the list we expected to have										
											UE_AUTORTFM_OPEN
											{
												MultiServerCommitRemoteServers.Sort();
												MultiServerCommitReadyServers.Sort();
											};

											for (int32 ServerIndex = 0; ServerIndex < MultiServerCommitRemoteServers.Num(); ServerIndex++)
											{
												FRemoteServerId ServerId = MultiServerCommitRemoteServers[ServerIndex];
												FRemoteServerId ReadyServerId = MultiServerCommitReadyServers[ServerIndex];

												if (ServerId != ReadyServerId)
												{
													UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : Multi-server commit expected server %s but got %s"),
														LocalIterationNumber,
														*ExecutingWork->RequestId.ToString(),
														*ServerId.ToString(),
														*ReadyServerId.ToString());
												}

												check(ServerId == ReadyServerId);
											}

											if (bMultiServerCommitRequiresAbort)
											{
												UE_LOG(LogRemoteExec, Verbose, TEXT("ExecutePendingWork[%d,%s] : Multi-server commit ALL servers READY, but we are flagged with ABORT"),
													LocalIterationNumber,
													*ExecutingWork->RequestId.ToString());

												UE_AUTORTFM_OPEN
												{
													for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
													{
														FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
														Subsystem->AbortMultiServerCommit();
													}

													// aborted, tell all servers to abandon this commit
													UE::RemoteExecutor::AbandonMultiServerCommitDelegate.ExecuteIfBound(ExecutingWork->RequestId, MultiServerCommitRemoteServers);

													MultiServerCommitRequestId = FRemoteTransactionId::Invalid();
													MultiServerCommitReadyServers.Reset();
													bMultiServerCommitRequiresAbort = false;
												};

												UE::RemoteExecutor::AbortTransactionRequiresDependencies(TEXT("bMultiServerCommitRequiresAbort"));
												return;
											}
											else
											{
												// the commit was accepted by every server, notify each subsystem that we are committing
												UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : Multi-server commit ALL servers READY, COMMITTING"),
													LocalIterationNumber,
													*ExecutingWork->RequestId.ToString());

												for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
												{
													FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
													Subsystem->CommitMultiServerCommit();
												}

												break;
											}
										}

										UE_AUTORTFM_OPEN
										{
											UE::RemoteExecutor::TickNetworkDelegate.ExecuteIfBound();
										};
									}

									UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : DONE with multi-server commit"),
										LocalIterationNumber,
										*ExecutingWork->RequestId.ToString());

									UE_AUTORTFM_OPEN
									{
										MultiServerCommitRequestId = FRemoteTransactionId::Invalid();
										MultiServerCommitReadyServers.Reset();
										bMultiServerCommitRequiresAbort = false;
									};

									UE_AUTORTFM_OPEN
									{
										// tell each server that we are committed and they should commit
										UE::RemoteExecutor::EndMultiServerCommitDelegate.ExecuteIfBound(ExecutingWork->RequestId, MultiServerCommitRemoteServers);
									};
								};
							});

					bool bWorkComplete = false;
					bool bWorkTransactionAborted = false;

					if (WorkTransactResult == AutoRTFM::ETransactionResult::AbortedByRequest)
					{
						bWorkTransactionAborted = true;

						UE::RemoteExecutor::OnTransactionAbortedDelegate.Broadcast(RequestId);

						if (AbortReason == ERemoteExecutorAbortReason::RequiresDependencies) // -V547
						{
							UE_LOG(LogRemoteExec, Verbose, TEXT("ExecutePendingWork[%d,%s] : Work ABORTED for required dependency '%s'"),
								LocalIterationNumber,
								*ExecutingWork->RequestId.ToString(),
								*AbortReasonDescription);
						}
						else if (AbortReason == ERemoteExecutorAbortReason::AbandonWork) // -V547
						{
							UE_LOG(LogRemoteExec, Verbose, TEXT("ExecutePendingWork[%d,%s] : Work ABORTED for ABANDONMENT '%s'"),
								LocalIterationNumber,
								*ExecutingWork->RequestId.ToString(),
								*AbortReasonDescription);
							bWorkComplete = true;
						}
						else
						{
							UE_LOG(LogRemoteExec, Verbose, TEXT("ExecutePendingWork[%d,%s] : Work ABORTED for unknown reason '%s'"),
								LocalIterationNumber,
								*ExecutingWork->RequestId.ToString(),
								*AbortReasonDescription);
						}
					}
					else
					{
						check(AbortReason == ERemoteExecutorAbortReason::Unspecified);

						UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : Work COMPLETED after %u attempts"),
							LocalIterationNumber,
							*ExecutingWork->RequestId.ToString(),
							ExecutingWork->ExecutionAttempts);
						UE::RemoteExecutor::OnTransactionCompletedDelegate.Broadcast(RequestId);

						bWorkComplete = true;
					}

					if (bWorkComplete)
					{
						for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
						{
							FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
							UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ExecutePendingWork[%d,%s] : End request[%s]"),
								LocalIterationNumber,
								*ExecutingWork->RequestId.ToString(),
								Subsystem->NameForDebug());
							Subsystem->EndRequest(!bWorkTransactionAborted);
							Subsystem->ClearActiveRequest();
						}

						for (int32 SubsystemIndex = 0; SubsystemIndex < Subsystems.Num(); SubsystemIndex++)
						{
							FRemoteSubsystemBase* Subsystem = Subsystems[SubsystemIndex];
							Subsystem->DestroyRequest(ExecutingWork->RequestId);
						}

						// remove this work from the list and do not increment PendingWorkIndex
						PendingWorks.RemoveAt(PendingWorkIndex, EAllowShrinking::No);
					}
					else
					{
						// we will revisit this later, move on to the next work
						PendingWorkIndex++;
					}
				}
				else
				{
					// execute non-transactional work
					ExecutingWork->Work();

					// remove this work from the list and do not increment PendingWorkIndex
					PendingWorks.RemoveAt(PendingWorkIndex, EAllowShrinking::No);
				}

				// it's possible this work ended with trying to perform a multi-server
				// commit that got aborted because a remote multi-server commit of higher
				// priority came in. if so, we need to break out of processing further work
				// so we can service the remote multi-server commit
				if (ActiveRemoteMultiServerCommitRequestId.IsValid())
				{
					break;
				}
			}
		}
	}
};

FRemoteExecutor GRemoteExecutor;

void ExecuteTransactionalInternal(FName WorkName, FRemoteWorkPriority InWorkPriority, const TFunctionRef<void(void)>& InWork)
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	check(!AutoRTFM::IsClosed());

	GRemoteExecutor.EnqueueWork(WorkName, InWorkPriority, true, [&InWork]() { InWork(); });
	GRemoteExecutor.ExecutePendingWork();
#else
	InWork();
#endif
}

namespace UE::RemoteExecutor
{
void RegisterRemoteSubsystem(FRemoteSubsystemBase* Subsystem)
{
	GRemoteExecutor.RegisterSubsystem(Subsystem);
}

void AbortTransactionRequiresDependencies(FStringView Description)
{
	check(AutoRTFM::IsClosed());

	UE_AUTORTFM_OPEN
	{
		GRemoteExecutor.AbortReason = ERemoteExecutorAbortReason::RequiresDependencies;
		GRemoteExecutor.AbortReasonDescription = Description;
	};

	AutoRTFM::AbortTransaction();
}

void RollbackTransactionRequiresDependencies(FStringView Description)
{
	check(!AutoRTFM::IsClosed());

	GRemoteExecutor.AbortReason = ERemoteExecutorAbortReason::RequiresDependencies;
	GRemoteExecutor.AbortReasonDescription = Description;

	AutoRTFM::ForTheRuntime::RollbackTransaction();
}

void AbortTransactionAndAbandonWork(FStringView Description)
{
	check(AutoRTFM::IsClosed());

	UE_AUTORTFM_OPEN
	{
		GRemoteExecutor.AbortReason = ERemoteExecutorAbortReason::AbandonWork;
		GRemoteExecutor.AbortReasonDescription = Description;
	};

	AutoRTFM::AbortTransaction();
}

void TransactionRequiresMultiServerCommit(FStringView Description)
{
	if (GRemoteExecutor.ExecutingWork && !GRemoteExecutor.ExecutingWork->bRequiresMultiServerCommit)
	{
		GRemoteExecutor.ExecutingWork->bRequiresMultiServerCommit = true;
		GRemoteExecutor.ExecutingWork->RequiresMultiServerCommitReason = Description;

		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("TransactionRequiresMultiServerCommit ACTIVATED because: %s"), *GRemoteExecutor.ExecutingWork->RequiresMultiServerCommitReason);
	}
}

void BeginRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId, FRemoteWorkPriority RequestPriority)
{
	bool bAcceptRequest = false;
	bool bAbortLocalCommit = false;

	if (GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId.IsValid())
	{
		if (GRemoteExecutor.bActiveRemoteMultiServerCommitReady)
		{
			// we already told the remote server that we are READY, so we are locked in for a moment until we finish
			UE_LOG(LogRemoteExec, VeryVerbose, TEXT("BeginRemoteMultiServerCommit %s from %s %s DENYING because we are already READY with remote multi-server commit %s %s"),
				*RequestId.ToString(),
				*ServerId.ToString(),
				*RequestPriority.ToString(),
				*GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId.ToString(),
				*GRemoteExecutor.ActiveRemoteMultiServerCommitPriority.ToString());
		}
		else if (IsHigherPriority(RequestPriority, GRemoteExecutor.ActiveRemoteMultiServerCommitPriority))
		{
			UE_LOG(LogRemoteExec, VeryVerbose, TEXT("BeginRemoteMultiServerCommit %s from %s %s ACCEPTING because our remote multi-server commit %s is lower priority %s"),
				*RequestId.ToString(),
				*ServerId.ToString(),
				*RequestPriority.ToString(),
				*GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId.ToString(),
				*GRemoteExecutor.ActiveRemoteMultiServerCommitPriority.ToString());

			bAcceptRequest = true;
		}
		else
		{
			UE_LOG(LogRemoteExec, VeryVerbose, TEXT("BeginRemoteMultiServerCommit %s from %s %s DENYING because we are servicing higher priority remote multi-server commit %s %s"),
				*RequestId.ToString(),
				*ServerId.ToString(),
				*RequestPriority.ToString(),
				*GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId.ToString(),
				*GRemoteExecutor.ActiveRemoteMultiServerCommitPriority.ToString());
		}
	}
	else if (GRemoteExecutor.MultiServerCommitRequestId.IsValid())
	{
		// we are in a local multi-server commit - should we abort it in favor of this remote one?
		check(GRemoteExecutor.ExecutingWork);
		if (IsHigherPriority(RequestPriority, GRemoteExecutor.ExecutingWork->Priority))
		{
			UE_LOG(LogRemoteExec, VeryVerbose, TEXT("BeginRemoteMultiServerCommit %s from %s %s ACCEPTING because our local multi-server commit %s is lower priority %s"),
				*RequestId.ToString(),
				*ServerId.ToString(),
				*RequestPriority.ToString(),
				*GRemoteExecutor.MultiServerCommitRequestId.ToString(),
				*GRemoteExecutor.ExecutingWork->Priority.ToString());

			bAcceptRequest = true;
			bAbortLocalCommit = true;
		}
		else
		{
			UE_LOG(LogRemoteExec, VeryVerbose, TEXT("BeginRemoteMultiServerCommit %s from %s %s DENYING because we are locally in multi-server commit %s %s"),
				*RequestId.ToString(),
				*ServerId.ToString(),
				*RequestPriority.ToString(),
				*GRemoteExecutor.MultiServerCommitRequestId.ToString(),
				*GRemoteExecutor.ExecutingWork->Priority.ToString());
		}
	}
	else
	{
		// we aren't currently in a local multi-server commit, and we aren't
		// servicing a remote multi-server commit, so accept this
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("BeginRemoteMultiServerCommit %s %s from %s ACCEPTED"),
			*RequestId.ToString(),
			*RequestPriority.ToString(),
			*ServerId.ToString());

		bAcceptRequest = true;
	}

	if (bAcceptRequest)
	{
		// do we have to first abandon one we're working on?
		if (GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId.IsValid())
		{
			AbortRemoteMultiServerCommitDelegate.Execute(GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId, GRemoteExecutor.ActiveRemoteMultiServerCommitServerId);

			GRemoteExecutor.ActiveRemoteMultiServerCommitServerId = FRemoteServerId{};
			GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId = FRemoteTransactionId::Invalid();
			GRemoteExecutor.ActiveRemoteMultiServerCommitPriority = FRemoteWorkPriority{};
			GRemoteExecutor.bActiveRemoteMultiServerCommitReady = false;
			GRemoteExecutor.ActiveRemoteMultiServerCommitDeferredActions.Reset();
		}

		check(!GRemoteExecutor.ActiveRemoteMultiServerCommitServerId.IsValid());
		check(!GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId.IsValid());
		check(!GRemoteExecutor.ActiveRemoteMultiServerCommitPriority.IsValid());
		check(!GRemoteExecutor.bActiveRemoteMultiServerCommitReady);
		check(GRemoteExecutor.ActiveRemoteMultiServerCommitDeferredActions.Num() == 0);

		GRemoteExecutor.ActiveRemoteMultiServerCommitServerId = ServerId;
		GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId = RequestId;
		GRemoteExecutor.ActiveRemoteMultiServerCommitPriority = RequestPriority;
	}
	else
	{
		AbortRemoteMultiServerCommitDelegate.Execute(RequestId, ServerId);
	}

	if (bAbortLocalCommit)
	{
		// we can't immediately abort the transaction, set this flag for it to properly shut down gracefully
		GRemoteExecutor.bMultiServerCommitRequiresAbort = true;
	}
}

void EndRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId)
{
	UE_LOG(LogRemoteExec, VeryVerbose, TEXT("EndRemoteMultiServerCommit : %s %s"), *ServerId.ToString(), *RequestId.ToString());

	check(GRemoteExecutor.ActiveRemoteMultiServerCommitServerId == ServerId);
	check(GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId == RequestId);

	for (TFunction<void()>& DeferredAction : GRemoteExecutor.ActiveRemoteMultiServerCommitDeferredActions)
	{
		DeferredAction();
	}

	GRemoteExecutor.ActiveRemoteMultiServerCommitServerId = FRemoteServerId{};
	GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId = FRemoteTransactionId::Invalid();
	GRemoteExecutor.ActiveRemoteMultiServerCommitPriority = FRemoteWorkPriority{};
	GRemoteExecutor.bActiveRemoteMultiServerCommitReady = false;
	GRemoteExecutor.ActiveRemoteMultiServerCommitDeferredActions.Reset();
}

void AbandonRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId)
{
	if ((GRemoteExecutor.ActiveRemoteMultiServerCommitServerId == ServerId) &&
		GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId == RequestId)
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("AbandonRemoteMultiServerCommit : %s %s"), *ServerId.ToString(), *RequestId.ToString());

		GRemoteExecutor.ActiveRemoteMultiServerCommitServerId = FRemoteServerId{};
		GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId = FRemoteTransactionId::Invalid();
		GRemoteExecutor.ActiveRemoteMultiServerCommitPriority = FRemoteWorkPriority{};
		GRemoteExecutor.bActiveRemoteMultiServerCommitReady = false;
		GRemoteExecutor.ActiveRemoteMultiServerCommitDeferredActions.Reset();
	}
	else
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("AbandonRemoteMultiServerCommit : %s %s IGNORING"), *ServerId.ToString(), *RequestId.ToString());
	}
}

void EnqueueRemoteMultiServerCommitAction(FRemoteServerId ServerId, FRemoteTransactionId RequestId, const TFunction<void()>& Action)
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	// THIS has to sit in a holding pen...
	if ((GRemoteExecutor.ActiveRemoteMultiServerCommitServerId == ServerId) &&
		GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId == RequestId)
	{
		UE_LOG(LogRemoteExec, Verbose, TEXT("EnqueueRemoteMultiServerCommitAction Enqueueing action from %s request %s"), *ServerId.ToString(), *RequestId.ToString());

		GRemoteExecutor.ActiveRemoteMultiServerCommitDeferredActions.Add(Action);
	}
	else
	{
		// else we've received this message for a different multi-server commit that we no longer have active, so just abandon
		// since they would have been told previously that we were aborting it
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("EnqueueRemoteMultiServerCommitAction : IGNORING action from %s request %s"), *ServerId.ToString(), *RequestId.ToString());
	}
#endif
}

void ReadyMultiServerCommitResponse(FRemoteServerId ServerId, FRemoteTransactionId RequestId)
{
	if (GRemoteExecutor.MultiServerCommitRequestId == RequestId)
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ReadyMultiServerCommitResponse got ready server: %s"), *ServerId.ToString());
		GRemoteExecutor.MultiServerCommitReadyServers.Add(ServerId);
	}
	else
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ReadyMultiServerCommitResponse ignoring %s because we are working on %s"), *RequestId.ToString(), *GRemoteExecutor.MultiServerCommitRequestId.ToString());
	}
}

void AbortMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId)
{
	if (GRemoteExecutor.MultiServerCommitRequestId == RequestId)
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("AbortMultiServerCommit got valid: %s"), *RequestId.ToString());
		GRemoteExecutor.bMultiServerCommitRequiresAbort = true;
		GRemoteExecutor.MultiServerCommitReadyServers.Add(ServerId);
	}
	else
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("AbortMultiServerCommit ignoring %s because we are working on %s"), *RequestId.ToString(), *GRemoteExecutor.MultiServerCommitRequestId.ToString());
	}
}

void ReadyRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId)
{
	if ((GRemoteExecutor.ActiveRemoteMultiServerCommitServerId == ServerId) &&
		(GRemoteExecutor.ActiveRemoteMultiServerCommitRequestId == RequestId))
	{
		UE_LOG(LogRemoteExec, VeryVerbose, TEXT("ReadyRemoteMultiServerCommit %s from %s"), *RequestId.ToString(), *ServerId.ToString());

		check(!GRemoteExecutor.bActiveRemoteMultiServerCommitReady);
		GRemoteExecutor.bActiveRemoteMultiServerCommitReady = true;

		// if everything passed OK we need to send a message back saying we're ready
		// TODO: this is where we'll need to hook code in that verifies everything
		// we received is able to be accepted
		ReadyRemoteMultiServerCommitDelegate.Execute(RequestId, ServerId);
	}
}

FRemoteWorkPriority CreateRootWorkPriority()
{
	FRemoteWorkPriority RootWorkPriority = FRemoteWorkPriority::CreateRootWorkPriority(UE::RemoteObject::GetGlobalServerId(), GRemoteExecutor.GenerateNextTransactionId());
	return RootWorkPriority;
}

void ExecutePendingWork()
{
	GRemoteExecutor.ExecutePendingWork();
}

void ExecuteTransactionalWithExplicitPriority(FName WorkName, FRemoteWorkPriority WorkPriority, const TFunctionRef<void(void)>& Work)
{
#if UE_WITH_REMOTE_OBJECT_HANDLE
	ExecuteTransactionalInternal(WorkName, WorkPriority, Work);
#else
	Work();
#endif
}

void ExecuteTransactional(FName WorkName, const TFunctionRef<void(void)>& Work)
{
	FRemoteWorkPriority RootWorkPriority = FRemoteWorkPriority::CreateRootWorkPriority(UE::RemoteObject::GetGlobalServerId(), GRemoteExecutor.GenerateNextTransactionId());
	ExecuteTransactionalWithExplicitPriority(WorkName, RootWorkPriority, Work);
}

} // namespace UE::RemoteExecutor

FString FRemoteWorkPriority::ToString() const
{
	return FString::Format(TEXT("[pri: rsi {0} depth {1} id {2}]"),
		{ *GetRootServerId().ToString(),
		GetWorkDepth(),
		*GetRootWorkTransactionId().ToString() });
}

FRemoteWorkPriority FRemoteWorkPriority::CreateRootWorkPriority(FRemoteServerId ServerId, FRemoteTransactionId TransactionId)
{
	FRemoteWorkPriority Result{};

	uint32 RawServerId = 0;
	FMemory::Memcpy(&RawServerId, &ServerId, sizeof(FRemoteServerId));

	uint32 RawTransactionId = 0;
	FMemory::Memcpy(&RawTransactionId, &TransactionId, sizeof(FRemoteTransactionId));

	Result.PackedData |= (static_cast<uint64>(RawServerId) << 32);
	Result.PackedData |= (0xFFull << 24);
	Result.PackedData |= (RawTransactionId & 0xFFFFFFu);
	return Result;
}

FRemoteWorkPriority FRemoteWorkPriority::CreateDependentWorkPriority() const
{
	FRemoteWorkPriority Result = *this;
	Result.PackedData &= 0xFFFFFFFF00FFFFFFull;

	// work depth must be non-zero else we would wrap back around
	check(GetWorkDepth() > 0);

	Result.PackedData |= static_cast<uint64>(GetWorkDepth() - 1) << 24;
	return Result;
}

// returns if Lhs is higher priority than Rhs
bool IsHigherPriority(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs)
{
	return Lhs.PackedData < Rhs.PackedData;
}

bool IsEqualPriority(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs)
{
	// they're equal if neither is higher than the other
	return !IsHigherPriority(Lhs, Rhs) && !IsHigherPriority(Rhs, Lhs);
}

bool IsHigherOrEqualPriority(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs)
{
	return IsHigherPriority(Lhs, Rhs) || IsEqualPriority(Lhs, Rhs);
}

bool operator==(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs)
{
	return Lhs.PackedData == Rhs.PackedData;
}

FArchive& operator<<(FArchive& Ar, FRemoteWorkPriority& Priority)
{
	Ar << Priority.PackedData;
	return Ar;
}
