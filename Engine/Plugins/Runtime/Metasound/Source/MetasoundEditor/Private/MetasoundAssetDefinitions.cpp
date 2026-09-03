// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAssetDefinitions.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "Components/AudioComponent.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundAssetSubsystem.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundSource.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFactory.h"
#include "MetasoundUObjectRegistry.h"
#include "Misc/AssetRegistryInterface.h"
#include "Modules/ModuleManager.h"
#include "ObjectEditorUtils.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"


#define LOCTEXT_NAMESPACE "MetaSoundEditor"


namespace Metasound::Editor
{
	namespace AssetDefinitionsPrivate
	{
		const FSlateBrush* GetClassBrush(const FAssetData& InAssetData, FName InClassName, bool bIsThumbnail = false)
		{
			using namespace Frontend;

			const FMetaSoundAssetClassInfo ClassInfo(InAssetData);
			if (!ClassInfo.bIsValid)
			{
				UE_LOG(LogMetaSound, VeryVerbose,
					TEXT("ClassBrush for asset '%s' may return incorrect preset icon. Asset requires reserialization."),
					*InAssetData.GetObjectPathString());
			}

			FString BrushName = FString::Printf(TEXT("MetasoundEditor.%s"), *InClassName.ToString());
			if (ClassInfo.DocInfo.bIsPreset)
			{
				BrushName += TEXT(".Preset");
			}
			BrushName += bIsThumbnail ? TEXT(".Thumbnail") : TEXT(".Icon");

			return &Metasound::Editor::Style::GetSlateBrushSafe(FName(*BrushName));
		}
	} // namespace AssetDefinitionsPrivate
} // namespace Metasound::Editor


FLinearColor UAssetDefinition_MetaSoundPatch::GetAssetColor() const
{
	if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
	{
		return MetasoundStyle->GetColor("MetaSoundPatch.Color").ToFColorSRGB();
	}

	return FColor::White;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaSoundPatch::GetAssetClass() const
{
	return UMetaSoundPatch::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaSoundPatch::GetAssetCategories() const
{
	static const auto Pinned_Categories = { EAssetCategoryPaths::Audio };
	static const auto Categories = { EAssetCategoryPaths::Audio / LOCTEXT("AssetSoundMetaSoundsSubMenu", "MetaSounds") };

	if (GetDefault<UMetasoundEditorSettings>()->bPinMetaSoundPatchInAssetMenu)
	{
		return Pinned_Categories;
	}

	return Categories;
}

EAssetCommandResult UAssetDefinition_MetaSoundPatch::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	Metasound::Editor::IMetasoundEditorModule& MetaSoundEditorModule = FModuleManager::GetModuleChecked<Metasound::Editor::IMetasoundEditorModule>("MetaSoundEditor");
	if (!MetaSoundEditorModule.IsRestrictedMode())
	{
		for (UMetaSoundPatch* Metasound : OpenArgs.LoadObjects<UMetaSoundPatch>())
		{
			TSharedRef<Metasound::Editor::FEditor> NewEditor = MakeShared<Metasound::Editor::FEditor>();
			NewEditor->InitMetasoundEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Metasound);
		}
	}
	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_MetaSoundPatch::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	using namespace Metasound::Editor;
	constexpr bool bIsThumbnail = true;
	return AssetDefinitionsPrivate::GetClassBrush(InAssetData, InClassName, bIsThumbnail);
}

const FSlateBrush* UAssetDefinition_MetaSoundPatch::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	using namespace Metasound::Editor;
	return AssetDefinitionsPrivate::GetClassBrush(InAssetData, InClassName);
}

