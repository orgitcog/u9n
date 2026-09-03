// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/Task.h"
#include "Async/Fundamental/Task.h"

class ULevel;
class FSceneInterface;
class UPrimitiveComponent;

struct FAsyncRegisterLevelContext;

struct FAsyncAddPrimitiveQueue
{
private:
	using FPrimitiveBatch = TArray<TWeakObjectPtr<UPrimitiveComponent>, FConcurrentLinearArrayAllocator>;
	FAsyncAddPrimitiveQueue(FAsyncRegisterLevelContext& InContext);
	~FAsyncAddPrimitiveQueue();
	void AddPrimitive(UPrimitiveComponent* InComponent);
	void Push(FPrimitiveBatch& InAddPrimitives);
	bool Tick();
	void Flush();
	bool IsRunningAsync() const;
	bool HasRemainingWork() const;
	void WaitForAsyncTask();

	struct FAddPrimitivesTask
	{
	public:
		void Launch(TArray<FPrimitiveBatch>& InBatch, FSceneInterface* InScene);
		bool IsValid() const;
		bool IsCompleted() const;
		void Wait();
		void Reset();
		static void Execute(const TArray<FPrimitiveBatch>& InBatch, FSceneInterface* InScene);
		static void Execute(UPrimitiveComponent* Component, FSceneInterface* InScene, bool bAppCanEverRender);

		TArray<FPrimitiveBatch> Batches;
		UE::Tasks::TTask<void> Task;
	};

	FAsyncRegisterLevelContext& Context;
	FPrimitiveBatch NextBatch;
	TArray<FPrimitiveBatch> AddPrimitivesArray;
	FAddPrimitivesTask AsyncTask;
	friend FAsyncRegisterLevelContext;
};

struct FSendRenderDynamicDataPrimitivesQueue
{
private:
	FSendRenderDynamicDataPrimitivesQueue(FAsyncRegisterLevelContext& InContext);
	~FSendRenderDynamicDataPrimitivesQueue();
	using FPrimitiveBatch = TArray<TWeakObjectPtr<UPrimitiveComponent>, FConcurrentLinearArrayAllocator>;
	void AddSendRenderDynamicData(UPrimitiveComponent* InComponent);
	void Push(FPrimitiveBatch& InSendRenderDynamicDataPrimitives);
	bool Tick();
	void Flush();
	bool IsRunningAsync() const { return false; }
	bool HasRemainingWork() const;
	void WaitForAsyncTask() {}

	FAsyncRegisterLevelContext& Context;
	FPrimitiveBatch NextBatch;
	TArray<FPrimitiveBatch> SendRenderDynamicDataPrimitivesArray;
	friend FAsyncRegisterLevelContext;
};

struct FAsyncRegisterLevelContext
{
public:
	static TUniquePtr<FAsyncRegisterLevelContext> CreateInstance(ULevel* InLevel);
	FAsyncRegisterLevelContext(ULevel* InLevel);
	void AddPrimitive(UPrimitiveComponent* InComponent);
	void AddSendRenderDynamicData(UPrimitiveComponent* InComponent);
	void SetCanLaunchNewTasks(bool bValue);
	void SetIncrementalRegisterComponentsDone(bool bValue);
	bool GetIncrementalRegisterComponentsDone() const { return bIncrementalRegisterComponentsDone; }
	bool Tick();
	bool IsRunningAsync() const;
	bool HasRemainingWork() const;
	void WaitForAsyncTasks();
	
private:
	void Flush();

	ULevel* Level;
	FAsyncAddPrimitiveQueue AsyncAddPrimitiveQueue;
	FSendRenderDynamicDataPrimitivesQueue SendRenderDynamicDataPrimitivesQueue;
	bool bCanLaunchNewTasks;
	bool bIncrementalRegisterComponentsDone;

	friend FAsyncAddPrimitiveQueue;
	friend FSendRenderDynamicDataPrimitivesQueue;
};