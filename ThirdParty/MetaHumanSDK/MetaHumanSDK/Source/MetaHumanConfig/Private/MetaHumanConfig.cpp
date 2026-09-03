// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanConfig.h"
#include "MetaHumanConfigLog.h"
#include "CaptureData.h"
#if WITH_EDITOR
#include "MetaHumanConformer.h"
#endif

#include "DNAUtils.h"
#include "DNAAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "IPlatformCrypto.h"
#include "EncryptionContextOpenSSL.h"
#include "PlatformCryptoTypes.h"
#include "Misc/MessageDialog.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Serialization/EditorBulkData.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"

#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"
#include "TrackerOpticalFlowConfiguration.h"

#define LOCTEXT_NAMESPACE "MetaHumanConfig"
#define USE_BASE_CONFIG_DATA "UseBaseConfigData"

static FName CompressionFormatName = TEXT("Zlib");
static int32 BulkDataMask = 64;

static void SetBulkData(FByteBulkData& InBulkData, const TArray<uint8>& InData)
{
	InBulkData.RemoveBulkData(); // Need to clear out any existing data as below is an append operation

	const bool bIsPersistent = true;
	FBulkDataWriter Writer(InBulkData, bIsPersistent);
	Writer << const_cast<TArray<uint8>&>(InData);
}

static void ResetBulkData(FByteBulkData& InBulkData)
{
	TArray<uint8> Empty;
	SetBulkData(InBulkData, Empty);
}

static TArray<uint8> ReadBulkData(const FByteBulkData& InBulkData)
{
	TArray<uint8> Data;

	const bool bIsPersistent = true;
	FBulkDataReader Reader(const_cast<FByteBulkData&>(InBulkData), bIsPersistent);
	Reader << Data;

	return Data;
}

static void UpgradeEditorBulkData(const UE::Serialization::FEditorBulkData& InEditorData, FByteBulkData& OutBulkData)
{
	if (InEditorData.HasPayloadData())
	{
		TFuture<FSharedBuffer> PayloadFuture = InEditorData.GetPayload();

		if (PayloadFuture.Get().GetSize() > TNumericLimits<int32>::Max()) // Blocking call. Max that can be stored in a TArray
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Payload size too large"));
			return;
		}

		TArray<uint8> Data;
		Data.Append((const uint8*)PayloadFuture.Get().GetData(), PayloadFuture.Get().GetSize());

		SetBulkData(OutBulkData, Data);
	}
}

bool FMetaHumanConfig::GetInfo(UCaptureData* InCaptureData, const FString& InComponent, FString& OutDisplayName)
{
	UMetaHumanConfig* Config;
	return GetInfo(InCaptureData, InComponent, OutDisplayName, Config);
}

bool FMetaHumanConfig::GetInfo(UCaptureData* InCaptureData, const FString& InComponent, UMetaHumanConfig*& OutConfig)
{
	FString DisplayName;
	return GetInfo(InCaptureData, InComponent, DisplayName, OutConfig);
}

bool FMetaHumanConfig::GetInfo(UCaptureData* InCaptureData, const FString& InComponent, FString& OutDisplayName, UMetaHumanConfig*& OutConfig)
{
	bool bSpecifiedCaptureData = true;
	FString ConfigAsset;

	if (InCaptureData)
	{
		if (InCaptureData->IsA<UMeshCaptureData>())
		{
			if (InComponent == TEXT("Solver"))
			{
				ConfigAsset = TEXT("stereo_hmc");
			}
			else
			{
				ConfigAsset = TEXT("Mesh2MetaHuman");
			}

			OutDisplayName = TEXT("Mesh2MetaHuman");
		}
		else if (InCaptureData->IsA<UFootageCaptureData>())
		{
			UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(InCaptureData);

			switch (FootageCaptureData->Metadata.DeviceClass)
			{
			case EFootageDeviceClass::iPhone11OrEarlier:
			case EFootageDeviceClass::iPhone12:
				ConfigAsset = TEXT("iphone12");
				break;
			case EFootageDeviceClass::iPhone13:
			case EFootageDeviceClass::iPhone14OrLater:
			case EFootageDeviceClass::OtheriOSDevice:
				ConfigAsset = TEXT("iphone13");
				break;
			case EFootageDeviceClass::StereoHMC:
				ConfigAsset = TEXT("stereo_hmc");
				break;
			case EFootageDeviceClass::Unspecified:
			default:
				UE_LOG(LogMetaHumanConfig, Warning, TEXT("Unspecified device class, assuming iPhone 13"));
				ConfigAsset = TEXT("iphone13");
				break;
			};

			// Display name is currently the DeviceClass name as text, eg "iPhone 12". In time this maybe more complicated
			// and use the DeviceModel (eg "iphone13,3") to have a more user friendly display name, eg "iPhone 12 Pro".
			FText DeviceClassText;
			UEnum::GetDisplayValueAsText(FootageCaptureData->Metadata.DeviceClass, DeviceClassText);
			OutDisplayName = DeviceClassText.ToString();

			bSpecifiedCaptureData = FootageCaptureData->Metadata.DeviceClass != EFootageDeviceClass::Unspecified;
		}
		else
		{
			checkf(false, TEXT("Unhandled capture data type"));
		}
	}
	else
	{
		// The Identity editor "finalizes" the identity (creates PCA model) upon creation and before any capture data has been set.
		// In order for this to succeed and not produce any log errors an arbitrary, but valid, config is needed.
		// Finalize is called again once the identity has been setup and capture data set, so the results of the initial finalize
		// are never actually used.
		ConfigAsset = TEXT("stereo_hmc");
		OutDisplayName = TEXT("");

		bSpecifiedCaptureData = false;
	}

	FString PluginContentDir = TEXT("/" UE_PLUGIN_NAME);

	if (InComponent == TEXT("Solver"))
	{
		PluginContentDir += TEXT("/Solver/");
	}
	else
	{
		PluginContentDir += TEXT("/MeshFitting/");
	}

	FString Path = PluginContentDir + ConfigAsset + TEXT(".") + ConfigAsset;

	check(IsInGameThread());

	OutConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), *Path);

	return bSpecifiedCaptureData;
}



