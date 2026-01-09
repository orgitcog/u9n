// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultEditorPipelineUEFN.h"
#include "MetaHumanDefaultPipelineBase.h"
#include "MetaHumanDefaultEditorPipelineLog.h"
#include "Subsystem/MetaHumanCharacterBuild.h"
#include "MetaHumanCollection.h"
#include "SubobjectDataSubsystem.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanComponentUE.h"
#include "MetaHumanCharacterInstance.h"

#include "Algo/RemoveIf.h"
#include "Logging/StructuredLog.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/SavePackage.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/PackageName.h"
#include "Algo/ForEach.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ScopedSlowTask.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "MaterialEditingLibrary.h"
#include "PluginDescriptor.h"
#include "Interfaces/IPluginManager.h"
#include "PluginUtils.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/AssetGuideline.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshCompiler.h"
#include "Misc/UObjectToken.h"
#include "PackageTools.h"
#include "BlueprintCompilationManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanDefaultPipelineUEFN"

namespace UE::MetaHuman::Private
{
	static void SavePackageDirect(TNotNull<UObject*> InObject)
	{
		UPackage* Package = InObject->GetPackage();

		// Wait for the package to fully load
		Package->ConditionalPostLoad();

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;

		// Mark package as private to minimize the public API of UEFN projects
		Package->SetPackageFlags(PKG_NotExternallyReferenceable);

		UPackage::SavePackage(Package, nullptr, *PackageFilename, SaveArgs);
	}

	static void ResizeTexture(TNotNull<UTexture2D*> InTexture)
	{
		// Maximum resolution for textures in UEFN is 2048 so make sure all textures are resized before saving
		ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		FMetaHumanCharacterEditorBuild::DownsizeTexture(InTexture, static_cast<int32>(EMetaHumanBuildTextureResolution::Res2048), RunningPlatform);
	}

	static TArray<UMaterialExpression*> ReconnectPathTracingQualitySwitches(TNotNull<UObject*> InMaterialOrMaterialFunction)
	{
		TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions;

		if (UMaterial* Material = Cast<UMaterial>(InMaterialOrMaterialFunction))
		{
			Expressions = Material->GetExpressions();
		}
		else if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(InMaterialOrMaterialFunction))
		{
			Expressions = MaterialFunction->GetExpressions();
		}
		else
		{
			checkNoEntry();
		}

		TArray<UMaterialExpression*> PathTracingQualitySwitchExpressions = Expressions.FilterByPredicate(
			[](const UMaterialExpression* Expression)
			{
				TArray<FString> Captions;
				Expression->GetCaption(Captions);

				// Expression is a UMaterialExpressionPathTracingQualitySwitch
				return !Captions.IsEmpty() && Captions[0] == TEXT("PathTracingQualitySwitchReplace");
			}
		);

		for (UMaterialExpression* PathTrackingQualitySwitchExpression : PathTracingQualitySwitchExpressions)
		{
			// Get the input expression connected to the Normal input
			FExpressionInput* NormalExpressionInput = PathTrackingQualitySwitchExpression->GetInput(0);

			// Now find all other expressions that have their input as the quality switch
			for (UMaterialExpression* CandidateExpression : Expressions)
			{
				for (FExpressionInputIterator InputIt{ CandidateExpression }; InputIt; ++InputIt)
				{
					if (InputIt->Expression == PathTrackingQualitySwitchExpression)
					{
						// Connect the expression that was connected to the normal input of quality switch
						// to the input of where the output of the quality switch was connected to
						InputIt->Connect(NormalExpressionInput->OutputIndex, NormalExpressionInput->Expression);
					}
				}
			}

			if (UMaterial* Material = Cast<UMaterial>(InMaterialOrMaterialFunction))
			{
				// Check material parameter inputs, to make sure that if the expression is not connected to it
				// the correct link will be made
				for (int32 InputIndex = 0; InputIndex < MP_MAX; InputIndex++)
				{
					FExpressionInput* Input = Material->GetExpressionInputForProperty((EMaterialProperty) InputIndex);
					if (Input && Input->Expression == PathTrackingQualitySwitchExpression)
					{
						Input->Connect(NormalExpressionInput->OutputIndex, NormalExpressionInput->Expression);
					}
				}
			}
		}

