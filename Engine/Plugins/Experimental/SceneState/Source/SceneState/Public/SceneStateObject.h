// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "SceneState.h"
#include "SceneStateEnums.h"
#include "SceneStateEventStream.h"
#include "SceneStateExecutionContext.h"
#include "SceneStateRange.h"
#include "UObject/Object.h"
#include "SceneStateObject.generated.h"

class USceneStateBlueprintableTask;
class USceneStateEventStream;
class USceneStateGeneratedClass;
struct FPropertyBindingDataView;
struct FSceneStateBindingDataHandle;
struct FSceneStateExecutionContext;

namespace UE::SceneState
{
	class FExecutionContextRegistry;
}

UCLASS(MinimalAPI, Blueprintable)
class USceneStateObject : public UObject
{
	GENERATED_BODY()

	friend USceneStateGeneratedClass;

public:
	SCENESTATE_API USceneStateObject();

	SCENESTATE_API FString GetContextName() const;

	UFUNCTION(BlueprintCallable, Category="State")
	SCENESTATE_API UObject* GetContextObject() const;

	UFUNCTION(BlueprintPure, Category="Events")
	USceneStateEventStream* GetEventStream() const
	{
		return EventStream;
	}

	/** Returns true if the object has an active root state */
	UFUNCTION(BlueprintPure, Category="State")
	SCENESTATE_API bool IsActive() const;

	void Setup();

	void Enter();

	void Tick(float InDeltaSeconds);

	void Exit();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Enter", Category = "State Events")
	void ReceiveEnter();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Tick", Category = "State Events")
	void ReceiveTick(float InDeltaSeconds);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "Exit", Category = "State Events")
	void ReceiveExit();

	TSharedRef<UE::SceneState::FExecutionContextRegistry> GetContextRegistry() const;

	//~ Begin UObject
	SCENESTATE_API virtual UWorld* GetWorld() const override final;
	SCENESTATE_API virtual void BeginDestroy() override;
	//~ End UObject

private:
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient, NonTransactional, meta=(NoBinding))
	TObjectPtr<USceneStateGeneratedClass> GeneratedClass;

	/** Runtime Event System keeping track of Active Events that have been added */
	UPROPERTY(Transient, Instanced, meta=(NoBinding))
	TObjectPtr<USceneStateEventStream> EventStream;

	/** The top-level execution context of the Scene State */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient, NonTransactional, meta=(NoBinding))
	FSceneStateExecutionContext RootExecutionContext;

	/** The registry containing all the execution contexts in this object */
	TSharedPtr<UE::SceneState::FExecutionContextRegistry> ContextRegistry;
};