static FString FindVersion(const TArray<FString>& InVersionLines, const FString& InDeviceName)
{
	for (const FString& VersionLine : InVersionLines)
	{
		if (VersionLine.Contains(InDeviceName))
		{
			return FPaths::GetPathLeaf(VersionLine);
		}
	}

	return TEXT("");
}

bool UMetaHumanConfig::VerifySolverConfig(const FString& InSolverTemplateDataJson, const FString& InSolverConfigDataJson, const FString& InSolverDefinitionsDataJson,
	const FString& InSolverHierarchicalDefinitionsDataJson, const FString& InSolverPCAFromDNADataJson, FString& OutErrorString) const
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		OutErrorString = TEXT("Please make sure Depth Processing plugin is enabled");
		return false;
	}
	
	IFaceTrackerNodeImplFactory& FaceTrackerImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
	TSharedPtr<IMetaHumanFaceTrackerInterface> FaceTracker = FaceTrackerImplFactory.CreateFaceTrackerImplementor();
	
	if (!FaceTracker->Init(InSolverTemplateDataJson, InSolverConfigDataJson, {}, ""))
	{
		OutErrorString = TEXT("face tracking config contains invalid data.");
		return false;
	}

	// check the optical flow config
	auto OpticalFlow = FaceTrackerImplFactory.CreateOpticalFlowImplementor();
	
	if (!OpticalFlow->Init(InSolverConfigDataJson, FString{}))
	{
		OutErrorString = TEXT("optical flow part of face tracking config contains invalid data.");
		return false;
	}

	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
	const FString PathToDNA = PluginDir + TEXT("/IdentityTemplate/Face_Archetype.ardna");
	TObjectPtr<UDNAAsset> ArchetypeDnaAsset = GetDNAAssetFromFile(PathToDNA, GetTransientPackage());
	
	if (!ArchetypeDnaAsset)
	{
		OutErrorString = TEXT("failed to get face archetype DNA");
		return false;
	}

#if WITH_EDITOR
	// check the PCA from DNA data; note this functionality is only available with editor
	if (!UE::Wrappers::FMetaHumanConformer::CheckPcaModelFromDnaRigConfig(InSolverPCAFromDNADataJson, ArchetypeDnaAsset))
	{
		OutErrorString = TEXT("PCA model from DNA rig config contains invalid data.");
		return false;
	}
#endif

	// check the face tracking post processing config
	TSharedPtr<IFaceTrackerPostProcessingInterface> FaceTrackerPostProcessing = FaceTrackerImplFactory.CreateFaceTrackerPostProcessingImplementor();
	if (!FaceTrackerPostProcessing->Init(InSolverTemplateDataJson, InSolverConfigDataJson))
	{
		OutErrorString = TEXT("face tracking post-processing config contains invalid data.");
		return false;
	}

#if WITH_EDITOR
	// check the solver definitions; note this functionality is only available with editor
	if (!FaceTrackerPostProcessing->LoadDNA(ArchetypeDnaAsset, InSolverDefinitionsDataJson))
	{
		OutErrorString = TEXT("face tracking solver definitions contains invalid data.");
		return false;
	}

	// check the solver definitions; note this functionality is only available with editor
	if (!FaceTrackerPostProcessing->LoadDNA(ArchetypeDnaAsset, InSolverHierarchicalDefinitionsDataJson))
	{
		OutErrorString = TEXT("face tracking hierarchical solver definitions contains invalid data.");
		return false;
	}
#endif

	return true;
}

