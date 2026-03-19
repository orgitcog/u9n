// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidatorSubsystem.h"

#include "Algo/AnyOf.h"
#include "Algo/RemoveIf.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetDataToken.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "DataValidationChangelist.h"
#include "EditorValidatorHelpers.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "EditorClassUtils.h"
#include "EditorUtilityBlueprint.h"
#include "EditorValidatorBase.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "IDirectoryWatcher.h"
#include "ISourceControlModule.h"
#include "LoadExternalObjectsForValidation.h"
#include "Logging/MessageLog.h"
#include "Misc/App.h"
#include "Misc/DataValidation.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "UObject/ICookInfo.h"
#include "UObject/TopLevelAssetPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidatorSubsystem)

#define LOCTEXT_NAMESPACE "EditorValidationSubsystem"

DEFINE_LOG_CATEGORY(LogContentValidation);

FValidateAssetsSettings::FValidateAssetsSettings()
	: ShowMessageLogSeverity(EMessageSeverity::Warning)
	, MessageLogName(UE::DataValidation::MessageLogName)
	, MessageLogPageTitle(LOCTEXT("DataValidation.MessagePageTitle", "Data Validation"))
{
}

FValidateAssetsSettings::~FValidateAssetsSettings()
{
}

UDataValidationSettings::UDataValidationSettings()
{

}

FScopedDisableValidateOnSave::FScopedDisableValidateOnSave()
	: EditorValidationSubsystem(GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr)
{
	if (EditorValidationSubsystem)
	{
		EditorValidationSubsystem->PushDisableValidateOnSave();
	}
}

FScopedDisableValidateOnSave::~FScopedDisableValidateOnSave()
{
	if (EditorValidationSubsystem)
	{
		EditorValidationSubsystem->PopDisableValidateOnSave();
	}
}

UEditorValidatorSubsystem::UEditorValidatorSubsystem()
	: UEditorSubsystem()
{
	bAllowBlueprintValidators = true;
}

bool UEditorValidatorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (GetClass() == UEditorValidatorSubsystem::StaticClass())
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(UEditorValidatorSubsystem::StaticClass(), ChildClasses, true);
		for (UClass* Child : ChildClasses)
		{
			if (Child->GetDefaultObject<UEditorSubsystem>()->ShouldCreateSubsystem(Outer))
			{
				// Do not create this class because one of our child classes wants to be created
				return false;
			}
		}
	}
	return true;
}

void UEditorValidatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// C++ registration
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UEditorValidatorSubsystem::RegisterNativeValidators);

	// Blueprint registration
	if (IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		AssetRegistry.IsLoadingAssets())
	{
		// We are still discovering assets, listen for the completion delegate before building the validator list
		AssetRegistry.OnFilesLoaded().AddUObject(this, &UEditorValidatorSubsystem::RegisterBlueprintValidators);
	}
	else
	{
		RegisterBlueprintValidators();
	}

	// Register to SCC pre-submit callback
	ISourceControlModule::Get().RegisterPreSubmitDataValidation(FSourceControlPreSubmitDataValidationDelegate::CreateUObject(this, &UEditorValidatorSubsystem::ValidateChangelistPreSubmit));
}

bool UEditorValidatorSubsystem::ShouldValidateAsset(
	const FAssetData& 				Asset,
	const FValidateAssetsSettings& 	Settings,
	FDataValidationContext& 		InContext) const
{
	if (Asset.HasAnyPackageFlags(PKG_Cooked))
	{
		return false;
	}

	FNameBuilder AssetPackageNameBuilder(Asset.PackageName);
	FStringView AssetPackageNameView = AssetPackageNameBuilder.ToView();

	if (FPackageName::IsTempPackage(AssetPackageNameView))
	{
		return false;
	}

	if (FPackageName::IsVersePackage(AssetPackageNameView))
	{
		return false;
	}

	if (Settings.bSkipExcludedDirectories && IsPathExcludedFromValidation(AssetPackageNameView))
	{
		return false;
	}

	return true;
}

void UEditorValidatorSubsystem::RegisterNativeValidators()
{
	ensureAlwaysMsgf(!bHasRegisteredNativeValidators, TEXT("Native validators have already been registered!"));
	if (bHasRegisteredNativeValidators)
	{
		return;
	}

	TArray<UClass*> ValidatorClasses;
	GetDerivedClasses(UEditorValidatorBase::StaticClass(), ValidatorClasses);

	for (UClass* ValidatorClass : ValidatorClasses)
	{
		// GetDerivedClasses may include a mix of C++ and loaded BP classes
		// Skip any non-C++ classes, as well as anything that has already been registered by this point
		if (!ValidatorClass->HasAllClassFlags(CLASS_Abstract) && !Validators.Contains(ValidatorClass->GetClassPathName()) 
			&& FPackageName::IsScriptPackage(FNameBuilder(ValidatorClass->GetPackage()->GetFName())))
		{
			UEditorValidatorBase* Validator = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass);
			AddValidator(Validator);
		}
	}

	// Watch for native modules being added/removed
	FModuleManager::Get().OnModulesChanged().AddUObject(this, &UEditorValidatorSubsystem::OnNativeModulesChanged);

	bHasRegisteredNativeValidators = true;
}

void UEditorValidatorSubsystem::RegisterBlueprintValidators()
{
	ensureAlwaysMsgf(!bHasRegisteredBlueprintValidators, TEXT("Blueprint validators have already been registered!"));
	if (bHasRegisteredBlueprintValidators)
	{
		return;
	}

	if (bAllowBlueprintValidators)
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

		// Locate all Blueprint-based validator classes (include unloaded)
		TSet<FTopLevelAssetPath> ValidatorClasses;
		AssetRegistry.GetDerivedClassNames({ UEditorValidatorBase::StaticClass()->GetClassPathName() }, {}, ValidatorClasses);

		for (const FTopLevelAssetPath& ValidatorClass : ValidatorClasses)
		{
			// IAssetRegistry::GetDerivedClassNames may include a mix of C++ and BP classes
			// Skip any C++ classes, as well as anything that has already been registered by this point
			if (!Validators.Contains(ValidatorClass) && !FPackageName::IsScriptPackage(FNameBuilder(ValidatorClass.GetPackageName())))
			{
				AddValidator(ValidatorClass);
			}
		}

		// Watch for BPs being added/removed
		// Recompilation is handled by the standard reinstancing logic, as Validators is a UPROPERTY
		AssetRegistry.OnAssetsAdded().AddUObject(this, &UEditorValidatorSubsystem::OnAssetsAdded);
		AssetRegistry.OnAssetsRemoved().AddUObject(this, &UEditorValidatorSubsystem::OnAssetsRemoved);
	}

	bHasRegisteredBlueprintValidators = true;
}

