// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionHLODsBuilder.h"

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Algo/ForEach.h"
#include "UObject/SavePackage.h"

#include "ActorFolder.h"
#include "EditorWorldUtils.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreamingGCHelper.h"
#include "EngineUtils.h"
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DerivedDataCacheInterface.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/ExternalDataLayerEngineSubsystem.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODProviderInterface.h"
#include "WorldPartition/HLOD/StandaloneHLODSubsystem.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"

#define LOCTEXT_NAMESPACE "WorldPartitionHLODsBuilder"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionHLODsBuilder, All, All);

static const FString DistributedBuildWorkingDirName = TEXT("HLODTemp");
static const FString DistributedBuildManifestName = TEXT("HLODBuildManifest.ini");
static const FString BuildProductsFileName = TEXT("BuildProducts.txt");

FString GetHLODBuilderFolderName(uint32 BuilderIndex) { return FString::Printf(TEXT("HLODBuilder%d"), BuilderIndex); }
FString GetToSubmitFolderName() { return TEXT("ToSubmit"); }

UWorldPartitionHLODsBuilder::UWorldPartitionHLODsBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BuilderIdx(INDEX_NONE)
	, BuilderCount(INDEX_NONE)
	, bBuildingStandaloneHLOD(false)
{
	if (!IsTemplate())
	{
		BuildOptions = HasParam("SetupHLODs") ? EHLODBuildStep::HLOD_Setup : EHLODBuildStep::None;
		BuildOptions |= HasParam("BuildHLODs") ? EHLODBuildStep::HLOD_Build : EHLODBuildStep::None;
		BuildOptions |= HasParam("RebuildHLODs") ? EHLODBuildStep::HLOD_Build : EHLODBuildStep::None;
		BuildOptions |= HasParam("DeleteHLODs") ? EHLODBuildStep::HLOD_Delete : EHLODBuildStep::None;
		BuildOptions |= HasParam("FinalizeHLODs") ? EHLODBuildStep::HLOD_Finalize : EHLODBuildStep::None;
		BuildOptions |= HasParam("DumpStats") ? EHLODBuildStep::HLOD_Stats : EHLODBuildStep::None;

		bResumeBuild = GetParamValue("ResumeBuild=", ResumeBuildIndex);

		bDistributedBuild = HasParam("DistributedBuild");
		bForceBuild = HasParam("RebuildHLODs");
		bReportOnly = HasParam("ReportOnly");

		GetParamValue("BuildManifest=", BuildManifest);
		GetParamValue("BuilderIdx=", BuilderIdx);
		GetParamValue("BuilderCount=", BuilderCount);
		GetParamValue("BuildHLODLayer=", HLODLayerToBuild);
		GetParamValue("BuildSingleHLOD=", HLODActorToBuild);

		if (!HLODActorToBuild.IsNone() || !HLODLayerToBuild.IsNone())
		{
			BuildOptions |= EHLODBuildStep::HLOD_Build;
			bForceBuild = bForceBuild || !HLODActorToBuild.IsNone();
		}

		// Default behavior without any option is to setup + build
		if (BuildOptions == EHLODBuildStep::None)
		{
			BuildOptions = EHLODBuildStep::HLOD_Setup | EHLODBuildStep::HLOD_Build;
		}

		UExternalDataLayerEngineSubsystem::Get().OnExternalDataLayerOverrideInjection.AddUObject(this, &UWorldPartitionHLODsBuilder::AllowExternalDataLayerInjection);
	}
}

void UWorldPartitionHLODsBuilder::AllowExternalDataLayerInjection(const UWorld* InWorld, const UExternalDataLayerAsset* InExternalDataLayerAsset, bool& bOutAllowInjection)
{
	// Always allow EDL injections during HLOD builds
	bOutAllowInjection = true;
}

bool UWorldPartitionHLODsBuilder::RequiresCommandletRendering() const
{
	// Commandlet requires rendering only for building HLODs
	// Building will occur either if -BuildHLODs is provided or no explicit step arguments are provided
	return EnumHasAnyFlags(BuildOptions, EHLODBuildStep::HLOD_Build);
}

bool UWorldPartitionHLODsBuilder::ShouldRunStep(const EHLODBuildStep BuildStep) const
{
	return (BuildOptions & BuildStep) == BuildStep;
}

bool UWorldPartitionHLODsBuilder::ValidateParams() const
{
	if (ShouldRunStep(EHLODBuildStep::HLOD_Setup) && IsUsingBuildManifest())
	{
		if (BuilderCount <= 0)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Missing parameter -BuilderCount=N (where N > 0), exiting..."));
			return false;
		}
	}

	if (ShouldRunStep(EHLODBuildStep::HLOD_Build) && IsUsingBuildManifest())
	{
		if (BuilderIdx < 0)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Missing parameter -BuilderIdx=i, exiting..."));
			return false;
		}

		if (!FPaths::FileExists(BuildManifest))
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Build manifest file \"%s\" not found, exiting..."), *BuildManifest);
			return false;
		}

		FString CurrentEngineVersion = FEngineVersion::Current().ToString();
		FString ManifestEngineVersion = TEXT("unknown");

		FConfigFile ConfigFile;
		ConfigFile.Read(BuildManifest);
		ConfigFile.GetString(TEXT("General"), TEXT("EngineVersion"), ManifestEngineVersion);
		if (ManifestEngineVersion != CurrentEngineVersion)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Build manifest engine version doesn't match current engine version (%s vs %s), exiting..."), *ManifestEngineVersion, *CurrentEngineVersion);
			return false;
		}
	}

	return true;
}

FString GetDistributedBuildWorkingDir(UWorld* InWorld)
{
	uint32 WorldPackageHash = GetTypeHash(InWorld->GetPackage()->GetFullName());
	return FString::Printf(TEXT("%s/%s/%08x"), *FPaths::RootDir(), *DistributedBuildWorkingDirName, WorldPackageHash);
}

bool UWorldPartitionHLODsBuilder::ShouldProcessWorld(UWorld* InWorld) const
{
	bool bShouldProcessWorld = true;

	// When building HLODs in a distributed build, if there is no config section for the given builder index
	// it means that the builder can skip processing this world altogether.
	if (bDistributedBuild && ShouldRunStep(EHLODBuildStep::HLOD_Build))
	{
		const FString BuildManifestDirName = GetDistributedBuildWorkingDir(InWorld);
		const FString BuildManifestFileName = BuildManifestDirName / DistributedBuildManifestName;

		FConfigFile ConfigFile;
		ConfigFile.Read(BuildManifestFileName);

		FString SectionName = GetHLODBuilderFolderName(BuilderIdx);

		const FConfigSection* ConfigSection = ConfigFile.FindSection(SectionName);
		if (!ConfigSection || ConfigSection->IsEmpty())
		{
			bShouldProcessWorld = false;
		}
	}

	return bShouldProcessWorld;
}

