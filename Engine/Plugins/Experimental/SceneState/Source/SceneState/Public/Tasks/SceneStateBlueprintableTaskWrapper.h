// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateTask.h"
#include "SceneStateTaskInstance.h"
#include "Templates/SubclassOf.h"
#include "SceneStateBlueprintableTaskWrapper.generated.h"

class USceneStateBlueprintableTask;

USTRUCT()
struct FSceneStateBlueprintableTaskInstance : public FSceneStateTaskInstance
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Instanced, Category="Scene State")
	TObjectPtr<USceneStateBlueprintableTask> Task;
};

USTRUCT(meta=(Hidden))
struct FSceneStateBlueprintableTaskWrapper : public FSceneStateTask
{
	GENERATED_BODY()

	using FInstanceDataType = FSceneStateBlueprintableTaskInstance;

	SCENESTATE_API FSceneStateBlueprintableTaskWrapper();

	SCENESTATE_API bool SetTaskClass(TSubclassOf<USceneStateBlueprintableTask> InTaskClass);

	TSubclassOf<USceneStateBlueprintableTask> GetTaskClass() const
	{
		return TaskClass;
	}

protected:
	//~ Begin FSceneStateTask
#if WITH_EDITOR
	virtual const UScriptStruct* OnGetTaskInstanceType() const override;
	virtual void OnBuildTaskInstance(UObject* InOuter, FStructView InTaskInstance) const override;
#endif
	virtual void OnSetup(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const override;
	virtual void OnStart(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const override;
	virtual void OnTick(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance, float InDeltaSeconds) const override;
	virtual void OnStop(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance, ESceneStateTaskStopReason InStopReason) const override;
	//~ End FSceneStateTask

private:
	UPROPERTY()
	TSubclassOf<USceneStateBlueprintableTask> TaskClass;
};