void UEditorValidatorSubsystem::Deinitialize()
{
	CleanupValidators();

	// Unregister to SCC pre-submit callback
	ISourceControlModule::Get().UnregisterPreSubmitDataValidation();

	Super::Deinitialize();
}

void UEditorValidatorSubsystem::PushDisableValidateOnSave()
{
	checkf(DisableValidateOnSaveCount < std::numeric_limits<decltype(DisableValidateOnSaveCount)>::max(), TEXT("PushDisableValidateOnSave overflow!"));
	++DisableValidateOnSaveCount;
}

void UEditorValidatorSubsystem::PopDisableValidateOnSave()
{
	checkf(DisableValidateOnSaveCount > 0, TEXT("PopDisableValidateOnSave underflow!"));
	--DisableValidateOnSaveCount;
}

bool UEditorValidatorSubsystem::ShouldValidateOnSave(bool bProceduralSave) const
{
	// Skip if not enabled
	if (DisableValidateOnSaveCount > 0 || !GetDefault<UDataValidationSettings>()->bValidateOnSave)
	{
		return false;
	}

	// Skip auto and procedural saves
	// For performance reasons, don't validate when making a procedural save by default. Assumption is we validated when saving previously. 
	if (bProceduralSave || GEditor->IsAutosaving(EPackageAutoSaveType::Any))
	{
		return false;
	}

	return true;
}

void UEditorValidatorSubsystem::AddValidator(UEditorValidatorBase* InValidator)
{
	if (InValidator)
	{
		const FTopLevelAssetPath ValidatorClass = InValidator->GetClass()->GetClassPathName();
		Validators.Add(ValidatorClass, InValidator);
		ValidatorClassesPendingLoad.Remove(ValidatorClass);
	}
}

void UEditorValidatorSubsystem::AddValidator(const FTopLevelAssetPath& InValidatorClass)
{
	if (InValidatorClass.IsValid())
	{
		Validators.Add(InValidatorClass, nullptr);
		ValidatorClassesPendingLoad.Add(InValidatorClass);
	}
}

void UEditorValidatorSubsystem::RemoveValidator(UEditorValidatorBase* InValidator)
{
	if (InValidator)
	{
		RemoveValidator(InValidator->GetClass()->GetClassPathName());
	}
}

void UEditorValidatorSubsystem::RemoveValidator(const FTopLevelAssetPath& InValidatorClass)
{
	if (InValidatorClass.IsValid())
	{
		Validators.Remove(InValidatorClass);
		ValidatorClassesPendingLoad.Remove(InValidatorClass);
	}
}

void UEditorValidatorSubsystem::CleanupValidators()
{
	Validators.Empty();
	ValidatorClassesPendingLoad.Empty();
	NativeModulesPendingLoad.Empty();
	NativeModulesPendingUnload.Empty();
}

void UEditorValidatorSubsystem::ForEachEnabledValidator(TFunctionRef<bool(UEditorValidatorBase* Validator)> Callback) const
{
	UpdateValidators();

	for (const TPair<FTopLevelAssetPath, TObjectPtr<UEditorValidatorBase>>& ValidatorPair : Validators)
	{
		if (UEditorValidatorBase* Validator = ValidatorPair.Value;
			Validator && Validator->IsEnabled())
		{
			if (!Callback(Validator))
			{
				break;
			}
		}
	}
}

EDataValidationResult UEditorValidatorSubsystem::IsObjectValid(
	UObject* InObject,
	TArray<FText>& ValidationErrors,
	TArray<FText>& ValidationWarnings,
	const EDataValidationUsecase InValidationUsecase) const
{
	FDataValidationContext Context(false, InValidationUsecase, {}); // No associated objects in this context
	EDataValidationResult Result = IsObjectValidWithContext(InObject, Context);
	Context.SplitIssues(ValidationWarnings, ValidationErrors);
	return Result;
}

EDataValidationResult UEditorValidatorSubsystem::IsAssetValid(
	const FAssetData& AssetData,
	TArray<FText>& ValidationErrors,
	TArray<FText>& ValidationWarnings,
	const EDataValidationUsecase InValidationUsecase) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset({ ULevel::LoadAllExternalObjectsTag });
		if (Obj)
		{
			FDataValidationContext Context(false, InValidationUsecase, {}); // No associated objects in this context
			EDataValidationResult Result = ValidateObjectInternal(AssetData, Obj, Context);
			Context.SplitIssues(ValidationWarnings, ValidationErrors);
			return Result;
		}
		return EDataValidationResult::NotValidated;
	}

	return EDataValidationResult::Invalid;
}

EDataValidationResult UEditorValidatorSubsystem::IsObjectValidWithContext(
	UObject* InObject,
	FDataValidationContext& InContext) const
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	
	if (ensure(InObject))
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(InObject), true);
		if (!AssetData.IsValid())
		{
			// Construct dynamically with potentially fewer tags 
			AssetData = FAssetData(InObject);
		}
		
		return ValidateObjectInternal(AssetData, InObject, InContext);
	}

	return Result;
}

EDataValidationResult UEditorValidatorSubsystem::IsAssetValidWithContext(
	const FAssetData& AssetData,
	FDataValidationContext& InContext) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset({ ULevel::LoadAllExternalObjectsTag });
		if (Obj)
		{
			return ValidateObjectInternal(AssetData, Obj, InContext);
		}
		return EDataValidationResult::NotValidated;
	}

	return EDataValidationResult::Invalid;
}

