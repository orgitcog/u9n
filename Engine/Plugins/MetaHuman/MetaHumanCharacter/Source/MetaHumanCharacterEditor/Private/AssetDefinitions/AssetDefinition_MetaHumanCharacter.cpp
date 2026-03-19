// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaHumanCharacter.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterAssetEditor.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanCharacterAnalytics.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCharacterInstance.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Misc/MessageDialog.h"
#include "Misc/UObjectToken.h"
#include "Logging/StructuredLog.h"
#include "Logging/MessageLog.h"
#include "Dialogs/Dialogs.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_MetaHumanCharacter"

namespace MenuExtension_MetaHumanCharacter
{
	static void ExecuteRemoveTexturesAndRigs(const UContentBrowserAssetContextMenuContext* InCBContext)
	{
		if (InCBContext)
		{
			const TArray<UMetaHumanCharacter*> Characters = InCBContext->LoadSelectedObjects<UMetaHumanCharacter>();

			for (UMetaHumanCharacter* Character : Characters)
			{
				UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

				const bool bFocusIfOpened = false;
				const bool bHasOpenedEditor = AssetEditorSubsystem->FindEditorForAsset(Character, bFocusIfOpened) != nullptr;

				if (bHasOpenedEditor)
				{
					// Get confirmation from the user that its ok to proceed
					const FText Title = FText::Format(LOCTEXT("RemoveTexturesAndRigs_CloseAssetTitle", "Remove Textures and Rigs from '{0}'"),
													  FText::FromString(Character->GetName()));
					const FText Message = FText::Format(LOCTEXT("RemoveTexturesAndRigs_CloseAssetMessage", "'{0}' has its asset editor opened. Removing textures and rigs requires the asset editor to be closed first. Would you like to proceed?"), FText::FromString(Character->GetName()));
					const EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::YesNo, Message, Title);
					if (Response == EAppReturnType::No)
					{
						UE_LOGFMT(LogMetaHumanCharacterEditor, Display, "Skipping convertion to preset for character '{CharacterName}'", Character->GetName());
						continue;
					}
				}

				if (bHasOpenedEditor)
				{
					AssetEditorSubsystem->CloseAllEditorsForAsset(Character);
				}

				const bool bConverted = UMetaHumanCharacterEditorSubsystem::Get()->RemoveTexturesAndRigs(Character);

				if (!bConverted)
				{
					const FText Message = FText::Format(LOCTEXT("ConvertToPreset_Failed", "Failed to remove texures and rigs from '{0}'"),
														FText::FromString(Character->GetName()));
					FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, Message);
				}
			}
		}
	}

	static void ExecuteRemoveUnusedWardrobeItems(const UContentBrowserAssetContextMenuContext* InCBContext)
	{
		if (InCBContext)
		{
			const TArray<UMetaHumanCharacter*> Characters = InCBContext->LoadSelectedObjects<UMetaHumanCharacter>();

			for (UMetaHumanCharacter* Character : Characters)
			{
				UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

				const bool bFocusIfOpened = false;
				const bool bHasOpenedEditor = AssetEditorSubsystem->FindEditorForAsset(Character, bFocusIfOpened) != nullptr;

				if (bHasOpenedEditor)
				{
					// Get confirmation from the user that its ok to proceed
					const FText Title = FText::Format(LOCTEXT("RemoveUnusedWadrobeItems_CloseAssetTitle", "Remove unused wardrobe items from {0}"),
													  FText::FromString(Character->GetName()));
					const FText Message = FText::Format(LOCTEXT("RemoveUnusedWardrobeItems_CloseAssetMessage", "{0} has its asset editor opened. Removing unused wardrobe items requires the asset editor to be closed first. Would you like to proceed?"),
														FText::FromString(Character->GetName()));
					const EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::YesNo, Message, Title);
					if (Response == EAppReturnType::No)
					{
						UE_LOGFMT(LogMetaHumanCharacterEditor, Display, "Skipping removing unused wardrobe items for '{CharacterName}'", Character->GetName());
						continue;
					}
				}

				if (bHasOpenedEditor)
				{
					AssetEditorSubsystem->CloseAllEditorsForAsset(Character);
				}

				// TODO: Move this to the subsystem so it can be called from other places

				Character->WardrobePaths.Empty();
				Character->CharacterIndividualAssets.Empty();
				Character->PipelinesPerClass.Empty();

				// Remove items that are not selected from the collection
				if (UMetaHumanCollection* Collection = Character->GetMutableInternalCollection())
				{
					TArray<FMetaHumanCharacterPaletteItem> ItemsToRemove;

					for (const FMetaHumanCharacterPaletteItem& Item : Collection->GetItems())
					{
						if (Item.SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
						{
							continue;
						}

						TNotNull<const UMetaHumanCharacterInstance*> Instance = Collection->GetDefaultInstance();
						const FMetaHumanPaletteItemKey PaletteItemKey = Item.GetItemKey();

						const FMetaHumanPipelineSlotSelection SlotSelection{ Item.SlotName, PaletteItemKey };
						if (!Instance->ContainsSlotSelection(SlotSelection))
						{
							ItemsToRemove.Add(Item);
						}
					}

					for (const FMetaHumanCharacterPaletteItem& Item : ItemsToRemove)
					{
						const FMetaHumanPaletteItemKey ItemKeyToRemove = Item.GetItemKey();

						if (Collection->TryRemoveItem(ItemKeyToRemove))
						{
							// Remove any external wardrobe items from WardrobeIndividualAssets since they are not being referenced
							TSoftObjectPtr<UMetaHumanWardrobeItem> ExternalWardrobeItem;
							if (ItemKeyToRemove.TryGetExternalWardrobeItem(ExternalWardrobeItem))
							{
								if (FMetaHumanCharacterWardrobeIndividualAssets* IndividualAssets = Character->WardrobeIndividualAssets.Find(Item.SlotName))
								{
									IndividualAssets->Items.Remove(ExternalWardrobeItem);
								}
							}
						}
						else
						{
							UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Error removing {ItemKey} from {CharacterName}", ItemKeyToRemove.ToDebugString(), Character->GetName());
						}
					}
				}

				Character->MarkPackageDirty();
			}
		}
	}

	static void ExtendAssetActions()
	{
		FToolMenuOwnerScoped OwnderScoped(UE_MODULE_NAME);
		{
			UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMetaHumanCharacter::StaticClass())
				->AddDynamicSection(
					NAME_None,
					FNewToolMenuDelegate::CreateLambda(
						[](UToolMenu* InMenu)
						{
							const UContentBrowserAssetContextMenuContext* Context = InMenu->FindContext< UContentBrowserAssetContextMenuContext>();
							if (Context && Context->SelectedAssets.Num() > 0)
							{
								FToolMenuSection& Section = InMenu->FindOrAddSection(TEXT("GetAssetActions"));
								{
									{
										const FText Label = LOCTEXT("MetaHumanCharacter_RemoveTexturesAndRigs", "Remove Textures and Rigs");
										const FText Tooltip = LOCTEXT("MetaHumanCharacter_RemoveTexturesAndRigsTooltip", "Remove all textures and rigs from the character.");
										const FSlateIcon Icon{ FAppStyle::GetAppStyleSetName(), "ClassIcon.MetaHumanCharacter" };
										const FUIAction UIAction = FUIAction(
											FExecuteAction::CreateStatic(&ExecuteRemoveTexturesAndRigs, Context)
										);
										Section.AddMenuEntry(TEXT("MetaHumanCharacter_MakePreset"), Label, Tooltip, Icon, UIAction);
									}
									{
										const FText Label = LOCTEXT("MetaHumanCharacter_RemoveUnusedWardrobeItems", "Remove Unused Wardrobe Items");
										const FText Tooltip = LOCTEXT("MetaHumanCharacter_RemoveTexturesAndRigsTooltip", "Removes referecens to unused wardrobe items from this character."
																														 "Useful to prevent invalid references when packaging the character for Fab");
										const FSlateIcon Icon{ FAppStyle::GetAppStyleSetName(), "ClassIcon.MetaHumanCharacter" };
										const FUIAction UIAction = FUIAction(
											FExecuteAction::CreateStatic(&ExecuteRemoveUnusedWardrobeItems, Context)
										);
										Section.AddMenuEntry(TEXT("MetaHumanCharacter_RemoveUnusedWardrobeItems"), Label, Tooltip, Icon, UIAction);
									}
								}
							}
						}
					)
				);
		}
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(
		EDelayedRegisterRunPhase::EndOfEngineInit,
		[]
		{
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&ExtendAssetActions));
		}
	);
}

