// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureData.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentityPose.h"
#include "CameraCalibration.h"
#include "Channels/MovieSceneChannel.h"

#include "MetaHumanToolkitBase.h"

class FMetaHumanIdentityAssetEditorToolkit
	: public FMetaHumanToolkitBase
{
public:
	FMetaHumanIdentityAssetEditorToolkit(UAssetEditor* InOwningAssetEditor);
	virtual ~FMetaHumanIdentityAssetEditorToolkit();

public:
	//~ FMetaHumanToolkitBase interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void HandleSequencerGlobalTimeChanged() override;
	//~ End FMetaHumanToolkitBase interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	//~ Begin FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged) override;
	//~ End FNotifyHook interface

	//~ Begin FAssetEditorToolkit interface
	virtual void InitToolMenuContext(struct FToolMenuContext& InMenuContext) override;
	virtual void SetEditingObject(UObject* InObject) override;
	//~ End FAssetEditorToolkit interface

	/** Updates which objects is being displayed in the details panel */
	void HandleIdentityTreeSelectionChanged(UObject* InObject, enum class EIdentityTreeNodeIdentifier InNodeIdentifier);

	/** Handle to a newly created promoted frame to initialize curve and group data */
	void HandlePromotedFrameAdded(class UMetaHumanIdentityPromotedFrame* InPromotedFrame);

	/** Applies tracking related viewport settings and runs tracking pipeline */
	void HandleTrackCurrent();

	/** Gets the default contour position for promoted frame based on what curves are available for this pose */
	FFrameTrackingContourData GetPoseSpecificContourDataForPromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame,
		TWeakObjectPtr<class UMetaHumanIdentityPose> InPose, bool bInProjectFootage = false) const;

	const TSharedPtr<class SMetaHumanIdentityPartsEditor> GetIdentityPartsEditor() const;

protected:
	//~Begin FMetaHumanToolkitBase interface
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void CreateWidgets() override;
	virtual void PostInitAssetEditor() override;
	virtual void BindCommands() override;
	virtual TSharedRef<SWidget> GetViewportExtraContentWidget() override;
	virtual void HandleGetViewABMenuContents(enum class EABImageViewMode InViewMode, class FMenuBuilder& InMenuBuilder) override;
	virtual void HandleSequencerMovieSceneDataChanged(enum class EMovieSceneDataChangeType InDataChangeType) override;
	virtual void HandleSequencerKeyAdded(struct FMovieSceneChannel* InChannel, const TArray<struct FKeyAddOrDeleteEventItem>& InItems) override;
	virtual void HandleSequencerKeyRemoved(struct FMovieSceneChannel* InChannel, const TArray<struct FKeyAddOrDeleteEventItem>& InItems) override;
	virtual void HandleFootageDepthDataChanged(float InNear, float InFar) override;
	virtual void HandleUndoOrRedoTransaction(const class FTransaction* InTransaction) override;
	virtual bool IsTimelineEnabled() const override;
	//~End FMetaHumanToolkitBase interface