EDataValidationResult UEditorValidatorSubsystem::ValidateObjectInternal(
	const FAssetData& InAssetData,
	UObject* InObject,
	FDataValidationContext& InContext) const
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	
	if (ensure(InObject) && ensure(InAssetData.IsValid()))
	{
		// First check the class level validation
		Result = const_cast<const UObject*>(InObject)->IsDataValid(InContext);

		// If the asset is still valid or there wasn't a class-level validation, keep validating with custom validators
		if (Result == EDataValidationResult::Invalid)
		{
			return Result;
		}
		
		ForEachEnabledValidator([InObject, &InAssetData, &InContext, &Result](UEditorValidatorBase* Validator)
		{
			UE_LOG(LogContentValidation, Verbose, TEXT("Validating '%s' with '%s'..."), *InObject->GetPathName(), *Validator->GetClass()->GetName());
			GInitRunaway(); // Reset runaway counter, as ValidateLoadedAsset may be implemented in a BP and could overflow the runaway count due to being called in a loop
			EDataValidationResult NewResult = Validator->ValidateLoadedAsset(InAssetData, InObject, InContext);
			Result = CombineDataValidationResults(Result, NewResult);
			return true;
		});
	}

	return Result;
}

int32 UEditorValidatorSubsystem::ValidateAssetsWithSettings(
	const TArray<FAssetData>& AssetDataList,
	const FValidateAssetsSettings& InSettings,
	FValidateAssetsResults& OutResults) const
{
	FMessageLog DataValidationLog(InSettings.MessageLogName);
	DataValidationLog.SetCurrentPage(InSettings.MessageLogPageTitle);
	ValidateAssetsInternal(DataValidationLog, TSet<FAssetData>{AssetDataList}, InSettings, OutResults);
	
	if (const EMessageSeverity::Type* Severity = InSettings.ShowMessageLogSeverity.GetPtrOrNull())
	{
		DataValidationLog.Open(*Severity, false);
	}

	return OutResults.NumWarnings + OutResults.NumInvalid;
}