bool UMetaHumanConfig::ReadFromDirectory(const FString& InPath)
{
	EMetaHumanConfigType ConfigType = EMetaHumanConfigType::Unspecified;

	FString SolverTemplateDataFile = InPath + TEXT("/template_description.json");
	FString SolverConfigDataFile = InPath + TEXT("/configuration.json");
	FString SolverDefinitionsDataFile = InPath + TEXT("/solver_definitions.json");
	FString SolverHierarchicalDefinitionsDataFile = InPath + TEXT("/hierarchical_solver_definitions.json");
	FString SolverPCAFromDNADataFile = InPath + TEXT("/pca_from_dna_configuration.json");
	FString FittingTemplateDataFile = InPath + TEXT("/template_description.json");
	FString FittingConfigDataFile = InPath + TEXT("/configuration_autorig.json");
	FString FittingConfigTeethDataFile = InPath + TEXT("/configuration_teeth_fitting.json");
	FString FittingIdentityModelDataFile = InPath + TEXT("/dna_database_description.json");
	FString FittingControlsDataFile = InPath + TEXT("/controls.json");
	FString PredictiveBrowseDataFile = InPath + TEXT("/nnsolver_brows_data.bin");
	FString PredictiveEyesDataFile = InPath + TEXT("/nnsolver_eyes_data.bin");
	FString PredictiveJawDataFile = InPath + TEXT("/nnsolver_jaw_no_teeth_data.bin");
	FString PredictiveLowerDataFile = InPath + TEXT("/nnsolver_lower_data.bin");

	if (ConfigType == EMetaHumanConfigType::Unspecified &&
		FPaths::FileExists(SolverTemplateDataFile) &&
		FPaths::FileExists(SolverConfigDataFile) &&
		FPaths::FileExists(SolverDefinitionsDataFile) &&
		FPaths::FileExists(SolverHierarchicalDefinitionsDataFile) &&
		FPaths::FileExists(SolverPCAFromDNADataFile))
	{
		ConfigType = EMetaHumanConfigType::Solver;
	}

	if (ConfigType == EMetaHumanConfigType::Unspecified &&
		FPaths::FileExists(FittingTemplateDataFile) &&
		FPaths::FileExists(FittingConfigDataFile) &&
		FPaths::FileExists(FittingConfigTeethDataFile) &&
		FPaths::FileExists(FittingIdentityModelDataFile) &&
		FPaths::FileExists(FittingControlsDataFile))
	{
		ConfigType = EMetaHumanConfigType::Fitting;
	}

	if (ConfigType == EMetaHumanConfigType::Unspecified &&
		FPaths::FileExists(PredictiveBrowseDataFile) &&
		FPaths::FileExists(PredictiveEyesDataFile) &&
		FPaths::FileExists(PredictiveJawDataFile) &&
		FPaths::FileExists(PredictiveLowerDataFile))
	{
		ConfigType = EMetaHumanConfigType::PredictiveSolver;
	}

	if (ConfigType == EMetaHumanConfigType::Unspecified)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Missing config files", "Missing configuration files"));
		return false;
	}

	FString SolverTemplateDataJson, SolverConfigDataJson, SolverDefinitionsDataJson, SolverHierarchicalDefinitionsDataJson, SolverPCAFromDNADataJson;
	FString FittingTemplateDataJson, FittingConfigDataJson, FittingConfigTeethDataJson, FittingIdentityModelDataJson, FittingControlsDataJson;
	TArray<uint8> GlobalTeethPredictiveSolverTrainingDataMemoryBuffer, PredictiveSolversTrainingDataMemoryBuffer;
	FString ParentDirectoryName;
	FString VersionFilename;

	IFaceTrackerNodeImplFactory& FaceTrackerImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
	TSharedPtr<IMetaHumanFaceTrackerInterface> FaceTracker = FaceTrackerImplFactory.CreateFaceTrackerImplementor();
	
	if (ConfigType == EMetaHumanConfigType::Solver)
	{
		if (FaceTracker->CreateFlattenedJsonStringWrapper(SolverTemplateDataFile, SolverTemplateDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *SolverTemplateDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(SolverConfigDataFile, SolverConfigDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *SolverConfigDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(SolverDefinitionsDataFile, SolverDefinitionsDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *SolverDefinitionsDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(SolverHierarchicalDefinitionsDataFile, SolverHierarchicalDefinitionsDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *SolverHierarchicalDefinitionsDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(SolverPCAFromDNADataFile, SolverPCAFromDNADataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *SolverPCAFromDNADataFile);
			return false;
		}

		ParentDirectoryName = FPaths::GetPathLeaf(FPaths::GetPath(InPath));
		VersionFilename = InPath + TEXT("/../../../config_versions.txt");

		FString ErrorString;
		if (!VerifySolverConfig(SolverTemplateDataJson, SolverConfigDataJson, SolverDefinitionsDataJson, SolverHierarchicalDefinitionsDataJson, SolverPCAFromDNADataJson, ErrorString))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Solving Config validation error: %s"), *ErrorString);
			return false;
		}
	}
	else if (ConfigType == EMetaHumanConfigType::Fitting)
	{
		if (FaceTracker->CreateFlattenedJsonStringWrapper(FittingTemplateDataFile, FittingTemplateDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *FittingTemplateDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(FittingConfigDataFile, FittingConfigDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *FittingConfigDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(FittingConfigTeethDataFile, FittingConfigTeethDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *FittingConfigTeethDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(FittingIdentityModelDataFile, FittingIdentityModelDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *FittingIdentityModelDataFile);
			return false;
		}

		if (FaceTracker->CreateFlattenedJsonStringWrapper(FittingControlsDataFile, FittingControlsDataJson))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load file %s"), *FittingControlsDataFile);
			return false;
		}

		ParentDirectoryName = FPaths::GetPathLeaf(InPath);
		VersionFilename = InPath + TEXT("/../../config_versions.txt");

		FString ErrorString;
		if (!VerifyFittingConfig(FittingTemplateDataJson, FittingConfigDataJson, FittingConfigTeethDataJson, FittingIdentityModelDataJson, FittingControlsDataJson, ErrorString))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Fitting Config validation error: %s"), *ErrorString);
			return false;
		}
	}
	else if (ConfigType == EMetaHumanConfigType::PredictiveSolver) //-V547
	{
		TArray<FString> PredictiveSolverDataFiles;
		PredictiveSolverDataFiles.Push(PredictiveLowerDataFile);
		PredictiveSolverDataFiles.Push(PredictiveEyesDataFile);
		PredictiveSolverDataFiles.Push(PredictiveBrowseDataFile);

		// this also validates the content of the predictive solver training data
		if (!FaceTracker->LoadPredictiveSolverTrainingDataWrapper(PredictiveJawDataFile, PredictiveSolverDataFiles,
			GlobalTeethPredictiveSolverTrainingDataMemoryBuffer, PredictiveSolversTrainingDataMemoryBuffer))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to load predictive solver files"));
			return false;
		}

		ParentDirectoryName = FPaths::GetPathLeaf(InPath);
		VersionFilename = InPath + TEXT("/../../config_versions.txt");
	}
	else
	{
		check(false);
	}

	if (ParentDirectoryName != TEXT("iphone12") &&
		ParentDirectoryName != TEXT("iphone13") &&
		ParentDirectoryName != TEXT("stereo_hmc") &&
		ParentDirectoryName != TEXT("predictivesolvers") &&
		ParentDirectoryName != TEXT("Mesh2MetaHuman"))
	{
		UE_LOG(LogMetaHumanConfig, Warning, TEXT("Unknown directory name %s"), *ParentDirectoryName);
	}

	TArray<FString> VersionLines;

	if (FPaths::FileExists(VersionFilename))
	{
		if (!FFileHelper::LoadANSITextFileToStrings(*VersionFilename, nullptr, VersionLines))
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to read versions file %s"), *VersionFilename);
			return false;
		}
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Warning, TEXT("Missing version file %s"), *VersionFilename);
	}

	Type = ConfigType;

	// Version number is a combination of the lower 6 bits to defined the content 
	// OR'd with a bit which represents if the data is stored in FEditorBulkData or FBulkData.
	// 1 = no compression, editor bulk data
	// 2 = compressed, editor bulk data
	// 1 | BulkDataMask = 65 = no compression, bulk data
	// 2 | BulkDataMask = 66 = compressed, bulk data
	InternalVersion = (2 | BulkDataMask);

	if (ParentDirectoryName == TEXT("iphone12"))
	{
		Name = TEXT("iPhone 12");
		Version = FindVersion(VersionLines, ParentDirectoryName);
	}
	else if (ParentDirectoryName == TEXT("iphone13"))
	{
		Name = TEXT("iPhone 13");
		Version = FindVersion(VersionLines, ParentDirectoryName);
	}
	else if (ParentDirectoryName == TEXT("stereo_hmc"))
	{
		Name = TEXT("Stereo HMC");
		Version = FindVersion(VersionLines, TEXT("hmc"));
	}
	else if (ParentDirectoryName == TEXT("predictivesolvers"))
	{
		Name = TEXT("Predictive solvers");
		Version = FindVersion(VersionLines, TEXT("posed_based_solver"));
	}
	else if (ParentDirectoryName == TEXT("Mesh2MetaHuman"))
	{
		Name = TEXT("Mesh2MetaHuman");
		Version = FindVersion(VersionLines, TEXT("ue_mesh2metahuman"));
	}
	else
	{
		Name = TEXT("Unknown");
		Version = TEXT("Unknown");
	}

	ResetBulkData(SolverTemplateDataCipherText);
	ResetBulkData(SolverConfigDataCipherText);
	ResetBulkData(SolverDefinitionsCipherText);
	ResetBulkData(SolverHierarchicalDefinitionsCipherText);
	ResetBulkData(SolverPCAFromDNACipherText);
	ResetBulkData(FittingTemplateDataCipherText);
	ResetBulkData(FittingConfigDataCipherText);
	ResetBulkData(FittingConfigTeethDataCipherText);
	ResetBulkData(FittingIdentityModelDataCipherText);
	ResetBulkData(FittingControlsDataCipherText);
	ResetBulkData(PredictiveGlobalTeethTrainingData);
	ResetBulkData(PredictiveTrainingData);

	if (ConfigType == EMetaHumanConfigType::Solver || ConfigType == EMetaHumanConfigType::Fitting)
	{
		UMetaHumanConfig* BaseConfig = GetBaseConfig();

		if (BaseConfig)
		{
			if (ConfigType == EMetaHumanConfigType::Solver)
			{
				if (SolverTemplateDataFile == BaseConfig->GetSolverTemplateData())
				{
					SolverTemplateDataFile = USE_BASE_CONFIG_DATA;
				}

				if (SolverConfigDataFile == BaseConfig->GetSolverConfigData())
				{
					SolverConfigDataFile = USE_BASE_CONFIG_DATA;
				}

				if (SolverDefinitionsDataFile == BaseConfig->GetSolverDefinitionsData())
				{
					SolverDefinitionsDataFile = USE_BASE_CONFIG_DATA;
				}

				if (SolverHierarchicalDefinitionsDataFile == BaseConfig->GetSolverHierarchicalDefinitionsData())
				{
					SolverHierarchicalDefinitionsDataFile = USE_BASE_CONFIG_DATA;
				}

				if (SolverPCAFromDNADataFile == BaseConfig->GetSolverPCAFromDNAData())
				{
					SolverPCAFromDNADataFile = USE_BASE_CONFIG_DATA;
				}
			}
			else if (ConfigType == EMetaHumanConfigType::Fitting)
			{
				if (FittingTemplateDataJson == BaseConfig->GetFittingTemplateData())
				{
					FittingTemplateDataJson = USE_BASE_CONFIG_DATA;
				}

				if (FittingConfigDataJson == BaseConfig->GetFittingConfigData())
				{
					FittingConfigDataJson = USE_BASE_CONFIG_DATA;
				}

				if (FittingConfigTeethDataJson == BaseConfig->GetFittingConfigTeethData())
				{
					FittingConfigTeethDataJson = USE_BASE_CONFIG_DATA;
				}

				if (FittingIdentityModelDataJson == BaseConfig->GetFittingIdentityModelData())
				{
					FittingIdentityModelDataJson = USE_BASE_CONFIG_DATA;
				}

				if (FittingControlsDataJson == BaseConfig->GetFittingControlsData())
				{
					FittingControlsDataJson = USE_BASE_CONFIG_DATA;
				}
			}
		}
	}

	Encrypt(SolverTemplateDataJson, SolverTemplateDataCipherText);
	Encrypt(SolverConfigDataJson, SolverConfigDataCipherText);
	Encrypt(SolverDefinitionsDataJson, SolverDefinitionsCipherText);
	Encrypt(SolverHierarchicalDefinitionsDataJson, SolverHierarchicalDefinitionsCipherText);
	Encrypt(SolverPCAFromDNADataJson, SolverPCAFromDNACipherText);
	Encrypt(FittingTemplateDataJson, FittingTemplateDataCipherText);
	Encrypt(FittingConfigDataJson, FittingConfigDataCipherText);
	Encrypt(FittingConfigTeethDataJson, FittingConfigTeethDataCipherText);
	Encrypt(FittingIdentityModelDataJson, FittingIdentityModelDataCipherText);
	Encrypt(FittingControlsDataJson, FittingControlsDataCipherText);

	SetBulkData(PredictiveGlobalTeethTrainingData, GlobalTeethPredictiveSolverTrainingDataMemoryBuffer);
	SetBulkData(PredictiveTrainingData, PredictiveSolversTrainingDataMemoryBuffer);

	MarkPackageDirty();

	return true;
}