private:
	TSharedRef<SDockTab> SpawnPartsTab(const FSpawnTabArgs& InArgs);
	TSharedRef<SDockTab> SpawnOutlinerTab(const FSpawnTabArgs& InArgs);

	/** Get the viewport client as a FMetaHumanIdentityViewportClient */
	TSharedRef<class FMetaHumanIdentityViewportClient> GetMetaHumanIdentityViewportClient() const;

	/** Create the scene capture component used to take screenshots for tracking */
	void CreateSceneCaptureComponent();

	/** Load generic face contour tracker models */
	void LoadGenericFaceContourTracker();

	/** Extend the editor's main menu with custom entries */
	void ExtendMenu();

	/** Extend the editor's toolbar with custom entries */
	void ExtendToolBar();

	/** Called to retrieve if the current frame is valid (has both image and depth) */
	UMetaHumanIdentityPose::ECurrentFrameValid GetIsCurrentFrameValid() const;

	/** Called when the tracking mode of a promoted frame changes */
	void HandlePromotedFrameTrackingModeChanged(class UMetaHumanIdentityPromotedFrame* InPromotedFrame);

	/** Handles when a Promoted Frame is selected in the Promoted Frames panel */
	void HandlePromotedFrameSelectedInPromotedFramesPanel(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bForceNotify);

	/** Handle to a removed promoted frame to deal with removing sequencer keys */
	void HandlePromotedFrameRemoved(UMetaHumanIdentityPromotedFrame* InPromotedFrame);

	/** Called when the camera in the viewport stops moving. Used for retracking in case track on change is enabled */
	void HandleCameraStopped();

	void HandleConform();
	void HandleResetTemplateMesh();

	void HandleSubmitToAutoRigging();

	void HandlePredictiveSolverTraining();

	void HandleImportDNA();
	void HandleExportDNA();
	void HandleFitTeeth();
	void HandleExportTemplateMeshClicked();

	void HandleTogglePlayback(EABImageViewMode InViewMode);

	bool CanExportTemplateMesh() const;
	bool CanActivateMarkersForCurrent() const;
	bool CanActivateMarkersForAll() const;
	bool CanTrackCurrent() const;
	bool CanTrackAll() const;
	bool CanConform() const;
	bool CanResetTemplateMesh() const;
	bool CanSubmitToAutoRigging() const;
	bool ActiveCurvesAreValidForConforming() const;
	bool CanImportDNA() const;
	bool CanExportDNA() const;
	bool CanFitTeeth() const;
	bool CanRunSolverTraining() const;
	bool CanCreateComponents() const;
	bool CanTogglePlayback(EABImageViewMode InViewMode) const;
	bool FaceIsConformed() const;

	/** TODO: Instead of checking for consistency filter out the classes that should be selectable */
	bool CaptureDataIsConsistentForPoses(const UCaptureData* InCaptureData) const;

	/** Track the given Promoted Frame using the image data provided */
	void TrackPromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame, const TArray<FColor>& InImageData, int32 InWidth, int32 InHeight, const FString& InDepthFramePath);

	/** Captures the scene using SceneCaptureComponent and the camera transform from the given Promoted Frame. 
	For footage 2 MetaHuman this also returns the path of the depthmap used (empty if Mesh 2 MetaHuman case)*/
	bool CaptureSceneForPromotedFrame(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, FIntPoint& OutImageSize, TArray<FColor>& OutLocalSamples, FString & OutDepthFramePath);

	/** Returns true if PromotedFrameTexture has been updated with valid texture */
	bool UpdatePromotedFrameTexture(const FFrameNumber& InFrameNumber);

	/** Returns true if image component of the PromotedFrameTexture has been successfully loaded */
	bool PopulateImageTextureFromDisk(const FFrameNumber& InFrameNumber, FString& OutTexturePath);

	/** Returns true if depth component of the PromotedFrameTexture has been successfully loaded */
	bool PopulateDepthTextureFromDisk(const FFrameNumber& InFrameNumber, FString& OutTexturePath);

	/** Creates an Asset Picker widget based on the given CaptureData type */
	TSharedRef<SWidget> MakeAssetPickerForCaptureDataType(UClass* InCaptureDataClass);

	/** Adds the "Create Components" menu in the toolbar */
	void MakeCreateComponentsMenu(UToolMenu* InToolMenu);
	
	/** Get tooltip for Components from Mesh */
	FText GetComponentsFromMeshTooltip() const;

	/** Get tooltip for Components from Mesh */
	FText GetComponentsFromFootageTooltip() const;

	/** */
	void MakeMeshAssetPickerMenu(UToolMenu* InToolMenu, TFunction<void(const FAssetData& InAssetData)> InCallbackFunction) const;

	/** Setups the editor based on the current capture data type being help by the neutral pose */
	void SetUpEditorForCaptureDataType();

	/** Check for a first valid capture data source set on the pose and return true if it's type is Footage */
	bool IsUsingFootageData() const;

	/** Check for a first valid capture data source set on the pose and return true if it's type is Mesh */
	bool IsUsingMeshData() const;

	/** Returns first available footage data source is there is one*/
	UFootageCaptureData* GetFootageCaptureData() const;

	/** Returns first available capture data source if there is one */
	UCaptureData* GetAvailableCaptureDataFromExistingPoses() const;

	/** Returns first available timecode alignment */
	ETimecodeAlignment GetTimecodeAlignment() const;

	/** Returns first available camera */
	FString GetCamera() const;

	/** Returns the pose with capture source, if there is one. Checks selected pose first */
	UMetaHumanIdentityPose* GetAvailablePoseWithCaptureData() const;

	void UpdateTimelineTabVisibility(bool InIsCaptureFootage);
	void HandleCaptureDataChanged(class UCaptureData* InFootageCaptureData, ETimecodeAlignment InTimecodeAlignment, const FString& InCamera, bool bInResetRanges);
	void HandleIdentityPartRemoved(class UMetaHumanIdentityPart* InIdentityPart);
	void HandleIdentityPoseAdded(class UMetaHumanIdentityPose* InIdentityPose, class UMetaHumanIdentityPart* InIdentityPart);
	void HandleIdentityPoseRemoved(class UMetaHumanIdentityPose* InIdentityPose, class UMetaHumanIdentityPart* InIdentityPart);

	void UpdateKeysForSelectedPose();
	void AddSequencerKeyForFrameNumber(int32 InFrameNumber);
	void UpdateTimelineForFootage(UFootageCaptureData* InFootageCaptureData, ETimecodeAlignment InTimecodeAlignment, const FString& InCamera);
	void UpdateContourDataAfterHeadAlignment(const TWeakObjectPtr<class UMetaHumanIdentityPose> InPose);