EDataValidationResult UEditorValidatorSubsystem::ValidateAssetsInternal(
	FMessageLog& 					DataValidationLog,
	TSet<FAssetData>				AssetDataList,
	const FValidateAssetsSettings& 	InSettings,
	FValidateAssetsResults& 		OutResults) const
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	// The number of assets to validate may decrease from merging when dealing with external objects but it shouldn't increase
	FScopedSlowTask SlowTask(AssetDataList.Num(), LOCTEXT("DataValidation.ValidateAssetsTask", "Validating Assets"));
	if (!InSettings.bSilent)
	{
		SlowTask.MakeDialog();
	}
	
	UE_LOG(LogContentValidation, Display, TEXT("Starting to validate %d assets"), AssetDataList.Num());
	UE_LOG(LogContentValidation, Log, TEXT("Enabled validators:"));
	ForEachEnabledValidator([](UEditorValidatorBase* Validator)
	{
		UE_LOG(LogContentValidation, Log, TEXT("\t%s"), *Validator->GetClass()->GetClassPathName().ToString());
		return true;
	});
	
	// Broadcast the Editor event before we start validating. This lets other systems (such as Sequencer) restore the state
	// of the level to what is actually saved on disk before performing validation.
	if (FEditorDelegates::OnPreAssetValidation.IsBound())
	{
		FEditorDelegates::OnPreAssetValidation.Broadcast();
	}
	
	// Filter external objects out from the asset data list to be validated indirectly via their outers 
	TMap<FAssetData, TArray<FAssetData>> AssetsToExternalObjects;
	for (auto It = AssetDataList.CreateIterator(); It; ++It)
	{
		if (It->GetOptionalOuterPathName().IsNone())
		{
			// Standalone asset, leave it in the list
			continue;
		}

		FAssetData OuterAsset = AssetRegistry.GetAssetByObjectPath(It->ToSoftObjectPath().GetWithoutSubPath(), true);
		if (!OuterAsset.IsValid())
		{
			// We can't validate this asset if we can't find the package to load it into
			It.RemoveCurrent();
			continue;
		}

		// Special case for level instances in world partition - if the outer asset we'd like to validate is loaded & streamed in to 
		// another world, validate that world instead. 
		if (UWorld* AssetWorld = Cast<UWorld>(OuterAsset.FastGetAsset(false)))
		{
			if (   AssetWorld->PersistentLevel 
				&& AssetWorld->PersistentLevel->OwningWorld != nullptr 
				&& AssetWorld->PersistentLevel->OwningWorld != AssetWorld)
			{
				OuterAsset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetWorld->PersistentLevel->OwningWorld), true);
			}
		}
		AssetsToExternalObjects.FindOrAdd(OuterAsset).Add(*It);
		It.RemoveCurrent();
	}
	
	// Add any packages which contain those external objects to be validated
	{
		FDataValidationContext ValidationContext(false, InSettings.ValidationUsecase, {});
		for (const TPair<FAssetData, TArray<FAssetData>>& Pair : AssetsToExternalObjects)
		{
			if (ShouldValidateAsset(Pair.Key, InSettings, ValidationContext))
			{
				AssetDataList.Add(Pair.Key);
			}
			else
			{
				UE_LOG(LogContentValidation, Display, TEXT("Package %s (owner of some external objects) being skipped for validation."), 
					*WriteToString<256>(Pair.Key.PackageName));
			}
		}
		UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		DataValidationLog.Flush();
	}

	// Dont let other async compilation warnings be attributed incorrectly to the package that is loading.
	WaitForAssetCompilationIfNecessary(InSettings.ValidationUsecase, !InSettings.bSilent);

	OutResults.NumRequested = AssetDataList.Num();
	
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	// Loaded assets ought to have the standalone flag and so not be garbage collected, but keep weak pointers here for memory safety
	TArray<TWeakObjectPtr<UPackage>> PackagesToUnload;	

	const bool bLoadExternalObjects = InSettings.bLoadExternalObjectsForValidation && InSettings.bLoadAssetsForValidation;

	// Now add to map or update as needed
	for (const FAssetData& Data : AssetDataList)
	{
		ensure(Data.IsValid());

		if (!InSettings.bSilent)
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("DataValidation.ValidatingFilename", "Validating {0}"), FText::FromString(Data.GetFullName())));
		}
		
		if (OutResults.NumChecked >= InSettings.MaxAssetsToValidate)
		{
			OutResults.bAssetLimitReached = true;
			DataValidationLog.Info(FText::Format(LOCTEXT("DataValidation.MaxAssetCountReached", "MaxAssetsToValidate count {0} reached."), InSettings.MaxAssetsToValidate));
			break;
		}

		if (Data.HasAnyPackageFlags(PKG_Cooked))
		{
			++OutResults.NumSkipped;
			continue;
		}

		// Check exclusion path
		if (InSettings.bSkipExcludedDirectories && IsPathExcludedFromValidation(Data.PackageName.ToString()))
		{
			++OutResults.NumSkipped;
			continue;
		}

		const bool bLoadAsset = false;
		if (!InSettings.bLoadAssetsForValidation && !Data.FastGetAsset(bLoadAsset))
		{
			++OutResults.NumSkipped;
			continue;
		}

		DataValidationLog.Info()
			->AddToken(FAssetDataToken::Create(Data))
			->AddToken(FTextToken::Create(LOCTEXT("Data.ValidatingAsset", "Validating asset")));

		UObject* LoadedAsset = Data.FastGetAsset(false);

		TConstArrayView<FAssetData> ValidationExternalObjects;
		if (const TArray<FAssetData>* ValidationExternalObjectsPtr = AssetsToExternalObjects.Find(Data))
		{
			ValidationExternalObjects = *ValidationExternalObjectsPtr;
		}

		FDataValidationContext ValidationContext(false, InSettings.ValidationUsecase, ValidationExternalObjects);
		EDataValidationResult AssetResult = EDataValidationResult::NotValidated;
		if (!LoadedAsset)
		{
			UE_LOG(LogContentValidation, Log, TEXT("Loading asset %s for validation. After the validation we will %sunload it"), *Data.ToSoftObjectPath().ToString(), InSettings.bUnloadAssetsLoadedForValidation ? TEXT("") : TEXT("not "));
			UE::DataValidation::FScopedLogMessageGatherer LogGatherer(InSettings.bCaptureAssetLoadLogs);
			
			// Don't pass the flag to load all external objects. Later we'll load external objects which are also part of the to-validate
			// set if the settings allow for it.
			LoadedAsset = Data.GetAsset(); 

			ValidationContext.MarkAssetLoadedForValidation();

			if (InSettings.bUnloadAssetsLoadedForValidation && LoadedAsset)
			{
				PackagesToUnload.Emplace(LoadedAsset->GetPackage());
			}

			WaitForAssetCompilationIfNecessary(InSettings.ValidationUsecase);
			
			// Associate any load errors with this asset in the message log
			TArray<FString> Warnings;
			TArray<FString> Errors;
			LogGatherer.Stop(Warnings, Errors);
			if (Warnings.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Warnings, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Warning)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.LoadWarnings", "Warnings loading asset {0}"), FText::FromStringView(Buffer.ToView()));
			}
			if(Errors.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Errors, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Error)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.LoadErrors", "Errors loading asset {0}"), FText::FromStringView(Buffer.ToView()));
				AssetResult = EDataValidationResult::Invalid;
			}
		}

		if (LoadedAsset)
		{
			UE::DataValidation::FScopedLoadExternalObjects ExternalObjectsLoader(LoadedAsset, ValidationContext, bLoadExternalObjects);

			UE::DataValidation::FScopedLogMessageGatherer LogGatherer(InSettings.bCaptureLogsDuringValidation);
			AssetResult = IsObjectValidWithContext(LoadedAsset, ValidationContext);
			
			// Associate any UE_LOG errors with this asset in the message log
			TArray<FString> Warnings;
			TArray<FString> Errors;
			LogGatherer.Stop(Warnings, Errors);
			if (Warnings.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Warnings, LINE_TERMINATOR);
				ValidationContext.AddMessage(InSettings.bCaptureWarningsDuringValidationAsErrors ? EMessageSeverity::Error : EMessageSeverity::Warning)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.DuringValidationWarnings", "Warnings logged while validating asset {0}"), FText::FromStringView(Buffer.ToView()));
				if (InSettings.bCaptureWarningsDuringValidationAsErrors)
				{
					AssetResult = EDataValidationResult::Invalid;
				}
			}
			if(Errors.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Errors, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Error)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.DuringValidationErrors", "Errors logged while validating asset {0}"), FText::FromStringView(Buffer.ToView()));
				AssetResult = EDataValidationResult::Invalid;
			}
		}
		else if (InSettings.bLoadAssetsForValidation)
		{
			ValidationContext.AddMessage(EMessageSeverity::Error)
				->AddToken(FAssetDataToken::Create(Data))
				->AddToken(FTextToken::Create(LOCTEXT("DataValidation.LoadFailed", "Failed to load object")));
			AssetResult = EDataValidationResult::Invalid;
		}
		else 
		{
			ValidationContext.AddMessage(EMessageSeverity::Error)
				->AddToken(FAssetDataToken::Create(Data))
				->AddToken(FTextToken::Create(LOCTEXT("DataValidation.CannotValidateNotLoaded", "Cannot validate unloaded asset")));
			AssetResult = EDataValidationResult::Invalid;
		}

		++OutResults.NumChecked;
		
		// Don't add more messages to ValidationContext after this point because we will no longer add them to the message log 
		UE::DataValidation::AddAssetValidationMessages(Data, DataValidationLog, ValidationContext);

		bool bAnyWarnings = Algo::AnyOf(ValidationContext.GetIssues(), [](const FDataValidationContext::FIssue& Issue){ return Issue.Severity == EMessageSeverity::Warning; });
		if (bAnyWarnings)
		{
			++OutResults.NumWarnings;
		}

		if (InSettings.bShowIfNoFailures)
		{
			switch (AssetResult)
			{
				case EDataValidationResult::Valid:
					if (bAnyWarnings)
					{
						DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
							->AddToken(FTextToken::Create(LOCTEXT("DataValidation.ContainsWarningsResult", "contains valid data, but has warnings.")));
					}
					else 
					{
						DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
							->AddToken(FTextToken::Create(LOCTEXT("DataValidation.ValidResult", "contains valid data.")));
					}
					break;
				case EDataValidationResult::Invalid:
					DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
						->AddToken(FTextToken::Create(LOCTEXT("DataValidation.InvalidResult", "contains invalid data.")));
					break;
				case EDataValidationResult::NotValidated:
					DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
						->AddToken(FTextToken::Create(LOCTEXT("DataValidation.NotValidatedDataResult", "has no data validation.")));
					break;
			}
		}	

		switch (AssetResult)
		{
			case EDataValidationResult::Valid:
				++OutResults.NumValid;
				break;
			case EDataValidationResult::Invalid:
				++OutResults.NumInvalid;
				break;
			case EDataValidationResult::NotValidated:
				++OutResults.NumUnableToValidate;
				break;
		}

		if (InSettings.bCollectPerAssetDetails)
		{
			FValidateAssetsDetails& Details = OutResults.AssetsDetails.Emplace(Data.GetObjectPathString());
			Details.PackageName = Data.PackageName;
			Details.AssetName = Data.AssetName;
			Details.Result = AssetResult;
			ValidationContext.SplitIssues(Details.ValidationWarnings, Details.ValidationErrors, &Details.ValidationMessages);

			Details.ExternalObjects.Reserve(ValidationExternalObjects.Num());
			for (const FAssetData& ExtData : ValidationExternalObjects)
			{
				FValidateAssetsExternalObject& ExtDetails = Details.ExternalObjects.Emplace_GetRef();
				ExtDetails.PackageName = ExtData.PackageName;
				ExtDetails.AssetName = ExtData.AssetName;
			}
		}
		
		DataValidationLog.Flush();
		
		Result = CombineDataValidationResults(Result, AssetResult);

		if (PackagesToUnload.Num() > 0)
		{
			TArray<UPackage*> LocalPackages;
			CopyFromWeakArray(LocalPackages, PackagesToUnload);
			UPackageTools::UnloadPackages(LocalPackages);
		}
	}

	// Broadcast now that we're complete so other systems can go back to their previous state.
	if (FEditorDelegates::OnPostAssetValidation.IsBound())
	{
		FEditorDelegates::OnPostAssetValidation.Broadcast();
	}
	
	return Result;
}

