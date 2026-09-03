// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHUmanCharacterEditorPresetsTool.h"

#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorViewportClient.h"
#include "ObjectTools.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"
#include "InteractiveToolManager.h"
#include "Editor/EditorEngine.h"
#include "ToolBuilderUtil.h"


extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

class FPresetsToolCommandChange : public FToolCommandChange
{
public:

	FPresetsToolCommandChange(TNotNull<UInteractiveToolManager*> InToolManager)
		: ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual FString ToString() const override
	{
		return TEXT("MetaHuman Character Edit Eyes");
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		return !ToolManager.IsValid();
	}

	void Apply(UObject* InObject) override
	{
		// TODO
	}

	void Revert(UObject* InObject) override
	{
		// TODO
	}
	//~End FToolCommandChange interface

protected:

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};

UInteractiveTool* UMetaHumanCharacterEditorPresetsToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	UMetaHumanCharacterEditorPresetsTool* PresetsTool = NewObject<UMetaHumanCharacterEditorPresetsTool>(InSceneState.ToolManager);
	PresetsTool->SetTarget(Target);

	return PresetsTool;
}

bool UMetaHumanCharacterEditorPresetsToolBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	bool bCanBuildTool = Super::CanBuildTool(InSceneState);

	UActorComponent* Component = ToolBuilderUtil::FindFirstComponent(InSceneState, [&](UActorComponent* Component)
	{
		return IsValid(Component) && Component->GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();;
	});

	if (Component)
	{
		IMetaHumanCharacterEditorActorInterface* CharacterActorInterface = Cast<IMetaHumanCharacterEditorActorInterface>(Component->GetOwner());
		bool bIsRequestingHighResTextures = UMetaHumanCharacterEditorSubsystem::Get()->IsRequestingHighResolutionTextures(CharacterActorInterface->GetCharacter());
		bCanBuildTool = bCanBuildTool && !bIsRequestingHighResTextures;
	}

	return bCanBuildTool;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorPresetsToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass(),
		}
	);

	return TypeRequirements;
}

bool UMetaHumanCharacterEditorPresetsToolProperties::CanEditChange(const FProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);

	if (bIsEditable && InProperty != nullptr)
	{
		UMetaHumanCharacterEditorPresetsTool* PresetsTool = GetTypedOuter<UMetaHumanCharacterEditorPresetsTool>();
		check(PresetsTool);

		UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(PresetsTool->GetTarget());
		check(Character);
	}

	return bIsEditable;
}

void UMetaHumanCharacterEditorPresetsTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("PresetsToolName", "Presets"));

	PresetsProperties = NewObject<UMetaHumanCharacterEditorPresetsToolProperties>(this);
	AddToolPropertySource(PresetsProperties);

	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);
}

void UMetaHumanCharacterEditorPresetsTool::Shutdown(EToolShutdownType InShutdownType)
{
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(Character);

	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

	// Add the undo command
	const FText CommandChangeDescription = FText::Format(LOCTEXT("PresetsToolCommandChangeTransaction", "{0} Presets Tool"), UEnum::GetDisplayValueAsText(InShutdownType));

	TUniquePtr<FPresetsToolCommandChange> CommandChange = MakeUnique<FPresetsToolCommandChange>(GetToolManager());
	GetToolManager()->GetContextTransactionsAPI()->AppendChange(Character, MoveTemp(CommandChange), CommandChangeDescription);
}

void UMetaHumanCharacterEditorPresetsTool::OnPropertyModified(UObject* InPropertySet, FProperty* InProperty)
{
	if (InPropertySet != PresetsProperties || !InProperty)
	{
		return;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterPresetsLibraryProperties, ProjectPath))
	{
		ObjectTools::SanitizeInvalidCharsInline(PresetsProperties->LibraryManagement.ProjectPath.Path, INVALID_LONGPACKAGE_CHARACTERS);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterPresetsLibraryProperties, Path))
	{
		ObjectTools::SanitizeInvalidCharsInline(PresetsProperties->LibraryManagement.Path.Path, INVALID_LONGPACKAGE_CHARACTERS);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterPresetsManagementProperties, ImagePath))
	{
		ObjectTools::SanitizeInvalidCharsInline(PresetsProperties->PresetsManagement.ImagePath.Path, INVALID_LONGPACKAGE_CHARACTERS);
	}
}

void UMetaHumanCharacterEditorPresetsTool::ApplyPresetCharacter(TNotNull<UMetaHumanCharacter*> InPresetCharacter)
{
	TNotNull<UMetaHumanCharacter*> Character = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);

	UMetaHumanCharacterEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	Subsystem->InitializeFromPreset(Character, InPresetCharacter);

	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	FMetaHumanCharacterViewportClient* MetaHumanCharacterViewportClient = static_cast<FMetaHumanCharacterViewportClient*>(Viewport->GetClient());
	if (MetaHumanCharacterViewportClient)
	{
		MetaHumanCharacterViewportClient->RescheduleFocus();
	}
}

#undef LOCTEXT_NAMESPACE
