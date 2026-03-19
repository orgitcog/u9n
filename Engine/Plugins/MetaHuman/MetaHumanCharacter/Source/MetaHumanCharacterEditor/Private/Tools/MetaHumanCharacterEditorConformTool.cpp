// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorConformTool.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorToolTargetUtil.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "DNAUtils.h"
#include "Editor/EditorEngine.h"
#include "Misc/ScopedSlowTask.h"
#include "DNAUtils.h"


#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorConformTool"

extern UNREALED_API UEditorEngine* GEditor;


class FConformToolStateCommandChange : public FToolCommandChange
{
public:

	FConformToolStateCommandChange(
		TSharedRef<const FMetaHumanCharacterIdentity::FState> InOldState,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldState{ InOldState }
		, NewState{ UMetaHumanCharacterEditorSubsystem::Get()->CopyFaceState(InCharacter) }
		, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitFaceState(Character, NewState);
	}

	virtual void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitFaceState(Character, OldState);
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}
	//~End FToolCommandChange interface


protected:

	TSharedRef<const FMetaHumanCharacterIdentity::FState> OldState;
	TSharedRef<const FMetaHumanCharacterIdentity::FState> NewState;

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;

};


class FConformToolDNACommandChange : public FToolCommandChange
{
public:

	FConformToolDNACommandChange(
		const TArray<uint8>& InOldDNABuffer,
		TSharedRef<const FMetaHumanCharacterIdentity::FState> InOldState,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldDNABuffer{ InOldDNABuffer }
		, NewDNABuffer{ InCharacter->GetFaceDNABuffer() }
		, OldState{ InOldState }
		, NewState{ UMetaHumanCharacterEditorSubsystem::Get()->CopyFaceState(InCharacter) }
		, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual void Apply(UObject* InObject) override
	{
		ApplyChange(InObject, NewDNABuffer, NewState);
	}

	virtual void Revert(UObject* InObject) override
	{
		ApplyChange(InObject, OldDNABuffer, OldState);
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}
	//~End FToolCommandChange interface


protected:

	void ApplyChange(UObject* InObject, const TArray<uint8>& InDNABuffer, TSharedRef<const FMetaHumanCharacterIdentity::FState> InState)
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

		// if an empty buffer, remove the rig from the character (special case)
		if (InDNABuffer.IsEmpty())
		{
			UMetaHumanCharacterEditorSubsystem::Get()->RemoveFaceRig(Character);
		}
		else
		{
			TArray<uint8> BufferCopy;
			BufferCopy.SetNumUninitialized(InDNABuffer.Num());
			FMemory::Memcpy(BufferCopy.GetData(), InDNABuffer.GetData(), InDNABuffer.Num());
			UMetaHumanCharacterEditorSubsystem::Get()->CommitFaceDNA(Character, ReadDNAFromBuffer(&BufferCopy, EDNADataLayer::All).ToSharedRef());
		}

		// reset the face state
		UMetaHumanCharacterEditorSubsystem::Get()->CommitFaceState(Character, InState);
	}

	TArray<uint8> OldDNABuffer;
	TArray<uint8> NewDNABuffer;

	TSharedRef<const FMetaHumanCharacterIdentity::FState> OldState;
	TSharedRef<const FMetaHumanCharacterIdentity::FState> NewState;


	TWeakObjectPtr<UInteractiveToolManager> ToolManager;

};

UInteractiveTool* UMetaHumanCharacterEditorConformToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	UMetaHumanCharacterEditorConformTool* ConformTool = NewObject<UMetaHumanCharacterEditorConformTool>(InSceneState.ToolManager);
	ConformTool->SetTarget(Target);

	return ConformTool;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorConformToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

void UMetaHumanCharacterImportSubToolBase::DisplayConformError(const FText& ErrorMessageText) const
{
	UInteractiveTool* OwnerTool = GetTypedOuter<UInteractiveTool>();
	check(OwnerTool);

	OwnerTool->GetToolManager()->DisplayMessage(ErrorMessageText, EToolMessageLevel::UserError);
	FMessageLog(UE::MetaHuman::MessageLogName).Error(ErrorMessageText);
	FMessageLog(UE::MetaHuman::MessageLogName).Open(EMessageSeverity::Error, false);
}