bool UWorldPartitionHLODsBuilder::ShouldProcessAdditionalWorlds(UWorld* InWorld, TArray<FString>& OutPackageNames) const
{
	// If during Build step and if building standalone HLOD, we want to run the builder on standalone HLOD levels,
	// so that the HLOD Actors, which were created in those levels can be built
	if (ShouldRunStep(EHLODBuildStep::HLOD_Build) == false)
	{
		return false;
	}

	UWorldPartition* WP = InWorld->GetWorldPartition();
	if (WP && WP->HasStandaloneHLOD())
	{
		if (const UWorldPartitionRuntimeHashSet* WorldPartitionRuntimeHashSet = Cast<UWorldPartitionRuntimeHashSet>(WP->RuntimeHash))
		{
			FString FolderPath, PackagePrefix;
			UWorldPartitionStandaloneHLODSubsystem::GetStandaloneHLODFolderPathAndPackagePrefix(InWorld->GetPackage()->GetName(), FolderPath, PackagePrefix);

			WorldPartitionRuntimeHashSet->ForEachHLODLayer([&FolderPath, &PackagePrefix, &OutPackageNames](FName RuntimePartitionName, FName HLODSetupName, int32 HLODSetupIndex)
			{
				const FString HLODLevelPackageName = FString::Printf(TEXT("%s/%s%d"), *FolderPath, *PackagePrefix, HLODSetupIndex);
				OutPackageNames.Add(HLODLevelPackageName);

				return true;
			});

			return true;
		}
	}
	return false;
}

bool UWorldPartitionHLODsBuilder::PreWorldInitialization(UWorld* InWorld, FPackageSourceControlHelper& PackageHelper)
{
	if (bDistributedBuild)
	{
		DistributedBuildWorkingDir = GetDistributedBuildWorkingDir(InWorld);
		DistributedBuildManifest = DistributedBuildWorkingDir / DistributedBuildManifestName;

		if (!BuildManifest.IsEmpty())
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Warning, TEXT("Ignoring parameter -BuildManifest when a distributed build is performed"));
		}

		BuildManifest = DistributedBuildManifest;
	}

	if (!ValidateParams())
	{
		return false;
	}

	bool bRet = true;

	// When running a distributed build, retrieve relevant build products from the previous steps
	if (IsDistributedBuild() && (ShouldRunStep(EHLODBuildStep::HLOD_Build) || ShouldRunStep(EHLODBuildStep::HLOD_Finalize)))
	{
		FString WorkingDirFolder = ShouldRunStep(EHLODBuildStep::HLOD_Build) ? GetHLODBuilderFolderName(BuilderIdx) : GetToSubmitFolderName();
		bRet = CopyFilesFromWorkingDir(WorkingDirFolder);
	}

	return bRet;
}

bool UWorldPartitionHLODsBuilder::RunInternal(UWorld* InWorld, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	World = InWorld;
	WorldPartition = World->GetWorldPartition();

	if (WorldPartition != nullptr)
	{
		bBuildingStandaloneHLOD = WorldPartition->HasStandaloneHLOD();
	}
	
	// Allows HLOD Streaming levels to be GCed properly
	FLevelStreamingGCHelper::EnableForCommandlet();

	SourceControlHelper = new FSourceControlHelper(PackageHelper, ModifiedFiles);

	bool bRet = true;

	if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Setup))
	{
		bRet = SetupHLODActors();
	}

	if (!bReportOnly)
	{
		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Build))
		{
			bRet = BuildHLODActors();
		}

		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Delete))
		{
			bRet = DeleteHLODActors();
		}

		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Finalize))
		{
			bRet = SubmitHLODActors();
		}

		if (bRet && ShouldRunStep(EHLODBuildStep::HLOD_Stats))
		{
			bRet = DumpStats();
		}
	}

	WorldPartition = nullptr;
	delete SourceControlHelper;

	return bRet;
}