FText UAssetDefinition_MetaHumanCharacter::GetAssetDisplayName() const
{
	return LOCTEXT("MetaHumanCharacterDisplayName", "MetaHuman Character");
}

FLinearColor UAssetDefinition_MetaHumanCharacter::GetAssetColor() const
{
	return FColor::Orange;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanCharacter::GetAssetClass() const
{
	return UMetaHumanCharacter::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanCharacter::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath{ LOCTEXT("MetaHumanAssetCategoryPath", "MetaHuman") } };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_MetaHumanCharacter::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_MetaHumanCharacter::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	const TArray<UMetaHumanCharacter*> LoadedCharacters = InOpenArgs.LoadObjects<UMetaHumanCharacter>();

	const FText FailedToOpenCharacterBaseMessage = LOCTEXT("FailedToOpenCharacter", "Failed to open");
	bool bHasErrors = false;

	for (UMetaHumanCharacter* MetaHumanCharacter : LoadedCharacters)
	{
		if (!MetaHumanCharacter->IsCharacterValid())
		{
			FMessageLog(UE::MetaHuman::MessageLogName)
				.Error(FailedToOpenCharacterBaseMessage)
				->AddToken(FUObjectToken::Create(MetaHumanCharacter))
				->AddText(LOCTEXT("CharacterInvalid", "is not valid"));

			bHasErrors = true;

			continue;
		}

		if (MetaHumanCharacter->HasFaceDNABlendshapes())
		{
			FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
			constexpr uint64 GB = 1024 * 1024 * 1024;
			if (MemoryStats.AvailableVirtual < (10 * GB))
			{
				const FText NotEnoughMemoryTitle = LOCTEXT("NotEnoughMemoryDialogTitle", "Not enough memory to load MetaHuman Character");
				const FText NotEnoughMemoryMessage = FText::Format(LOCTEXT("NotEnoughMemoryDialogMessage", "Loading a MetaHuman Character previously auto-rigged with blend shapes requires at least 10 GiB of free memory but only {0} is available.\n"
																										   "If you proceed the editor might crash. Would you like to continue?"),
																   FText::AsMemory(MemoryStats.AvailableVirtual));

				FSuppressableWarningDialog::FSetupInfo SetupInfo(NotEnoughMemoryMessage, NotEnoughMemoryTitle, TEXT("MetaHumanCharacterSupressNotEnoughMemory"));
				SetupInfo.ConfirmText = LOCTEXT("NotEnoughMemoryDialogConfirmText", "Yes");
				SetupInfo.CancelText = LOCTEXT("NotEnoughMemoryDialogCancelText", "Cancel");

				FSuppressableWarningDialog NotEnoughMemoryDialog{ SetupInfo };

				const FSuppressableWarningDialog::EResult Result = NotEnoughMemoryDialog.ShowModal();
				if (Result == FSuppressableWarningDialog::EResult::Cancel)
				{
					FMessageLog(UE::MetaHuman::MessageLogName)
						.Error(FailedToOpenCharacterBaseMessage)
						->AddToken(FUObjectToken::Create(MetaHumanCharacter))
						->AddText(NotEnoughMemoryTitle);

					bHasErrors = true;

					continue;
				}
			}
		}

		const bool bIsRunningPIE = GEditor && GEditor->IsPlaySessionInProgress();
		if (bIsRunningPIE && MetaHumanCharacter->HasFaceDNABlendshapes())
		{
			FMessageLog(UE::MetaHuman::MessageLogName)
				.Error(FailedToOpenCharacterBaseMessage)
				->AddToken(FUObjectToken::Create(MetaHumanCharacter))
				->AddText(LOCTEXT("PIEIsRunning", "because there is an active PIE session and this character was rigged with blend shapes"));

			bHasErrors = true;

			continue;
		}

		if (!UMetaHumanCharacterEditorSubsystem::Get()->TryAddObjectToEdit(MetaHumanCharacter))
		{
			FMessageLog(UE::MetaHuman::MessageLogName)
				.Error(FailedToOpenCharacterBaseMessage)
				->AddToken(FUObjectToken::Create(MetaHumanCharacter))
				->AddText(LOCTEXT("FailedToAdd", "failed to create editing state. The asset may be corrupted"));

			bHasErrors = true;
			
			continue;
		}

		UMetaHumanCharacterAssetEditor* MetaHumanCharacterEditor = NewObject<UMetaHumanCharacterAssetEditor>(GetTransientPackage(), NAME_None, RF_Transient);
		MetaHumanCharacterEditor->SetObjectToEdit(MetaHumanCharacter);
		MetaHumanCharacterEditor->Initialize();

		UE::MetaHuman::Analytics::RecordOpenCharacterEditorEvent(MetaHumanCharacter);
	}

	if (bHasErrors)
	{
		FMessageLog(UE::MetaHuman::MessageLogName)
			.Open();
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