bool UMetaHumanConfig::VerifyFittingConfig(const FString& InFittingTemplateDataJson, const FString& InFittingConfigDataJson, const FString& InFittingConfigTeethDataJson, 
	const FString& InFittingIdentityModelDataJson, const FString& InFittingControlsDataJson, FString& OutErrorString) const
{
#if WITH_EDITOR
	// try and instantiate a Fitting object
	UE::Wrappers::FMetaHumanConformer ConformerNeutral;
	if (!ConformerNeutral.Init(InFittingTemplateDataJson, InFittingIdentityModelDataJson, InFittingConfigDataJson))
	{
		OutErrorString = TEXT("neutral pose config contains invalid data.");
		return false;
	}

	UE::Wrappers::FMetaHumanConformer ConformerTeeth;
	if (!ConformerTeeth.Init(InFittingTemplateDataJson, InFittingIdentityModelDataJson, InFittingConfigTeethDataJson))
	{
		OutErrorString = TEXT("teeth pose config contains invalid data.");
		return false;
	}

	if (!ConformerTeeth.CheckControlsConfig(InFittingControlsDataJson))
	{
		OutErrorString = TEXT("fitting controls contains invalid data.");
		return false;
	}
#endif
	return true;
}

FString UMetaHumanConfig::GetSolverTemplateData() const
{
	FString Data = DecryptToString(SolverTemplateDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetSolverTemplateData();
	}

	return Data;
}

