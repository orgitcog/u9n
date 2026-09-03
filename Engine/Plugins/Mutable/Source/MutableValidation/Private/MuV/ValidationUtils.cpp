// Copyright Epic Games, Inc. All Rights Reserved.

#include "ValidationUtils.h"

#include "CustomizableObjectCompilationUtility.h"
#include "CustomizableObjectInstanceUpdateUtility.h"
#include "RHIGlobals.h"
#include "ScopedLogSection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commandlets/Commandlet.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/CoreMisc.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCOE/CustomizableObjectBenchmarkingUtils.h"
#include "MuR/Model.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectPtr.h"

void PrepareAssetRegistry()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	UE_LOG(LogMutable,Display,TEXT("Searching all assets (this will take some time)..."));
	
	const double AssetRegistrySearchStartSeconds = FPlatformTime::Seconds();
	AssetRegistryModule.Get().SearchAllAssets(true /* bSynchronousSearch */);
	const double AssetRegistrySearchEndSeconds = FPlatformTime::Seconds() - AssetRegistrySearchStartSeconds;
	UE_LOG(LogMutable, Log, TEXT("(double) asset_registry_search_time_s : %f "), AssetRegistrySearchEndSeconds);

	UE_LOG(LogMutable,Display,TEXT("Asset searching completed in \"%f\" seconds!"), AssetRegistrySearchEndSeconds);
}


void LogGlobalSettings()
{
	// Mutable Settings
	const int32 WorkingMemoryKB = UCustomizableObjectSystem::GetInstanceChecked()->GetWorkingMemory() ;
	UE_LOG(LogMutable,Log, TEXT("(int) working_memory_bytes : %d"), WorkingMemoryKB*1024)
	UE_LOG(LogMutable, Display, TEXT("The mutable updates will use as working memory the value of %d KB"), WorkingMemoryKB)
	
	// Expand this when adding new controls from the .xml file
	
	// RHI Settings
	UE_LOG(LogMutable, Log, TEXT("(string) rhi_adapter_name : %s"), *GRHIAdapterName )
}


void Wait(const double ToWaitSeconds)
{
	check (ToWaitSeconds > 0);
	
	const double EndSeconds = FPlatformTime::Seconds() + ToWaitSeconds;
	UE_LOG(LogMutable,Display,TEXT("Holding test execution for %f seconds."),ToWaitSeconds);
	while (FPlatformTime::Seconds() < EndSeconds)
	{
		// Tick the engine
		CommandletHelpers::TickEngine();

		// Stop if exit was requested
		if (IsEngineExitRequested())
		{
			break;
		}
	}

	UE_LOG(LogMutable,Display,TEXT("Resuming test execution."));
}


FCompilationOptions GetCompilationOptionsForBenchmarking (const UCustomizableObject& ReferenceCustomizableObject)
{
	// Override some configurations that may have been changed by the user
	FCompilationOptions CISCompilationOptions = ReferenceCustomizableObject.GetPrivate()->GetCompileOptions();
	CISCompilationOptions.OptimizationLevel = CustomizableObjectBenchmarkingUtils::GetOptimizationLevelForBenchmarking();
	CISCompilationOptions.TextureCompression = ECustomizableObjectTextureCompression::Fast;	// Does not affect instance update speed but does compilation
	return CISCompilationOptions;
}


TArray<FAssetData> FindAllAssetsAtPath(FName SearchPath, const UClass* TargetObjectClass)
{
	TArray<FAssetData> FoundAssetData;
	
	if (TargetObjectClass)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(TargetObjectClass->GetClassPathName());
		Filter.PackagePaths.Add( SearchPath);
		Filter.bRecursivePaths = true;
	
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);

		// Ensure the AR module is ready to search for stuff
		AssetRegistryModule.Get().SearchAllAssets(true);
	
		UE_LOG(LogMutable, Display, TEXT("Searching for all %s objects to test at path : %s ."), *TargetObjectClass->GetName(), *SearchPath.ToString());
		AssetRegistryModule.Get().GetAssets(Filter, FoundAssetData);
		UE_LOG(LogMutable, Display, TEXT("Search of %s objects completed. Found %i objects."), *TargetObjectClass->GetName(), FoundAssetData.Num());
	}
	else
	{
		UE_LOG(LogMutable, Error, TEXT("No objects can be retrieved using a null class."));
	}

	return FoundAssetData;
}