FLinearColor UAssetDefinition_MetaSoundSource::GetAssetColor() const
{
 	if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
 	{
 		return MetasoundStyle->GetColor("MetaSoundSource.Color").ToFColorSRGB();
 	}
 
 	return FColor::White;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaSoundSource::GetAssetClass() const
{
	return UMetaSoundSource::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaSoundSource::GetAssetCategories() const
{
	static const auto Pinned_Categories = { EAssetCategoryPaths::Audio };
	static const auto Categories = { EAssetCategoryPaths::Audio / LOCTEXT("AssetSoundMetaSoundSourceSubMenu", "MetaSounds") };

	if (GetDefault<UMetasoundEditorSettings>()->bPinMetaSoundSourceInAssetMenu)
	{
		return Pinned_Categories;
	}

	return Categories;
}

EAssetCommandResult UAssetDefinition_MetaSoundSource::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace Metasound;

	for (const FAssetData& AssetData : OpenArgs.Assets)
	{
		const UClass* AssetClass = AssetData.GetClass();
		if (AssetClass && IMetasoundUObjectRegistry::Get().IsRegisteredClass(*AssetClass))
		{
			Engine::FMetaSoundAssetManager& AssetManager = Engine::FMetaSoundAssetManager::GetChecked();
			TWeakPtr<IToolkitHost> ToolkitHost = OpenArgs.ToolkitHost;
			const bool bHostNull = !OpenArgs.ToolkitHost.IsValid();
			AssetManager.AddOrLoadAndUpdateFromObjectAsync(AssetData, [ToolkitMode = OpenArgs.GetToolkitMode(), bHostNull, ToolkitHost](FMetaSoundAssetKey, UObject& MetaSoundObject)
			{
				TSharedPtr<IToolkitHost> HostPtr = ToolkitHost.Pin();
				if (bHostNull || HostPtr)
				{
					Editor::IMetasoundEditorModule* EditorModule = FModuleManager::GetModulePtr<Editor::IMetasoundEditorModule>("MetaSoundEditor");
					if (EditorModule)
					{
						if (EditorModule->IsRestrictedMode())
						{
							TScriptInterface<const IMetaSoundDocumentInterface> DocInterface(&MetaSoundObject);
							check(DocInterface.GetObject());
							const Frontend::FMetaSoundAssetClassInfo ClassInfo(*DocInterface.GetInterface());
							if (!ClassInfo.bIsValid || !ClassInfo.DocInfo.bIsPreset)
							{
								return;
							}
						}

						TSharedRef<Editor::FEditor> NewEditor = MakeShared<Editor::FEditor>();
						NewEditor->InitMetasoundEditor(ToolkitMode, HostPtr, &MetaSoundObject);
					}
				}
			});
		}
	}

	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_MetaSoundSource::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	using namespace Metasound::Editor;
	constexpr bool bIsThumbnail = true;
	return AssetDefinitionsPrivate::GetClassBrush(InAssetData, InClassName, bIsThumbnail);
}

const FSlateBrush* UAssetDefinition_MetaSoundSource::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	using namespace Metasound::Editor;
	return AssetDefinitionsPrivate::GetClassBrush(InAssetData, InClassName);
}

void UAssetDefinition_MetaSoundSource::ExecutePlaySound(const FToolMenuContext& InContext)
{
	if (UMetaSoundSource* MetaSoundSource = UContentBrowserAssetContextMenuContext::LoadSingleSelectedAsset<UMetaSoundSource>(InContext))
	{
		// If editor is open, call into it to play to start all visualization requirements therein
		// specific to auditioning MetaSounds (ex. priming audio bus used for volume metering, playtime
		// widget, etc.)
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*MetaSoundSource);
		if (Editor.IsValid())
		{
			Editor->Play();
			return;
		}

		Metasound::Editor::FGraphBuilder::FGraphBuilder::RegisterGraphWithFrontend(*MetaSoundSource);
		UAssetDefinition_SoundBase::ExecutePlaySound(InContext);
	}
}