FString UMetaHumanConfig::GetSolverConfigData() const
{
	FString Data = DecryptToString(SolverConfigDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetSolverConfigData();
	}

	return Data;
}

FString UMetaHumanConfig::GetSolverDefinitionsData() const
{
	FString Data = DecryptToString(SolverDefinitionsCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetSolverDefinitionsData();
	}

	return Data;
}

FString UMetaHumanConfig::GetSolverHierarchicalDefinitionsData() const
{
	FString Data = DecryptToString(SolverHierarchicalDefinitionsCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetSolverHierarchicalDefinitionsData();
	}

	return Data;
}

FString UMetaHumanConfig::GetSolverPCAFromDNAData() const
{
	FString Data = DecryptToString(SolverPCAFromDNACipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetSolverPCAFromDNAData();
	}

	return Data;
}

FString UMetaHumanConfig::GetFittingTemplateData() const
{
	FString Data = DecryptToString(FittingTemplateDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetFittingTemplateData();
	}

	return Data;
}

FString UMetaHumanConfig::GetFittingConfigData() const
{
	FString Data = DecryptToString(FittingConfigDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetFittingConfigData();
	}

	return Data;
}

FString UMetaHumanConfig::GetFittingConfigTeethData() const
{
	FString Data = DecryptToString(FittingConfigTeethDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetFittingConfigTeethData();
	}

	return Data;
}