bool UMetaHumanCharacterImportDNAProperties::CanImport() const
{
	return FPaths::FileExists(DNAFile.FilePath);
}

void UMetaHumanCharacterImportDNAProperties::Import()
{
	const FText ErrorMessagePrefix = FText::Format(LOCTEXT("ImportDNAErrorPrefix", "Failed to import DNA file '{FilePath}'"), 
		FFormatNamedArguments{ {TEXT("FilePath"), FText::FromString(DNAFile.FilePath)} });

	const float ImportWorkProgress = 2.0f;
	FScopedSlowTask ImportDnaTask(ImportWorkProgress, LOCTEXT("ImportDnaTaskMessage", "Importing face from DNA"));
	ImportDnaTask.MakeDialog();

	UMetaHumanCharacterEditorConformTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorConformTool>();
	check(OwnerTool);

	if (!FPaths::FileExists(DNAFile.FilePath))
	{
		DisplayConformError(FText::Format(LOCTEXT("DNAFileDoesntExistError", "{0}. File doesn't exist"), ErrorMessagePrefix));
		return;
	}

	if (TSharedPtr<IDNAReader> DNAReader = ReadDNAFromFile(DNAFile.FilePath))
	{
		ImportDnaTask.EnterProgressFrame(0.5f);

		UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
		check(Character);

		EImportErrorCode ErrorCode = UMetaHumanCharacterEditorSubsystem::Get()->ImportFromFaceDna(Character, DNAReader.ToSharedRef(), ImportOptions);

		if (ErrorCode == EImportErrorCode::Success)
		{
			// Add command change to undo stack
			if (ImportOptions.bImportWholeRig)
			{
				TUniquePtr<FConformToolDNACommandChange> CommandChange = MakeUnique<FConformToolDNACommandChange>(
					OwnerTool->GetOriginalDNABuffer(),
					OwnerTool->GetOriginalState(),
					Character,
					OwnerTool->GetToolManager());
				OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("ConformToolDNAWholeRigCommandChangeUndo", "Conform Tool DNA Import Whole Rig"));
				
				// update original state and DNA in tool so undo works as expected
				OwnerTool->UpdateOriginalState();
				OwnerTool->UpdateOriginalDNABuffer();
			}
			else
			{
				TUniquePtr<FConformToolStateCommandChange> CommandChange = MakeUnique<FConformToolStateCommandChange>(
					OwnerTool->GetOriginalState(),
					Character,
					OwnerTool->GetToolManager());
				OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("ConformToolDNACommandChangeUndo", "Conform Tool DNA Import"));

				// update original state so undo works as expected
				OwnerTool->UpdateOriginalState();
			}

			
			// make sure we clear any errors
			OwnerTool->GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserError); 

			// broadcast that the rigging state has changed
			if (ImportOptions.bImportWholeRig)
			{
				Character->OnRiggingStateChanged.Broadcast();
			}
		}
		else
		{
			FText ErrorMessageText;
			switch (ErrorCode)
			{
			case EImportErrorCode::FittingError:
				ErrorMessageText = FText::Format(LOCTEXT("FailedToFitToDNA", "{0}. Failed to fit to DNA"), ErrorMessagePrefix);
				break;
			case EImportErrorCode::InvalidInputData:
				ErrorMessageText = FText::Format(LOCTEXT("FailedToImportDNAInvalidInputData", "{0}. DNA is not consistent with MetaHuman topology"), ErrorMessagePrefix);
				break;
			default:
				// just give a general error message
				ErrorMessageText = FText::Format(LOCTEXT("FailedToImportDNAGeneral", "{0}"), ErrorMessagePrefix);
				break;
			}

			DisplayConformError(ErrorMessageText);
		}


	}
	else
	{
		DisplayConformError(FText::Format(LOCTEXT("FailedToReadDNAFileError", "{0}. Failed to read DNA file"), ErrorMessagePrefix));
	}
}

bool UMetaHumanCharacterImportIdentityProperties::CanImport() const
{
	return !MetaHumanIdentity.IsNull();
}