void UEditorValidatorSubsystem::LogAssetValidationSummary(FMessageLog& DataValidationLog, const FValidateAssetsSettings& InSettings, EDataValidationResult Result, const FValidateAssetsResults& Results) const
{
	const bool bFailed = (Results.NumInvalid > 0) || Result != EDataValidationResult::Valid;
	const bool bAtLeastOneWarning = (Results.NumWarnings > 0);

	if (bFailed || bAtLeastOneWarning || InSettings.bShowIfNoFailures)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Result"), bFailed ? LOCTEXT("Failed", "FAILED") : LOCTEXT("Succeeded", "SUCCEEDED"));
		Arguments.Add(TEXT("NumChecked"), Results.NumChecked);
		Arguments.Add(TEXT("NumValid"), Results.NumValid);
		Arguments.Add(TEXT("NumInvalid"), Results.NumInvalid);
		Arguments.Add(TEXT("NumSkipped"), Results.NumSkipped);
		Arguments.Add(TEXT("NumUnableToValidate"), Results.NumUnableToValidate);

		DataValidationLog.Info()->AddToken(FTextToken::Create(FText::Format(LOCTEXT("DataValidation.SuccessOrFailure", "Data validation {Result}."), Arguments)));
		DataValidationLog.Info()->AddToken(FTextToken::Create(FText::Format(LOCTEXT("DataValidation.ResultsSummary", "Files Checked: {NumChecked}, Passed: {NumValid}, Failed: {NumInvalid}, Skipped: {NumSkipped}, Unable to validate: {NumUnableToValidate}"), Arguments)));

		DataValidationLog.Open(EMessageSeverity::Info, true);
	}
}

void UEditorValidatorSubsystem::ValidateOnSave(TArray<FAssetData> AssetDataList, bool bProceduralSave) const
{
	if (!ShouldValidateOnSave(bProceduralSave))
	{
		return;
	}
	
	FValidateAssetsSettings Settings;
	{
		FDataValidationContext Context {false, EDataValidationUsecase::Save, {}};
		AssetDataList.SetNum(Algo::RemoveIf(AssetDataList, [this, &Settings, &Context](const FAssetData& Asset) { 
			return !ShouldValidateAsset(Asset, Settings, Context); 
		}));
	}

	if (AssetDataList.IsEmpty())
	{
		return;
	}

	FText SavedAsset = AssetDataList.Num() == 1 ? FText::FromName(AssetDataList[0].AssetName) : LOCTEXT("MultipleAssets", "multiple assets");
	FValidateAssetsResults Results;

	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = false;
	Settings.ValidationUsecase = EDataValidationUsecase::Save;
	Settings.bLoadAssetsForValidation = false;
	Settings.MessageLogPageTitle = FText::Format(LOCTEXT("MessageLogPageTitle.ValidateSavedAssets", "Asset Save: {0}"), SavedAsset);

	if (ValidateAssetsWithSettings(AssetDataList, Settings, Results) > 0)
	{
		FMessageLog DataValidationLog(Settings.MessageLogName);

		const FText ErrorMessageNotification = FText::Format(
			LOCTEXT("ValidationFailureNotification", "Validation failed when saving {0}, check Data Validation log"), SavedAsset);
		DataValidationLog.Notify(ErrorMessageNotification, EMessageSeverity::Warning, /*bForce=*/ true);
	}
}

void UEditorValidatorSubsystem::ValidateSavedPackage(FName PackageName, bool bProceduralSave)
{
	if (!ShouldValidateOnSave(bProceduralSave))
	{
		return;
	}

	if (SavedPackagesToValidate.Num() == 0)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(this, &UEditorValidatorSubsystem::ValidateAllSavedPackages);
	}

	SavedPackagesToValidate.AddUnique(PackageName);
}