		return PathTracingQualitySwitchExpressions;
	}

	static void RemovePathTracingQualitySwitches(TNotNull<UObject*> InMaterialOrMaterialFunction)
	{
		TArray<UMaterialExpression*> ToDelete;

		if (UMaterial* Material = Cast<UMaterial>(InMaterialOrMaterialFunction))
		{
			ToDelete = ReconnectPathTracingQualitySwitches(Material);

			for (UMaterialExpression* Expression : ToDelete)
			{
				UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
			}

			if (!ToDelete.IsEmpty())
			{
				UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Display, "{NumSwitches} PathTracingQualitySwithces removed from Material {Material}", ToDelete.Num(), Material->GetName());
			}
		}
		else if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(InMaterialOrMaterialFunction))
		{
			ToDelete = ReconnectPathTracingQualitySwitches(MaterialFunction);

			for (UMaterialExpression* Expression : ToDelete)
			{
				UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(MaterialFunction, Expression);
			}

			if (!ToDelete.IsEmpty())
			{
				UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Display, "{NumSwitches} PathTracingQualitySwithces removed from Material Function {MaterialFunction}", ToDelete.Num(), MaterialFunction->GetName());
			}
		}
	}

	static void RemoveAssetGuidelines(TNotNull<UObject*> InObject)
	{
		if (IInterface_AssetUserData* AssetUserDataInterface = Cast<IInterface_AssetUserData>(InObject))
		{
			// Asset Guidelines are not supported in UEFN
			AssetUserDataInterface->RemoveUserDataOfClass(UAssetGuideline::StaticClass());
		}
	}

	/**
	 * Return the path to the root UEFN plugin for a given UEFN project file
	 *
	 * @param InUEFNProjectFile The .uefnproject file to parse
	 * @param OutUEFNPluginFileName The path to root UEFN plugin found in the project file
	 * @param OutFailReason Error message describing what happens in case of failure
	 * @return true if the root plugin file was found false otherwise
	 */
	static bool GetTargetUEFNRootPluginForProject(const FString& InUEFNProjectFile, FString& OutUEFNPluginFilename, FText& OutFailReason)
	{
		if (!FPaths::FileExists(InUEFNProjectFile))
		{
			OutFailReason = FText::Format(LOCTEXT("UEFNProjectFileDoesntExist", "Can't find UEFN project file '{0}'"), FText::FromString(InUEFNProjectFile));
			return false;
		}

		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *InUEFNProjectFile))
		{
			OutFailReason = FText::Format(LOCTEXT("FailedToReadUEFNProjectFile", "Failed to read UEFN project file '{0}'"), FText::FromString(InUEFNProjectFile));
			return false;
		}

		using FJsonReader = TJsonReader<TCHAR>;
		using FJsonReaderFactory = TJsonReaderFactory<TCHAR>;

		TSharedPtr<FJsonObject> JsonObject;

		TSharedRef<FJsonReader> Reader = FJsonReaderFactory::Create(Contents);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			OutFailReason = FText::Format(LOCTEXT("FailedToParseUEFNProjectFile", "Failed to parse UEFN project file '{0}'"), FText::FromString(InUEFNProjectFile));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Plugins;
		if (!JsonObject->TryGetArrayField(TEXT("plugins"), Plugins))
		{
			OutFailReason = FText::Format(LOCTEXT("FailedToFindPlugins", "Failed to find 'plugins' list in UEFN profile file '{0}'"), FText::FromString(InUEFNProjectFile));
			return false;
		}

		FString FoundPluginName;

		for (const TSharedPtr<FJsonValue>& PluginValue : *Plugins)
		{
			const TSharedPtr<FJsonObject>* PluginObject;
			if (PluginValue->TryGetObject(PluginObject))
			{
				bool bIsRoot = false;
				if ((*PluginObject)->TryGetBoolField(TEXT("bIsRoot"), bIsRoot))
				{
					if (bIsRoot)
					{
						if ((*PluginObject)->TryGetStringField(TEXT("name"), FoundPluginName))
						{
							break;
						}
					}
				}
			}
		}

		if (FoundPluginName.IsEmpty())
		{
			OutFailReason = FText::Format(LOCTEXT("FailedToFindRootPlugin", "Failed to find root plugin for UEFN project '{0}'"), FText::FromString(InUEFNProjectFile));
			return false;
		}

		TArray<FString> PluginFilePaths;
		IPluginManager::Get().FindPluginsUnderDirectory(FPaths::GetPath(InUEFNProjectFile), PluginFilePaths);

		for (const FString& PluginFilePath : PluginFilePaths)
		{
			const FString PluginName = FPluginUtils::GetPluginName(PluginFilePath);
			if (FoundPluginName == PluginName)
			{
				// Make sure the file exists
				if (FPaths::FileExists(PluginFilePath))
				{
					OutUEFNPluginFilename = PluginFilePath;
					return true;
				}
				else
				{
					OutFailReason = FText::Format(LOCTEXT("PluginFileDoesntExist", "Can't find plugin file '{0}'"), FText::FromString(FPaths::ConvertRelativePathToFull(PluginFilePath)));
					return false;
				}
			}
		}

		OutFailReason = FText::Format(LOCTEXT("PluginFileNotFound", "Can't find plugin file for plugin '{0}'"), FText::FromString(FoundPluginName));
		return false;
	}

	static void SaveObjectForUEFNProject(TNotNull<UObject*> TargetObject)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(TargetObject))
		{
			UE::MetaHuman::Private::ResizeTexture(Texture);
		}
		else if (UMaterialInterface* Material = Cast<UMaterialInterface>(TargetObject))
		{
			// Trying the solution from UE-23902
			// Call PostEditChange to regenerate material proxies
			Material->PostEditChange();
		}
		else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(TargetObject))
		{
			FSkinnedAssetCompilingManager::Get().FinishCompilation({ SkeletalMesh });
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(TargetObject))
		{
			FStaticMeshCompilingManager::Get().FinishCompilation({ StaticMesh });
		}

		UE::MetaHuman::Private::RemoveAssetGuidelines(TargetObject);
		UE::MetaHuman::Private::RemovePathTracingQualitySwitches(TargetObject);
		UE::MetaHuman::Private::SavePackageDirect(TargetObject);
	}

	// Adds the UEFN MH Component to the input BP, and set its properties from the input base MH Component
	static void AddUEFNMetaHumanComponent(
		TNotNull<const UMetaHumanComponentBase*> MetaHumanComponentBase, 
		const FSubobjectDataHandle& ParentHandle, 
		TNotNull<UBlueprint*> Blueprint, 
		int32 BodyLODThreshold)
	{
		USubobjectDataSubsystem* SubobjectDataSubsystem = USubobjectDataSubsystem::Get();
		if (!SubobjectDataSubsystem)
		{
			return;
		}

		// Create a class that replicates the MetaHumanComponent for UEFN. This will then be added to the blueprint to be loaded by UEFN
		UPackage* MetaHumanUEFNRuntimePackage = CreatePackage(TEXT("/Script/MetaHumanUEFNRuntime"));
		UClass* MetaHumanComponentUEFNClass = NewObject<UClass>(MetaHumanUEFNRuntimePackage, TEXT("MetaHumanComponent"), RF_Public);

		// Prevent the object from being deleted while we edit the blueprint
		TGCObjectsScopeGuard GCGuard{ TArray<const UObject*>{ MetaHumanComponentBase, MetaHumanComponentUEFNClass } };

		// The MetaHuman Component Base will be used as the template for the UEFN component class
		UClass* MetaHumanComponentBaseClass = UMetaHumanComponentBase::StaticClass();

		MetaHumanComponentUEFNClass->SetSuperStruct(MetaHumanComponentBaseClass);
		MetaHumanComponentUEFNClass->ClassConfigName = MetaHumanComponentBaseClass->ClassConfigName;
		MetaHumanComponentUEFNClass->ClassWithin = MetaHumanComponentBaseClass->ClassWithin;
		MetaHumanComponentUEFNClass->ClassFlags |= CLASS_Hidden;
		MetaHumanComponentUEFNClass->ClassConstructor = MetaHumanComponentBaseClass->ClassConstructor;
		MetaHumanComponentUEFNClass->ClassVTableHelperCtorCaller = MetaHumanComponentBaseClass->ClassVTableHelperCtorCaller;
		MetaHumanComponentUEFNClass->CppClassStaticFunctions = MetaHumanComponentBaseClass->CppClassStaticFunctions;
		MetaHumanComponentUEFNClass->PropertyLink = MetaHumanComponentBaseClass->PropertyLink;

		// Add an extra property for the BodyLODThreshold that is specific to the MetaHuman Component for UEFN
		FIntProperty* BodyLODThresholdProperty = new FIntProperty(MetaHumanComponentUEFNClass, TEXT("BodyLODThreshold"), EObjectFlags::RF_NoFlags);
		MetaHumanComponentUEFNClass->AddCppProperty(BodyLODThresholdProperty);
		MetaHumanComponentUEFNClass->SetPropertiesSize(MetaHumanComponentBaseClass->PropertiesSize + sizeof(int32));

		// Update the class
		MetaHumanComponentUEFNClass->Bind();

		// This is required for GC to work properly
		MetaHumanComponentUEFNClass->StaticLink(true);
		MetaHumanComponentUEFNClass->AssembleReferenceTokenStream();

		FAddNewSubobjectParams Params;
		Params.ParentHandle = ParentHandle;
		Params.NewClass = MetaHumanComponentUEFNClass;
		Params.BlueprintContext = Blueprint;

		FText FailReason;
		FSubobjectDataHandle NewHandle = SubobjectDataSubsystem->AddNewSubobject(Params, FailReason);

		if (!NewHandle.IsValid() && !FailReason.IsEmptyOrWhitespace())
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("FailToAddMetaHumanUEFNComponent", "Failed to add MetaHuman Component for UEFN"))
				->AddToken(FTextToken::Create(FailReason));
		}
		else
		{
			// Copy all the properties to the new object
			const UMetaHumanComponentBase* NewMetaHumanComponent = NewHandle.GetData()->GetObject<UMetaHumanComponentBase>();
			for (TFieldIterator<FProperty> PropertyIt(UMetaHumanComponentBase::StaticClass()); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				Property->CopyCompleteValue_InContainer((void*)NewMetaHumanComponent, MetaHumanComponentBase);
			}

			BodyLODThresholdProperty->SetValue_InContainer((void*)NewMetaHumanComponent, BodyLODThreshold);
		}
	}
}