FString UMetaHumanConfig::GetFittingIdentityModelData() const
{
	FString Data = DecryptToString(FittingIdentityModelDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetFittingIdentityModelData();
	}

	return Data;
}

FString UMetaHumanConfig::GetFittingControlsData() const
{
	FString Data = DecryptToString(FittingControlsDataCipherText);

	if (Data == USE_BASE_CONFIG_DATA)
	{
		Data = GetBaseConfig()->GetFittingControlsData();
	}

	return Data;
}

TArray<uint8> UMetaHumanConfig::GetPredictiveGlobalTeethTrainingData() const
{
	TArray<uint8> Data;

	if (PredictiveGlobalTeethTrainingData.GetElementCount() > 0)
	{
		if (PredictiveGlobalTeethTrainingData.GetElementCount() > TNumericLimits<int32>::Max())
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Payload size too large"));
			return Data;
		}

		Data = ReadBulkData(PredictiveGlobalTeethTrainingData);
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to get PredictiveGlobalTeethTraining payload"));
	}

	return Data;
}

TArray<uint8> UMetaHumanConfig::GetPredictiveTrainingData() const
{
	TArray<uint8> Data;

	if (PredictiveTrainingData.GetElementCount() > 0)
	{
		if (PredictiveTrainingData.GetElementCount() > TNumericLimits<int32>::Max())
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Payload size too large"));
			return Data;
		}

		Data = ReadBulkData(PredictiveTrainingData);
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to get PredictiveTrainingData payload"));
	}

	return Data;
}

void UMetaHumanConfig::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if ((InternalVersion & BulkDataMask) == 0) // Back compatibility case where data is stored in FEditorBulkData - need to move it to FByteBulkData
	{
		UE::Serialization::FEditorBulkData SolverTemplateDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData SolverConfigDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData SolverDefinitionsCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData SolverHierarchicalDefinitionsCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData SolverPCAFromDNACipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData FittingTemplateDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData FittingConfigDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData FittingConfigTeethDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData FittingIdentityModelDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData FittingControlsDataCipherText_DEPRECATED;
		UE::Serialization::FEditorBulkData PredictiveGlobalTeethTrainingData_DEPRECATED;
		UE::Serialization::FEditorBulkData PredictiveTrainingData_DEPRECATED;

		SolverTemplateDataCipherText_DEPRECATED.Serialize(Ar, this);
		SolverConfigDataCipherText_DEPRECATED.Serialize(Ar, this);
		SolverDefinitionsCipherText_DEPRECATED.Serialize(Ar, this);
		SolverHierarchicalDefinitionsCipherText_DEPRECATED.Serialize(Ar, this);
		SolverPCAFromDNACipherText_DEPRECATED.Serialize(Ar, this);
		FittingTemplateDataCipherText_DEPRECATED.Serialize(Ar, this);
		FittingConfigDataCipherText_DEPRECATED.Serialize(Ar, this);
		FittingConfigTeethDataCipherText_DEPRECATED.Serialize(Ar, this);
		FittingIdentityModelDataCipherText_DEPRECATED.Serialize(Ar, this);
		FittingControlsDataCipherText_DEPRECATED.Serialize(Ar, this);
		PredictiveGlobalTeethTrainingData_DEPRECATED.Serialize(Ar, this);
		PredictiveTrainingData_DEPRECATED.Serialize(Ar, this);

		UpgradeEditorBulkData(SolverTemplateDataCipherText_DEPRECATED, SolverTemplateDataCipherText);
		UpgradeEditorBulkData(SolverConfigDataCipherText_DEPRECATED, SolverConfigDataCipherText);
		UpgradeEditorBulkData(SolverDefinitionsCipherText_DEPRECATED, SolverDefinitionsCipherText);
		UpgradeEditorBulkData(SolverHierarchicalDefinitionsCipherText_DEPRECATED, SolverHierarchicalDefinitionsCipherText);
		UpgradeEditorBulkData(SolverPCAFromDNACipherText_DEPRECATED, SolverPCAFromDNACipherText);
		UpgradeEditorBulkData(FittingTemplateDataCipherText_DEPRECATED, FittingTemplateDataCipherText);
		UpgradeEditorBulkData(FittingConfigDataCipherText_DEPRECATED, FittingConfigDataCipherText);
		UpgradeEditorBulkData(FittingConfigTeethDataCipherText_DEPRECATED, FittingConfigTeethDataCipherText);
		UpgradeEditorBulkData(FittingIdentityModelDataCipherText_DEPRECATED, FittingIdentityModelDataCipherText);
		UpgradeEditorBulkData(FittingControlsDataCipherText_DEPRECATED, FittingControlsDataCipherText);
		UpgradeEditorBulkData(PredictiveGlobalTeethTrainingData_DEPRECATED, PredictiveGlobalTeethTrainingData);
		UpgradeEditorBulkData(PredictiveTrainingData_DEPRECATED, PredictiveTrainingData);

		InternalVersion = (InternalVersion | BulkDataMask);
	}
	else
	{
		SolverTemplateDataCipherText.Serialize(Ar, this);
		SolverConfigDataCipherText.Serialize(Ar, this);
		SolverDefinitionsCipherText.Serialize(Ar, this);
		SolverHierarchicalDefinitionsCipherText.Serialize(Ar, this);
		SolverPCAFromDNACipherText.Serialize(Ar, this);
		FittingTemplateDataCipherText.Serialize(Ar, this);
		FittingConfigDataCipherText.Serialize(Ar, this);
		FittingConfigTeethDataCipherText.Serialize(Ar, this);
		FittingIdentityModelDataCipherText.Serialize(Ar, this);
		FittingControlsDataCipherText.Serialize(Ar, this);
		PredictiveGlobalTeethTrainingData.Serialize(Ar, this);
		PredictiveTrainingData.Serialize(Ar, this);
	}
}