bool UEditorValidatorSubsystem::IsPathExcludedFromValidation(FStringView Path) const
{
	for (const FDirectoryPath& ExcludedPath : ExcludedDirectories)
	{
		if (Path.Contains(ExcludedPath.Path))
		{
			return true;
		}
	}

	return false;
}

void UEditorValidatorSubsystem::ValidateAllSavedPackages()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UEditorValidatorSubsystem::ValidateAllSavedPackages);

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	// Prior to validation, make sure Asset Registry is updated. This is done by ticking the DirectoryWatcher module, which 
	// is responsible of scanning modified asset files.
	if( !FApp::IsProjectNameEmpty() )
	{
		static FName DirectoryWatcherName("DirectoryWatcher");
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(DirectoryWatcherName);
		DirectoryWatcherModule.Get()->Tick(1.f);
	}
	// We need to query the in-memory data as the disk cache may not be accurate
	FARFilter Filter;
	Filter.PackageNames = SavedPackagesToValidate;
	Filter.bIncludeOnlyOnDiskAssets = false;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	bool bProceduralSave = false; // The optional suppression for ProceduralSaves was checked before adding to SavedPackagesToValidate
	ValidateOnSave(MoveTemp(Assets), bProceduralSave);

	SavedPackagesToValidate.Empty();
}

void UEditorValidatorSubsystem::ValidateChangelistPreSubmit(
	FSourceControlChangelistPtr InChangelist,
	EDataValidationResult& 		OutResult,
	TArray<FText>& 				OutValidationErrors,
	TArray<FText>& 				OutValidationWarnings) const
{
	check(InChangelist.IsValid());

	// Create temporary changelist object to do most of the heavy lifting
	UDataValidationChangelist* Changelist = NewObject<UDataValidationChangelist>();
	Changelist->Initialize(InChangelist);

	FValidateAssetsSettings Settings;
	Settings.ValidationUsecase = EDataValidationUsecase::PreSubmit;
	Settings.bLoadAssetsForValidation = GetDefault<UDataValidationSettings>()->bLoadAssetsWhenValidatingChangelists;
	Settings.MessageLogPageTitle = FText::Format(LOCTEXT("MessageLogPageTitle.ValidateChangelist", "Changelist Validation: {0}"), FText::FromString(InChangelist->GetIdentifier()));
	Settings.ShowMessageLogSeverity = { EMessageSeverity::Warning };

	FValidateAssetsResults Results;
	OutResult = ValidateChangelist(Changelist, Settings, Results);
	
	if (const FValidateAssetsDetails* Details = Results.AssetsDetails.Find(Changelist->GetPathName()))
	{
		OutValidationWarnings = Details->ValidationWarnings;
		OutValidationErrors = Details->ValidationErrors;
	}
}

EDataValidationResult UEditorValidatorSubsystem::ValidateChangelist(
	UDataValidationChangelist* InChangelist,
	const FValidateAssetsSettings& InSettings,
	FValidateAssetsResults& OutResults) const
{
	return ValidateChangelistsInternal(MakeArrayView(&InChangelist, 1), InSettings, OutResults);	
}

EDataValidationResult UEditorValidatorSubsystem::ValidateChangelists(
	const TArray<UDataValidationChangelist*> InChangelists,
	const FValidateAssetsSettings& InSettings,
	FValidateAssetsResults& OutResults) const
{
	return ValidateChangelistsInternal(MakeArrayView(InChangelists), InSettings, OutResults);	
}

EDataValidationResult UEditorValidatorSubsystem::ValidateChangelistsInternal(
	TConstArrayView<UDataValidationChangelist*>	Changelists,
	const FValidateAssetsSettings& 				Settings,
	FValidateAssetsResults& 					OutResults) const
{
	FScopedSlowTask SlowTask(Changelists.Num(), LOCTEXT("DataValidation.ValidatingChangelistTask", "Validating Changelists"));
	SlowTask.Visibility = ESlowTaskVisibility::Invisible;
	if (!Settings.bSilent)
	{
		SlowTask.MakeDialog();
	}

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	if (AssetRegistry.IsLoadingAssets())
	{
		UE_CLOG(FApp::IsUnattended(), LogContentValidation, Fatal, TEXT("Unable to perform unattended content validation while asset registry scan is in progress. Callers just wait for asset registry scan to complete."));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DataValidation.UnableToValidate_PendingAssetRegistry", "Unable to validate changelist while asset registry scan is in progress. Wait until asset discovery is complete."));
		return EDataValidationResult::NotValidated;
	}

	FMessageLog DataValidationLog(Settings.MessageLogName);

	// Choose a specific message log page for this output, flushing in case other recursive calls also write to this log
	DataValidationLog.SetCurrentPage(Settings.MessageLogPageTitle);

	for (UDataValidationChangelist* CL : Changelists)
	{
		CL->AddToRoot();
	}
	
	ON_SCOPE_EXIT {
		for (UDataValidationChangelist* CL : Changelists)
		{
			CL->RemoveFromRoot();		
		}
	};

	EDataValidationResult Result = EDataValidationResult::NotValidated;
	for (UDataValidationChangelist* Changelist : Changelists)
	{
		FText ValidationMessage = FText::Format(LOCTEXT("DataValidation.ValidatingChangelistMessage", "Validating changelist {0}"), Changelist->Description);
		DataValidationLog.Info(ValidationMessage);
		if (!Settings.bSilent)
		{
			SlowTask.EnterProgressFrame(1.0f, ValidationMessage);
		}

		FValidateAssetsDetails& Details = OutResults.AssetsDetails.FindOrAdd(Changelist->GetPathName());
		{
			FDataValidationContext ValidationContext(false, Settings.ValidationUsecase, {}); // No associated objects for changelist
			Details.Result = IsObjectValidWithContext(Changelist, ValidationContext);
			UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
			ValidationContext.SplitIssues(Details.ValidationWarnings, Details.ValidationErrors);
		}
		Result = CombineDataValidationResults(Result, Details.Result);
		DataValidationLog.Flush();
	}

	TSet<FAssetData> AssetsToValidate;	
	for (UDataValidationChangelist* Changelist : Changelists)
	{
		FDataValidationContext ValidationContext(false, Settings.ValidationUsecase, {}); // No associated objects for changelist
		GatherAssetsToValidateFromChangelist(Changelist, Settings, AssetsToValidate, ValidationContext);
		UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		DataValidationLog.Flush();
	}

	// Filter out assets that we don't want to validate
	{
		FDataValidationContext ValidationContext(false, Settings.ValidationUsecase, {}); 
		for (auto It = AssetsToValidate.CreateIterator(); It; ++It)
		{
			if (!ShouldValidateAsset(*It, Settings, ValidationContext))
			{
				UE_LOG(LogContentValidation, Display, TEXT("Excluding asset %s from validation"), *It->GetSoftObjectPath().ToString());
				It.RemoveCurrent();
			}
		}
		UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		DataValidationLog.Flush();
	}

	// Validate assets from all changelists
	EDataValidationResult AssetResult = ValidateAssetsInternal(DataValidationLog, MoveTemp(AssetsToValidate), Settings, OutResults);
	Result = CombineDataValidationResults(Result, AssetResult);
	LogAssetValidationSummary(DataValidationLog, Settings, Result, OutResults);

	if (const EMessageSeverity::Type* Severity = Settings.ShowMessageLogSeverity.GetPtrOrNull())
	{
		DataValidationLog.Open(*Severity, false);
	}

	return Result;
}