bool UWorldPartitionHLODsBuilder::SetupHLODActors()
{
	// No setup needed for non partitioned worlds and standalone HLOD worlds
	if (WorldPartition && !WorldPartition->IsStandaloneHLODWorld())
	{
		auto ActorFolderAddedDelegateHandle = GEngine->OnActorFolderAdded().AddLambda([this](UActorFolder* InActorFolder)
		{
			UPackage* ActorFolderPackage = InActorFolder->GetPackage();
			const bool bIsTempPackage = FPackageName::IsTempPackage(ActorFolderPackage->GetName());
			if (!bIsTempPackage && InActorFolder->IsInitiallyExpanded())
			{
				// We don't want the HLOD folders to be expanded by default
				InActorFolder->SetIsInitiallyExpanded(false);
				SourceControlHelper->Save(InActorFolder->GetPackage());
			}
		});
	
		ON_SCOPE_EXIT
		{
			GEngine->OnActorFolderAdded().Remove(ActorFolderAddedDelegateHandle);
		};

		UWorldPartition::FSetupHLODActorsParams SetupHLODActorsParams = UWorldPartition::FSetupHLODActorsParams()
			.SetSourceControlHelper(SourceControlHelper)
			.SetReportOnly(bReportOnly);

		WorldPartition->SetupHLODActors(SetupHLODActorsParams);

		if (bBuildingStandaloneHLOD)
		{
			// Retrieve additional Standalone HLOD levels that have to be processed
			AdditionalWorldPartitionsForStandaloneHLOD = MoveTemp(SetupHLODActorsParams.OutAdditionalWorldPartitionsForStandaloneHLOD);
			if (IsDistributedBuild())
			{
				// Generate working dirs for additional Standalone HLOD levels
				StandaloneHLODWorkingDirs.SetNum(AdditionalWorldPartitionsForStandaloneHLOD.Num());
				for (int32 Index = 0; Index < StandaloneHLODWorkingDirs.Num(); Index++)
				{
					StandaloneHLODWorkingDirs[Index] = GetDistributedBuildWorkingDir(AdditionalWorldPartitionsForStandaloneHLOD[Index]->GetWorld());
				}
			}

			// Refresh Asset Registry to include Standalone HLOD levels that were created during SetupHLODActors
			FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
			DirectoryWatcherModule.Get()->Tick(-1.0f);

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			for (const TObjectPtr<UWorldPartition>& AdditionalWorldPartition : AdditionalWorldPartitionsForStandaloneHLOD)
			{
				FString WorldName = AdditionalWorldPartition->GetWorld()->GetPackage()->GetName();
				TArray<FString> ExternalObjectsPaths = ULevel::GetExternalObjectsPaths(WorldName);

				AssetRegistry.ScanModifiedAssetFiles({WorldName});
				AssetRegistry.ScanPathsSynchronous(ExternalObjectsPaths, true);
			}
		}

		// When performing a distributed build, ensure our work folder is empty
		if (IsDistributedBuild())
		{
			IFileManager::Get().DeleteDirectory(*DistributedBuildWorkingDir, false, true);
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### World HLOD actors ####"));

		int32 NumActors = 0;
		TFunction<void(UWorldPartition*)> ListHLODActors = [&NumActors](UWorldPartition* WorldPartitionToProcess)
		{
			for (FActorDescContainerInstanceCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartitionToProcess); HLODIterator; ++HLODIterator)
			{
				FWorldPartitionActorDescInstance* HLODActorDescInstance = *HLODIterator;
				FString PackageName = HLODActorDescInstance->GetActorPackage().ToString();

				UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("    [%d] %s"), NumActors, *PackageName);

				NumActors++;
			}
		};

		ListHLODActors(WorldPartition);

		if (bBuildingStandaloneHLOD)
		{
			for (const TObjectPtr<UWorldPartition>& AdditionalWorldPartition : AdditionalWorldPartitionsForStandaloneHLOD)
			{
				ListHLODActors(AdditionalWorldPartition);
			}
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### World contains %d HLOD actors ####"), NumActors);
	}

	if (IsUsingBuildManifest())
	{
		// With Standalone HLOD levels we might be generating work for multiple builders across multiple worlds,
		// keep track of Builder Index and World Index for each file
		TMap<FString, TPair<int32, int32>> FilesToBuilderAndWorldIndexMap;
		bool bGenerated = GenerateBuildManifest(FilesToBuilderAndWorldIndexMap);
		if (!bGenerated)
		{
			return false;
		}

		// When performing a distributed build, move modified files to the temporary working dir, to be submitted later in the last "submit" step
		if (IsDistributedBuild())
		{
			// Ensure we don't hold on to packages of always loaded actors
			// When running distributed builds, we wanna leave the machine clean, so added files are deleted, check'd out files are reverted
			// and deleted files are restored.
			bool bCollectGarbage = false;
			if (WorldPartition)
			{
				WorldPartition->Uninitialize();
				bCollectGarbage = true;
			}
			
			// Clean up Standalone HLOD levels
			for (const TObjectPtr<UWorldPartition>& AdditionalWorldPartition : AdditionalWorldPartitionsForStandaloneHLOD)
			{
				if (AdditionalWorldPartition)
				{
					AdditionalWorldPartition->Uninitialize();
					bCollectGarbage = true;
				}
			}
			AdditionalWorldPartitionsForStandaloneHLOD.Empty();
			
			if (bCollectGarbage)
			{
				FWorldPartitionHelpers::DoCollectGarbage();
			}

			TArray<TArray<FBuilderModifiedFiles>> BuildersFilesPerWorld;
			BuildersFilesPerWorld.SetNum(bBuildingStandaloneHLOD ? StandaloneHLODWorkingDirs.Num() : 1);
			for (TArray<FBuilderModifiedFiles>& BuildersFiles : BuildersFilesPerWorld)
			{
				BuildersFiles.SetNum(BuilderCount);
			}

			for (int32 i = 0; i < FBuilderModifiedFiles::EFileOperation::NumFileOperations; i++)
			{
				FBuilderModifiedFiles::EFileOperation FileOp = (FBuilderModifiedFiles::EFileOperation)i;
				for (const FString& ModifiedFile : ModifiedFiles.Get(FileOp))
				{
					// Key - Builder Index
					// Value - World Index
					TPair<int32, int32>* Idx = FilesToBuilderAndWorldIndexMap.Find(ModifiedFile);
					if (Idx)
					{
						BuildersFilesPerWorld[Idx->Value][Idx->Key].Add(FileOp, ModifiedFile);
					}
					else
					{
						// Add general files to the last builder, first world
						BuildersFilesPerWorld[0].Last().Add(FileOp, ModifiedFile);
					}
				}
			}

			// Gather build product to ensure intermediary files are copied between the different HLOD generation steps
			TArray<FString> BuildProducts;

			// Copy files that will be handled by the different builders
			for (int32 WorldIndex = 0; WorldIndex < BuildersFilesPerWorld.Num(); WorldIndex++)
			{
				FString WorkingDir;
				if (bBuildingStandaloneHLOD)
				{
					WorkingDir = StandaloneHLODWorkingDirs[WorldIndex];
				}
				else
				{
					WorkingDir = DistributedBuildWorkingDir;
				}

				for (int32 Idx = 0; Idx < BuilderCount; Idx++)
				{
					if (!CopyFilesToWorkingDir(GetHLODBuilderFolderName(Idx), BuildersFilesPerWorld[WorldIndex][Idx], WorkingDir, BuildProducts))
					{
						return false;
					}
				}
			}

			// The build manifest must also be included as a build product to be available in the next steps
			BuildProducts.Add(BuildManifest);

			// Write build products to a file
			if (!AddBuildProducts(BuildProducts))
			{
				return false;
			}
		}
	}

	// Clean up Standalone HLOD levels if not cleaned up before
	for (const TObjectPtr<UWorldPartition>& AdditionalWorldPartition : AdditionalWorldPartitionsForStandaloneHLOD)
	{
		if (AdditionalWorldPartition)
		{
			AdditionalWorldPartition->Uninitialize();
		}
	}
	AdditionalWorldPartitionsForStandaloneHLOD.Empty();
	FWorldPartitionHelpers::DoCollectGarbage();

	return true;
}

