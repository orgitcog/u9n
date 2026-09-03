// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorPipelineTools.h"

#include "InteractiveToolManager.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "MetaHumanSDKSettings.h"

#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "SceneManagement.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "ToolTargetManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPtr.h"
#include "Dialogs/Dialogs.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

bool UMetaHumanCharacterEditorPipelineToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	const int32 NumTargets = InSceneState.TargetManager->CountSelectedAndTargetableWithPredicate(InSceneState, GetTargetRequirements(), [](UActorComponent& Component)
	{
		return Component.GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();
	});

	// Restrict the tool to a single target
	return NumTargets == 1;
}

UInteractiveTool* UMetaHumanCharacterEditorPipelineToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	switch (ToolType)
	{
		case EMetaHumanCharacterPipelineEditingTool::Pipeline:
		{
			UMetaHumanCharacterEditorPipelineTool* PipelineTool = NewObject<UMetaHumanCharacterEditorPipelineTool>(InSceneState.ToolManager);
			PipelineTool->SetTarget(Target);
			PipelineTool->SetTargetWorld(InSceneState.World);
			return PipelineTool;
		}

		default:
			checkNoEntry();
	}

	return nullptr;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorPipelineToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass()
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterEditorPipelineToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	//const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPipelineToolProperties, PipelineType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPipelineToolProperties, PipelineQuality))
	{
		if (PropertyChangedEvent.ChangeType & EPropertyChangeType::ValueSet)
		{
			UpdateSelectedPipeline();
		}
	}
}

void UMetaHumanCharacterEditorPipelineToolProperties::UpdateSelectedPipeline()
{
	TSoftClassPtr<UMetaHumanCharacterPipeline> PipelineClassPtr = GetSelectedPipelineClass();
	if (PipelineClassPtr != nullptr)
	{
		TNotNull<UMetaHumanCharacterEditorPipelineTool*> SkinTool = GetTypedOuter<UMetaHumanCharacterEditorPipelineTool>();
		TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(SkinTool->GetTarget());

		TObjectPtr<UMetaHumanCollectionPipeline>& CollectionPipeline = Character->PipelinesPerClass.FindOrAdd(GetSelectedPipelineClass().LoadSynchronous());
		if (CollectionPipeline == nullptr)
		{
			CollectionPipeline = NewObject<UMetaHumanCollectionPipeline>(Character, GetSelectedPipelineClass().LoadSynchronous());
		}
	}

	OnPipelineSelectionChanged.ExecuteIfBound();
}

TObjectPtr<UMetaHumanCollectionPipeline> UMetaHumanCharacterEditorPipelineToolProperties::GetSelectedPipeline() const
{
	TSoftClassPtr<UMetaHumanCollectionPipeline> PipelineClassPtr = GetSelectedPipelineClass();
	if (PipelineClassPtr != nullptr)
	{
		TNotNull<UMetaHumanCharacterEditorPipelineTool*> SkinTool = GetTypedOuter<UMetaHumanCharacterEditorPipelineTool>();
		TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(SkinTool->GetTarget());

		return Character->PipelinesPerClass.FindOrAdd(GetSelectedPipelineClass().LoadSynchronous());
	}

	return nullptr;
}

TObjectPtr<UMetaHumanCharacterEditorPipeline> UMetaHumanCharacterEditorPipelineToolProperties::GetSelectedEditorPipeline() const
{
	if (TObjectPtr<UMetaHumanCollectionPipeline> ActivePipeline = GetSelectedPipeline())
	{
		return ActivePipeline->GetMutableEditorPipeline();
	}

	return nullptr;
}

FMetaHumanCharacterEditorBuildParameters UMetaHumanCharacterEditorPipelineToolProperties::InitParametersForCollectionPipeline() const
{
	FMetaHumanCharacterEditorBuildParameters BuildParams;

	BuildParams.NameOverride = NameOverride;

	if (PipelineType == EMetaHumanDefaultPipelineType::Cinematic || PipelineType == EMetaHumanDefaultPipelineType::Optimized)
	{
		if (RootDirectory.Path.IsEmpty())
		{
			// Make the full output path to be used based on the MH SDK settings
			if (const UMetaHumanSDKSettings* Settings = GetDefault<UMetaHumanSDKSettings>())
			{
				if (PipelineQuality == EMetaHumanQualityLevel::Cinematic)
				{
					BuildParams.AbsoluteBuildPath = Settings->CinematicImportPath.Path;
				}
				else
				{
					BuildParams.AbsoluteBuildPath = Settings->OptimizedImportPath.Path;
				}
			}
		}
		else
		{
			BuildParams.AbsoluteBuildPath = RootDirectory.Path;
		}

		BuildParams.CommonFolderPath = CommonDirectory.Path;
	}

	return BuildParams;
}

FMetaHumanCharacterEditorDCCExportParameters UMetaHumanCharacterEditorPipelineToolProperties::InitParametersForDCCPipeline() const
{
	FMetaHumanCharacterEditorDCCExportParameters ExportParams;
	ExportParams.OutputFolderPath = OutputFolder.Path;
	ExportParams.ArchiveName = ArchiveName;
	ExportParams.bBakeFaceMakeup = bBakeMakeup;
	ExportParams.bExportZipFile = bExportZipFile;

	return ExportParams;
}