bool GetDiskCompilationArg(const FString& Params)
{
	bool bUseDiskCompilation = false;
	if (! FParse::Bool(*Params,TEXT("UseDiskCompilation="),bUseDiskCompilation))
	{
		UE_LOG(LogMutable, Display, TEXT("Disk compilation setting for the compilation of the CO not specified. Using default value : %hhd"), bUseDiskCompilation) ;
	}
	return bUseDiskCompilation;
}


uint32 GetTargetAmountOfInstances(const FString& Params)
{
	// Get the amount of instances to generate if parameter was provided (it will get multiplied by the amount of states later so this is a minimun value)
	uint32 InstancesToGenerate = 16;
	if (!FParse::Value(*Params, TEXT("InstanceGenerationCount="),InstancesToGenerate))
	{
		UE_LOG(LogMutable, Display, TEXT("Instance generation count not specified. Using default value : %u"), InstancesToGenerate);
	}
	return InstancesToGenerate;
}


ITargetPlatform* GetCompilationPlatform(const FString& Params)
{
	// Get the package name of the CO to test
	FString TargetPlatformName = "";
	if (!FParse::Value(*Params, TEXT("CompilationPlatformName="), TargetPlatformName))
	{
		UE_LOG(LogMutable, Error, TEXT("Failed to parse the target compilation platform. Have you even provided the argument?"))
		return nullptr;
	}

	// Set the target platform in the context. For now it is the current platform.
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);
	
	ITargetPlatform* TargetCompilationPlatform = nullptr;
	const TArray<ITargetPlatform*> TPMPlatforms = TPM->GetTargetPlatforms();
	for (ITargetPlatform* Platform : TPMPlatforms)
	{
		FString PlatformName = Platform->PlatformName();
		if (PlatformName.Compare(TargetPlatformName) == 0)
		{
			// We have found the platform provided
			TargetCompilationPlatform = Platform;
			break;
		}
	}
	
	if (!TargetCompilationPlatform)
	{
		UE_LOG(LogMutable, Error, TEXT("Unable to relate the provided platform name (%s) with the available platforms in this machine."), *TargetPlatformName);
	}

	return TargetCompilationPlatform;
}