void UMetaHumanCharacterImportIdentityProperties::Import()
{
	const float ImportWorkProgress = 2.0f;
	FScopedSlowTask ImportIdentityTask(ImportWorkProgress, LOCTEXT("ImportIdentityTaskMessage", "Importing face from Identity asset"));
	ImportIdentityTask.MakeDialog();

	TNotNull < UMetaHumanCharacterEditorConformTool*> OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorConformTool>();
	TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	ImportIdentityTask.EnterProgressFrame(0.5f);
	TNotNull<UMetaHumanIdentity*> ImportedMetaHumanIdentity = MetaHumanIdentity.LoadSynchronous();

	ImportIdentityTask.EnterProgressFrame(1.5f);
	EImportErrorCode ErrorCode = UMetaHumanCharacterEditorSubsystem::Get()->ImportFromIdentity(Character, ImportedMetaHumanIdentity, ImportOptions);

	if (ErrorCode == EImportErrorCode::Success)
	{
		// Add command change to undo stack
		TUniquePtr<FConformToolStateCommandChange> CommandChange = MakeUnique<FConformToolStateCommandChange>(
			OwnerTool->GetOriginalState(),
			Character,
			OwnerTool->GetToolManager());

		OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("ConformToolIdentityCommandChangeUndo", "Conform Tool Identity Import"));
		// make sure we clear any errors
		OwnerTool->GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserError);

		// update original state so undo works as expected
		OwnerTool->UpdateOriginalState();
	}
	else
	{
		FText ErrorMessageText;
		switch (ErrorCode)
		{
		case EImportErrorCode::FittingError:
			ErrorMessageText = LOCTEXT("FailedToImportIdentityFittingError", "Failed to import Identity: fitting error");
			break;
		case EImportErrorCode::NoHeadMeshPresent:
			ErrorMessageText = LOCTEXT("FailedToImportIdentityNoHeadMeshPresentError", "Failed to import Identity: no conformed head mesh present");
			break;
		case EImportErrorCode::NoEyeMeshesPresent:
			ErrorMessageText = LOCTEXT("FailedToImportIdentityNoEyeMeshesPresentError", "Failed to import Identity: no conformed eye meshes present");
			break;
		case EImportErrorCode::NoTeethMeshPresent:
			ErrorMessageText = LOCTEXT("FailedToImportIdentityNoTeethMeshPresentError", "Failed to import Identity: no conformed teeth mesh present");
			break;
		case EImportErrorCode::IdentityNotConformed:
			ErrorMessageText = LOCTEXT("FailedToImportIdentityIdentityNotConformedError", "Failed to import Identity: Identity asset has not been conformed");
			break;
		default:
			// just give a general error message
			ErrorMessageText = LOCTEXT("FailedToImportIdentityGeneral", "Failed to import Identity");
			break;
		}

		DisplayConformError(ErrorMessageText);
	}

}

bool UMetaHumanCharacterImportTemplateProperties::CanImport() const
{
	return !Mesh.IsNull();
}