bool UMetaHumanConfig::Encrypt(const FString& InPlainText, FByteBulkData& OutCipherText) const
{
	// Encrypting the data is just to stop casual inspection of the plain text json config data.
	// Is it not meant to hide the data from a determined attacker.
	// Think more simple data obfuscation than true data encryption. 

	TArray<uint8> CipherText;

	FModuleManager::LoadModuleChecked<IPlatformCrypto>("PlatformCrypto");
	TUniquePtr<FEncryptionContext> EncryptionContext = IPlatformCrypto::Get().CreateContext();

	TArray<uint8> Key; // Key present in both Encrypt and Decrypt functions
	Key.SetNumZeroed(32);
	Key[0] = 'a';
	Key[12] = 'L';
	Key[2] = 'x';
	Key[23] = '*';

	TUniquePtr<IPlatformCryptoEncryptor> Encryptor = EncryptionContext->CreateEncryptor_AES_256_ECB(Key);

	TArray<uint8> PlainText;
	PlainText.SetNumUninitialized(InPlainText.Len());
	StringToBytes(InPlainText, PlainText.GetData(), PlainText.Num());

	FCompression Compression;

	TArray<uint8> CompressedPlainText;
	CompressedPlainText.SetNumUninitialized(4 + Compression.CompressMemoryBound(CompressionFormatName, InPlainText.Len()));

	CompressedPlainText[0] = (InPlainText.Len() >> 24) & 0xFF;
	CompressedPlainText[1] = (InPlainText.Len() >> 16) & 0xFF;
	CompressedPlainText[2] = (InPlainText.Len() >> 8) & 0xFF;
	CompressedPlainText[3] = InPlainText.Len() & 0xFF;

	int32 CompressedPlainTextSize = CompressedPlainText.Num() - 4;
	Compression.CompressMemory(CompressionFormatName, &CompressedPlainText.GetData()[4], CompressedPlainTextSize, PlainText.GetData(), PlainText.Num());
	CompressedPlainText.SetNum(CompressedPlainTextSize + 4);

	TArray<uint8> PartialCipherText;
	PartialCipherText.SetNumUninitialized(Encryptor->GetUpdateBufferSizeBytes(CompressedPlainText));
	int32 PartialCipherTextSize;

	if (Encryptor->Update(CompressedPlainText, PartialCipherText, PartialCipherTextSize) == EPlatformCryptoResult::Success)
	{
		PartialCipherText.SetNum(PartialCipherTextSize);
		CipherText += PartialCipherText;

		PartialCipherText.SetNumUninitialized(Encryptor->GetFinalizeBufferSizeBytes());

		if (Encryptor->Finalize(PartialCipherText, PartialCipherTextSize) == EPlatformCryptoResult::Success)
		{
			PartialCipherText.SetNum(PartialCipherTextSize);
			CipherText += PartialCipherText;

			SetBulkData(OutCipherText, CipherText);

			return true;
		}
		else
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to finalize config encrypt"));
		}
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to update config encrypt"));
	}

	return false;
}