UMetaHumanDefaultEditorPipelineUEFN::UMetaHumanDefaultEditorPipelineUEFN()
{
}

bool UMetaHumanDefaultEditorPipelineUEFN::PreBuildCollection(TNotNull<UMetaHumanCollection*> InCollection, const FString& InCharacterName)
{
	if (Super::PreBuildCollection(InCollection, InCharacterName))
	{
		// Load the UEFN plugin so we can export the assets

		if (UefnProjectFilePath.FilePath.IsEmpty())
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("UEFNExportFailure_NoCollection", "No UEFN project file set for the UEFN export pipeline."));
			return false;
		}

		FString TargetUEFNPluginFilename;
		FText LoadFailReason;
		if (UE::MetaHuman::Private::GetTargetUEFNRootPluginForProject(UefnProjectFilePath.FilePath, TargetUEFNPluginFilename, LoadFailReason))
		{
			FPluginUtils::FLoadPluginParams LoadParams =
			{
				.bSynchronousAssetsScan = true,
				.OutFailReason = &LoadFailReason
			};

			UEFNPlugin = FPluginUtils::LoadPlugin(TargetUEFNPluginFilename, LoadParams);
		}

		if (!UEFNPlugin.IsValid())
		{
			FMessageLog(UE::MetaHuman::MessageLogName)
				.Error(LOCTEXT("UEFNExportFailure_ErrorLoadingPlugin", "Failed to load UEFN plugin."))
				->AddText(FText::FromString(TargetUEFNPluginFilename))
				->AddText(LoadFailReason);

			return false;
		}

		InCollection->UnpackPathMode = EMetaHumanCharacterUnpackPathMode::Absolute;
		InCollection->UnpackFolderPath = UEFNPlugin->GetMountedAssetPath() / TEXT("MetaHumans") / InCharacterName;
	}

	return true;
}

