// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet2/CompilerResultsLog.h"

class UAnimNextRigVMAssetEditorData;
class FCompilerResultsLog;

namespace UE::AnimNext::UncookedOnly
{

// RAII helper for scoping output of compiler results
class FScopedCompilerResults
{
public:
	ANIMNEXTUNCOOKEDONLY_API explicit FScopedCompilerResults(const FText& InJobName);
	ANIMNEXTUNCOOKEDONLY_API explicit FScopedCompilerResults(UObject* InObject);
	ANIMNEXTUNCOOKEDONLY_API FScopedCompilerResults(const FText& InJobName, UObject* InObject, TArrayView<UObject*> InAssets);
	ANIMNEXTUNCOOKEDONLY_API ~FScopedCompilerResults();

	// Get the log that is currently in scope, asserts if no scope is active
	ANIMNEXTUNCOOKEDONLY_API static FCompilerResultsLog& GetLog();

private:
	TSharedPtr<FCompilerResultsLog> Log;
	FText JobName;
	UObject* Object = nullptr;
	double StartTime = 0.0;
	double FinishTime = 0.0;
};

}