private:

	static const FName PartsTabId;
	static const FName OutlinerTabId;
	
	bool bDepthProcessingEnabled = false;

	/** Predictive solver progress notification. Valid only during the identity training */
	TWeakPtr<class SNotificationItem> PredictiveSolversTaskProgressNotification;

	/** A reference to the current selected Pose */
	TWeakObjectPtr<class UMetaHumanIdentityPose> SelectedIdentityPose;

	/** A reference to the Identity Parts editor Widget */
	TSharedPtr<class SMetaHumanIdentityPartsEditor> IdentityPartsEditor;

	/** The widget used to display Promoted Frames for a Identity Pose */
	TSharedPtr<class SMetaHumanIdentityPromotedFramesEditor> PromotedFramesEditorWidget;

	/** The widget used to display the Promoted Frame curves and landmarks outliner */
	TSharedPtr<class SMetaHumanIdentityOutliner> OutlinerWidget;

	/** A helper class for promoted frames and outliner to work with pose specific curves */
	TSharedPtr<class FLandmarkConfigIdentityHelper> LandmarkConfigHelper;

	/** A Reference to the Identity we are editing */
	TObjectPtr<class UMetaHumanIdentity> Identity;

	/** A component used to capture the scene in a texture for tracking purposes */
	TObjectPtr<class USceneCaptureComponent2D> SceneCaptureComponent;

	/** A pointer to currently loaded promoted frame texture */
	TPair<TObjectPtr<class UTexture2D>, TObjectPtr<UTexture2D>> PromotedFrameTexture;

	/** The range of valid frames for processing (where both image and depth tracks are defined) */
	TRange<FFrameNumber> ProcessingFrameRange = TRange<FFrameNumber>(0, 0);

	/** The range of frames for each media track */
	TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;

	TSharedPtr<SWidget> WarningTriangleWidget;

	TSharedPtr<class FMetaHumanIdentityStateValidator> IdentityStateValidator;

private:

	/** Called to show/hide the TemplateToMetaHuman menu item in response to setting the mh.Identity.EnableTemplateToMetaHuman cvar */
	void AddTemplateToMetaHumanToAssetMenu();

	/** */
	void UpdatedViewportForCaptureData(UCaptureData* InCaptureData, ETimecodeAlignment InTimecodeAlignment, const FString& InCamera);

	/** A delegate returning the CreateComponents toolbar combo button tooltip */
	FText GetCreateComponentsToolbarComboTooltip() const;

	/** A delegate returning the PromoteFrame button tooltip */
	FText GetPromoteFrameButtonTooltip() const;

	/** A delegate returning the DemoteFrame button tooltip */
	FText GetDemoteFrameButtonTooltip() const;

	/** A delegate returning the Track Active Frame button tooltip*/
	FText GetTrackActiveFrameButtonTooltip() const;

	/** A delegate returning the Track All Frames button tooltip*/
	FText GetTrackAllFramesButtonTooltip() const;

	/** A delegate returning the IdentitySolve button tooltip*/
	FText GetIdentitySolveButtonTooltip() const;

	/** A delegate returning the MeshToMetaHuman button tooltip*/
	FText GetMeshToMetaHumanButtonTooltip() const;

	/** A method to append after-processing info to the MeshToMetaHuman button tooltip depending on whether it's coming from mesh or footage capture data */
	FText GetMeshToMetaHumanButtonTooltipWithAfterProcessingInfo(FText Tooltip, bool bFullMetaHuman) const;

	/** A delegate returning the Fit Teeth button tooltip*/
	FText GetFitTeethButtonTooltip() const;

	/** A delegate returning MeshToMetaHuman (DNA Only) button tooltip */
	FText GetMeshToMetaHumanDNAOnlyButtonTooltip() const;

	/** A method to get dynamic tooltip text for MeshToMetaHuman command, with additional info how to enable the button added */
	FText GetMeshToMetaHumanButtonTooltipWithEnableInstructionsAdded(FText MainTooltipText) const;

	/** A method to get dynamic tooltip text for Prepare for Performance  */
	FText GetPrepareForPerformanceButtonTooltip() const;

	/** Creates a dialog warning the user that the Device Model has not been set, returns whether the user agrees to continue */
	bool WarnUnknownDeviceModelDialog() const;

	/** Returns a tooltip for Configure Components from Conformed command */
	FText GetConfigureComponentsFromConformedTooltipText() const;

	struct EVisibility GetIdentityInvalidationWarningIconVisibility() const;
	FText GetIdentityInvalidationWarningIconTooltip() const;

	void HandleAutoriggingServiceFinished(bool InSuccess);

	void GetExcludedFrameInfo(FFrameRate& OutSourceRate, FFrameRangeMap& OutExcludedFramesMap, int32& OutRGBMediaStartFrame, TRange<FFrameNumber>& OutProcessingLimit) const;
};