void UMetaHumanCharacterImportTemplateProperties::Import()
{
	const float ImportWorkProgress = 2.0f;
	FScopedSlowTask ImportTemplateTask(ImportWorkProgress, LOCTEXT("ImportTemplateTaskMessage", "Importing face from Template Mesh asset"));
	ImportTemplateTask.MakeDialog();

	TNotNull < UMetaHumanCharacterEditorConformTool*> OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorConformTool>();
	TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	ImportTemplateTask.EnterProgressFrame(0.5f);
	TNotNull<UObject*> ImportedMetaHumanTemplate = Mesh.LoadSynchronous();

	UObject* ImportedLeftEyeMesh = nullptr;
	UObject* ImportedRightEyeMesh = nullptr;
	UObject* ImportedTeethMesh = nullptr;

	if (Cast<UStaticMesh>(ImportedMetaHumanTemplate))
	{
		if (!LeftEyeMesh.IsNull())
		{
			ImportedLeftEyeMesh = LeftEyeMesh.LoadSynchronous();
		}
		if (!RightEyeMesh.IsNull())
		{
			ImportedRightEyeMesh = RightEyeMesh.LoadSynchronous();
		}
		if (!TeethMesh.IsNull())
		{
			ImportedTeethMesh = TeethMesh.LoadSynchronous();
		}
	}

	ImportTemplateTask.EnterProgressFrame(1.5f);
	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	EImportErrorCode ErrorCode = Subsystem->ImportFromTemplate(Character, ImportedMetaHumanTemplate, ImportedLeftEyeMesh,
		ImportedRightEyeMesh, ImportedTeethMesh, ImportOptions);

	if (ErrorCode == EImportErrorCode::Success)
	{
		// Add command change to undo stack
		TUniquePtr<FConformToolStateCommandChange> CommandChange = MakeUnique<FConformToolStateCommandChange>(
			OwnerTool->GetOriginalState(),
			Character,
			OwnerTool->GetToolManager());

		OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("ConformToolTemplateCommandChangeUndo", "Conform Tool Template Import"));	
		// make sure we clear any errors
		OwnerTool->GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserError);

		// update original state so undo works as expected
		OwnerTool->UpdateOriginalState();
	}
	else
	{
		FText ErrorMessageText;
		switch (ErrorCode)
		{
		case EImportErrorCode::FittingError:
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateFittingError", "Failed to import Template Mesh: failed to fit to mesh.");
			break;
		case EImportErrorCode::InvalidInputData:
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateInvalidInputError", "Failed to import Template Mesh: input mesh is not consistent with MetaHuman topology");
			break;
		case EImportErrorCode::InvalidHeadMesh:
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateInvalidHeadMeshError", "Failed to import Template Mesh: input head mesh is not consistent with MetaHuman topology");
			break;
		case EImportErrorCode::InvalidLeftEyeMesh:
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateInvalidLeftEyeMeshError", "Failed to import Template Mesh: input left eye mesh is not consistent with MetaHuman topology");
			break;
		case EImportErrorCode::InvalidRightEyeMesh:
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateInvalidRightEyeMeshError", "Failed to import Template Mesh: input right eye mesh is not consistent with MetaHuman topology");
			break;
		case EImportErrorCode::InvalidTeethMesh:
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateInvalidTeethMeshError", "Failed to import Template Mesh: input teeth mesh is not consistent with MetaHuman topology");
			break;
		default:
			// just give a general error message
			ErrorMessageText = LOCTEXT("FailedToImportFromTemplateGeneral", "Failed to import Template Mesh");
			break;
		}

		DisplayConformError(ErrorMessageText);
	}
}

void UMetaHumanCharacterEditorConformTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("ConformToolName", "Conform"));

	// Save the original state to restored in case the tool is cancelled
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	const UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	OriginalState = Subsystem->CopyFaceState(MetaHumanCharacter);
	OriginalDNABuffer = MetaHumanCharacter->GetFaceDNABuffer();

	ImportDNAProperties = NewObject<UMetaHumanCharacterImportDNAProperties>(this);
	ImportDNAProperties->RestoreProperties(this);

	ImportIdentityProperties = NewObject<UMetaHumanCharacterImportIdentityProperties>(this);
	ImportIdentityProperties->RestoreProperties(this);

	ImportTemplateProperties = NewObject<UMetaHumanCharacterImportTemplateProperties>(this);
	ImportTemplateProperties->RestoreProperties(this);

	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	SubTools->RegisterSubTools(
	{
		{ Commands.BeginConformImportDNATool, ImportDNAProperties },
		{ Commands.BeginConformImportIdentityTool, ImportIdentityProperties },
		{ Commands.BeginConformImportTemplateTool, ImportTemplateProperties },
	});
}


void UMetaHumanCharacterEditorConformTool::UpdateOriginalState()
{
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	const UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	OriginalState = Subsystem->CopyFaceState(MetaHumanCharacter);
}


void UMetaHumanCharacterEditorConformTool::UpdateOriginalDNABuffer()
{
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	const UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	OriginalState = Subsystem->CopyFaceState(MetaHumanCharacter);
	OriginalDNABuffer = MetaHumanCharacter->GetFaceDNABuffer();
}

void UMetaHumanCharacterEditorConformTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	ImportDNAProperties->SaveProperties(this);
	ImportIdentityProperties->SaveProperties(this);
	ImportTemplateProperties->SaveProperties(this);
}

TSharedRef<const FMetaHumanCharacterIdentity::FState> UMetaHumanCharacterEditorConformTool::GetOriginalState() const
{
	return OriginalState.ToSharedRef();
}

const TArray<uint8>& UMetaHumanCharacterEditorConformTool::GetOriginalDNABuffer() const
{
	return OriginalDNABuffer;
}


#undef LOCTEXT_NAMESPACE