void UMetaHumanDefaultEditorPipelineUEFN::UnpackCollectionAssets(
	TNotNull<UMetaHumanCollection*> InCharacterPalette, 
	FMetaHumanCollectionBuiltData& InCollectionBuiltData,
	const FOnUnpackComplete& InOnComplete) const
{
	ON_SCOPE_EXIT
	{
		if (UEFNPlugin.IsValid())
		{
			// TODO: Disables the verification for assets that are still in memory when unloading a plugin
			// This is a hack to prevent an engine crash until this issue is resolved. 
			// For some reason, the texture graph instances are keeping references to the assets in the UEFN plugin
			IConsoleVariable* CVarVerifyUnload = IConsoleManager::Get().FindConsoleVariable(TEXT("PluginManager.VerifyUnload"));
			check(CVarVerifyUnload);

			const bool bPreviousValue = CVarVerifyUnload->GetBool();

			CVarVerifyUnload->Set(false);

			FText UnloadFailReason;
			if (!FPluginUtils::UnloadPlugin(UEFNPlugin.ToSharedRef(), &UnloadFailReason))
			{
				FMessageLog(UE::MetaHuman::MessageLogName)
					.Error(LOCTEXT("UEFNExportFailure_Unload", "Faled to unload UEFN project"))
					->AddText(UnloadFailReason);
			}

			CVarVerifyUnload->Set(bPreviousValue);

			UEFNPlugin.Reset();
		}
	};

	// Override the common dependencies path for UEFN export
	const bool bWithoutSlashes = false;
	const FString UnpackFolder = InCharacterPalette->GetUnpackFolder();
	MountingPoint = FPackageName::GetPackageMountPoint(UnpackFolder, bWithoutSlashes).ToString();

	// WriteActorBlueprint() will also be called during this as it triggers the OnComplete delegate
	// See FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter()
	Super::UnpackCollectionAssets(InCharacterPalette, InCollectionBuiltData, InOnComplete);
}