void UAssetDefinition_MetaSoundSource::ExecuteStopSound(const FToolMenuContext& InContext)
{
	if (UMetaSoundSource* MetaSoundSource = UContentBrowserAssetContextMenuContext::LoadSingleSelectedAsset<UMetaSoundSource>(InContext))
	{
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*MetaSoundSource);
		if (Editor.IsValid())
		{
			Editor->Stop();
			return;
		}

		UAssetDefinition_SoundBase::ExecuteStopSound(InContext);
	}
}

bool UAssetDefinition_MetaSoundSource::CanExecutePlayCommand(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::CanExecutePlayCommand(InContext);
}

ECheckBoxState UAssetDefinition_MetaSoundSource::IsActionCheckedMute(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::IsActionCheckedMute(InContext);
}

ECheckBoxState UAssetDefinition_MetaSoundSource::IsActionCheckedSolo(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::IsActionCheckedSolo(InContext);
}

void UAssetDefinition_MetaSoundSource::ExecuteMuteSound(const FToolMenuContext& InContext)
{
	UAssetDefinition_SoundBase::ExecuteMuteSound(InContext);
}

void UAssetDefinition_MetaSoundSource::ExecuteSoloSound(const FToolMenuContext& InContext)
{
	UAssetDefinition_SoundBase::ExecuteSoloSound(InContext);
}

bool UAssetDefinition_MetaSoundSource::CanExecuteMuteCommand(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::CanExecuteMuteCommand(InContext);
}

bool UAssetDefinition_MetaSoundSource::CanExecuteSoloCommand(const FToolMenuContext& InContext)
{
	return UAssetDefinition_SoundBase::CanExecuteSoloCommand(InContext);
}

TSharedPtr<SWidget> UAssetDefinition_MetaSoundSource::GetThumbnailOverlay(const FAssetData& InAssetData) const
{
	auto OnClickedLambdaOverride = [InAssetData]() -> FReply
	{
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*InAssetData.GetAsset());
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			if (Editor.IsValid())
			{
				Editor->Stop();
			}
			else
			{
				UE::AudioEditor::StopSound();
			}
		}
		else
		{
			if (Editor.IsValid())
			{
				Editor->Play();
			}
			else
			{
				// Load and play sound
				UE::AudioEditor::PlaySound(Cast<USoundBase>(InAssetData.GetAsset()));
			}
		}
		return FReply::Handled();
	};
	return UAssetDefinition_SoundBase::GetSoundBaseThumbnailOverlay(InAssetData, MoveTemp(OnClickedLambdaOverride));
}

bool UAssetDefinition_MetaSoundSource::GetThumbnailActionOverlay(const FAssetData& InAssetData, FAssetActionThumbnailOverlayInfo& OutActionOverlayInfo) const
{
	auto OnGetDisplayBrushLambda = [InAssetData]() -> const FSlateBrush*
	{
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			return FAppStyle::GetBrush("ContentBrowser.AssetAction.StopIcon");
		}

		return FAppStyle::GetBrush("ContentBrowser.AssetAction.PlayIcon");
	};

	OutActionOverlayInfo.ActionImageWidget = SNew(SImage).Image_Lambda(OnGetDisplayBrushLambda);

	auto OnToolTipTextLambda = [InAssetData]() -> FText
	{
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			return LOCTEXT("Thumbnail_StopSoundToolTip", "Stop selected sound");
		}

		return LOCTEXT("Thumbnail_PlaySoundToolTip", "Play selected sound");
	};

	auto OnClickedLambda = [InAssetData]() -> FReply
	{
		TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*InAssetData.GetAsset());
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			if (Editor.IsValid())
			{
				Editor->Stop();
			}
			else
			{
				UE::AudioEditor::StopSound();
			}
		}
		else
		{
			if (Editor.IsValid())
			{
				Editor->Play();
			}
			else
			{
				// Load and play sound
				UE::AudioEditor::PlaySound(Cast<USoundBase>(InAssetData.GetAsset()));
			}
		}
		return FReply::Handled();
	};

	OutActionOverlayInfo.ActionButtonArgs = SButton::FArguments()
		.ToolTipText_Lambda(OnToolTipTextLambda)
		.OnClicked_Lambda(OnClickedLambda);

	return true;
}

