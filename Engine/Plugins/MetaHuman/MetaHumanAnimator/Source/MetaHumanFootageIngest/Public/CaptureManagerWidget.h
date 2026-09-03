// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SCompoundWidget.h"
#include "CaptureManagerCommands.h"

#include "CaptureManagerWidget.generated.h"

class UMetaHumanCaptureSource;
struct FFootageCaptureSource;

template <typename ItemType> class SListView;
template <typename NumericType> class SNumericEntryBox;

#define SHOW_MISC_WIDGETS

UCLASS()
class UCaptureManagerEditorContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<class SCaptureManagerWidget> CaptureManagerWidget;
};

/**
 * A widget that allows management of capture sources, footage ingestion and controlling of capture devices
 */
class SCaptureManagerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCaptureManagerWidget) {}
		SLATE_ARGUMENT(TSharedPtr<FCaptureManagerCommands>, CaptureManagerCommands)
	SLATE_END_ARGS()

	SCaptureManagerWidget();
	~SCaptureManagerWidget();

	void Construct(const FArguments& InArgs, const TSharedPtr<SWindow>& OwningWindow, const TSharedRef<class SDockTab>& OwningTab);

	struct FTabSpawnerEntry& RegisterCaptureSourcesTabSpawner();
	struct FTabSpawnerEntry& RegisterFootageIngestTabSpawner();

	TWeakPtr<class SDockTab> ShowMonitoringTab( UMetaHumanCaptureSource* CaptureSource );

	bool CanClose();
	void OnClose();
	void UpdateDefaultAssetCreationLocation();

private:

	void RegisterCommands();
	void Initialize();
	void RemoveDynamicToolbarSection() const;

	void ShowCMToolbar(IConsoleVariable* InVar);

	static const FName CaptureSourcesTabName;
	static const FName FootageIngestTabName;

	TSharedPtr<class FTabManager> TabManager;
	TMap<TWeakObjectPtr<UMetaHumanCaptureSource>, TWeakPtr<class SDockTab> > CaptureSourceToTabMap;

	TSharedPtr<class SExpandableArea> CaptureSourcesArea;
	TSharedPtr<class SExpandableArea> DeviceContentsArea;

	TSharedPtr<class SCaptureSourcesWidget> CaptureSourcesWidget;
	TSharedPtr<class SFootageIngestWidget> FootageIngestWidget;

	TSharedPtr<SListView<TSharedPtr<FFootageCaptureSource>>> SourceListView;

	//invoked by CaptureSourcesWidget after it has processed the event
	void OnCurrentCaptureSourceChanged(TSharedPtr<FFootageCaptureSource> InCaptureSource, ESelectInfo::Type InSelectInfo);

	//invoked by CaptureSourcesWidget after it has processed the event
	void OnCaptureSourcesChanged(TArray<TSharedPtr<FFootageCaptureSource>> InCaptureSources);

	//invoked by CaptureSourcesWidget after it has processed the event
	void OnCaptureSourceUpdated(TSharedPtr<FFootageCaptureSource> InCaptureSource);

	//invoked by CaptureSourcesWidget after it has processed the event
	void OnCaptureSourceFinishedImportingTakes(const TArray<struct FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource);

	//invoked by FootageIngestWidget after it has processed the event
	void OnTargetFolderAssetPathChanged(FText InTargetFolderAssetPath);

	FText GetStartStopCaptureButtonLabel() const;
	FText GetStartStopCaptureButtonTooltip() const;
	FSlateIcon GetStartStopCaptureButtonIcon() const;

	bool IsCurrentSourceRecording() const;

	void BindCommands();

	//~Begin AssetEditorToolkit interface
	// (this works as an asset editor, it just doesn't open on double-click,
	// so it is following the same interface even though it doesn't inherit from AssetEditorToolkit)

	void InitToolMenuContext(struct FToolMenuContext& InMenuContext);

	FName GetToolkitFName() const;
	FName GetToolMenuAppName() const;
	FName GetToolMenuName() const;
	FName GetToolMenuToolbarName() const;
	FName GetToolMenuToolbarName(FName& OutParentName) const;

	const TSharedRef<FUICommandList> GetToolkitCommands() const
	{
		return ToolkitUICommandList;
	}

	//generates the entire toolbar, with common actions, toolbar command buttons and misc widget on the right
	void GenerateToolbar();
	void RegisterDefaultToolBar();
	void RegisterSettingsToolBar(const struct FToolMenuContext& MenuContext);

	//this is a method to imitate UAssetEditorToolkit API (which we are not using as Capture Manager is not an asset editor)
	//so that the code to build toolbar doesn't change much to what it usually does
	FName GetToolMenuToolbarName() { return ToolMenuToolbarName; }

	class UToolMenu* GenerateCommonActionsToolbar(struct FToolMenuContext& MenuContext);
	class UToolMenu* GenerateSettingsToolbar(struct FToolMenuContext& MenuContext);
	static TSharedRef<SWidget> GenerateQuickSettingsMenu();

	void GenerateMessageWidget();

	const static FName ToolMenuToolbarName;
	const static FName DefaultToolbarName;

	/** List of UI commands for this toolkit. */  
	TSharedRef<FUICommandList> ToolkitUICommandList;

	/** The main Toolbar */
	TSharedPtr<SWidget> Toolbar; 

	/** The widget that will house the default Toolbar widget */
	TSharedPtr<class SBorder> ToolbarWidgetContent;

	/** Additional widgets to be added to the toolbar */
	TArray<TSharedRef<SWidget>> ToolbarWidgets;
	bool bIsInitialized = false;

	/** The information message widget */
	TSharedPtr<SWidget> MessageWidget;