bool UWorldPartitionHLODsBuilder::BuildHLODActors()
{
	auto SaveHLODActor = [this](AWorldPartitionHLOD* HLODActor)
	{
		UPackage* ActorPackage = HLODActor->GetPackage();
		if (ActorPackage->IsDirty())
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("HLOD actor %s was modified, saving..."), *HLODActor->GetActorLabel());

			bool bSaved = SourceControlHelper->Save(ActorPackage);
			if (!bSaved)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to save %s, exiting..."), *USourceControlHelpers::PackageFilename(ActorPackage));
				return false;
			}
		}

		return true;
	};

	if (WorldPartition)
	{
		TArray<FGuid> HLODActorsToBuild;
		if (!GetHLODActorsToBuild(HLODActorsToBuild))
		{
			return false;
		}

		FHLODWorkload WorkloadToValidate;
		WorkloadToValidate.PerWorldHLODWorkloads.Add(HLODActorsToBuild);
		if (!ValidateWorkload(WorkloadToValidate, /*bShouldConsiderExternalHLODActors=*/false))
		{
			return false;
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Building %d HLOD actors ####"), HLODActorsToBuild.Num());
		if (bResumeBuild)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Resuming build at %d ####"), ResumeBuildIndex);
		}

		for (int32 CurrentActor = ResumeBuildIndex; CurrentActor < HLODActorsToBuild.Num(); ++CurrentActor)
		{
			TRACE_BOOKMARK(TEXT("BuildHLOD Start - %d"), CurrentActor);

			{
				const FGuid& HLODActorGuid = HLODActorsToBuild[CurrentActor];

				FWorldPartitionReference ActorRef(WorldPartition, HLODActorGuid);

				AWorldPartitionHLOD* HLODActor = CastChecked<AWorldPartitionHLOD>(ActorRef.GetActor());

				UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("[%d / %d] %s %s..."), CurrentActor + 1, HLODActorsToBuild.Num(), *LOCTEXT("BuildingHLODActor", "Building HLOD actor").ToString(), *HLODActor->GetActorLabel());

				// Simulate an engine tick to make sure engine & render resources that are queued for deletion are processed.
				FWorldPartitionHelpers::FakeEngineTick(World);

				HLODActor->BuildHLOD(bForceBuild);

				bool bSaved = SaveHLODActor(HLODActor);
				if (!bSaved)
				{
					return false;
				}
			}

			TRACE_BOOKMARK(TEXT("BuildHLOD End - %d"), CurrentActor);

			if (FWorldPartitionHelpers::ShouldCollectGarbage())
			{
				FWorldPartitionHelpers::DoCollectGarbage();
			}
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Built %d HLOD actors ####"), HLODActorsToBuild.Num());
	}
	else
	{
		IWorldPartitionHLODProvider::FBuildHLODActorParams BuildHLODActorParams;
		BuildHLODActorParams.bForceRebuild = bForceBuild;
		BuildHLODActorParams.OnPackageModified.BindLambda([this](UPackage* ModifiedPackage)
		{
			bool bSaved = SourceControlHelper->Save(ModifiedPackage);
			if (!bSaved)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to save %s, exiting..."), *USourceControlHelpers::PackageFilename(ModifiedPackage));
			}

			return bSaved;
		});

		TArray<IWorldPartitionHLODProvider*> HLODProviders;

		// Gather all HLOD providers
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			if (IWorldPartitionHLODProvider* HLODProvider = Cast<IWorldPartitionHLODProvider>(*ActorIt))
			{
				HLODProviders.Add(HLODProvider);
			}
		}

		// Process them one by one
		for (IWorldPartitionHLODProvider* HLODProvider : HLODProviders)
		{
			bool bBuildResult = HLODProvider->BuildHLODActor(BuildHLODActorParams);
			if (!bBuildResult)
			{
				return false;
			}
		}

		UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Built %d HLOD actor ####"), HLODProviders.Num());
	}


	// Move modified files to the temporary working dir, to be submitted later in the final "submit" pass, from a single machine.
	if (IsDistributedBuild())
	{
		// Ensure we don't hold on to packages of always loaded actors
		// When running distributed builds, we wanna leave the machine clean, so added files are deleted, check'd out files are reverted
		// and deleted files are restored.
		if (WorldPartition)
		{
			WorldPartition->Uninitialize();
			FWorldPartitionHelpers::DoCollectGarbage();
		}

		TArray<FString> BuildProducts;

		if (!CopyFilesToWorkingDir("ToSubmit", ModifiedFiles, DistributedBuildWorkingDir, BuildProducts))
		{
			return false;
		}

		// Write build products to a file
		if (!AddBuildProducts(BuildProducts))
		{
			return false;
		}
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::DeleteHLODActors()
{
	UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Deleting HLOD actors ####"));

	TArray<UClass*> HLODActorClasses =
	{
		AWorldPartitionHLOD::StaticClass(),
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.SpatialHashRuntimeGridInfo"))
	};

	TArray<FString> PackagesToDelete;

	if (bBuildingStandaloneHLOD)
	{
		// Find all Stadalone HLOD levels and delete them and all their external actors
		UWorld* SourceWorld = WorldPartition->GetWorld();
		FString FolderPath, PackagePrefix;
		UWorldPartitionStandaloneHLODSubsystem::GetStandaloneHLODFolderPathAndPackagePrefix(SourceWorld->GetPackage()->GetName(), FolderPath, PackagePrefix);

		TArray<FString> Packages;
		FPackageName::FindPackagesInDirectory(Packages, FolderPath);

		for (const FString& Package : Packages)
		{
			if (!Package.Contains(PackagePrefix))
			{
				continue;
			}

			FString PackageName = FPackageName::FilenameToLongPackageName(Package);
			PackagesToDelete.Add(PackageName);

			const TArray<FString> ExternalObjectsPaths = ULevel::GetExternalObjectsPaths(PackageName);
			for (const FString& ExternaObjectsPath : ExternalObjectsPaths)
			{
				FString ExternalObjectsDirectoryPath = FPackageName::LongPackageNameToFilename(ExternaObjectsPath);
				if (IFileManager::Get().DirectoryExists(*ExternalObjectsDirectoryPath))
				{
					const bool bSuccess = IFileManager::Get().IterateDirectoryRecursively(*ExternalObjectsDirectoryPath, [&PackagesToDelete](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
					{
						if (!bIsDirectory)
						{
							PackagesToDelete.Add(FilenameOrDirectory);
						}

						return true;
					});

					if (!bSuccess)
					{
						UE_LOG(LogWorldPartitionHLODsBuilder, Log, TEXT("Failed to iterate external package folder: %s"), *ExternalObjectsDirectoryPath);
					}
				}
			}
		}
	}

	for (FActorDescContainerInstanceCollection::TIterator<> Iterator(WorldPartition); Iterator; ++Iterator)
	{
		if (HLODActorClasses.FindByPredicate([ActorClass = Iterator->GetActorNativeClass()](const UClass* HLODClass) { return ActorClass->IsChildOf(HLODClass); }))
		{
			FString PackageName = Iterator->GetActorPackage().ToString();
			PackagesToDelete.Add(PackageName);
		}
	}

	// Ensure we don't hold on to packages of always loaded actors
	// When running distributed builds, we wanna leave the machine clean, so added files are deleted, checked out files are reverted
	// and deleted files are restored.
	WorldPartition->Uninitialize();
	FWorldPartitionHelpers::DoCollectGarbage();

	for (int32 PackageIndex = 0; PackageIndex < PackagesToDelete.Num(); ++PackageIndex)
	{
		const FString& PackageName = PackagesToDelete[PackageIndex];

		bool bDeleted = SourceControlHelper->Delete(PackageName);
		if (bDeleted)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("[%d / %d] Deleting %s..."), PackageIndex + 1, PackagesToDelete.Num(), *PackageName);
		}
		else
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to delete %s, exiting..."), *PackageName);
			return false;
		}
	}

	UE_LOG(LogWorldPartitionHLODsBuilder, Display, TEXT("#### Deleted %d HLOD actors ####"), PackagesToDelete.Num());

	return true;
}

