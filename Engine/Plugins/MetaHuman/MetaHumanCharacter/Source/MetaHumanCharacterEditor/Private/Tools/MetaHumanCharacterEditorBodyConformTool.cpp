// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorBodyConformTool.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorToolTargetUtil.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "Logging/MessageLog.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "DNAUtils.h"
#include "Editor/EditorEngine.h"
#include "Misc/ScopedSlowTask.h"
#include "DNAUtils.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorBodyConformTool"

class FBodyConformToolStateCommandChange : public FToolCommandChange
{
public:

	FBodyConformToolStateCommandChange(
		TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InOldState,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldState{ InOldState }
	, NewState{ UMetaHumanCharacterEditorSubsystem::Get()->CopyBodyState(InCharacter) }
	, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyState(Character, NewState);
	}

	virtual void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyState(Character, OldState);
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}
	//~End FToolCommandChange interface


protected:

	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> OldState;
	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> NewState;

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;

};

class FBodyConformToolDNACommandChange : public FToolCommandChange
{
public:

	FBodyConformToolDNACommandChange(
		const TArray<uint8>& InOldDNABuffer,
		TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InOldState,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldDNABuffer{ InOldDNABuffer }
		, NewDNABuffer{ InCharacter->GetBodyDNABuffer() }
		, OldState{ InOldState }
		, NewState{ UMetaHumanCharacterEditorSubsystem::Get()->CopyBodyState(InCharacter) }
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

	void ApplyChange(UObject* InObject, const TArray<uint8>& InDNABuffer, TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InState)
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

		// if an empty buffer, remove the rig from the character (special case)
		if (InDNABuffer.IsEmpty())
		{
			UMetaHumanCharacterEditorSubsystem::Get()->RemoveBodyRig(Character);
		}
		else
		{
			TArray<uint8> BufferCopy;
			BufferCopy.SetNumUninitialized(InDNABuffer.Num());
			FMemory::Memcpy(BufferCopy.GetData(), InDNABuffer.GetData(), InDNABuffer.Num());
			UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyDNA(Character, ReadDNAFromBuffer(&BufferCopy, EDNADataLayer::All).ToSharedRef());
		}

		// reset the face state
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyState(Character, InState);
	}

	TArray<uint8> OldDNABuffer;
	TArray<uint8> NewDNABuffer;

	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> OldState;
	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> NewState;


	TWeakObjectPtr<UInteractiveToolManager> ToolManager;

};

UInteractiveTool* UMetaHumanCharacterEditorBodyConformToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	UMetaHumanCharacterEditorBodyConformTool* ConformTool = NewObject<UMetaHumanCharacterEditorBodyConformTool>(InSceneState.ToolManager);
	ConformTool->SetTarget(Target);

	return ConformTool;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorBodyConformToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

bool UMetaHumanCharacterImportBodyDNAProperties::CanImport() const
{
	return FPaths::FileExists(DNAFile.FilePath);
}

void UMetaHumanCharacterImportBodyDNAProperties::Import()
{
	const FText ErrorMessagePrefix = FText::Format(LOCTEXT("ImportDNAErrorPrefix", "Failed to import DNA file '{FilePath}'"), 
		FFormatNamedArguments{ {TEXT("FilePath"), FText::FromString(DNAFile.FilePath)} });

	const float ImportWorkProgress = 2.0f;
	FScopedSlowTask ImportDnaTask(ImportWorkProgress, LOCTEXT("ImportBodyDNATaskMessage", "Importing body from DNA"));
	ImportDnaTask.MakeDialog();

	UMetaHumanCharacterEditorBodyConformTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyConformTool>();
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

		EImportErrorCode ErrorCode = UMetaHumanCharacterEditorSubsystem::Get()->ImportFromBodyDna(Character, DNAReader.ToSharedRef(), ImportOptions);

		if (ErrorCode == EImportErrorCode::Success)
		{
			// Add command change to undo stack
			if (ImportOptions.bImportWholeRig)
			{
				TUniquePtr<FBodyConformToolDNACommandChange> CommandChange = MakeUnique<FBodyConformToolDNACommandChange>(
					OwnerTool->GetOriginalDNABuffer(),
					OwnerTool->GetOriginalState(),
					Character,
					OwnerTool->GetToolManager());
				OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("BodyConformToolDNAWholeRigCommandChangeUndo", "Body Conform Tool DNA Import Whole Rig"));
			
				// update original state and DNA in tool so undo works as expected
				OwnerTool->UpdateOriginalState();
				OwnerTool->UpdateOriginalDNABuffer();
			}
			else
			{
				TUniquePtr<FBodyConformToolStateCommandChange> CommandChange = MakeUnique<FBodyConformToolStateCommandChange>(
					OwnerTool->GetOriginalState(),
					Character,
					OwnerTool->GetToolManager());
				OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("BodyConformToolDNACommandChangeUndo", "Body Conform Tool DNA Import"));
			
				// update original state so undo works as expected
				OwnerTool->UpdateOriginalState();
			}

			// make sure we clear any errors
			OwnerTool->GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserError);
		}
		else
		{
			FText ErrorMessageText;
			switch (ErrorCode)
			{
			case EImportErrorCode::FittingError:
				ErrorMessageText = FText::Format(LOCTEXT("FailedToFitToBodyDNA", "{0}. Failed to fit to body DNA"), ErrorMessagePrefix);
				break;
			case EImportErrorCode::InvalidInputData:
				ErrorMessageText = FText::Format(LOCTEXT("FailedToImportBodyDNAInvalidInputData", "{0}. DNA is not consistent with MetaHuman topology"), ErrorMessagePrefix);
				break;
			case EImportErrorCode::CombinedBodyCannotBeImportedAsWholeRig:
				ErrorMessageText = FText::Format(LOCTEXT("FailedToImportCombinedDNAAsRig", "{0}. Cannot import combined body DNA as a rig. Uncheck the 'Import Whole Rig' checkbox to fit the MetaHumanCharacter to the combined body DNA."), ErrorMessagePrefix);
				break;
			default:
				// just give a general error message
				ErrorMessageText = FText::Format(LOCTEXT("FailedToImportBodyDNAGeneral", "{0}"), ErrorMessagePrefix);
				break;
			}

			DisplayConformError(ErrorMessageText);
		}
	}
	else
	{
		DisplayConformError(FText::Format(LOCTEXT("FailedToReadBodyDNAFileError", "{0}. Failed to read DNA file"), ErrorMessagePrefix));
	}
}

