// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuV/CustomizableObjectBulkValidationCommandlet.h"

#include "ScopedLogSection.h"
#include "ValidationUtils.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/ICustomizableObjectEditorModule.h"
#include "MuCO/LoadUtils.h"
#include "MuCO/LogBenchmarkUtil.h"


int32 UCustomizableObjectBulkValidationCommandlet::Main(const FString& Params)
{
	LLM_SCOPE_BYNAME(TEXT("CustomizableObjectBulkValidationCommandlet"));

	// Prepare the environment for the testing -------------------------------------------------------------------------------------------------------
	
	// Ensure we have set the mutable system to the benchmarking mode and that we are reporting benchmarking data
	FLogBenchmarkUtil::SetBenchmarkReportingStateOverride(true);
	UCustomizableObjectSystemPrivate::SetUsageOfBenchmarkingSettings(true);
	
	// Ensure we do not show any OK dialog since we are not a user that can interact with them
	GIsRunningUnattendedScript = true;

	// Parse the arguments provided ------------------------------------------------------------------------------------------------------------------
	
	// Parse the value for the platform to be used for the compilation of the COs
	const ITargetPlatform* TargetCompilationPlatform = GetCompilationPlatform(Params);
	if (!TargetCompilationPlatform)
	{
		return 1;
	}
	// Parse if we want to use disk compilation or not
	const bool bUseDiskCompilation = GetDiskCompilationArg(Params);
	// Get the amount of instances to generate if parameter was provided (it will get multiplied by the amount of states later so this is a minimun value)
	const uint32 InstancesToGenerate = GetTargetAmountOfInstances(Params);

	// Work only for the root Customizable Objects found
	bool bOnlyTestRootObjects = false;
	FParse::Bool(*Params,TEXT("SkipNonRootObjects="),bOnlyTestRootObjects);
	if (bOnlyTestRootObjects)
	{
		UE_LOG(LogMutable, Display, TEXT("Only the root COs will be tested")) ;
	}

	// Find the assets to be tested based on the path provided ---------------------------------------------------------------------------------------
	
	// Parse the path to search for COs on
	FName CustomizableObjectsSearchPath;
	{
		if (!FParse::Value(*Params, TEXT("CustomizableObjectsSearchPath="), CustomizableObjectsSearchPath))
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to parse Customizable Object search path from the provided argument : %s"), *Params)
			return 1;
		}

		if (CustomizableObjectsSearchPath.IsNone())
		{
			UE_LOG(LogMutable, Error, TEXT("The path to scan can not be empty"))
			return 1;
		}
	}

	// Perform a blocking search to ensure all assets used by mutable are reachable using the AssetRegistry
	PrepareAssetRegistry();
	
	// Then run what we know as the UCustomizableObjectValidationCommandlet body for each one of them. Keep in mind that you may need to do some cleanup
	// between runs.
	TArray<FAssetData> FoundAssetData;
	{
		LLM_SCOPE_BYNAME(TEXT("CustomizableObjectBulkValidationCommandlet/AssetsSearch"));

		FoundAssetData = FindAllAssetsAtPath(CustomizableObjectsSearchPath, UCustomizableObject::StaticClass());

		// Early exit if no instances could be found for testing.
		if (FoundAssetData.IsEmpty())
		{
			UE_LOG(LogMutable, Error, TEXT("Aborting Bulk Customizable Object Validation Test: No assets could be found at the provided package path"));
			return 1;
		}
		
		// Log all the Customizable Objects to be tested:
		UE_LOG(LogMutable, Display, TEXT("Found a total of %u Customizable Objects to validate. Some may be discarded based on the test settings."), FoundAssetData.Num())
		for (const FAssetData& MutableAssetData : FoundAssetData)
		{
			UE_LOG(LogMutable, Display, TEXT("\t - %s (%s)"), *MutableAssetData.AssetName.ToString(), *MutableAssetData.PackageName.ToString());
		}
	}
	
	// Make sure there is nothing else that the engine needs to do before starting our test
	Wait(60);
	
	LogGlobalSettings();
	
	// All pre-testing operations completed. Starting the testing of COs -----------------------------------------------------------------------------

	const int32 FoundAssetsCount = FoundAssetData.Num();
	for (int32 AssetIndex = 0; AssetIndex < FoundAssetsCount; ++AssetIndex)
	{
		LLM_SCOPE_BYNAME(TEXT("CustomizableObjectBulkValidationCommandlet/COTest"));

		const FAssetData& CustomizableObjectAssetData = FoundAssetData[AssetIndex];
		TObjectPtr<UObject> FoundObject = MutablePrivate::LoadObject(CustomizableObjectAssetData);
		if (!FoundObject)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to load the asset with path : %s ."), *CustomizableObjectAssetData.GetSoftObjectPath().ToString())
			continue;
		}

		TObjectPtr<UCustomizableObject> TargetCustomizableObject = Cast<UCustomizableObject>(FoundObject);
		if (!TargetCustomizableObject)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to cast found UObject to UCustomizableObject."));
			continue;
		}		
		
		const FString CustomizableObjectName = TargetCustomizableObject->GetName();
		
		// If we only want to test root objects and this is not one go to the next CO
		if (bOnlyTestRootObjects && !ICustomizableObjectEditorModule::GetChecked().GetRootObject(TargetCustomizableObject.Get()))
		{
			UE_LOG(LogMutable, Display, TEXT("Skipping CO \"%s\" as it is not a root CO."), *CustomizableObjectName)
			continue;
		}
		
		// Body of the test ----------------------------------------------------------------------------------------------------------------------
		TestCustomizableObject(*TargetCustomizableObject, *TargetCompilationPlatform, InstancesToGenerate, bUseDiskCompilation);
		// ---------------------------------------------------------------------------------------------------------------------------------------
		
		// Try to collect the garbage
		{
			if (GIsInitialLoad)
			{
				UE_LOG(LogMutable, Warning, TEXT("GC will not run as GIsInitialLoad is currently set to true."));
			}
			
			CollectGarbage(RF_NoFlags, true);
		}

		UE_LOG(LogMutable, Display, TEXT("Validated %u/%u assets."), AssetIndex + 1, FoundAssetsCount);
	}

	UE_LOG(LogMutable, Display, TEXT("Mutable commandlet finished."));
	return 0;
}