TSoftClassPtr<UMetaHumanCollectionPipeline> UMetaHumanCharacterEditorPipelineToolProperties::GetSelectedPipelineClass() const
{
	TSoftClassPtr<UMetaHumanCollectionPipeline> OutPipelineClass;
	
	if (const UMetaHumanCharacterPaletteProjectSettings* Settings = GetDefault<UMetaHumanCharacterPaletteProjectSettings>())
	{
		switch (PipelineType)
		{
		case EMetaHumanDefaultPipelineType::Cinematic:
			OutPipelineClass = Settings->DefaultCharacterLegacyPipelines[EMetaHumanQualityLevel::Cinematic];
			break;
		case EMetaHumanDefaultPipelineType::Optimized:
			OutPipelineClass = Settings->DefaultCharacterLegacyPipelines[PipelineQuality];
			break;
		case EMetaHumanDefaultPipelineType::UEFN:
		{
			const EMetaHumanQualityLevel ValidQuality = PipelineQuality != EMetaHumanQualityLevel::Cinematic ? PipelineQuality : EMetaHumanQualityLevel::High;
			OutPipelineClass = Settings->DefaultCharacterUEFNPipelines[ValidQuality];
		}
			break;
		case EMetaHumanDefaultPipelineType::DCC:
			break;

		default:
			checkNoEntry()
		}
	}

	return OutPipelineClass;
}

void UMetaHumanCharacterEditorPipelineTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("AssemblyToolName", "Assembly"));

	TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);

	PropertyObject = NewObject<UMetaHumanCharacterEditorPipelineToolProperties>(this);
	
	// We are not storing the previous selection, so start for the first
	PropertyObject->PipelineType = EMetaHumanDefaultPipelineType::Cinematic;
	PropertyObject->UpdateSelectedPipeline();

	// Set the MH SDK settings path as the default
	if (const UMetaHumanSDKSettings* Settings = GetDefault<UMetaHumanSDKSettings>())
	{
		// TODO: store these with the character asset?
		PropertyObject->RootDirectory.Path = Settings->CinematicImportPath.Path;
		PropertyObject->NameOverride = Character->GetName();
	}

	AddToolPropertySource(PropertyObject);

	PropertyObject->RestoreProperties(this, Character->GetName());
}

void UMetaHumanCharacterEditorPipelineTool::Shutdown(EToolShutdownType ShutdownType)
{
	TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);

	PropertyObject->SaveProperties(this, Character->GetName());

	PropertyObject->OnPipelineSelectionChanged.Unbind();
}

bool UMetaHumanCharacterEditorPipelineTool::CanBuild(FText& OutErrorMsg) const
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	if (!Character)
	{
		return false;
	}

	return UMetaHumanCharacterEditorSubsystem::Get()->CanBuildMetaHuman(Character, OutErrorMsg);
}

void UMetaHumanCharacterEditorPipelineTool::Build() const
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	if (Character)
	{
		if (PropertyObject->PipelineType == EMetaHumanDefaultPipelineType::DCC)
		{
			FMetaHumanCharacterEditorDCCExportParameters ExportParams = PropertyObject->InitParametersForDCCPipeline();

			FMetaHumanCharacterEditorDCCExport::ExportCharacterForDCC(Character, ExportParams);
		}
		else if (TObjectPtr<UMetaHumanCollectionPipeline> SelectedPipeline = PropertyObject->GetSelectedPipeline())
		{
			if (SelectedPipeline->GetEditorPipeline()->CanBuild())
			{
				FPlatformMemoryStats Stats;

				FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
				constexpr uint64 GB = 1024 * 1024 * 1024;
				if (MemoryStats.AvailableVirtual < (10 * GB))
				{
					const FText NotEnoughMemoryTitle = LOCTEXT("NotEnoughMemoryDialogTitle", "Not enough memory to assemble MetaHuman");
					const FText NotEnoughMemoryMessage = FText::Format(LOCTEXT("PipelineNotEnoughMemoryDialogMessage", "Assembling a MetaHuman Character requires at least 10 GiB of free memory but only {0} is available.\n"
																											   "If you proceed the editor might crash. Would you like to continue?"),
																	   FText::AsMemory(MemoryStats.AvailableVirtual));

					FSuppressableWarningDialog::FSetupInfo SetupInfo(NotEnoughMemoryMessage, NotEnoughMemoryTitle, TEXT("MetaHumanCharacterSupressNotEnoughMemory"));
					SetupInfo.ConfirmText = LOCTEXT("PipelineNotEnoughMemoryDialogConfirmText", "Yes");
					SetupInfo.CancelText = LOCTEXT("PipelineNotEnoughMemoryDialogCancelText", "Cancel");

					const FSuppressableWarningDialog NotEnoughMemoryDialog{ SetupInfo };

					const FSuppressableWarningDialog::EResult Result = NotEnoughMemoryDialog.ShowModal();
					if (Result == FSuppressableWarningDialog::EResult::Cancel)
					{
						return;
					}
				}

				FMetaHumanCharacterEditorBuildParameters BuildParams = PropertyObject->InitParametersForCollectionPipeline();
				BuildParams.PipelineOverride = SelectedPipeline;

				FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter(Character, BuildParams);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE 