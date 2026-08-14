// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAInterchangeModule.h"
#include "InterchangeDnaTranslator.h"

#include "InterchangeManager.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericMeshPipeline.h"

#include "SkelMeshDNAUtils.h"
#include "DNAToSkelMeshMap.h"
#include "SkelMeshDNAReader.h"
#include "DNAAsset.h"
#include "DNAUtils.h"

#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/AssetUserData.h"
#include "Engine/Engine.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimCurveMetadata.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "AssetRegistry/IAssetRegistry.h"

#define LOCTEXT_NAMESPACE "FDNAInterchangeModule"

void FDNAInterchangeModule::StartupModule()
{
	auto RegisterItems = []()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

		// Register DNA translator here. In this way we don't have to change Project Settings. 
		// Interchange manager will recognize DNA file extension and run the translator overriding existing DNA Factory.
		InterchangeManager.RegisterTranslator(UInterchangeDnaTranslator::StaticClass());
	};

	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}

	UInterchangeManager::SetInterchangeImportEnabled(true);
}

void FDNAInterchangeModule::ShutdownModule()
{
	UInterchangeManager::SetInterchangeImportEnabled(false);
}

FDNAInterchangeModule& FDNAInterchangeModule::GetModule()
{
	static const FName ModuleName = TEXT("DNAInterchange");
	return FModuleManager::LoadModuleChecked<FDNAInterchangeModule>(ModuleName);
}

USkeletalMesh* FDNAInterchangeModule::ImportSync(const FString& InNewRigAssetName, const FString& InNewRigPath)
{
	USkeletalMesh* ImportedMesh = nullptr;
	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();

	if (!PluginDir.IsEmpty())
	{
		const FString DNAPath = PluginDir + TEXT("/IdentityTemplate/Face_Archetype.ardna");
		if (FPaths::FileExists(DNAPath))
		{
			UE::Interchange::FScopedSourceData ScopedSourceData(DNAPath);

			FImportAssetParameters ImportAssetParameters;
			ImportAssetParameters.bIsAutomated = true;
			ImportAssetParameters.bFollowRedirectors = false;
			ImportAssetParameters.ReimportAsset = nullptr;
			ImportAssetParameters.bReplaceExisting = true;
			ImportAssetParameters.DestinationName = InNewRigAssetName;

			const FString PipelinePath = TEXT("/Interchange/Pipelines/DefaultAssetsPipeline");
			UInterchangeGenericAssetsPipeline* PipeAsset = LoadObject<UInterchangeGenericAssetsPipeline>(nullptr, *PipelinePath);
			PipeAsset->CommonMeshesProperties->bKeepSectionsSeparate = true;
			PipeAsset->CommonMeshesProperties->bImportLods = false;
			PipeAsset->MeshPipeline->bCreatePhysicsAsset = false;

			USkeleton* PluginSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/MetaHuman/IdentityTemplate/Face_Archetype_Skeleton.Face_Archetype_Skeleton"));
			if (PluginSkeleton)
			{
				PipeAsset->CommonSkeletalMeshesAndAnimationsProperties->Skeleton = PluginSkeleton;
			}

			FSoftObjectPath AssetPipeline = FSoftObjectPath(PipeAsset);
			ImportAssetParameters.OverridePipelines.Add(AssetPipeline);
			UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

			UE::Interchange::FAssetImportResultRef ImportRes = InterchangeManager.ImportAssetWithResult(InNewRigPath, ScopedSourceData.GetSourceData(), ImportAssetParameters);

			for (UObject* Object : ImportRes->GetImportedObjects())
			{
				if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Object))
				{
					PopulateSkelMeshData(SkelMesh, DNAPath);
					ImportedMesh = SkelMesh;
				}
				else if (USkeleton* Skeleton = Cast<USkeleton>(Object))
				{
					//TODO: Decide if this is actually needed as it introduces another file to plugin content
					PopulateSkeletonData(Skeleton, PluginDir);
				}
			}
		}
	}

	return ImportedMesh;
}