bool UWorldPartitionHLODsBuilder::SubmitHLODActors()
{
	// Wait for pending async file writes before submitting
	UPackage::WaitForAsyncFileWrites();

	// Check in all modified files
	const FString ChangeDescription = FString::Printf(TEXT("Rebuilt HLODs for %s"), *World->GetPackage()->GetName());
	return OnFilesModified(ModifiedFiles.GetAllFiles(), ChangeDescription);
}

bool UWorldPartitionHLODsBuilder::DumpStats()
{
	const FString HLODStatsOutputFilename = FPaths::ProjectSavedDir() / TEXT("WorldPartition") / FString::Printf(TEXT("HLODStats-%08x.csv"), FPlatformProcess::GetCurrentProcessId());

	IWorldPartitionEditorModule::FWriteHLODStatsParams StatsParams;
	StatsParams.Filename = HLODStatsOutputFilename;
	StatsParams.World = World;
	StatsParams.StatsType = IWorldPartitionEditorModule::FWriteHLODStatsParams::EStatsType::Default;
	return IWorldPartitionEditorModule::Get().WriteHLODStats(StatsParams);
}

bool UWorldPartitionHLODsBuilder::GetHLODActorsToBuild(TArray<FGuid>& HLODActorsToBuild) const
{
	bool bRet = true;

	if (!BuildManifest.IsEmpty())
	{
		// Get HLOD actors to build from the BuildManifest file
		FConfigFile ConfigFile;
		ConfigFile.Read(BuildManifest);

		FString SectionName = GetHLODBuilderFolderName(BuilderIdx);

		const FConfigSection* ConfigSection = ConfigFile.FindSection(SectionName);
		if (ConfigSection)
		{
			TArray<FString> HLODActorGuidStrings;
			ConfigSection->MultiFind(TEXT("+HLODActorGuid"), HLODActorGuidStrings, /*bMaintainOrder=*/true);

			for (const FString& HLODActorGuidString : HLODActorGuidStrings)
			{
				FGuid HLODActorGuid;
				bRet = FGuid::Parse(HLODActorGuidString, HLODActorGuid);
				if (bRet)
				{
					HLODActorsToBuild.Add(HLODActorGuid);
				}
				else
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error parsing section [%s] in config file \"%s\""), *SectionName, *BuildManifest);
					break;
				}
			}
		}
		else
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Log, TEXT("No section [%s] found in config file \"%s\", assuming no HLOD needs to be built."), *SectionName, *BuildManifest);
			bRet = false;
		}
	}
	else
	{
		// When getting HLOD Workloads during Build step, we don't want to consider Standalone HLOD Actors in Standalone HLOD Levels,
		// as they'll be considered when the builder runs directly on those levels
		TArray<FHLODWorkload> HLODWorkloads = GetHLODWorkloads(1, /*bShouldConsiderExternalHLODActors=*/false);
		HLODActorsToBuild = MoveTemp(HLODWorkloads[0].PerWorldHLODWorkloads[0]);
	}

	return bRet;
}