//AssetEditorToolkit API End

	TSharedPtr<FCaptureManagerCommands> Commands;

	TUniquePtr<FAutoConsoleCommand> StartCapture;
	TUniquePtr<FAutoConsoleCommand> StopCapture;

	void ExtendToolBar( bool bRegenerateDynamicSection = false);
	void ExtendMenu( bool bRegenerateMenu = false );

	TSharedRef<SWidget> GenerateTakeSlateWidget();

	static TSharedRef<SWidget> PopulateAddCaptureSourceComboBox();

	void CreateSelectedCaptureSourceType(int64 InType);
	FText GetCreationTooltip(FText InTypeName) const;

	bool CanSave();
	bool CanSaveAll();
	bool CanRefresh();
	bool CanStartStopCapture() const;
	bool IsCaptureSourceSelected() const;

	void HandleSave();
	void HandleSaveAll();
	void HandleRefresh();
	void HandleStartStopCaptureToggle();

	static void PresentTargetPicker();
	
	static FText GetAutoSaveOnImportTooltip();
	static void ToggleAutoSaveOnImport();
	static bool IsAutoSaveOnImportToggled();

	void StartCaptureConsoleHandler(const TArray<FString>& InArguments);
	void StopCaptureConsoleHandler();
	void StopCaptureHandler(FFootageCaptureSource* InSource, bool bInShouldFetchTake);

	bool VerifyTakeNumber(const FText& InNewNumber, FText& OutErrorText);
	void HandleTakeNumberCommited(const FText& InNewNumber, ETextCommit::Type InCommitType);
	FText GetTakeNumber() const;

	bool VerifySlateName(const FText& InNewName, FText& OutErrorText);
	void HandleSlateNameTextCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetSlateName() const;

	void ToggleShowDevelopersContent();
	bool IsShowingDevelopersContent();
	FText GetShowOtherDevelopersToolTip() const;
	void OnShowOtherDevelopersCheckStateChanged(ECheckBoxState InCheckboxState);
	ECheckBoxState GetShowOtherDevelopersCheckState() const;

	TSharedPtr<SEditableTextBox> SlateNameTextBox;
	TSharedPtr<SEditableTextBox> TakeNumberTextBox;

	bool bAutosaveAfterImport = true;
	FString DefaultAssetCreationPath;

	void RegenerateMenusAndToolbars();

	FReply OpenLiveLinkHub();

protected:
	friend class UAssetEditorToolkitMenuContext;
	friend class FAssetEditorCommonCommands;
};