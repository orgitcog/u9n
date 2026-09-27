// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AUsdStageActor;
class UPackage;
class UUsdStageImportOptions;

class USDSTAGEEDITORVIEWMODELS_API FUsdStageViewModel
{
public:
	void NewStage();
	void OpenStage(const TCHAR* FilePath);
	void SetLoadAllRule();
	void SetLoadNoneRule();
	// Returns false if we have a payload in the current stage that has been manually loaded or unloaded. Returns true otherwise.
	// The intent is to get the LoadAll/LoadNone buttons to *both* show as unchecked in case the user manually toggled any,
	// to indicate that picking either LoadAll or LoadNone would trigger some change.
	bool HasDefaultLoadRule();
	void ReloadStage();
	void ResetStage();
	void CloseStage();
	void SaveStage();
	/** Temporary until SaveAs feature is properly implemented, may be removed in a future release */
	void SaveStageAs(const TCHAR* FilePath);
	void ImportStage(const TCHAR* TargetContentFolder = nullptr, UUsdStageImportOptions* Options = nullptr);

public:
	TWeakObjectPtr<AUsdStageActor> UsdStageActor;
};