void UEditorValidatorSubsystem::GatherAssetsToValidateFromChangelist(
	UDataValidationChangelist* 		InChangelist,
	const FValidateAssetsSettings& 	Settings,
	TSet<FAssetData>& 				OutAssets,
	FDataValidationContext& 		InContext) const
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	
	for (const FName& PackageName : InChangelist->ModifiedPackageNames)
	{
		TArray<FAssetData> NewAssets;
		AssetRegistry.GetAssetsByPackageName(PackageName, NewAssets, true);	
		OutAssets.Append(NewAssets);
	}
	
	// Gather assets requested by plugin/project validators 
	ForEachEnabledValidator([this, InChangelist, &Settings, &InContext, &OutAssets](UEditorValidatorBase* Validator)
	{
		TArray<FAssetData> NewAssets = Validator->GetAssetsToValidateFromChangelist(InChangelist, InContext);
		for (const FAssetData& Asset : NewAssets)
		{
			// It's not strictly necessary to filter assets here but it makes logging simpler
			if (ShouldValidateAsset(Asset, Settings, InContext))
			{
				UE_LOG(LogContentValidation, Display, TEXT("Asset validator %s adding %s to be validated from changelist %s."), *Validator->GetPathName(), *Asset.GetSoftObjectPath().ToString(), *InChangelist->Description.ToString());
				OutAssets.Add(Asset);
			}
		}
		return true;
	});
	
	if (Settings.bValidateReferencersOfDeletedAssets)
	{
		TSet<FAssetData> AdditionalAssetsToValidate;
		for (FName DeletedPackageName : InChangelist->DeletedPackageNames)
		{
			InContext.AddMessage(EMessageSeverity::Info, 
				FText::Format(LOCTEXT("DataValidation.AddDeletedPackageReferencers", "Adding referencers of deleted package {0} to be validated"), FText::FromName(DeletedPackageName)));

			TArray<FName> PackageReferencers;
			AssetRegistry.GetReferencers(DeletedPackageName, PackageReferencers, UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName& Referencer : PackageReferencers)
			{
				UE_LOG(LogContentValidation, Display, TEXT("Adding %s to to validated as it is a referencer of deleted asset %s"), *Referencer.ToString(), *DeletedPackageName.ToString());
				TArray<FAssetData> NewAssets;
				AssetRegistry.GetAssetsByPackageName(Referencer, NewAssets, true);
				OutAssets.Append(NewAssets);
			}
		}
	}
}

void UEditorValidatorSubsystem::OnNativeModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason)
{
	switch (InModuleChangeReason)
	{
		case EModuleChangeReason::ModuleLoaded:
			NativeModulesPendingLoad.Add(InModuleName);
			NativeModulesPendingUnload.Remove(InModuleName);
			break;

		case EModuleChangeReason::ModuleUnloaded:
			NativeModulesPendingUnload.Add(InModuleName);
			NativeModulesPendingLoad.Remove(InModuleName);
			break;

		default:
			break;
	}
}

void UEditorValidatorSubsystem::OnAssetsAdded(TConstArrayView<FAssetData> InAssets)
{
	OnAssetsAddedOrRemoved(InAssets, [this](const FTopLevelAssetPath& BPGC) { AddValidator(BPGC); });
}

void UEditorValidatorSubsystem::OnAssetsRemoved(TConstArrayView<FAssetData> InAssets)
{
	OnAssetsAddedOrRemoved(InAssets, [this](const FTopLevelAssetPath& BPGC) { RemoveValidator(BPGC); });
}

void UEditorValidatorSubsystem::OnAssetsAddedOrRemoved(TConstArrayView<FAssetData> InAssets, TFunctionRef<void(const FTopLevelAssetPath& BPGC)> Callback)
{
	for (const FAssetData& Asset : InAssets)
	{
		if (const UClass* AssetClass = Asset.GetClass())
		{
			if (AssetClass->IsChildOf<UEditorUtilityBlueprint>())
			{
				// Uncooked BP
				if (const UClass* ParentClass = UBlueprint::GetBlueprintParentClassFromAssetTags(Asset);
					ParentClass && ParentClass->IsChildOf<UEditorValidatorBase>())
				{
					FTopLevelAssetPath BPGC = FEditorClassUtils::GetClassPathNameFromAssetTag(Asset);
					if (BPGC.IsNull())
					{
						BPGC = FTopLevelAssetPath(Asset.PackageName, FName(WriteToString<128>(Asset.AssetName, TEXT("_C"))));
					}
					Callback(BPGC);
				}
			}
			else if (AssetClass->IsChildOf<UBlueprintGeneratedClass>())
			{
				// Cooked BPGC
				if (const UClass* ParentClass = UBlueprint::GetBlueprintParentClassFromAssetTags(Asset);
					ParentClass && ParentClass->IsChildOf<UEditorValidatorBase>())
				{
					Callback(FTopLevelAssetPath(Asset.PackageName, Asset.AssetName));
				}
			}
		}
	}
}

