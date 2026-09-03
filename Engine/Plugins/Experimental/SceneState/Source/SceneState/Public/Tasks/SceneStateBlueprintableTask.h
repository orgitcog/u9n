// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneStateEventHandlerProvider.h"
#include "PropertyBindingTypes.h"
#include "SceneStateEnums.h"
#include "SceneStateRange.h"
#include "SceneStateTaskExecutionContext.h"
#include "UObject/Object.h"
#include "SceneStateBlueprintableTask.generated.h"

class USceneStateEventStream;
class USceneStateObject;
struct FSceneStateBlueprintableTaskWrapper;
struct FSceneStateExecutionContext;

UCLASS(MinimalAPI, Abstract, Blueprintable, BlueprintType, AutoExpandCategories=("Task Settings"), EditInlineNew)
class USceneStateBlueprintableTask : public UObject, public ISceneStateEventHandlerProvider
{
	GENERATED_BODY()

	friend FSceneStateBlueprintableTaskWrapper;

public:
	/** Called once when the Task starts */
	UFUNCTION(BlueprintImplementableEvent, Category="Task")
	void ReceiveStart();

	/**
	 * Called every frame after the Task has started
	 * Must have bCanTick set to true
	 * @see USceneStateTask::CanTick
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Task")
	void ReceiveTick(float InDeltaSeconds);

	/** Called once when the Task ends */
	UFUNCTION(BlueprintImplementableEvent, Category="Task")
	void ReceiveStop(ESceneStateTaskStopReason InStopReason);

	UFUNCTION(BlueprintCallable, Category="Task")
	USceneStateObject* GetRootState() const;

	const FSceneStateExecutionContext& GetExecutionContext() const;

	UFUNCTION(BlueprintCallable, Category="Task")
	UObject* GetContextObject() const;

	UFUNCTION(BlueprintCallable, Category="Task")
	USceneStateEventStream* GetEventStream() const;

	UFUNCTION(BlueprintCallable, Category="Task")
	void FinishTask(ESceneStateTaskResult InTaskResult = ESceneStateTaskResult::Succeeded);

	UFUNCTION(BlueprintPure, Category="Task")
	float GetTaskElapsedTime() const;

	UFUNCTION(BlueprintPure, Category="Task")
	ESceneStateTaskResult GetTaskResult() const;

	//~ Begin ISceneStateEventHandlerProvider
	virtual bool FindEventHandlerId(const FSceneStateEventSchemaHandle& InEventSchemaHandle, FGuid& OutHandlerId) const;
	//~ End ISceneStateEventHandlerProvider

	//~ Begin UObject
	virtual UWorld* GetWorld() const override final;
	//~ End UObject

private:
	/** Context to retrieve the underlying Task/Task Instance for this blueprint task */
	UE::SceneState::FTaskExecutionContext TaskExecutionContext;
};
