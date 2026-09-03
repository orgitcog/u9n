// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Engine/World.h"
#include "IAssetRegistryTagProviderInterface.h"
#include "Misc/AsyncTaskNotification.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "EditorUtilityTask.generated.h"

class FAsyncTaskNotification;
class FText;
class UEditorUtilitySubsystem;
struct FFrame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditorUtilityTaskDynamicDelegate, UEditorUtilityTask*, Task);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEditorUtilityTaskDelegate, UEditorUtilityTask* /*Task*/);

/**
 * 
 */
UCLASS(Abstract, Blueprintable, meta = (ShowWorldContextPin))
class BLUTILITY_API UEditorUtilityTask : public UObject, public IAssetRegistryTagProviderInterface
{
	GENERATED_BODY()

public:
	FOnEditorUtilityTaskDelegate OnFinished;

public:
	UEditorUtilityTask();

	UFUNCTION()
	void Run();

	//~ Begin IAssetRegistryTagProviderInterface interface
	virtual bool ShouldAddCDOTagsToBlueprintClass() const override
	{
		return true;
	}
	//~ End IAssetRegistryTagProviderInterface interface

	virtual UWorld* GetWorld() const override;

public:
	UFUNCTION(BlueprintCallable, Category=Task)
	void FinishExecutingTask(const bool bSuccess = true);

	UFUNCTION(BlueprintCallable, Category = Task)
	void SetTaskNotificationText(const FText& Text);

	// Calls CancelRequested() and ReceiveCancelRequested()
	void RequestCancel();

	UFUNCTION(BlueprintCallable, Category = Task)
	bool WasCancelRequested() const;

protected:
	virtual void BeginExecution() {}
	virtual void CancelRequested() {}
	virtual FText GetTaskTitle() const;

	UFUNCTION(BlueprintImplementableEvent, Category=Task, meta=(DisplayName="BeginExecution"))
	void ReceiveBeginExecution();

	UFUNCTION(BlueprintImplementableEvent, Category=Task, meta=(DisplayName="CancelRequested"))
	void ReceiveCancelRequested();

	UFUNCTION(BlueprintImplementableEvent, Category=Task)
	FText GetTaskTitleOverride() const;

private:
	// Calls GetTaskTitle() and GetTaskTitleOverride()
	void CreateNotification();

	// Calls BeginExecution() and ReceiveBeginExecution()
	void StartExecutingTask();

protected:
	/** Run this editor utility on start-up (after asset discovery)? */
	UPROPERTY(Category=Settings, EditDefaultsOnly, AssetRegistrySearchable, DisplayName="Run on Start-up")
	bool bRunEditorUtilityOnStartup = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UEditorUtilitySubsystem> MyTaskManager;

	UPROPERTY(Transient)
	TObjectPtr<UEditorUtilityTask> MyParentTask;

	UPROPERTY(Transient)
	bool bCancelRequested = false;

	bool Cached_GIsRunningUnattendedScript = false;

	TUniquePtr<FAsyncTaskNotification> TaskNotification;

	friend UEditorUtilitySubsystem;
};