TArray<UWorldPartitionHLODsBuilder::FHLODWorkload> UWorldPartitionHLODsBuilder::GetHLODWorkloads(int32 NumWorkloads, bool bShouldConsiderExternalHLODActors) const
{
	if (!WorldPartition)
	{
		return { FHLODWorkload() };
	}

	// Build a mapping of HLODActor to WorldPartition Index to be used when splitting actors into workloads
	// 0 - World Partition currently processed by the builder
	// 1 ... N - World Partitions from AdditionalWorldPartitionsForStandaloneHLOD array
	TMap<FGuid, uint32> HLODActorToWorldPartitionIndex;

	// Build a mapping of 1 HLOD[Level] -> N HLOD[Level - 1]
	TMap<FGuid, TArray<FGuid>>	HLODParenting;
	TFunction<void(UWorldPartition*, uint32)> ProcessWorldPartition = [this, &HLODParenting, &HLODActorToWorldPartitionIndex, bShouldConsiderExternalHLODActors](UWorldPartition* WorldPartitionToProcess, uint32 WorldPartitionIndex)
	{
		for (FActorDescContainerInstanceCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartitionToProcess); HLODIterator; ++HLODIterator)
		{
			const FHLODActorDesc& HLODActorDesc = *(FHLODActorDesc*)HLODIterator->GetActorDesc();
			// Filter by HLOD actor
			if (!HLODActorToBuild.IsNone() && HLODActorDesc.GetActorLabel() != HLODActorToBuild)
			{
				continue;
			}

			// Filter by HLOD layer
			if (!HLODLayerToBuild.IsNone() && HLODActorDesc.GetSourceHLODLayer().GetAssetName() != HLODLayerToBuild)
			{
				continue;
			}

			if (bBuildingStandaloneHLOD && IsDistributedBuild())
			{
				HLODActorToWorldPartitionIndex.Add(HLODIterator->GetGuid(), WorldPartitionIndex);
			}

			// When requested to build a single HLOD Layer, skip the child actors
			if (HLODLayerToBuild.IsNone())
			{
				TArray<FGuid>& ChildActors = HLODParenting.Add(HLODIterator->GetGuid(), HLODActorDesc.GetChildHLODActors());

				if (bShouldConsiderExternalHLODActors)
				{
					ChildActors.Append(HLODActorDesc.GetExternalChildHLODActors());
				}
			}
			else
			{
				HLODParenting.Add(HLODIterator->GetGuid());
			}
		}
	};

	// In distributed builds all workloads are prepared during the Setup step, which doesn't run on Standalone HLOD Levels, so we have to generate the workloads for them as well
	// If building Standalone HLODs, all HLOD actors are in Standalone HLOD Levels, so we can skip processing the main world
	// In non-distributed builds, workloads are generated during the Build step, which runs on Standalone HLOD Levels
	if (bBuildingStandaloneHLOD && IsDistributedBuild())
	{
		for (int32 WorldIndex = 0; WorldIndex < AdditionalWorldPartitionsForStandaloneHLOD.Num(); WorldIndex++)
		{
			ProcessWorldPartition(AdditionalWorldPartitionsForStandaloneHLOD[WorldIndex], WorldIndex);
		}
	}
	else
	{
		ProcessWorldPartition(WorldPartition, 0);
	}

	// All child HLODs must be built before their parent HLOD
	// Create groups to ensure those will be processed in the correct order, on the same builder
	TMap<FGuid, TArray<FGuid>> HLODGroups;
	TSet<FGuid>				   TriagedHLODs;

	TFunction<void(TArray<FGuid>&, const FGuid&)> RecursiveAdd = [&TriagedHLODs, &HLODParenting, &HLODGroups, &RecursiveAdd](TArray<FGuid>& HLODGroup, const FGuid& HLODGuid)
	{
		if (!TriagedHLODs.Contains(HLODGuid))
		{
			TriagedHLODs.Add(HLODGuid);
			HLODGroup.Insert(HLODGuid, 0); // Child will come first in the list, as they need to be built first...
			TArray<FGuid>* ChildHLODs = HLODParenting.Find(HLODGuid);
			if (ChildHLODs)
			{
				for (const auto& ChildGuid : *ChildHLODs)
				{
					RecursiveAdd(HLODGroup, ChildGuid);
				}
			}
		}
		else
		{
			HLODGroup.Insert(MoveTemp(HLODGroups.FindChecked(HLODGuid)), 0);
			HLODGroups.Remove(HLODGuid);
		}
	};

	for (const auto& Pair : HLODParenting)
	{
		if (!TriagedHLODs.Contains(Pair.Key))
		{
			TArray<FGuid>& HLODGroup = HLODGroups.Add(Pair.Key);
			RecursiveAdd(HLODGroup, Pair.Key);
		}
	}

	// Sort groups by number of HLOD actors
	HLODGroups.ValueSort([](const TArray<FGuid>& GroupA, const TArray<FGuid>& GroupB) { return GroupA.Num() > GroupB.Num(); });

	// Dispatch them in multiple lists and try to balance the workloads as much as possible
	TArray<FHLODWorkload> Workloads;
	Workloads.SetNum(NumWorkloads);

	for (FHLODWorkload& Workload : Workloads)
	{
		if (bBuildingStandaloneHLOD && IsDistributedBuild())
		{
			Workload.PerWorldHLODWorkloads.SetNum(AdditionalWorldPartitionsForStandaloneHLOD.Num());
		}
		else
		{
			Workload.PerWorldHLODWorkloads.SetNum(1);
		}
	}

	int32 Idx = 0;
	for (const auto& Pair : HLODGroups)
	{
		int32 WorkloadNum = Idx % NumWorkloads;
		for (const FGuid& HLODActorGuid : Pair.Value)
		{
			int32 WorldIndex;
			if (bBuildingStandaloneHLOD && IsDistributedBuild())
			{
				// We might be generating workloads for a few Worlds at the same time. Find which one, so that we can assign actor to the right workload
				const uint32* IndexPtr = HLODActorToWorldPartitionIndex.Find(HLODActorGuid);
				check(IndexPtr);
				WorldIndex = *IndexPtr;
			}
			else
			{
				WorldIndex = 0;
			}
			
			Workloads[WorkloadNum].PerWorldHLODWorkloads[WorldIndex].Add(HLODActorGuid);
		}
		Idx++;
	}

	// Validate workloads to ensure our meshes are built in the correct order
	for (const FHLODWorkload& Workload : Workloads)
	{
		check(ValidateWorkload(Workload, bShouldConsiderExternalHLODActors));
	}

	return Workloads;
}

bool UWorldPartitionHLODsBuilder::ValidateWorkload(const FHLODWorkload& Workload, bool bShouldConsiderExternalHLODActors) const
{
	check(WorldPartition);

	uint32 NumHLODs = 0;
	for (const TArray<FGuid>& HLODActorArray : Workload.PerWorldHLODWorkloads)
	{
		NumHLODs += HLODActorArray.Num();
	}

	TSet<FGuid> ProcessedHLOD;
	ProcessedHLOD.Reserve(NumHLODs);

	// For each HLOD entry in the workload, validate that its children are found before itself
	for (int32 WorldIndex = 0; WorldIndex < Workload.PerWorldHLODWorkloads.Num(); WorldIndex++)
	{
		UWorldPartition* CurrentWorldPartition = (bBuildingStandaloneHLOD && IsDistributedBuild()) ? AdditionalWorldPartitionsForStandaloneHLOD[WorldIndex].Get() : WorldPartition;
		for (const FGuid& HLODActorGuid : Workload.PerWorldHLODWorkloads[WorldIndex])
		{
			const FWorldPartitionActorDescInstance* ActorDescInstance = CurrentWorldPartition->GetActorDescInstance(HLODActorGuid);
			if(!ActorDescInstance)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Unknown actor guid found (\"%s\"), your HLOD actors are probably out of date. Run with -SetupHLODs to fix this. Exiting..."), *HLODActorGuid.ToString());
				return false;
			}

			if (!ActorDescInstance->GetActorNativeClass()->IsChildOf<AWorldPartitionHLOD>())
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Unexpected actor guid found in HLOD workload (\"%s\"), exiting..."), *HLODActorGuid.ToString());
				return false;
			}

			// When requested to build a single HLOD Layer, do not validate that child actors are included
			if (HLODLayerToBuild.IsNone())
			{
				const FHLODActorDesc* HLODActorDesc = static_cast<const FHLODActorDesc*>(ActorDescInstance->GetActorDesc());

				for (const FGuid& ChildHLODActorGuid : HLODActorDesc->GetChildHLODActors())
				{
					if (!ProcessedHLOD.Contains(ChildHLODActorGuid))
					{
						UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Child HLOD actor (\"%s\") missing or out of order in HLOD workload, exiting..."), *ChildHLODActorGuid.ToString());
						return false;
					}
				}

				// Skip checking whether external child actors are included if we're not considering them
				if (bShouldConsiderExternalHLODActors)
				{
					for (const FGuid& ExternalChildHLODActorGuid : HLODActorDesc->GetExternalChildHLODActors())
					{
						if (!ProcessedHLOD.Contains(ExternalChildHLODActorGuid))
						{
							UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("External child HLOD actor (\"%s\") missing or out of order in HLOD workload, exiting..."), *ExternalChildHLODActorGuid.ToString());
							return false;
						}
					}
				}
			}

			ProcessedHLOD.Add(HLODActorGuid);
		}
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::GenerateBuildManifest(TMap<FString, TPair<int32, int32>>& FilesToBuilderAndWorldIndexMap) const
{
	// We're generating manifest for Standalone HLOD levels as well (if any), so we want to consider External HLOD actors
	TArray<FHLODWorkload> BuildersWorkload = GetHLODWorkloads(BuilderCount, /*bShouldConsiderExternalHLODActors=*/true);

	// If we're generating manifest for Standalone HLOD levels, each of them needs a separate config file
	TArray<FConfigFile> ConfigFiles;
	bool bHasStandaloneHLOD = WorldPartition ? WorldPartition->HasStandaloneHLOD() : false;
	ConfigFiles.SetNum(bHasStandaloneHLOD ? AdditionalWorldPartitionsForStandaloneHLOD.Num() : 1);
	for (FConfigFile& ConfigFile : ConfigFiles)
	{
		ConfigFile.SetInt64(TEXT("General"), TEXT("BuilderCount"), BuilderCount);
		ConfigFile.SetString(TEXT("General"), TEXT("EngineVersion"), *FEngineVersion::Current().ToString());
	}

	// When processing multiple maps, ensure that the worldload is distributed evenly between builders.
	// Otherwise, maps with a single HLOD would all end up being processed by the first builder, while the others would have no work.
	static int32 BuilderDispatchOffset = 0;

	for(int32 Idx = 0; Idx < BuilderCount; Idx++)
	{
		const int32 WorkloadIndex = Idx;
		const int32 BuilderIndex = (BuilderDispatchOffset + Idx) % BuilderCount;

		if (!BuildersWorkload.IsValidIndex(WorkloadIndex) || BuildersWorkload[WorkloadIndex].PerWorldHLODWorkloads.IsEmpty())
		{
			continue;
		}

		FString SectionName = GetHLODBuilderFolderName(BuilderIndex);

		for (int32 WorldIndex = 0; WorldIndex < BuildersWorkload[WorkloadIndex].PerWorldHLODWorkloads.Num(); WorldIndex++)
		{
			UWorldPartition* CurrentWorldPartition = bHasStandaloneHLOD ? AdditionalWorldPartitionsForStandaloneHLOD[WorldIndex].Get() : WorldPartition;
			for(const FGuid& ActorGuid : BuildersWorkload[WorkloadIndex].PerWorldHLODWorkloads[WorldIndex])
			{
				ConfigFiles[WorldIndex].AddToSection(*SectionName, TEXT("+HLODActorGuid"), ActorGuid.ToString(EGuidFormats::Digits));

				if (CurrentWorldPartition)
				{
					// Track which builder is responsible to handle each actor
					const FWorldPartitionActorDescInstance* ActorDescInstance = CurrentWorldPartition->GetActorDescInstance(ActorGuid);
					if (!ActorDescInstance)
					{
						UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Invalid actor GUID found while generating the HLOD build manifest, exiting..."));
						return false;
					}
					FString ActorPackageFilename = USourceControlHelpers::PackageFilename(ActorDescInstance->GetActorPackage().ToString());
					FilesToBuilderAndWorldIndexMap.Emplace(ActorPackageFilename, TPair<int32, int32>(BuilderIndex, WorldIndex));
				}
			}
		}
	}

	BuilderDispatchOffset++;

	for (int32 Index = 0; Index < ConfigFiles.Num(); Index++)
	{
		FString BuildManifestFile;
		if (bBuildingStandaloneHLOD)
		{
			BuildManifestFile = StandaloneHLODWorkingDirs[Index] / DistributedBuildManifestName;
		}
		else
		{
			BuildManifestFile = BuildManifest;
		}

		ConfigFiles[Index].Dirty = true;

		if (!ConfigFiles[Index].Write(BuildManifestFile))
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to write HLOD build manifest \"%s\""), *BuildManifestFile);
			return false;
		}
	}

	return true;
}

/*
	Working Dir structure
		/HLODBuilder0
			/Add
				NewFileA
				NewFileB
			/Delete
				DeletedFileA
				DeletedFileB
			/Edit
				EditedFileA
				EditedFileB

		/HLODBuilder1
			...
		/ToSubmit
			...

	Distributed mode
		* Distributed mode is ran into 3 steps
			* Setup (1 job)		
			* Build (N jobs)	
			* Submit (1 job)	
		
		* The Setup step will place files under the "HLODBuilder[0-N]" folder. Those files could be new or modified HLOD actors that will be built in the Build step. The setup step will also place files into the "ToSubmit" folder (deleted HLOD actors for example).
		* Each parallel job in the Build step will retrieve files from the "HLODBuilder[0-N]" folder. They will then proceed to build the HLOD actors as specified in the build manifest file. All built HLOD actor files will then be placed in the /ToSubmit folder.
		* The Submit step will gather all files under /ToSubmit and submit them.
		

		|			Setup			|					Build					  |		   Submit			|
		/Content -----------> /HLODBuilder -----------> /Content -----------> /ToSubmit -----------> /Content
*/

const FName FileAction_Add(TEXT("Add"));
const FName FileAction_Edit(TEXT("Edit"));
const FName FileAction_Delete(TEXT("Delete"));

bool UWorldPartitionHLODsBuilder::CopyFilesToWorkingDir(const FString& TargetDir, const FBuilderModifiedFiles& Files, const FString& WorkingDir, TArray<FString>& BuildProducts)
{
	const FString AbsoluteTargetDir = WorkingDir / TargetDir / TEXT("");

	bool bSuccess = true;

	auto CopyFileToWorkingDir = [&](const FString& SourceFilename, const FName FileAction)
	{
		FString SourceFilenameRelativeToRoot = SourceFilename;
		FPaths::MakePathRelativeTo(SourceFilenameRelativeToRoot, *FPaths::RootDir());

		FString TargetFilename = AbsoluteTargetDir / FileAction.ToString() / SourceFilenameRelativeToRoot;

		BuildProducts.Add(TargetFilename);

		if (FileAction != FileAction_Delete)
		{
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			bool bRet = IFileManager::Get().Copy(*TargetFilename, *SourceFilename, bReplace, bEvenIfReadOnly) == COPY_OK;
			if (!bRet)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to copy file from \"%s\" to \"%s\""), *SourceFilename, *TargetFilename);
				bSuccess = false;
			}
		}
		else
		{
			bool bRet = FFileHelper::SaveStringToFile(TEXT(""), *TargetFilename);
			if (!bRet)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to create empty file at \"%s\""), *TargetFilename);
				bSuccess = false;
			}
		}
	};

	// Wait for pending async file writes before copying to working dir
	UPackage::WaitForAsyncFileWrites();

	Algo::ForEach(Files.Get(FBuilderModifiedFiles::EFileOperation::FileAdded), [&](const FString& SourceFilename) { CopyFileToWorkingDir(SourceFilename, FileAction_Add); });
	Algo::ForEach(Files.Get(FBuilderModifiedFiles::EFileOperation::FileEdited), [&](const FString& SourceFilename) { CopyFileToWorkingDir(SourceFilename, FileAction_Edit); });
	Algo::ForEach(Files.Get(FBuilderModifiedFiles::EFileOperation::FileDeleted), [&](const FString& SourceFilename) { CopyFileToWorkingDir(SourceFilename, FileAction_Delete); });
	if (!bSuccess)
	{
		return false;
	}

	// Revert any file changes
	if (ISourceControlModule::Get().IsEnabled())
	{
		bool bRet = USourceControlHelpers::RevertFiles(Files.GetAllFiles());
		if (!bRet)
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to revert modified files: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
			return false;
		}
	}

	// Delete files we added
	for (const FString& FileToDelete : Files.Get(FBuilderModifiedFiles::EFileOperation::FileAdded))
	{
		if (!IFileManager::Get().Delete(*FileToDelete, false, true))
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error deleting file %s locally"), *FileToDelete);
			return false;
		}
	}

	return true;
}

