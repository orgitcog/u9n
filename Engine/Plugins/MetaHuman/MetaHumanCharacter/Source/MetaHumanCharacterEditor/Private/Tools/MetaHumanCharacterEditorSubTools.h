// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SingleSelectionTool.h"
#include "InteractiveToolBuilder.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"

#include "MetaHumanCharacterEditorSubTools.generated.h"

/**
 * SubTools property set to be used for a tool that is divided into subtools.
 * Each subtool is represented as an UInteractiveToolPropertySet in the tool
 *
 * UMetaHumanCharacterSubToolsPropertySet::RegisterSubTools is used by a tool
 * to register a map of commands to property sets. The commands are mapped
 * to actions that activate each subtool, which also determines the checked
 * state of a tool, i.e., which one is active
 */
UCLASS()
class UMetaHumanCharacterEditorSubToolsProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnSubToolActivated, UInteractiveToolPropertySet*, bool);
	FOnSubToolActivated OnSetSubToolPropertySetEnabledDelegate;

	void RegisterSubTools(const TMap<TSharedPtr<FUICommandInfo>, TObjectPtr<UInteractiveToolPropertySet>>& InSubTools, const TSharedPtr<FUICommandInfo> InDefaultCommand = nullptr);

	TArray<UInteractiveToolPropertySet*> GetSubToolsPropertySets() const;

	TArray<TSharedPtr<FUICommandInfo>> GetSubToolCommands() const;

	FName GetActiveSubToolName() const { return ActiveSubToolName; }

	TSharedPtr<FUICommandList> GetCommandList() const;

	TSharedPtr<FUICommandInfo> GetDefaultCommand() const { return DefaultCommand; };

private:

	UPROPERTY()
	FName ActiveSubToolName;

	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<FUICommandInfo> DefaultCommand;

	TMap<TSharedPtr<FUICommandInfo>, TObjectPtr<UInteractiveToolPropertySet>> SubToolsCommands;
};

/**
 * Subclass this to allow a tool to be split into subtools. Setup/Shutdown will take
 * care of initializing the subtools object and enabling/disabling which tool is currently
 * active. At the end of the subclass Setup implementation, call SubTools->RegisterSubTools()
 * to register all available subtools for this tool.
 */
UCLASS()
class UMetaHumanCharacterEditorToolWithSubTools : public USingleSelectionTool
{
	GENERATED_BODY()

public:
	UMetaHumanCharacterEditorSubToolsProperties* GetSubTools() const { return SubTools; }

	//~Begin USingleSelectionTool
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;

	virtual bool HasCancel() const override
	{
		return false;
	}

	virtual bool HasAccept() const override
	{
		return false;
	}

	virtual bool CanAccept() const override
	{
		return false;
	}
	//~End USingleSelectionTool


protected:

	UPROPERTY()
	TObjectPtr<class UMetaHumanCharacterEditorSubToolsProperties> SubTools;

private:

	friend class UMetaHumanCharacterEditorSubToolsProperties;

};

/**
 * Subclass which simply implements the CanBuildTool function which is common to all subclasses
 */
UCLASS()
class UMetaHumanCharacterEditorToolWithToolTargetsBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual bool CanBuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

};