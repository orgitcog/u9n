// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"


#include "MetaHumanCharacterEditorMode.generated.h"

class UMetaHumanCharacter;
enum class EMetaHumanCharacterRigState : uint8;

UCLASS(Transient)
class UMetaHumanCharacterEditorMode : public UEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_MetaHumanCharacterEditorModeId;

	UMetaHumanCharacterEditorMode();

	//~Begin UEdMode Interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void ModeTick(float DeltaTime) override;
	virtual void RegisterTool(TSharedPtr<FUICommandInfo> UICommand, FString ToolIdentifier, UInteractiveToolBuilder* Builder, EToolsContextScope ToolScope = EToolsContextScope::Default) override;
	//~End UEdMode Interface

	/** Set the Character which we are editing */
	void SetCharacter(TNotNull<UMetaHumanCharacter*> InCharacter);

protected:

	//~Begin UEdMode Interface
	virtual void CreateToolkit() override;
	virtual void OnToolStarted(UInteractiveToolManager* InManager, UInteractiveTool* InTool) override;
	virtual void OnToolEnded(UInteractiveToolManager* InManager, UInteractiveTool* InTool) override;
	//~End UEdMode Interface

private:
	/** Register the tools used by this mode (see ToolTarget.h for more info on tools) */
	void RegisterModeTools();

	/** Register the tool target factories (see ToolTarget.h for more info tool targets) */
	void RegisterModeToolTargetFactories();

	/** the character being edited */
	TObjectPtr<UMetaHumanCharacter> Character;

	/** a delegate handle for a delegate called when the character rigging state changes */
	FDelegateHandle CharacterRiggingStateChanged;
	/** the function which gets called */
	void OnRiggingStateChanged();

	/** Delegate handle for when the subsystem downloading textures state changes*/
	FDelegateHandle DownloadingTexturesStateChanged;
	/** Called when change in downloading textures state */
	void OnDownloadingTexturesStateChanged(TNotNull<const UMetaHumanCharacter*> InCharacter);

	/** Updates warning text dependent on rigging state and if downloading textures*/
	void UpdateWarningText();
};