bool UMetaHumanCharacterImportBodyTemplateProperties::CanImport() const
{
	return !Mesh.IsNull();
}

void UMetaHumanCharacterImportBodyTemplateProperties::Import()
{
	const float ImportWorkProgress = 2.0f;
	FScopedSlowTask ImportTemplateTask(ImportWorkProgress, LOCTEXT("ImportTemplateTaskMessage", "Importing body from Template Mesh asset"));
	ImportTemplateTask.MakeDialog();

	TNotNull < UMetaHumanCharacterEditorBodyConformTool*> OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyConformTool>();
	TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	ImportTemplateTask.EnterProgressFrame(0.5f);
	TNotNull<UObject*> ImportedMetaHumanTemplate = Mesh.LoadSynchronous();

	ImportTemplateTask.EnterProgressFrame(1.5f);
	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	EImportErrorCode ErrorCode = Subsystem->ImportFromBodyTemplate(Character, ImportedMetaHumanTemplate, BodyFitOptions);
	
	if (ErrorCode == EImportErrorCode::Success)
	{
		// Add command change to undo stack
		TUniquePtr<FBodyConformToolStateCommandChange> CommandChange = MakeUnique<FBodyConformToolStateCommandChange>(
			OwnerTool->GetOriginalState(),
			Character,
			OwnerTool->GetToolManager());

		// update original state in tool so undo works as expected
		OwnerTool->UpdateOriginalState();

		OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), LOCTEXT("BodyConformToolTemplateCommandChangeUndo", "Body Conform Tool Template Import"));
	
		// make sure we clear any errors
		OwnerTool->GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserError);
	}
	else
	{
		FText ErrorMessageText;
		switch (ErrorCode)
		{
		case EImportErrorCode::FittingError:
			ErrorMessageText = LOCTEXT("FailedToFitToToBodyTemplate", "Failed to import Template Mesh: failed to fit to mesh");
			break;
		case EImportErrorCode::InvalidInputData:
			ErrorMessageText = LOCTEXT("FailedToImportBodyTemplateInvalidInputData", "Failed to import Template Mesh: input mesh is not consistent with MetaHuman topology");
			break;
		case EImportErrorCode::InvalidInputBones:
			ErrorMessageText = LOCTEXT("FailedToImportBodyTemplateInvalidInputBones", "Failed to import Template Mesh: input mesh bones are not consistent with MetaHuman topology");
			break;
		default:
			// just give a general error message
			ErrorMessageText = LOCTEXT("FailedToImportBodyMeshGeneral", "Failed to import Template Mesh");
			break;
		}

		DisplayConformError(ErrorMessageText);
	}

}

void UMetaHumanCharacterEditorBodyConformTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("BodyConformToolName", "Conform"));

	// Save the original state to restored in case the tool is cancelled
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	const UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	OriginalState = Subsystem->CopyBodyState(MetaHumanCharacter);
	OriginalDNABuffer = MetaHumanCharacter->GetBodyDNABuffer();

	ImportDNAProperties = NewObject<UMetaHumanCharacterImportBodyDNAProperties>(this);
	ImportDNAProperties->RestoreProperties(this);
	
	ImportTemplateProperties = NewObject<UMetaHumanCharacterImportBodyTemplateProperties>(this);
	ImportTemplateProperties->RestoreProperties(this);

	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();
	SubTools->RegisterSubTools(
	{
		{ Commands.BeginBodyConformImportBodyDNATool, ImportDNAProperties },
		{ Commands.BeginBodyConformImportBodyTemplateTool, ImportTemplateProperties },
	});
}

void UMetaHumanCharacterEditorBodyConformTool::UpdateOriginalState()
{
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	const UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	OriginalState = Subsystem->CopyBodyState(MetaHumanCharacter);
}


void UMetaHumanCharacterEditorBodyConformTool::UpdateOriginalDNABuffer()
{
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	const UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	OriginalState = Subsystem->CopyBodyState(MetaHumanCharacter);
	OriginalDNABuffer = MetaHumanCharacter->GetBodyDNABuffer();
}



void UMetaHumanCharacterEditorBodyConformTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	ImportDNAProperties->SaveProperties(this);
}

TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> UMetaHumanCharacterEditorBodyConformTool::GetOriginalState() const
{
	return OriginalState.ToSharedRef();
}

const TArray<uint8>& UMetaHumanCharacterEditorBodyConformTool::GetOriginalDNABuffer() const
{
	return OriginalDNABuffer;
}


#undef LOCTEXT_NAMESPACE