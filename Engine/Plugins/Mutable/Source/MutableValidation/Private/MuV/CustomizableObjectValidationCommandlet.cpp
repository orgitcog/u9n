// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuV/CustomizableObjectValidationCommandlet.h"

#include "CustomizableObjectCompilationUtility.h"
#include "CustomizableObjectInstanceUpdateUtility.h"
#include "ValidationUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "Containers/SpscQueue.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectCompilerTypes.h"
#include "MuR/Model.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/LoadUtils.h"
#include "MuCOE/CustomizableObjectBenchmarkingUtils.h"
#include "Serialization/MemoryWriter.h"


int32 UCustomizableObjectValidationCommandlet::Main(const FString& Params)
{
	LLM_SCOPE_BYNAME(TEXT("CustomizableObjectValidationCommandlet"));
	
	// Execution arguments for commandlet from IDE
	// -run=CustomizableObjectValidation -CustomizableObject=(PathToCO)

	// Ensure we have set the mutable system to the benchmarking mode and that we are reporting benchmarking data
	FLogBenchmarkUtil::SetBenchmarkReportingStateOverride(true);
	UCustomizableObjectSystemPrivate::SetUsageOfBenchmarkingSettings(true);
	
	// Ensure we do not show any OK dialog since we are not an user that can interact with them
	GIsRunningUnattendedScript = true;
	
	// Get the package name of the CO to test
	TStrongObjectPtr<UCustomizableObject> TargetCustomizableObject = nullptr;
	{
		FString CustomizableObjectAssetPath = "";
		if (!FParse::Value(*Params, TEXT("CustomizableObject="), CustomizableObjectAssetPath))
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to parse Customizable Object package name from provided argument : %s"), *Params)
			return 1;
		}
	
		// Load the resource
		UObject* FoundObject = MutablePrivate::LoadObject(FSoftObjectPath(CustomizableObjectAssetPath));
		if (!FoundObject)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to retrieve UObject from path %s"), *CustomizableObjectAssetPath);
			return 1;
		}
	
		// Get the CustomizableObject.
		TargetCustomizableObject = TStrongObjectPtr(Cast<UCustomizableObject>(FoundObject));
		if (!TargetCustomizableObject)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to cast found UObject to UCustomizableObject."));
			return 1;
		}
	}
	
	// What platform we want to compile the CO against
	ITargetPlatform* TargetCompilationPlatform = GetCompilationPlatform(Params);
	if (!TargetCompilationPlatform)
	{
		return 1;
	}
	// Parse if we want to use disk compilation or not
	const bool bUseDiskCompilation = GetDiskCompilationArg(Params);
	const uint32 InstancesToGenerate = GetTargetAmountOfInstances(Params);
	
	// Perform a blocking search to ensure all assets used by mutable are reachable using the AssetRegistry
	PrepareAssetRegistry();

	// Make sure there is nothing else that the engine needs to do before starting our test
	Wait(60);

	LogGlobalSettings();

	// Body of the test ------------------------------------------------------------------------------------------------------------------------------
	TestCustomizableObject(*TargetCustomizableObject, *TargetCompilationPlatform, InstancesToGenerate, bUseDiskCompilation);
	// -----------------------------------------------------------------------------------------------------------------------------------------------
	
	UE_LOG(LogMutable, Display, TEXT("Mutable commandlet finished."));
	return 0;
}