bool TestCustomizableObject(UCustomizableObject& InTargetCustomizableObject, const ITargetPlatform& TargetCompilationPlatform,
	const uint32 InstancesToGenerate, const bool bUseDiskCompilation)
{
	const FScopedLogSection ObjectSection (EMutableLogSection::Object, FName( InTargetCustomizableObject.GetPathName()));
	
	// Keep a strong object pointer pointing at the CO to prevent it from being GCd during the test
	const TStrongObjectPtr<UCustomizableObject> TargetCO = TStrongObjectPtr{&InTargetCustomizableObject};
	
	// Compile the Customizable Object ------------------------------------------------------------------------------ //
	bool bWasCoCompilationSuccessful = false;
	{
		LLM_SCOPE_BYNAME(TEXT("MutableValidation/Compile"));
		// Override some configurations that may have been changed by the user
		FCompilationOptions CompilationOptions = GetCompilationOptionsForBenchmarking(InTargetCustomizableObject);
		
		// Set the target compilation platform based on what the caller wants
		CompilationOptions.TargetPlatform = &TargetCompilationPlatform;
		// Disk cache usage for compilation operation
		CompilationOptions.bUseDiskCompilation = bUseDiskCompilation;
		
		TSharedRef<FCustomizableObjectCompilationUtility> CompilationUtility = MakeShared<FCustomizableObjectCompilationUtility>();
		bWasCoCompilationSuccessful = CompilationUtility->CompileCustomizableObject(InTargetCustomizableObject, true, &CompilationOptions);
	}
	// -------------------------------------------------------------------------------------------------------------- //

	if (!bWasCoCompilationSuccessful)
	{
		UE_LOG(LogMutable, Error, TEXT("The compilation of the Customizable object was not successful : No instances will be generated."));
		return false;		// Validation failed
	}
	
	// GHet the total size of the streaming data of the model ---------------------------------------------- //
	{
		const TSharedPtr<const mu::FModel> MutableModel = InTargetCustomizableObject.GetPrivate()->GetModel();
		check (MutableModel);

		// Roms ---------------------- //
		{
			const int32 RomCount =  MutableModel->GetRomCount();
			int64 TotalRomSizeBytes = 0;
			for (int32 RomIndex = 0; RomIndex < RomCount; RomIndex++)
			{
				const uint32 RomByteSize = MutableModel->GetRomSize(RomIndex);
				TotalRomSizeBytes += RomByteSize;
			}

			// Print MTU parseable logs
			UE_LOG(LogMutable, Log, TEXT("(int) model_rom_count : %d "), RomCount);
			UE_LOG(LogMutable, Log, TEXT("(int) model_roms_size : %lld "), TotalRomSizeBytes);
		}

		// CO embedded data size ------ //
		{
			TArray<uint8> EmbeddedDataBytes{};
			FMemoryWriter SerializationTarget{EmbeddedDataBytes, false};
		
			InTargetCustomizableObject.GetPrivate()->SaveEmbeddedData(SerializationTarget);
			const int64 COEmbeddedDataSizeBytes = EmbeddedDataBytes.Num();
		
			UE_LOG(LogMutable, Log, TEXT("(int) co_embedded_data_bytes : %lld "), COEmbeddedDataSizeBytes);
		}
	}
	
	// Skip instances updating if no instances should be updated 
	if (InstancesToGenerate <= 0)
	{
		UE_LOG(LogMutable, Display, TEXT("Instances to generate are 0 : No instances will be generated."));
		return true;	// No instances are targeted for generation, this will be taken as compilation only test.
	}

	// Do not generate instances if the selected platform is not the running platform
	if (&TargetCompilationPlatform != GetTargetPlatformManagerRef().GetRunningTargetPlatform())
	{
		UE_LOG(LogMutable, Display, TEXT("RunningPlatform != UserProvidedCompilationPlatform : No instances will be generated."));
		return true;
	}

	// At this point we know the compilation has been successful. Generate a deterministically random set of instances now.
	
	// Generate target random instances to be tested ------------------------------------------------------------ //
	bool bWasInstancesCreationSuccessful = true;
	TSpscQueue<TStrongObjectPtr<UCustomizableObjectInstance>> InstancesToProcess;
	uint32 GeneratedInstances = 0;
	{
		LLM_SCOPE_BYNAME(TEXT("MutableValidation/GenerateInstances"));
		
		// Create a set of instances so we can later test them out
		bWasInstancesCreationSuccessful = CustomizableObjectBenchmarkingUtils::GenerateDeterministicSetOfInstances(InTargetCustomizableObject, InstancesToGenerate, InstancesToProcess, GeneratedInstances);
	}
	// ---------------------------------------------------------------------------------------------------------- //

	UE_LOG(LogMutable, Log, TEXT("(int) generated_instances_count : %u "), GeneratedInstances);
	
	// Update the instances generated --------------------------------------------------------------------------- //
	UE_LOG(LogMutable, Display, TEXT("Updating generated instances..."));
	bool bInstanceFailedUpdate = false;
	const double InstancesUpdateStartSeconds = FPlatformTime::Seconds();
	{
		LLM_SCOPE_BYNAME(TEXT("MutableValidation/Update"));
		
		TSharedRef<FCustomizableObjectInstanceUpdateUtility> InstanceUpdatingUtility = MakeShared<FCustomizableObjectInstanceUpdateUtility>();

		TStrongObjectPtr<UCustomizableObjectInstance> InstanceToUpdate;
		while (InstancesToProcess.Dequeue(InstanceToUpdate))
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
			check(InstanceToUpdate);

			if (!InstanceUpdatingUtility->UpdateInstance(*InstanceToUpdate))
			{
				bInstanceFailedUpdate = true;
			}
		}
	}
	const double InstancesUpdateEndSeconds = FPlatformTime::Seconds();
	
	// Notify and log time required by the instances to get updated
	const double CombinedInstanceUpdateSeconds = InstancesUpdateEndSeconds - InstancesUpdateStartSeconds;
	UE_LOG(LogMutable, Log, TEXT("(double) combined_update_time_ms : %f "), CombinedInstanceUpdateSeconds * 1000);

	check(GeneratedInstances > 0);
	const double AverageInstanceUpdateSeconds = CombinedInstanceUpdateSeconds / GeneratedInstances;
	UE_LOG(LogMutable, Log, TEXT("(double) avg_update_time_ms : %f "), AverageInstanceUpdateSeconds * 1000);

	UE_LOG(LogMutable, Display, TEXT("Generation of Customizable object instances took %f seconds (%f seconds avg)."), CombinedInstanceUpdateSeconds, AverageInstanceUpdateSeconds);
	// ---------------------------------------------------------------------------------------------------------- //

	// Compute instance update result
	const bool bInstancesTestedSuccessfully = !bInstanceFailedUpdate && bWasInstancesCreationSuccessful;
	if (bInstancesTestedSuccessfully)
    {
        UE_LOG(LogMutable, Display, TEXT("Generation of Customizable object instances was successful."));
    }
    else
    {
        UE_LOG(LogMutable, Error, TEXT("The generation of Customizable object instances was not successful."));
    }
	
	return bInstancesTestedSuccessfully;
}


