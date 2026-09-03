// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeOpenUSDImportModule.h"

#include "Algo/NoneOf.h"
#include "Engine/Engine.h"
#include "InterchangeManager.h"
#include "InterchangeProjectSettings.h"
#include "InterchangeUsdTranslator.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

void FInterchangeOpenUSDImportModule::StartupModule()
{
	using namespace UE::Interchange;
	auto RegisterItems = []()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
		InterchangeManager.RegisterTranslator(UInterchangeUSDTranslator::StaticClass());

		// Don't go through FInterchangeProjectSettingsUtils::GetMutableDefaultImportSettings as we'll need a member of the actual
		// FInterchangeContent/SceneImportSettings struct anyway, and this is likely safer than casting the struct pointer
		UInterchangeProjectSettings* ProjectSettings = GetMutableDefault<UInterchangeProjectSettings>();
		if (!ProjectSettings)
		{
			return;
		}

		FInterchangeContentImportSettings& AssetImportSettings = ProjectSettings->ContentImportSettings;
		FInterchangeSceneImportSettings& SceneImportSettings = ProjectSettings->SceneImportSettings;

		TSoftClassPtr<UInterchangeTranslatorBase> TranslatorClassPath = TSoftClassPtr<UInterchangeTranslatorBase>(
			UInterchangeUSDTranslator::StaticClass()
		);

		FInterchangePerTranslatorDialogOverride ImportDialogOverride;
		ImportDialogOverride.Translator = TranslatorClassPath;
		ImportDialogOverride.bShowImportDialog = true;
		ImportDialogOverride.bShowReimportDialog = true;

		// Asset import pipelines
		{
			FInterchangeTranslatorPipelines TranslatorPipelines;
			TranslatorPipelines.Translator = TranslatorClassPath;
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/InterchangeOpenUSD/Pipelines/DefaultUSDAssetsPipeline.DefaultUSDAssetsPipeline")));
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/InterchangeOpenUSD/Pipelines/DefaultUSDPipelineAssetImport.DefaultUSDPipelineAssetImport")));
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/Interchange/Pipelines/DefaultMaterialXPipeline.DefaultMaterialXPipeline")));

			FInterchangePipelineStack& PipelineStack = AssetImportSettings.PipelineStacks[TEXT("Assets")];
			PipelineStack.PerTranslatorPipelines.Add(TranslatorPipelines);
		}

		// Scene import pipelines
		{
			FInterchangeTranslatorPipelines TranslatorPipelines;
			TranslatorPipelines.Translator = TranslatorClassPath;
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/InterchangeOpenUSD/Pipelines/DefaultUSDSceneAssetsPipeline.DefaultUSDSceneAssetsPipeline")));
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/Interchange/Pipelines/DefaultSceneLevelPipeline.DefaultSceneLevelPipeline")));
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/InterchangeOpenUSD/Pipelines/DefaultUSDPipeline.DefaultUSDPipeline")));
			TranslatorPipelines.Pipelines.Add(FSoftObjectPath(TEXT("/Interchange/Pipelines/DefaultMaterialXPipeline.DefaultMaterialXPipeline")));

			FInterchangePipelineStack& PipelineStack = SceneImportSettings.PipelineStacks[TEXT("Scene")];
			PipelineStack.PerTranslatorPipelines.Add(TranslatorPipelines);
		}

		// Asset import dialog overrides (we want to show the import and reimport dialog for all USD imports, of all asset types)
		{
			// TODO: Add a Count value to the enum and then use ENUM_RANGE_BY_COUNT?
			static const TArray<EInterchangeTranslatorAssetType> AssetTypes = {
				EInterchangeTranslatorAssetType::Textures,
				EInterchangeTranslatorAssetType::Materials,
				EInterchangeTranslatorAssetType::Meshes,
				EInterchangeTranslatorAssetType::Animations,
			};

			for (EInterchangeTranslatorAssetType AssetType : AssetTypes)
			{
				FInterchangeDialogOverride& DialogOverrides = AssetImportSettings.ShowImportDialogOverride.FindOrAdd(AssetType);

				// Don't set the dialog overrides for the USD translator if the user already has done that (with potentially different values)
				bool bHasUSDOverrides = false;
				for (const FInterchangePerTranslatorDialogOverride& Override : DialogOverrides.PerTranslatorImportDialogOverride)
				{
					if (Override.Translator == TranslatorClassPath)
					{
						bHasUSDOverrides = true;
						break;
					}
				}

				if (!bHasUSDOverrides)
				{
					DialogOverrides.PerTranslatorImportDialogOverride.Add(ImportDialogOverride);
				}
			}
		}

		// Scene import dialog overrides
		{
			// Don't set the dialog overrides for the USD translator if the user already has done that (with potentially different values)
			bool bHasUSDOverrides = false;
			for (const FInterchangePerTranslatorDialogOverride& Override : SceneImportSettings.PerTranslatorDialogOverride)
			{
				if (Override.Translator == TranslatorClassPath)
				{
					bHasUSDOverrides = true;
					break;
				}
			}

			if (!bHasUSDOverrides)
			{
				SceneImportSettings.PerTranslatorDialogOverride.Add(ImportDialogOverride);
			}
		}
	};

	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
}
void FInterchangeOpenUSDImportModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
}

IMPLEMENT_MODULE(FInterchangeOpenUSDImportModule, InterchangeOpenUSDImport)