bool UMetaHumanConfig::Decrypt(const FByteBulkData& InCipherText, FString& OutPlainText) const
{
	// A limit on the data size at each stage - encryted data can not be bigger than this size, nor can compressed or
	// uncompressed data. This prevents any possible buffer overflow, eg a maliciously modified config asset that would 
	// result in a decrpyted config bigger than the int32 bit limit of a TArray. Keeping the check simple - no part bigger
	// than 1Gb - since to do it accurately using TNumericLimits<int32>::Max would be error prone since the max size of some
	// stages is less than this to account for headers and cypher block size.
	constexpr int64 MaxDataSize = 1024 * 1024 * 1024; // 1Gb

	TArray<uint8> CipherText;

	if (InCipherText.GetElementCount() > 0)
	{
		if (InCipherText.GetElementCount() > MaxDataSize)
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Payload size too large"));
			return false;
		}

		CipherText = ReadBulkData(InCipherText);
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to get payload"));
	}

	FModuleManager::LoadModuleChecked<IPlatformCrypto>("PlatformCrypto");
	TUniquePtr<FEncryptionContext> EncryptionContext = IPlatformCrypto::Get().CreateContext();

	TArray<uint8> Key; // Key present in both Encrypt and Decrypt functions
	Key.SetNumZeroed(32);
	Key[0] = 'a';
	Key[12] = 'L';
	Key[2] = 'x';
	Key[23] = '*';

	TUniquePtr<IPlatformCryptoDecryptor> Decryptor = EncryptionContext->CreateDecryptor_AES_256_ECB(Key);

	TArray<uint8> PartialPlainText;
	PartialPlainText.SetNumUninitialized(Decryptor->GetUpdateBufferSizeBytes(CipherText));
	int32 PartialPlainTextSize;

	TArray<uint8> PlainText;

	if (Decryptor->Update(CipherText, PartialPlainText, PartialPlainTextSize) == EPlatformCryptoResult::Success)
	{
		if (PartialPlainTextSize > MaxDataSize || PlainText.Num() + PartialPlainTextSize > MaxDataSize)
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("PartialPlainTextSize too large"));
			return false;
		}

		PartialPlainText.SetNum(PartialPlainTextSize);
		PlainText += PartialPlainText;

		PartialPlainText.SetNumUninitialized(Decryptor->GetFinalizeBufferSizeBytes());

		if (Decryptor->Finalize(PartialPlainText, PartialPlainTextSize) == EPlatformCryptoResult::Success)
		{
			if (PartialPlainTextSize > MaxDataSize || PlainText.Num() + PartialPlainTextSize > MaxDataSize)
			{
				UE_LOG(LogMetaHumanConfig, Fatal, TEXT("PartialPlainTextSize too large"));
				return false;
			}

			PartialPlainText.SetNum(PartialPlainTextSize);
			PlainText += PartialPlainText;

			if ((InternalVersion & ~BulkDataMask) == 2)
			{
				if (PlainText.Num() < 5) // 4 for size header plus at least 1 for data
				{
					UE_LOG(LogMetaHumanConfig, Fatal, TEXT("PlainText too small"));
					return false;
				}

				int32 UncompressedPlainTextSize = 0;

				UncompressedPlainTextSize += PlainText[0] << 24;
				UncompressedPlainTextSize += PlainText[1] << 16;
				UncompressedPlainTextSize += PlainText[2] << 8;
				UncompressedPlainTextSize += PlainText[3];

				if (UncompressedPlainTextSize < 0 || UncompressedPlainTextSize > MaxDataSize)
				{
					UE_LOG(LogMetaHumanConfig, Fatal, TEXT("UncompressedPlainTextSize bad size"));
					return false;
				}

				FCompression Compression;

				TArray<uint8> UncompressedPlainText;
				UncompressedPlainText.SetNumUninitialized(UncompressedPlainTextSize);

				if (!Compression.UncompressMemory(CompressionFormatName, UncompressedPlainText.GetData(), UncompressedPlainTextSize, &PlainText.GetData()[4], PlainText.Num() - 4))
				{
					UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to decompress config"));
					return false;
				}

				OutPlainText = BytesToString(UncompressedPlainText.GetData(), UncompressedPlainText.Num());
			}
			else
			{
				OutPlainText = BytesToString(PlainText.GetData(), PlainText.Num());
			}

			return true;
		}
		else
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to finalize config decrypt"));
		}
	}
	else
	{
		UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed to update config decrypt"));
	}

	return false;
}

FString UMetaHumanConfig::DecryptToString(const FByteBulkData& InCipherText) const
{
	FString PlainText;

	Decrypt(InCipherText, PlainText);

	return PlainText;
}

UMetaHumanConfig* UMetaHumanConfig::GetBaseConfig() const
{
	if (Name == "iPhone 12")
	{
		return nullptr;
	}
	else
	{
		FString Path = FString("/" UE_PLUGIN_NAME "/");

		if (Type == EMetaHumanConfigType::Fitting)
		{
			Path += TEXT("MeshFitting");
		}
		else if (Type == EMetaHumanConfigType::Solver)
		{
			Path += TEXT("Solver");
		}
		else
		{
			check(false);
		}

		Path += TEXT("/iphone12.iphone12");

		UMetaHumanConfig* BaseConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), *Path);

		if (!BaseConfig)
		{
			UE_LOG(LogMetaHumanConfig, Fatal, TEXT("Failed load base config"));
		}

		return BaseConfig;
	}
}

#undef USE_BASE_CONFIG_DATA
#undef LOCTEXT_NAMESPACE
