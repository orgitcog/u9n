// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSubmitToolEditor, Log, All);

enum class FSubmitOverrideReply;
class USubmitToolEditorSettings;
class ISourceControlProvider;
struct SSubmitOverrideParameters;

class FSubmitToolEditorModule : public IModuleInterface
{
public:
	/**
	 * Get reference to the SubmitToolEditor module instance
	 */
	static inline FSubmitToolEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FSubmitToolEditorModule>("SubmitToolEditor");
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterSubmitOverrideDelegate(const USubmitToolEditorSettings* InSettings);
	void UnregisterSubmitOverrideDelegate();
	
private:

	FSubmitOverrideReply OnSubmitToolOverrideCallback(SSubmitOverrideParameters InParameters);
	FSubmitOverrideReply InvokeSubmitTool(ISourceControlProvider& InProvider, const FString& InPath, const FString& InArgs, const FString& InDescription, const FString& InIdentifier);
	FSubmitOverrideReply InvokeSubmitTool(ISourceControlProvider& InProvider, const FString& InPath, const FString& InArgs, const FString& InDescription, const TArray<FString>& InFiles);
	FSubmitOverrideReply InvokeSubmitTool(ISourceControlProvider& InProvider, const FString& InPath, const FString& InArgs, const FString& InIdentifier);

	bool GetPerforceParameter(ISourceControlProvider& InProvider, FString& OutPort, FString& OutUser, FString& OutClient, FString& OutWorkspacePath);
	bool EditChangelistDescription(ISourceControlProvider& InProvider, const FString& InNewChangelistDescription, const FString& InIdentifier, FString& OutIdentifier);
	void SaveChangelistDescription(ISourceControlProvider& InProvider, const FString& InNewChangelistDescription, const FString& InIdentifier);
	bool CreateChangelist(ISourceControlProvider& InProvider, const FString& InNewChangelistDescription, const TArray<FString>& InFiles, FString& OutIdentifier);
	bool GetChangelistValidationResult(ISourceControlProvider& InProvider, const FString& InIdentifier);
	FString UpdateValidationTag(const FString& InDescription, bool bIsvalid);

	bool Tick(float InDeltaTime);

	FProcHandle SubmitToolProcHandle;
};