bool UWorldPartitionHLODsBuilder::CopyFilesFromWorkingDir(const FString& SourceDir)
{
	const FString AbsoluteSourceDir = DistributedBuildWorkingDir / SourceDir / TEXT("");

	auto CopyFromWorkingDir = [](const TMap<FString, FString>& FilesToCopy) -> bool
	{
		for (const auto& Pair : FilesToCopy)
		{
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			bool bRet = IFileManager::Get().Copy(*Pair.Key, *Pair.Value, bReplace, bEvenIfReadOnly) == COPY_OK;
			if (!bRet)
			{
				UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to copy file from \"%s\" to \"%s\""), *Pair.Value, *Pair.Key);
				return false;
			}
		}
		return true;
	};

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *AbsoluteSourceDir, TEXT("*.*"), true, false);

	TMap<FString, FString>	FilesToAdd;
	TMap<FString, FString>	FilesToEdit;
	TArray<FString>			FilesToDelete;

	bool bRet = true;

	for(const FString& File : Files)
	{
		FString PathRelativeToWorkingDir = File;
		FPaths::MakePathRelativeTo(PathRelativeToWorkingDir, *AbsoluteSourceDir);

		FString FileActionString;
		const int32 SlashIndex = PathRelativeToWorkingDir.Find(TEXT("/"));
		if (SlashIndex != INDEX_NONE)
		{
			FileActionString = PathRelativeToWorkingDir.Mid(0, SlashIndex);
		}

		FPaths::MakePathRelativeTo(PathRelativeToWorkingDir, *(FileActionString / TEXT("")));
		FString FullPathInRootDirectory =  FPaths::RootDir() / PathRelativeToWorkingDir;

		FName FileAction(FileActionString);
		if (FileAction == FileAction_Add)
		{
			FilesToAdd.Add(*FullPathInRootDirectory, File);
		}
		else if (FileAction == FileAction_Edit)
		{
			FilesToEdit.Add(FullPathInRootDirectory, File);
		}
		else if (FileAction == FileAction_Delete)
		{
			FilesToDelete.Add(*FullPathInRootDirectory);
		}
		else
		{
			UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Unsupported file action %s for file %s"), *FileActionString, *FullPathInRootDirectory);
		}
	}

	TArray<FString> ToAdd;
	FilesToAdd.GetKeys(ToAdd);

	TArray<FString> ToEdit;
	FilesToEdit.GetKeys(ToEdit);

	// When resuming a build (after a crash for example) we don't need to perform any file operation as these modification were done in the first run.
	if (!bResumeBuild)
	{
	    // Add
	    if (!FilesToAdd.IsEmpty())
	    {
			bRet = CopyFromWorkingDir(FilesToAdd);
			if (!bRet)
			{
				return false;
			}

			if (ISourceControlModule::Get().IsEnabled())
			{
				bRet = USourceControlHelpers::MarkFilesForAdd(ToAdd);
				if (!bRet)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Adding files to revision control failed: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
					return false;
				}
			}
	    }
    
	    // Delete
	    if (!FilesToDelete.IsEmpty())
	    {
			if (ISourceControlModule::Get().IsEnabled())
			{
				bRet = USourceControlHelpers::MarkFilesForDelete(FilesToDelete);
				if (!bRet)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Deleting files from revision control failed: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
					return false;
				}
			}
			else
			{
				for (const FString& FileToDelete : FilesToDelete)
				{
					const bool bRequireExists = false;
					const bool bEvenIfReadOnly = true;
					bRet = IFileManager::Get().Delete(*FileToDelete, bRequireExists, bEvenIfReadOnly);
					if (!bRet)
					{
						UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Failed to delete file from disk: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
						return false;
					}
				}
			}
	    }
    
	    // Edit
	    if (!FilesToEdit.IsEmpty())
	    {
		    if (ISourceControlModule::Get().IsEnabled())
		    {
				bRet = USourceControlHelpers::CheckOutFiles(ToEdit);
				if (!bRet)
				{
					UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Checking out files from revision control failed: %s"), *USourceControlHelpers::LastErrorMsg().ToString());
					return false;
				}
			}
		
			bRet = CopyFromWorkingDir(FilesToEdit);
			if (!bRet)
			{
				return false;
			}
	    }
	}

	// Keep track of all modified files
	ModifiedFiles.Append(FBuilderModifiedFiles::EFileOperation::FileAdded, ToAdd);
	ModifiedFiles.Append(FBuilderModifiedFiles::EFileOperation::FileDeleted, FilesToDelete);
	ModifiedFiles.Append(FBuilderModifiedFiles::EFileOperation::FileEdited, ToEdit);

	// Force a rescan of the updated files
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanModifiedAssetFiles(ModifiedFiles.GetAllFiles());

	return true;
}

bool UWorldPartitionHLODsBuilder::AddBuildProducts(const TArray<FString>& BuildProducts) const
{
	// Write build products to a file
	FString BuildProductsFile = FString::Printf(TEXT("%s/%s/%s"), *FPaths::RootDir(), *DistributedBuildWorkingDirName, *BuildProductsFileName);
	bool bRet = FFileHelper::SaveStringArrayToFile(BuildProducts, *BuildProductsFile, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
	if (!bRet)
	{
		UE_LOG(LogWorldPartitionHLODsBuilder, Error, TEXT("Error writing build product file %s"), *BuildProductsFile);
	}
	return bRet;
}

#undef LOCTEXT_NAMESPACE