EAssetCommandResult UAssetDefinition_MetaSoundSource::ActivateAssets(const FAssetActivateArgs& ActivateArgs) const
{
	if (ActivateArgs.ActivationMethod == EAssetActivationMethod::Previewed)
	{
		if (UMetaSoundSource* MetaSoundSource = ActivateArgs.LoadFirstValid<UMetaSoundSource>())
		{
			TSharedPtr<Metasound::Editor::FEditor> Editor = Metasound::Editor::FGraphBuilder::GetEditorForMetasound(*MetaSoundSource);
			UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
	
			// If the editor is open, we need to stop or start the editor so it can light up while previewing it in the CB
			if (Editor.IsValid())
			{
				if (PreviewComp && PreviewComp->IsPlaying())
				{
					if (!MetaSoundSource || PreviewComp->Sound == MetaSoundSource)
					{
						Editor->Stop();
					}
				}
				else
				{
					Editor->Play();
				}

				return EAssetCommandResult::Handled;
			}
			else
			{
				return UAssetDefinition_SoundBase::ActivateSoundBase(ActivateArgs);
			}
		}
	}
	return EAssetCommandResult::Unhandled;
}

void UAssetDefinition_MetaSoundSource::GetAssetActionButtonExtensions(const FAssetData& InAssetData, TArray<FAssetButtonActionExtension>& OutExtensions) const
{
	UAssetDefinition_SoundBase::GetSoundBaseAssetActionButtonExtensions(InAssetData, OutExtensions);
}

