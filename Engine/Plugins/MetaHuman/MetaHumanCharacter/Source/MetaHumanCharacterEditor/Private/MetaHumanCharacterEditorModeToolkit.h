// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatusBarSubsystem.h"
#include "Toolkits/BaseToolkit.h"

/**
 * The Mode toolkit is responsible for the panel on the side in the asset editor
 * that shows mode and tool properties. Tool buttons would go in Init().
 * It also builds the toolbar used in the asset editor
 */
class FMetaHumanCharacterEditorModeToolkit : public FModeToolkit
{
public:
	/** Constructor. */
	FMetaHumanCharacterEditorModeToolkit();

	// ~Begin FModeToolkit interface
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetActiveToolDisplayName() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;
	virtual void OnToolStarted(UInteractiveToolManager* InManager, UInteractiveTool* InTool) override;
	virtual void OnToolEnded(UInteractiveToolManager* InManager, UInteractiveTool* InTool) override;
	// ~End FModeToolkit interface

	/** Set/Clear notification messages displayed in the status bar. */
	virtual void PostNotification(const FText& InMessage);
	virtual void ClearNotification();

	/** Set/Clear warnings messages. */
	virtual void PostWarning(const FText& InMessage);
	virtual void ClearWarning();

private:

	EVisibility GetCustomWarningVisibility() const;
	FText GetCustomWarning() const;

	/** Creates the toolbar widget used to display the subtools of the given tool. */
	TSharedRef<SWidget> CreateSubToolsToolbar(TNotNull<UInteractiveTool*> Tool) const;

	/** Creates the view used to display the current active tool. */
	TSharedRef<SWidget> CreateToolView(UInteractiveTool* Tool) const;

	/** Creates a details view used to display the current active tool. */
	TSharedRef<IDetailsView> CreateToolDetailsView(UInteractiveTool* Tool) const;

	/** Creates the widget used to contain the active tool view. */
	TSharedRef<SWidget> MakeActiveToolViewWidget();

	/** Creates the widget used to display custom warnings. */
	TSharedRef<SWidget> MakeCustomWarningsWidget();

	/** Creates the tool palettes and register them in the FToolkitBuilder.  */
	void RegisterPalettes();

	/** Updates the active tool view widget according to the active tool. */
	void UpdateActiveToolViewWidget();

	/** Updates the subtools toolbar according to the active tool. */
	void UpdateSubToolsToolbar();

	/** Handles the activation of auto-activating tools. */
	void HandleAutoActivatingTools();

	/** Handles the recovery of the last tool activation. */
	void HandleLastToolActivation(UInteractiveTool* Tool);

	/** Called when the active palette has been changed. */
	void OnActivePaletteChanged();

	/** Called when the property set of a tool with subtools gets modified. */
	void OnSubToolPropertySetsModified(UInteractiveTool* Tool, const FName ToolName);

private:
	/** Widget used to display warning messages raised by tools */
	TSharedPtr<STextBlock> ToolWarningArea;

	/** The name of the active tool */
	FText ActiveToolName;

	/** The icon of the active tool, used in the Accept/Cancel widget */
	const FSlateBrush* ActiveToolIcon = nullptr;

	/** Handle of the message being displayed in the toolkit status bar */
	FStatusBarMessageHandle ActiveToolMessageHandle;

	/** Contains the widget container for the SubTools Toolbar */
	TSharedPtr<SWidget> SubToolsToolbarWidget;

	/** Contains the widget container for the Active Tool view */
	TSharedPtr<SWidget> ActiveToolViewWidget;

	/** The map used for remembering the last active tool of a mode */
	TMap<FName, FName> ModeNameToLastActiveToolNameMap;

	/** The map used for remembering the last active subtool of a tool */
	TMap<FName, FName> ToolNameToLastActiveSubToolNameMap;

	/** Custom warning */
	FText CustomWarning;
};