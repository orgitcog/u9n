// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"
#include "ScriptableToolsEditorMode.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "StatusBarSubsystem.h"


class IDetailsView;
class SButton;
class STextBlock;
class UBlueprint;
class UBaseScriptableToolBuilder;

class SCRIPTABLETOOLSEDITORMODE_API FScriptableToolsEditorModeToolkit : public FModeToolkit
{
public:

	FScriptableToolsEditorModeToolkit();
	~FScriptableToolsEditorModeToolkit();
	
	/** FModeToolkit interface */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override { return ToolkitWidget; }

	// initialize toolkit widgets that need to wait until mode is initialized/entered
	virtual void InitializeAfterModeSetup();

	// set/clear notification message area
	virtual void PostNotification(const FText& Message);
	virtual void ClearNotification();

	// set/clear warning message area
	virtual void PostWarning(const FText& Message);
	virtual void ClearWarning();

	// Async Tool Loading
	void StartAsyncToolLoading();
	void SetAsyncProgress(float PercentLoaded);
	void EndAsyncToolLoading();
	bool AreToolsLoading() const;
	TOptional<float> GetToolPercentLoaded() const;

	/** Returns the Mode specific tabs in the mode toolbar **/ 
	virtual void GetToolPaletteNames(TArray<FName>& InPaletteName) const override;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override; 
	virtual void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder) override;
	virtual void OnToolPaletteChanged(FName PaletteName) override;
	virtual bool HasIntegratedToolPalettes() const override { return false; }
	virtual bool HasExclusiveToolPalettes() const override { return false; }

	virtual FText GetActiveToolDisplayName() const override { return ActiveToolName; }
	virtual FText GetActiveToolMessage() const override { return ActiveToolMessage; }

	virtual void EnableShowRealtimeWarning(bool bEnable);

	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

	virtual void CustomizeModeDetailsViewArgs(FDetailsViewArgs& ArgsInOut) override;

	void OnActiveViewportChanged(TSharedPtr<IAssetViewport>, TSharedPtr<IAssetViewport> );

	virtual void InvokeUI() override;

	virtual void ForceToolPaletteRebuild();

	void GetActiveToolPaletteNames(TArray<FName>& OutPaletteNames);

protected:

	/** FModeToolkit interface */
	virtual void RebuildModeToolBar() override;
	virtual bool ShouldShowModeToolbar() const override;

	void RebuildModeToolPaletteWidgets();
	void RebuildModeToolkitBuilderPalettes();


private:
	const static TArray<FName> PaletteNames_Standard;

	FText ActiveToolName;
	FText ActiveToolMessage;
	FStatusBarMessageHandle ActiveToolMessageHandle;
	const FSlateBrush* ActiveToolIcon = nullptr;

	TSharedPtr<SWidget> ToolkitWidget;
	void UpdateActiveToolProperties();
	void InvalidateCachedDetailPanelState(UObject* ChangedObject);
	
	void RegisterPalettes();
	FDelegateHandle ActivePaletteChangedHandle;

	TSharedPtr<SWidget> ViewportOverlayWidget;

	TSharedPtr<STextBlock> ModeWarningArea;
	TSharedPtr<STextBlock> ModeHeaderArea;
	TSharedPtr<STextBlock> ToolWarningArea;
	TSharedPtr<SButton> AcceptButton;
	TSharedPtr<SButton> CancelButton;
	TSharedPtr<SButton> CompletedButton;

	// Palette
	bool bAsyncLoadInProgress = false;
	float AsyncLoadProgress;

	TSharedPtr<SVerticalBox> ToolBoxVBox;
	FDelegateHandle SettingsUpdateHandle;

	TSharedPtr<SWidget> ToolPaletteHeader;
	TSharedPtr<SWidget> ToolPaletteTagPanel;
	TSharedPtr<SWidget> ToolPaletteLoadBar;

	bool bShowRealtimeWarning = false;
	void UpdateShowWarnings();

	struct FScriptableToolData
	{
		FText Category;
		UClass* ToolClass = nullptr;
		UBaseScriptableToolBuilder* Builder = nullptr;
	};
	TMap<FName, TArray<FScriptableToolData>> ActiveToolCategories;
	void UpdateActiveToolCategories();

	bool bFirstInitializeAfterModeSetup = true;

	bool bShowActiveSelectionActions = false;


	// custom accept/cancel/complete handlers
};
