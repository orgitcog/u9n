// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserItemPath.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "DirectoryPlaceholder.h"
#include "DirectoryPlaceholderFactory.h"
#include "DirectoryPlaceholderSettings.h"
#include "DirectoryPlaceholderUtils.h"
#include "DirectoryWatcherModule.h"
#include "FileHelpers.h"
#include "IDirectoryWatcher.h"
#include "ObjectTools.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY_STATIC(LogDirectoryPlaceholder, Log, All)

#define LOCTEXT_NAMESPACE "DirectoryPlaceholderModule"

/** 
 * Directory Placeholder Module
 * Manages automatic creation and deletion of placeholder assets, allowing folders to be checked in to source control.
 */
class FDirectoryPlaceholderModule : public IModuleInterface
{
public:

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	/** Register the Directory Watchers */
	void RegisterDirectoryWatchers();

	/** Callback when the directory watcher detects a file/folder change */
	void OnDirectoryChanged(const TArray<FFileChangeData>& InFileChanges);

	/** Executes when one or more folders are being deleted in the content browser */
	void OnDeleteFolders(const TArray<FContentBrowserItemPath>& PathsToDelete);

private:
	FDelegateHandle EngineDirectoryWatcherHandle;
	FDelegateHandle OnDeleteFoldersHandle;
};

void FDirectoryPlaceholderModule::StartupModule()
{
	// Only enable this behavior in an interactive editor
	if (GIsEditor && !IsRunningCommandlet())
	{
		// Delay registration of the directory watchers until after the asset registry has finished its initial scan
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FDirectoryPlaceholderModule::RegisterDirectoryWatchers);

		// Register a callback to execute when one or more folders are being deleted in the content browser
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		OnDeleteFoldersHandle = ContentBrowserModule.GetOnDeleteFolders().AddRaw(this, &FDirectoryPlaceholderModule::OnDeleteFolders);

		// Extend the content browser folder context menu with the option to cleanup directory placeholders
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
		{
			FToolMenuSection& BulkOpsSection = Menu->FindOrAddSection("PathContextBulkOperations");

			FToolMenuEntry& Entry = BulkOpsSection.AddDynamicEntry("CleanupDirectoryPlaceholders", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
				{
					UContentBrowserFolderContext* Context = InSection.FindContext<UContentBrowserFolderContext>();
					if (Context && Context->NumAssetPaths > 0)
					{
						const TArray<FString>& Paths = Context->GetSelectedPackagePaths();

						FToolMenuEntry& Entry = InSection.AddMenuEntry(
							"CleanupDirectoryPlaceholders",
							LOCTEXT("CleanupDirectoryPlaceholdersLabel", "Cleanup Directory Placeholders"),
							LOCTEXT("CleanupDirectoryPlaceholdersToolTip", "Delete all unnecessary placeholder assets in this folder (and sub-folders)"),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed"),
							FUIAction(FExecuteAction::CreateLambda([Paths]() { UDirectoryPlaceholderLibrary::CleanupPlaceholdersInPaths(Paths); }))
						);

						Entry.InsertPosition = FToolMenuInsert("Delete", EToolMenuInsertType::After);
					}
				}));
		}
	}
}

void FDirectoryPlaceholderModule::ShutdownModule()
{
	if (GIsEditor && !IsRunningCommandlet())
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FPaths::ProjectContentDir(), EngineDirectoryWatcherHandle);
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.GetOnDeleteFolders().Remove(OnDeleteFoldersHandle);
	}
}

void FDirectoryPlaceholderModule::RegisterDirectoryWatchers()
{
	// Register a callback with the directory watcher to be notified about file/folder changes
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
	if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
	{
		if (FPaths::DirectoryExists(FPaths::ProjectContentDir()))
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				FPaths::ProjectContentDir(),
				IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FDirectoryPlaceholderModule::OnDirectoryChanged),
				EngineDirectoryWatcherHandle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges
			);
		}

		if (FPaths::DirectoryExists(FPaths::ProjectPluginsDir()))
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				FPaths::ProjectPluginsDir(),
				IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FDirectoryPlaceholderModule::OnDirectoryChanged),
				EngineDirectoryWatcherHandle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges
			);
		}
	}
}