UBlueprint* UMetaHumanDefaultEditorPipelineUEFN::WriteActorBlueprint(const FString& InBlueprintPath) const
{
	UBlueprint* Blueprint = Super::WriteActorBlueprint(InBlueprintPath);

	USubobjectDataSubsystem* SubobjectDataSubsystem = USubobjectDataSubsystem::Get();

	TArray<FSubobjectDataHandle> SubobjectHandles;
	SubobjectDataSubsystem->GatherSubobjectData(Blueprint->GeneratedClass->GetDefaultObject(), SubobjectHandles);

	// Search for the UMetaHumanComponentUE handle
	const FSubobjectDataHandle* FoundHandle = SubobjectHandles.FindByPredicate(
		[](const FSubobjectDataHandle& CandidateHandle)
		{
			if (FSubobjectData* SubobjectData = CandidateHandle.GetData())
			{
				if (const UActorComponent* TemplateComponent = SubobjectData->GetComponentTemplate())
				{
					return TemplateComponent->IsA<UMetaHumanComponentUE>();
				}
			}

			return false;
		}
	);

	if (FoundHandle != nullptr)
	{
		// If there a UE MetaHuman Component then it needs to be replaced by the UEFN MH Component

		FSubobjectDataHandle MetaHumanComponentUEHandle = *FoundHandle;
		if (MetaHumanComponentUEHandle.IsValid())
		{
			// Reference to the MetaHuman Component object to copy properties from
			if (const UMetaHumanComponentBase* MetaHumanComponentBase = MetaHumanComponentUEHandle.GetData()->GetObject<UMetaHumanComponentBase>())
			{
				// Replace the MetaHuman Component UE with the UEFN version

				// First remove the component from the blueprint
				const int32 NumObjectsRemoved = SubobjectDataSubsystem->DeleteSubobject(SubobjectHandles[0], MetaHumanComponentUEHandle, Blueprint);
				check(NumObjectsRemoved == 1);

				UE::MetaHuman::Private::AddUEFNMetaHumanComponent(MetaHumanComponentBase, SubobjectHandles[0], Blueprint, BodyLODThreshold);
			}
		}

		// Remove all graphs from the blueprint to prevent validation issues in UEFN
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		FBlueprintEditorUtils::RemoveGraphs(Blueprint, Graphs);

		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	}
	else
	{
		// No UE MetaHuman Component, search if the BP contains UEFN MetaHuman Component
		const FSubobjectDataHandle* FoundMHComponentUEFNHandle = SubobjectHandles.FindByPredicate(
			[](const FSubobjectDataHandle& CandidateHandle)
			{
				if (FSubobjectData* SubobjectData = CandidateHandle.GetData())
				{
					if (const UActorComponent* TemplateComponent = SubobjectData->GetComponentTemplate())
					{
						return TemplateComponent->GetClass()->GetFName() == TEXT("MetaHumanComponent");
					}
				}

				return false;
			}
		);

		if (!FoundMHComponentUEFNHandle)
		{
			// Add the UEFN MH Component if not present, setting its properties from the UE MH Component of the input BP (i.e. default values for this pipeline)
			
			// Get the base component from the BP
			const FString BlueprintShortName = FPackageName::GetShortName(InBlueprintPath);
			UBlueprint* SourceBlueprint = Cast<UBlueprint>(TemplateClass->ClassGeneratedBy);

			TArray<FSubobjectDataHandle> SourceBPSubobjectHandles;
			SubobjectDataSubsystem->GatherSubobjectData(SourceBlueprint->GeneratedClass->GetDefaultObject(), SourceBPSubobjectHandles);

			// Search for the UMetaHumanComponentUE handle in the source BP
			FoundHandle = SourceBPSubobjectHandles.FindByPredicate(
				[](const FSubobjectDataHandle& CandidateHandle)
				{
					if (FSubobjectData* SubobjectData = CandidateHandle.GetData())
					{
						if (const UActorComponent* TemplateComponent = SubobjectData->GetComponentTemplate())
						{
							return TemplateComponent->IsA<UMetaHumanComponentUE>();
						}
					}

					return false;
				}
			);

			if (FoundHandle != nullptr)
			{
				FSubobjectDataHandle MetaHumanComponentUEHandle = *FoundHandle;
				if (MetaHumanComponentUEHandle.IsValid())
				{
					if (const UMetaHumanComponentBase* MetaHumanComponentBase = MetaHumanComponentUEHandle.GetData()->GetObject<UMetaHumanComponentBase>())
					{
						UE::MetaHuman::Private::AddUEFNMetaHumanComponent(MetaHumanComponentBase, SubobjectHandles[0], Blueprint, BodyLODThreshold);
					}
				}

				// Remove all graphs from the blueprint to prevent validation issues in UEFN
				TArray<UEdGraph*> Graphs;
				Blueprint->GetAllGraphs(Graphs);
				FBlueprintEditorUtils::RemoveGraphs(Blueprint, Graphs);

				FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
			}
		}
	}

	return Blueprint;
}