void UEditorValidatorSubsystem::UpdateValidators() const
{
	const_cast<UEditorValidatorSubsystem*>(this)->UpdateValidators();
}

void UEditorValidatorSubsystem::UpdateValidators()
{
	UE_CLOG(!bHasRegisteredNativeValidators, LogContentValidation, Warning, TEXT("UpdateValidators request made before RegisterNativeValidators. Native validators may be missing!"));
	UE_CLOG(!bHasRegisteredBlueprintValidators, LogContentValidation, Warning, TEXT("UpdateValidators request made before RegisterBlueprintValidators. Blueprint validators may be missing!"));

	// Remove any existing validators for unloaded modules
	if (NativeModulesPendingUnload.Num() > 0)
	{
		TSet<FName> ModulePackageNames;
		ModulePackageNames.Reserve(NativeModulesPendingUnload.Num());
		Algo::Transform(NativeModulesPendingUnload, ModulePackageNames, [](const FName ModuleName) { return FPackageName::GetModuleScriptPackageName(ModuleName); });

		for (auto It = Validators.CreateIterator(); It; ++It)
		{
			if (ModulePackageNames.Contains(It->Key.GetPackageName()))
			{
				It.RemoveCurrent();
			}
		}

		NativeModulesPendingUnload.Reset();
	}

	// Add any new validators for loaded modules
	if (NativeModulesPendingLoad.Num() > 0)
	{
		TSet<FName> ModulePackageNames;
		ModulePackageNames.Reserve(NativeModulesPendingLoad.Num());
		Algo::Transform(NativeModulesPendingLoad, ModulePackageNames, [](const FName ModuleName) { return FPackageName::GetModuleScriptPackageName(ModuleName); });

		// GetDerivedClasses has an accelerator table, so it's faster to query all the native UEditorValidatorBase classes (of which there will be relatively few) and 
		// then filter them down by module, than it is to get all the classes of each pending module (of which there may be many) and then filter them by type
		TArray<UClass*> ValidatorClasses;
		GetDerivedClasses(UEditorValidatorBase::StaticClass(), ValidatorClasses);

		for (UClass* ValidatorClass : ValidatorClasses)
		{
			// GetDerivedClasses may include a mix of C++ and loaded BP classes
			// Skip any classes outside of the modules requested, as well as anything that has already been registered by this point
			if (!ValidatorClass->HasAllClassFlags(CLASS_Abstract) && !Validators.Contains(ValidatorClass->GetClassPathName())
				&& ModulePackageNames.Contains(ValidatorClass->GetPackage()->GetFName()))
			{
				UEditorValidatorBase* Validator = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass);
				AddValidator(Validator);
			}
		}

		NativeModulesPendingLoad.Reset();
	}

	// Add any new validators from pending Blueprint classes
	if (ValidatorClassesPendingLoad.Num() > 0)
	{
		for (const FTopLevelAssetPath& ValidatorClassPendingLoad : ValidatorClassesPendingLoad)
		{
			if (TObjectPtr<UEditorValidatorBase>* ValidatorInstance = Validators.Find(ValidatorClassPendingLoad);
				ensure(ValidatorInstance))
			{
				TSoftClassPtr<UEditorValidatorBase> ValidatorClassSoftPtr = TSoftClassPtr<UEditorValidatorBase>(FSoftObjectPath(ValidatorClassPendingLoad));
				UClass* ValidatorClass = ValidatorClassSoftPtr.Get();

				// If this class isn't currently loaded, load it
				if (!ValidatorClass)
				{
					FCookLoadScope EditorOnlyLoadScope(ECookLoadType::EditorOnly);
					FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::EditorOnlyCollect, ESoftObjectPathSerializeType::AlwaysSerialize);
					ValidatorClass = ValidatorClassSoftPtr.LoadSynchronous();
				}

				if (ValidatorClass && !ValidatorClass->HasAnyClassFlags(CLASS_Abstract))
				{
					*ValidatorInstance = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass);
				}
			}
		}
		ValidatorClassesPendingLoad.Reset();
	}
}

TArray<FAssetData> UEditorValidatorSubsystem::GetAssetsResolvingRedirectors(FARFilter& InFilter)
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	TArray<FAssetData> Found;
	AssetRegistry.GetAssets(InFilter, Found);
	
	TArray<FAssetData> Redirectors;	
	for (int32 i=Found.Num()-1; i >= 0; --i)
	{
		if (Found[i].IsRedirector())
		{
			Redirectors.Add(Found[i]);
			Found.RemoveAt(i);
		}
	}
	
	for (const FAssetData& RedirectorAsset : Redirectors)
	{
		FSoftObjectPath Path = AssetRegistry.GetRedirectedObjectPath(RedirectorAsset.GetSoftObjectPath());		
		if (!Path.IsNull())
		{
			FAssetData Destination = AssetRegistry.GetAssetByObjectPath(Path, true);
			if (Destination.IsValid())
			{
				Found.Add(Destination);
			}
		}
	}
	return Found;
}

void UEditorValidatorSubsystem::WaitForAssetCompilationIfNecessary(EDataValidationUsecase InUsecase, bool bShowProgress) const
{
	if (InUsecase == EDataValidationUsecase::Save)
	{
		return;
	}

	if (FAssetCompilingManager::Get().GetNumRemainingAssets())
	{
		if (bShowProgress)
		{
			FScopedSlowTask CompileAssetsSlowTask(0.f, LOCTEXT("DataValidation.CompilingAssetsBeforeCheckingContentTask", "Finishing asset compilations before checking content..."));
			CompileAssetsSlowTask.MakeDialog();
		}
		FAssetCompilingManager::Get().FinishAllCompilation();
	}
}

#undef LOCTEXT_NAMESPACE

