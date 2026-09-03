// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorSubTools.h"
#include "ToolBuilderUtil.h"
#include "MetaHumanCharacterEditorActor.h"
#include "ToolTargetManager.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Editor.h"


void UMetaHumanCharacterEditorSubToolsProperties::RegisterSubTools(const TMap<TSharedPtr<FUICommandInfo>, TObjectPtr<UInteractiveToolPropertySet>>& InSubTools
	, const TSharedPtr<FUICommandInfo> InDefaultCommand /* = nullptr */)
{
	CommandList = MakeShared<FUICommandList>();
	SubToolsCommands = InSubTools;
	DefaultCommand = InDefaultCommand;

	for (TPair<TSharedPtr<FUICommandInfo>, TObjectPtr<UInteractiveToolPropertySet>>& SubToolsCommandsPair : SubToolsCommands)
	{
		TSharedPtr<FUICommandInfo> Command = SubToolsCommandsPair.Key;
		UInteractiveToolPropertySet* CurrPropertySet = SubToolsCommandsPair.Value;

		// Add the property set object in the tool owner
		GetTypedOuter<UMetaHumanCharacterEditorToolWithSubTools>()->AddToolPropertySource(CurrPropertySet);

		if (OnSetSubToolPropertySetEnabledDelegate.IsBound())
		{
			const bool bEnabled = Command->GetCommandName() == ActiveSubToolName;
			OnSetSubToolPropertySetEnabledDelegate.Execute(CurrPropertySet, bEnabled);
		}

		CommandList->MapAction(Command,
							   FExecuteAction::CreateWeakLambda(this, [this, Command, CurrPropertySet]
							   {
								   ActiveSubToolName = Command->GetCommandName();

								   if (OnSetSubToolPropertySetEnabledDelegate.IsBound())
								   {
									   for (UInteractiveToolPropertySet* PropertySet : GetSubToolsPropertySets())
									   {
										   const bool bIsEnabled = PropertySet == CurrPropertySet;
										   OnSetSubToolPropertySetEnabledDelegate.Execute(PropertySet, PropertySet == CurrPropertySet);
									   }
								   }

							   }),
							   FCanExecuteAction{},
							   FIsActionChecked::CreateWeakLambda(this, [this, Command]
							   {
								   return Command->GetCommandName() == ActiveSubToolName;
							   }));
	}
}

TArray<UInteractiveToolPropertySet*> UMetaHumanCharacterEditorSubToolsProperties::GetSubToolsPropertySets() const
{
	TArray<TObjectPtr<UInteractiveToolPropertySet>> PropertySets;
	SubToolsCommands.GenerateValueArray(PropertySets);
	return PropertySets;
}

TArray<TSharedPtr<FUICommandInfo>> UMetaHumanCharacterEditorSubToolsProperties::GetSubToolCommands() const
{
	TArray<TSharedPtr<FUICommandInfo>> Commands;
	SubToolsCommands.GetKeys(Commands);
	return Commands;
}

TSharedPtr<FUICommandList> UMetaHumanCharacterEditorSubToolsProperties::GetCommandList() const
{
	return CommandList;
}

void UMetaHumanCharacterEditorToolWithSubTools::Setup()
{
	Super::Setup();

	SubTools = NewObject<UMetaHumanCharacterEditorSubToolsProperties>(this);
	SubTools->RestoreProperties(this);

	AddToolPropertySource(SubTools);

	SubTools->OnSetSubToolPropertySetEnabledDelegate.BindUObject(this, &UMetaHumanCharacterEditorToolWithSubTools::SetToolPropertySourceEnabled);
}

void UMetaHumanCharacterEditorToolWithSubTools::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	SubTools->SaveProperties(this);
}


bool UMetaHumanCharacterEditorToolWithToolTargetsBuilder::CanBuildTool(const FToolBuilderState& InSceneState) const
{
	const int32 NumTargets = InSceneState.TargetManager->CountSelectedAndTargetableWithPredicate(InSceneState, GetTargetRequirements(), [](UActorComponent& Component)
		{
			return Component.GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();
		});


	bool bIsRiggedOrPending = false;

	UActorComponent* Component = ToolBuilderUtil::FindFirstComponent(InSceneState, [&](UActorComponent* Component)
		{
			return IsValid(Component) && Component->GetOwner()->Implements<UMetaHumanCharacterEditorActorInterface>();;
		});


	if (Component)
	{
		IMetaHumanCharacterEditorActorInterface* CharacterActorInterface = Cast<IMetaHumanCharacterEditorActorInterface>(Component->GetOwner());
		if (GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>()->GetRiggingState(CharacterActorInterface->GetCharacter()) !=
			EMetaHumanCharacterRigState::Unrigged)
		{
			bIsRiggedOrPending = true;
		}
	}


	// Restrict the tool to a single target which is being edited
	return NumTargets == 1 && !bIsRiggedOrPending;
}