bool UMetaHumanDefaultEditorPipelineUEFN::UpdateActorBlueprint(const UMetaHumanCharacterInstance* InCharacterInstance, UBlueprint* InBlueprint) const
{
	if (!InCharacterInstance)
	{
		return false;
	}

	// Keep track of any objects in the assembly output that have been duplicated to the target UEFN project 
	// This is typically for user created Skeletal Mesh assets assigned as clothing; Grooms & Outfit pipelines always generate new assets to unpack
	TMap<UObject*, UObject*> DuplicatedRootObjects;

	// Collect the dependencies of all objects in the assembly output and save both the assembly output and its dependencies to the UEFN project
	// See FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter() for the UE reference implementation
	{
		// Garbage collection may run while duplicating dependencies (when duplicating blueprints for example),
		// so prevent assets there were already generated from being GC'ed
		FGCScopeGuard GCGuard;

		TArray<UObject*> RootObjects;
		const FInstancedStruct& AssemblyOutput = InCharacterInstance->GetAssemblyOutput();
		FMetaHumanCharacterEditorBuild::CollectUObjectReferencesFromStruct(AssemblyOutput.GetScriptStruct(), AssemblyOutput.GetMemory(), RootObjects);

		UnpackCommonDependencies(RootObjects, InCharacterInstance->GetMetaHumanCollection(), DuplicatedRootObjects);
	}

	// The BP update only replaces duplicated skeletal meshes, so check if there are any non Skeletal Meshes in the duplicated objects
	// TODO: support more types of assets 
	for (const TPair<UObject*, UObject*>& Pair : DuplicatedRootObjects)
	{
		if (Pair.Key == nullptr || !Pair.Key->IsA<USkeletalMesh>())
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Warning(LOCTEXT("UEFNExportWarning_InvalidDuplicateRootObject", 
				"Assembly output contains user generated asset that is not of a Skeletal Mesh type and may fail exporting to the UEFN project."))
				->AddToken(FUObjectToken::Create(Pair.Key));
		}
	}

	if (!Super::UpdateActorBlueprint(InCharacterInstance, InBlueprint))
	{
		return false;
	}

	USubobjectDataSubsystem* SubobjectDataSubsystem = USubobjectDataSubsystem::Get();

	TArray<FSubobjectDataHandle> SubobjectDataHandles;
	SubobjectDataSubsystem->GatherSubobjectData(InBlueprint->GeneratedClass->GetDefaultObject(), SubobjectDataHandles);
	FSubobjectDataHandle RootHandle = SubobjectDataHandles[0];

	// Get rid of duplicate data handle objects
	SubobjectDataHandles = TSet(SubobjectDataHandles).Array();

	TArray<TPair<FSubobjectDataHandle, USkeletalMeshComponent*>> SkelMeshClothingHandles;
	TArray<TPair<FSubobjectDataHandle, USkeletalMeshComponent*>> LegacySkelMeshComponentHandles;
	FSubobjectDataHandle BodyHandle = FSubobjectDataHandle::InvalidHandle;

	const TSet<FString> LegacySkelMeshComponentNames = 
	{
		TEXT("Torso"),
		TEXT("Legs"),
		TEXT("Feet")
	};

	TSet<FString> MissingLegacySkelMeshComponentNames = LegacySkelMeshComponentNames;

	for (const FSubobjectDataHandle& Handle : SubobjectDataHandles)
	{
		if (USkeletalMeshComponent* SkelMeshComponent = const_cast<USkeletalMeshComponent*>(Handle.GetData()->GetObjectForBlueprint<USkeletalMeshComponent>(InBlueprint)))
		{
			FString ComponentName = SkelMeshComponent->GetName();
			ComponentName.RemoveFromEnd(UActorComponent::ComponentTemplateNameSuffix);

			if (USkeletalMesh* SkeletalMesh = SkelMeshComponent->GetSkeletalMeshAsset())
			{
				// Replace with the duplicated object if needed
				if (DuplicatedRootObjects.Contains(SkeletalMesh))
				{
					USkeletalMesh* OrigSkeletalMesh = SkeletalMesh;
					SkeletalMesh = Cast<USkeletalMesh>(DuplicatedRootObjects[SkeletalMesh]);
					SkelMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);
				}

				// Check if the referenced skel mesh is mounted to the UEFN project
				// NOTE: this check should never fail now that non mounted skel meshes are duplicated but leaving it as a fallback
				if (!(SkeletalMesh->GetPackage() && FPackageName::GetPackageMountPoint(SkeletalMesh->GetPackage()->GetName(), /*InWithoutSlashes =*/ false).ToString() == MountingPoint))
				{
					// Invalid skeletal mesh package
					FMessageLog(UE::MetaHuman::MessageLogName).Warning(LOCTEXT("UEFNExportWarning_InvalidSkeletalMesh", "Skeletal Mesh was not mounted to the UEFN project; all references to it will be cleared in the exported assets."))
						->AddToken(FUObjectToken::Create(SkeletalMesh));
					SkelMeshComponent->SetSkeletalMeshAsset(nullptr);
				}
			}

			if (ComponentName == TEXT("Face"))
			{
				continue;
			}
			else if (ComponentName == TEXT("Body"))
			{
				BodyHandle = Handle;
			}
			else if (LegacySkelMeshComponentNames.Contains(ComponentName))
			{
				LegacySkelMeshComponentHandles.Add({ Handle, SkelMeshComponent });
				MissingLegacySkelMeshComponentNames.Remove(ComponentName);
			}
			else
			{
				// Legacy pipeline will attach a number of skel mesh components to the blueprint
				SkelMeshClothingHandles.Add({ Handle, SkelMeshComponent });

				// Re-save the asset since it may have been modified by the BP update
				if (USkeletalMesh* SkeletalMesh = SkelMeshComponent->GetSkeletalMeshAsset())
				{
					FSkinnedAssetCompilingManager::Get().FinishCompilation({ SkeletalMesh });
					UE::MetaHuman::Private::SavePackageDirect(SkeletalMesh);
				}
			}
		}
	}

	// Add missing legacy skel mesh components
	if (BodyHandle.IsValid())
	{
		int32 SkelMeshClothingHandleIndex = 0;
		for (const FString& ComponentName : MissingLegacySkelMeshComponentNames)
		{
			if (SkelMeshClothingHandleIndex < SkelMeshClothingHandles.Num())
			{
				// Assign clothing skel meshes to the legacy UEFN named components in order
				// NOTE: the assumption here is that any skel mesh asset was assigned in the same order of the legacy named components
				SubobjectDataSubsystem->RenameSubobject(SkelMeshClothingHandles[SkelMeshClothingHandleIndex].Key, FText::FromString(ComponentName));
				++SkelMeshClothingHandleIndex;
			}
			else
			{
				FAddNewSubobjectParams Params;
				Params.ParentHandle = BodyHandle;
				Params.NewClass = USkeletalMeshComponent::StaticClass();
				Params.bConformTransformToParent = true;
				Params.BlueprintContext = InBlueprint;
				Params.bSkipMarkBlueprintModified = true;

				FText OutFailText;
				FSubobjectDataHandle NewComponentHandle = SubobjectDataSubsystem->AddNewSubobject(Params, OutFailText);

				if (NewComponentHandle.IsValid())
				{
					SubobjectDataSubsystem->RenameSubobject(NewComponentHandle, FText::FromString(ComponentName));

					if (USkeletalMeshComponent* SkelMeshComponent = const_cast<USkeletalMeshComponent*>(NewComponentHandle.GetData()->GetObjectForBlueprint<USkeletalMeshComponent>(InBlueprint)))
					{
						LegacySkelMeshComponentHandles.Add({ NewComponentHandle, SkelMeshComponent });
					}
				}
			}
		}
	}

	FKismetEditorUtilities::CompileBlueprint(InBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	InBlueprint->MarkPackageDirty();

	UE::MetaHuman::Private::SavePackageDirect(InBlueprint);

	return true;
}