void FDNAInterchangeModule::PopulateSkelMeshData(USkeletalMesh* InSkelMesh, const FString& InPathToDNA)
{
	if (TObjectPtr<UDNAAsset> DNAAsset = GetDNAAssetFromFile(InPathToDNA, InSkelMesh))
	{
		InSkelMesh->AddAssetUserData(DNAAsset);
	}

	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	TArray<FAssetData> AnimBPData;
	AssetRegistry.GetAssetsByPackageName(TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/Face_PostProcess_AnimBP"), AnimBPData);
	if (!AnimBPData.IsEmpty())
	{
		FAssetData& AnimBPAsset = AnimBPData[0];
		if (AnimBPAsset.IsValid())
		{
			if (AnimBPAsset.IsInstanceOf(UAnimBlueprint::StaticClass()))
			{
				// UE editor is going through this route
				UAnimBlueprint* LoadedAnimBP = Cast<UAnimBlueprint>(AnimBPAsset.GetAsset());
				InSkelMesh->SetPostProcessAnimBlueprint(LoadedAnimBP->GetAnimBlueprintGeneratedClass());
			}
			else if (AnimBPAsset.IsInstanceOf(UAnimBlueprintGeneratedClass::StaticClass()))
			{
				// Cooked UEFN seems to be going via this route
				UAnimBlueprintGeneratedClass* LoadedAnimBP = Cast<UAnimBlueprintGeneratedClass>(AnimBPAsset.GetAsset());
				InSkelMesh->SetPostProcessAnimBlueprint(LoadedAnimBP);
			}
		}
	}

	TArray<FSkeletalMaterial>& MeshMaterials = InSkelMesh->GetMaterials();
	for (FSkeletalMaterial& Material : MeshMaterials)
	{
		const FString Name = Material.MaterialSlotName.ToString();
		if(Name.Contains("head"))
		{
			UMaterial* HeadMaterial = LoadObject<UMaterial>(nullptr, TEXT("/MetaHuman/IdentityTemplate/M_MetaHumanIdentity_Head.M_MetaHumanIdentity_Head"));
			Material.MaterialInterface = HeadMaterial;
		}
		else if (Name.Contains("teeth"))
		{
			UMaterial* TeethMaterial = LoadObject<UMaterial>(nullptr, TEXT("/MetaHuman/IdentityTemplate/M_MetaHumanIdentity_Teeth.M_MetaHumanIdentity_Teeth"));
			Material.MaterialInterface = TeethMaterial;
		}
		else if (Name.Contains("eyeLeft") || Name.Contains("eyeRight"))
		{
			UMaterial* EyeMaterial = LoadObject<UMaterial>(nullptr, TEXT("/MetaHuman/IdentityTemplate/M_MetaHumanIdentity_Eye.M_MetaHumanIdentity_Eye"));
			Material.MaterialInterface = EyeMaterial;
		}
		else
		{
			UMaterial* EmptyMaterial = LoadObject<UMaterial>(nullptr, TEXT("/MetaHuman/IdentityTemplate/M_MetaHumanIdentity_Empty.M_MetaHumanIdentity_Empty"));
			Material.MaterialInterface = EmptyMaterial;
		}
	}
}

void FDNAInterchangeModule::PopulateSkeletonData(USkeleton* InSkeleton, const FString& InPluginDir)
{
	UAnimCurveMetaData* AnimCurveMetaData = NewObject<UAnimCurveMetaData>(InSkeleton, NAME_None, RF_Transactional);
	const FString PathToCurves = InPluginDir + "/Content/IdentityTemplate/SkelCurves.txt";
	TArray<FString> CurveData;

	FFileHelper::LoadFileToStringWithLineVisitor(*PathToCurves, [&AnimCurveMetaData](FStringView Line)
	{
		FString CurrentString = FString(Line);
		FString Left, Right;
		CurrentString.Split(TEXT("\""), nullptr, &Right);
		Right.Split(TEXT("\""), &Left, &Right);

		FName CurveName = FName(*Left);
		AnimCurveMetaData->AddCurveMetaData(CurveName);
		if (Right.Contains("bMorphtarget"))
		{
			FCurveMetaData* CurveMetadata = AnimCurveMetaData->GetCurveMetaData(CurveName);
			CurveMetadata->Type.bMorphtarget = true;
		}
	});

	InSkeleton->AddAssetUserData(AnimCurveMetaData);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDNAInterchangeModule, DNAInterchange)