namespace MenuExtension_MetaSoundSourceTemplate
{
	template <typename TClass, typename TFactoryClass>
	void ExecuteCreateMetaSoundPreset(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			using namespace Metasound::Editor;
			TArray<UObject*> ObjectsToSync;

			for (TClass* ReferencedMetaSound : Context->LoadSelectedObjects<TClass>())
			{
				FString PackagePath;
				FString AssetName;
				UObject* NewMetaSound;

				IAssetTools::Get().CreateUniqueAssetName(ReferencedMetaSound->GetOutermost()->GetName(), TEXT("_Preset"), PackagePath, AssetName);

				Metasound::Editor::IMetasoundEditorModule& MetaSoundEditorModule = FModuleManager::GetModuleChecked<Metasound::Editor::IMetasoundEditorModule>("MetaSoundEditor");
				if (MetaSoundEditorModule.IsRestrictedMode())
				{
					// Cannot duplicate cooked assets in restricted mode, so create new object and copy over properties
					// for sources in InitAsset below. Since copying properties is done manually, 
					// SetSoundWaveSettingsFromTemplate may need to be updated with properties to be copied. 
					UMetaSoundBaseFactory* Factory = NewObject<TFactoryClass>();
					Factory->ReferencedMetaSoundObject = ReferencedMetaSound;

					NewMetaSound = IAssetTools::Get().CreateAssetWithDialog(AssetName, FPackageName::GetLongPackagePath(PackagePath), Factory->GetSupportedClass(), Factory);
				}
				else
				{
					// Duplicate asset to preserve properties from referenced asset (ex. quality settings, soundwave properties)
					NewMetaSound = IAssetTools::Get().DuplicateAssetWithDialogAndTitle(AssetName, FPackageName::GetLongPackagePath(PackagePath), ReferencedMetaSound, LOCTEXT("CreateMetaSoundPresetTitle", "Create MetaSound Preset"));
				}
				
				if (NewMetaSound)
				{
					UMetaSoundEditorSubsystem::GetChecked().InitAsset(*NewMetaSound, ReferencedMetaSound, /*bClearDocument=*/true);

					FGraphBuilder::RegisterGraphWithFrontend(*NewMetaSound);
					ObjectsToSync.Add(NewMetaSound);
				}
				else
				{
					UE_LOG(LogMetaSound, Display, TEXT("Error creating new asset when creating preset '%s' or asset creation was canceled by user."), *AssetName);
				}
			}

			// Sync content browser to newly created valid assets
			// Assets can be invalid if multiple assets are created with the same name 
			// then force overwritten within the same operation
			ObjectsToSync.RemoveAllSwap([](const UObject* InObject)
			{
				return !InObject || !InObject->IsValidLowLevelFast();
			});

			if (ObjectsToSync.Num() > 0)
			{
				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
			}
		}
	}

 	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
 		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
 		{
 			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
 
 			{
				UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMetaSoundSource::StaticClass());

				FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				{

					Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
					{
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_PlaySound", "Play");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Play.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecutePlaySound);
							UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::CanExecutePlayCommand);
							InSection.AddMenuEntry("Sound_PlaySound", Label, ToolTip, Icon, UIAction);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_StopSound", "Stop");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Stop.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecuteStopSound);
							InSection.AddMenuEntry("Sound_StopSound", Label, ToolTip, Icon, UIAction);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_MuteSound", "Mute");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_MuteSoundTooltip", "Mutes the selected sounds.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Mute.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecuteMuteSound);
							UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::CanExecuteMuteCommand);
							UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&UAssetDefinition_MetaSoundSource::IsActionCheckedMute);
							InSection.AddMenuEntry("Sound_SoundMute", Label, ToolTip, Icon, UIAction, EUserInterfaceActionType::ToggleButton);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("Sound_SoloSound", "Solo");
							const TAttribute<FText> ToolTip = LOCTEXT("Sound_SoloSoundTooltip", "Solos the selected sounds.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Solo.Small");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::ExecuteSoloSound);
							UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&UAssetDefinition_MetaSoundSource::CanExecuteSoloCommand);
							UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&UAssetDefinition_MetaSoundSource::IsActionCheckedSolo);
							InSection.AddMenuEntry("Sound_StopSolo", Label, ToolTip, Icon, UIAction, EUserInterfaceActionType::ToggleButton);
						}
						{
							const TAttribute<FText> Label = LOCTEXT("MetaSoundSource_CreatePreset", "Create MetaSound Source Preset");
							const TAttribute<FText> ToolTip = LOCTEXT("MetaSoundSource_CreatePresetToolTip", "Creates a MetaSoundSource Preset using the selected MetaSound's root graph as a reference.");
							const FSlateIcon Icon = Metasound::Editor::Style::CreateSlateIcon("ClassIcon.MetasoundSource");

							FToolUIAction UIAction;
							UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreateMetaSoundPreset<UMetaSoundSource, UMetaSoundSourceFactory>);
							InSection.AddMenuEntry("MetaSoundSource_CreatePreset", Label, ToolTip, Icon, UIAction);
						}
					}));
				}
 			}
	
			{
				UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMetaSoundPatch::StaticClass());

				FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
				{
					const TAttribute<FText> Label = LOCTEXT("MetaSoundPatch_CreatePreset", "Create MetaSound Patch Preset");
					const TAttribute<FText> ToolTip = LOCTEXT("MetaSoundPatch_CreatePresetToolTip", "Creates a MetaSoundSource Patch Preset using the selected MetaSound Patch's root graph as a reference.");

					const FSlateIcon Icon = Metasound::Editor::Style::CreateSlateIcon("ClassIcon.MetasoundPatch");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreateMetaSoundPreset<UMetaSoundPatch, UMetaSoundFactory>);
					InSection.AddMenuEntry("MetaSoundPatch_CreatePreset", Label, ToolTip, Icon, UIAction);
				}));
			}
 		}));
	});
}


#undef LOCTEXT_NAMESPACE //MetaSoundEditor