TNotNull<USkeleton*> UMetaHumanDefaultEditorPipelineUEFN::GenerateSkeleton(FMetaHumanCharacterGeneratedAssets& InGeneratedAssets,
																		   TNotNull<USkeleton*> InBaseSkeleton,
																		   const FString& InTargetFolderName,
																		   TNotNull<UObject*> InOuterForGeneratedAssets) const
{
	if (IsPluginAsset(InBaseSkeleton))
	{
		// Same logic as default base pipeline, skeleton will be unpacked in common folder
		return Super::GenerateSkeleton(InGeneratedAssets, InBaseSkeleton, InTargetFolderName, InOuterForGeneratedAssets);
	}
	else
	{
		// Custom logic for UEFN, keep project folder structure when unpacking
		FString RelativePath;
		FPackageName::SplitPackageNameRoot(*InBaseSkeleton->GetPackage()->GetName(), &RelativePath);

		USkeleton* BaseSkeleton = InBaseSkeleton;
		USkeleton* NewSkeleton = DuplicateObject<USkeleton>(BaseSkeleton, InOuterForGeneratedAssets);

		const bool bIsAbsolutePath = true;
		InGeneratedAssets.Metadata.Emplace(NewSkeleton, MountingPoint / RelativePath, FaceSkeleton->GetName(), bIsAbsolutePath);

		return NewSkeleton;
	}
}

void UMetaHumanDefaultEditorPipelineUEFN::OnCommonDependenciesUnpacked(const TMap<UObject*, UObject*>& InDuplicatedDependencies) const
{
	FScopedSlowTask SavingPackagesTask(InDuplicatedDependencies.Num(), LOCTEXT("SavingCommonAssetsTask", "Saving Common Assets"));
	SavingPackagesTask.MakeDialog();

	for (const TPair<UObject*, UObject*>& It : InDuplicatedDependencies)
	{
		SavingPackagesTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("SavingCommonAsset", "Saving Common Asset '{0}'"), FText::FromName(GetFNameSafe(It.Value))));

		if (UObject* TargetObject = It.Value)
		{
			UE::MetaHuman::Private::SaveObjectForUEFNProject(TargetObject);
		}
	}
}