void FDirectoryPlaceholderModule::OnDirectoryChanged(const TArray<FFileChangeData>& InFileChanges)
{
	const UDirectoryPlaceholderSettings* const Settings = GetDefault<UDirectoryPlaceholderSettings>();
	if (!Settings->bAutomaticallyCreatePlaceholders)
	{
		return;
	}

	TArray<UPackage*> PackagesToSave;

	for (const FFileChangeData& FileChange : InFileChanges)
	{
		// Check if the FileChangeData represent a directory that was just added
		if (FPaths::DirectoryExists(FileChange.Filename) && (FileChange.Action == FFileChangeData::FCA_Added))
		{
			FString PackagePath;
			if (!FPackageName::TryConvertFilenameToLongPackageName(FileChange.Filename, PackagePath))
			{
				continue;
			}

			// Test if there are any assets already in the path of the new directory. If there are, we do not create a placeholder.
			// Note: This could occur if the directory was just renamed, copied, or moved.
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			TArray<FAssetData> AssetDataList;
			constexpr bool bRecursive = true;
			if (AssetRegistryModule.Get().HasAssets(*PackagePath, bRecursive))
			{
				continue;
			}

			// Create a new UDirectoryPlaceholder asset in the directory that was just added
			const FString AssetName = FString(TEXT("UE_Placeholder"));

			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			const UObject* const PlaceholderAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDirectoryPlaceholder::StaticClass(), NewObject<UDirectoryPlaceholderFactory>());

			// If the asset was created successfully, save it now, because it will be hidden from the user by default
			// If source control is enabled, it will also be marked for add.
			if (PlaceholderAsset)
			{
				PackagesToSave.Add(PlaceholderAsset->GetPackage());
				UE_LOG(LogDirectoryPlaceholder, Verbose, TEXT("New Directory Placeholder was created in %s"), *PackagePath);
			}
			else
			{
				UE_LOG(LogDirectoryPlaceholder, Warning, TEXT("Failed to create new Directory Plcaeholder in %s"), *PackagePath);
			}
		}
	}

	const bool bOnlyDirty = false;
	UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, bOnlyDirty);
}

void FDirectoryPlaceholderModule::OnDeleteFolders(const TArray<FContentBrowserItemPath>& PathsToDelete)
{
	const UDirectoryPlaceholderSettings* const Settings = GetDefault<UDirectoryPlaceholderSettings>();
	if (!Settings->bAutomaticallyCreatePlaceholders)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// The only place we automatically create directory placeholders is under project content ("/Game").
	// These are also the only paths that we want to automatically delete placeholders from.
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (const FContentBrowserItemPath& Path : PathsToDelete)
	{
		FString InternalPath = Path.GetInternalPathName().ToString();
		if (InternalPath.Equals(TEXT("/Game")) || InternalPath.StartsWith(TEXT("/Game/")))
		{
			Filter.PackagePaths.Add(Path.GetInternalPathName());
		}
	}

	if (Filter.PackagePaths.IsEmpty())
	{
		return;
	}

	// Find all of the assets in the folders being deleted
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

	// Check if all of the assets being deleted are placeholders, or if there are any others present
	bool bAllPlaceholders = true;
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.GetClass() != UDirectoryPlaceholder::StaticClass())
		{
			bAllPlaceholders = false;
		}
	}

	// If all of the assets are directory placeholders, delete them from these folders
	if (bAllPlaceholders)
	{
		const bool bShowConfirmation = false;
		const int32 NumAssetsDeleted = ObjectTools::DeleteAssets(AssetDataList, bShowConfirmation);

		UE_LOG(LogDirectoryPlaceholder, Verbose, TEXT("Deleted %d Directory Placeholders"), NumAssetsDeleted);
	}
}

IMPLEMENT_MODULE(FDirectoryPlaceholderModule, DirectoryPlaceholder);

#undef LOCTEXT_NAMESPACE