void UMetaHumanDefaultEditorPipelineUEFN::UnpackCommonDependencies(TArray<UObject*> InRootObjects, TNotNull<const UMetaHumanCollection*> InCollection, TMap<UObject*, UObject*>& OutDuplicatedRootObjects) const
{
	// Similar implementation to FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter

	// Build a list of dependencies to check
	TSet<UObject*> AllAssetDependencies;
	FMetaHumanCharacterEditorBuild::CollectDependencies(InRootObjects, { MountingPoint.RightChop(1).LeftChop(1) }, AllAssetDependencies);


	const FString AnimPresetAssetPath = TEXT("/MetaHumanCharacter/Optional/Animation/UEFNAnimPreset/AnimPreset_MetaHumanLocomotion.AnimPreset_MetaHumanLocomotion");
	UBlueprint* AnimPreset = LoadObject<UBlueprint>(nullptr, *AnimPresetAssetPath);
	check(AnimPreset);

	// Gather the anim sequences from the anim preset by first collecting the asset dependencies and then filtering them for animations.
	TSet<UObject*> PresetSequences;
	FMetaHumanCharacterEditorBuild::CollectDependencies({ AnimPreset }, { MountingPoint.RightChop(1).LeftChop(1) }, PresetSequences);

	for (auto It = PresetSequences.CreateIterator(); It; ++It)
	{
		UObject* Dependency = *It;

		if (!Dependency->IsA<UAnimSequence>())
		{
			It.RemoveCurrent();
		}
	}

	// Add all animations as well as their dependencies.
	AllAssetDependencies.Append(PresetSequences);
	AllAssetDependencies.Add(AnimPreset);

	FMetaHumanCharacterEditorBuild::CollectDependencies(PresetSequences.Array(), { MountingPoint.RightChop(1).LeftChop(1) }, AllAssetDependencies);

	TSet<UObject*> PluginDependencies;
	TSet<UObject*> UnpackedDependencies;

	// Select the packages of the objects that are in the plugin content or in the user's project
	Algo::CopyIf(AllAssetDependencies, PluginDependencies,
		[](const UObject* Obj) -> bool
		{
			const FName PackageRoot = FPackageName::GetPackageMountPoint(Obj->GetPackage()->GetName());
			return PackageRoot == UE_PLUGIN_NAME
					|| PackageRoot == TEXT("Game");
		});

	// Add any root objects that are not in the unpack folder, typically user generated, so that they are duplicated
	Algo::CopyIf(InRootObjects, PluginDependencies,
		[](const UObject* Obj) -> bool
		{
			const FName PackageRoot = FPackageName::GetPackageMountPoint(Obj->GetPackage()->GetName());
			return PackageRoot == TEXT("Game");
		});

	// Select the packages of the objects that are in the plugin content
	// The following is based on the assumption that unpacked assets were create in the project and do not reference any non-assembled assets
	const FString UnpackFolder = InCollection->GetUnpackFolder();
	Algo::CopyIf(AllAssetDependencies, UnpackedDependencies,
		[UnpackFolder](const UObject* Obj) -> bool
		{
			const FString PackageName = Obj->GetPackage()->GetName();
			return PackageName.StartsWith(UnpackFolder);
		});

	// Add the root objects to get the full array of everything unpacked by the assembly
	Algo::CopyIf(InRootObjects, UnpackedDependencies,
		[UnpackFolder](const UObject* Obj) -> bool
		{
			const FString PackageName = Obj->GetPackage()->GetName();
			return PackageName.StartsWith(UnpackFolder);
		});

	// Get the common dependencies path for UEFN export
	const FString CommonFolderPath = MountingPoint / TEXT("MetaHumans/Common");

	UnpackedDependencies.Remove(nullptr);

	TMap<UObject*, UObject*> DuplicatedDependencies;
	FMetaHumanCharacterEditorBuild::DuplicateDepedenciesToNewRoot(PluginDependencies, CommonFolderPath, UnpackedDependencies, DuplicatedDependencies,
		[](const UObject* InObject) -> bool
		{
			// AnimBlueprints are not supported in UEFN
			if (InObject->IsA<UAnimBlueprint>())
			{
				return false;
			}

			return true;
		});

	// If the anim preset was duplicated, replace the blueprint's parent class
	// with the AnimPreset_BipedLocomotion that is in UEFN
	if (UObject** DuplicatedAnimPresetObject = DuplicatedDependencies.Find(AnimPreset))
	{
		if (UBlueprint* DuplicatedAnimPreset = Cast<UBlueprint>(*DuplicatedAnimPresetObject))
		{
			UPackage* AnimPresetBipedLocomotionPackage = CreatePackage(TEXT("/AnimPresets/AnimPreset_BipedLocomotion"));

			// Duplicate the parent class blueprint to the new package so it can replace the existing parent
			UBlueprint* ParentBlueprint = CastChecked<UBlueprint>(DuplicatedAnimPreset->ParentClass->ClassGeneratedBy);
			UBlueprint* DuplicatedParentBlueprint = DuplicateObject(ParentBlueprint, AnimPresetBipedLocomotionPackage);

			DuplicatedAnimPreset->ParentClass = DuplicatedParentBlueprint->GeneratedClass;

			// Compile the blueprint to register the changes before saving
			FKismetEditorUtilities::CompileBlueprint(DuplicatedAnimPreset, EBlueprintCompileOptions::SkipGarbageCollection);
		}
	}

	// Update the output root objects that have been duplicated
	for (UObject* RootObject : InRootObjects)
	{
		if (UObject** DuplicatedRootPtr = DuplicatedDependencies.Find(RootObject))
		{
			if (*DuplicatedRootPtr)
			{
				OutDuplicatedRootObjects.Add(RootObject, *DuplicatedRootPtr);
			}
		}
	}

	// Notify the pipeline that the common dependencies have been unpacked for further processing
	OnCommonDependenciesUnpacked(DuplicatedDependencies);

	// Save all the unpacked assets too
	for (UObject* Object : UnpackedDependencies)
	{
		if (Object)
		{
			UE::MetaHuman::Private::SaveObjectForUEFNProject(Object);
		}
	}
}

#undef LOCTEXT_NAMESPACE