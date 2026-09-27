// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "MetaHumanIdentityPose.h"

#include "SMetaHumanIdentityPromotedFramesEditor.generated.h"

struct FSlateColorBrush;

/**
 * An object used to store selected promoted frame as UPROPERTY
 */
UCLASS(MinimalAPI)
class USelectedPromotedFrameIndexHolder
	: public UObject
{
	GENERATED_BODY()

public:
	/** A reference to the Promoted Frames Editor widget */
	TWeakPtr<class SMetaHumanIdentityPromotedFramesEditor> PromotedFramesEditor;

	/** The index of the promoted frame we are creating the context menu for */
	UPROPERTY()
	int32 PromotedFrameIndex = INDEX_NONE;
};

DECLARE_DELEGATE_TwoParams(FOnPromotedFrameSelectionChanged, class UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bForceNotify)
DECLARE_DELEGATE_OneParam(FOnPromotedFrameTrackingModeChanged, class UMetaHumanIdentityPromotedFrame* InPromotedFrame)
DECLARE_DELEGATE_OneParam(FOnPromotedFrameNavigationLockedChanged, class UMetaHumanIdentityPromotedFrame* InPromotedFrame)
DECLARE_DELEGATE_OneParam(FOnPromotedFrameAdded, class UMetaHumanIdentityPromotedFrame*)
DECLARE_DELEGATE_OneParam(FOnPromotedFrameRemoved, class UMetaHumanIdentityPromotedFrame* InPromotedFrame)

/**
 * The Promoted Frames editor is responsible for creating new Promoted Frames and displaying a UI that allows the user to select it.
 * Each Promoted Frame is displayed as a custom SButton that can be selected by clicking. There are also buttons to promote
 * a new frame and remove the selected one.
 * The Promoted Frames editor also handles undo/redo events and updates the UI accordingly.
 */
class SMetaHumanIdentityPromotedFramesEditor
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanIdentityPromotedFramesEditor) {}

		// A reference to the viewport client used to read the camera transform for a given promoted frame
		SLATE_ARGUMENT(TSharedPtr<class FMetaHumanIdentityViewportClient>, ViewportClient)

		// The Identity the Pose belongs to (needed for Promote Frame button tooltip)
		SLATE_ARGUMENT(TWeakObjectPtr<class UMetaHumanIdentity>, Identity)

		// The command list with mapped actions that can be executed by this editor
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)

		// An attribute that retrieves the playback range from the sequencer
		SLATE_ATTRIBUTE(TRange<int32>, FrameRange)

		// An attribute that retrieves if the current frame is valid
		SLATE_ATTRIBUTE(UMetaHumanIdentityPose::ECurrentFrameValid, IsCurrentFrameValid)

		// Attribute to retrieve if the current promoted frame is being tracked
		SLATE_ATTRIBUTE(bool, IsTrackingCurrentFrame)

		// Delegate called when the current selected Promoted Frame changes
		SLATE_EVENT(FOnPromotedFrameSelectionChanged, OnPromotedFrameSelectionChanged)

		// Delegate called when the navigation locked state of a Promoted Frame changes
		SLATE_EVENT(FOnPromotedFrameNavigationLockedChanged, OnPromotedFrameNavigationLockedChanged)

		// Delegated called when the tracking mode of a Promoted Frame changes
		SLATE_EVENT(FOnPromotedFrameTrackingModeChanged, OnPromotedFrameTrackingModeChanged)

		// Delegate called when adding a new Promoted Frame
		SLATE_EVENT(FOnPromotedFrameAdded, OnPromotedFrameAdded)

		// Delegate called when removing a Promoted Frame
		SLATE_EVENT(FOnPromotedFrameRemoved, OnPromotedFrameRemoved)

	SLATE_END_ARGS()

	~SMetaHumanIdentityPromotedFramesEditor();

	void Construct(const FArguments& InArgs);

	/** Sets a Identity Pose to be edited by this widget */
	void SetIdentityPose(class UMetaHumanIdentityPose* InPose);

	/** Called when removing the keys from sequencer*/
	void HandlePromotedFrameRemovedFromSequencer(int32 InFrameNumber);

	/** Returns the current pose being edited */
	class UMetaHumanIdentityPose* GetIdentityPose() const;

	/** Returns the current selected Promoted Frame or nullptr if there isn't one selected */
	class UMetaHumanIdentityPromotedFrame* GetSelectedPromotedFrame() const;

	/** Set the current selection to the the one pointed by the InIndex
	  * called by Outliner when selecting a frame in the Outliner's Frames Panel */
	void SetSelection(int32 InIndex, bool bInForceNotify = false);

	/** Removes and creates all promoted frames buttons */
	void RecreateAllPromotedFramesButtons();

public:

	/** Handles an undo/redo transaction */
	void HandleUndoOrRedoTransaction(const class FTransaction* InTransaction);

	/** Called when the promote button is clicked */
	void HandleOnAddPromotedFrameClicked();

	/** Returns true if a new Promoted Frame can be added for the Pose being edited */
	bool CanAddPromotedFrame() const;

	/** Returns true if the option to select front frame should be visible */
	bool CanSetViewAsFront() const;

	void RecreatePromotedFrameButtonsForUndoRedo(const UMetaHumanIdentityPromotedFrame* InSelectedPromotedFrame);

	/** Called whenever a property is edited in the details panel */
	void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged);

	/** Returns a dynamic tooltip for Promoted Frames Container depending on whether a Pose is selected */
	FText GetPromotedFramesContainerTooltip() const;

	/** Returns a dynamic tooltip for Promote a Frame button */
	FText GetPromoteFrameButtonTooltip() const;

	/** Returns a dynamic tooltip for Demote a Frame button */
	FText GetDemoteFrameButtonTooltip() const;

private:
	/** Bind commands to actions that are specific for handling promoted frames */
	void BindCommands();

	/** Returns true if the current selection is a valid index */
	bool IsSelectionValid() const;

	/** Adds a Promoted Frame button at the given index */
	void AddPromotedFrameButton(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InIndex);

	/** Removes an Promoted Frame button at the given index. Does nothing if the index is not in valid */
	void RemovePromotedFrameButton(int32 InIndex);

	/** Adds Promoted Frame buttons to the UI with playback restriction added where applicable */
	void AddAllPromotedFrameButtons();

	/** Adds Promoted Frame buttons for the list provided */
	void AddButtonsForPromotedFrames(const TArray<UMetaHumanIdentityPromotedFrame*>& InPromotedFrames);

	/** Remove all Promoted Frame buttons from the UI. This will also clear the current selection, if there is one */
	void RemoveAllPromotedFrameButtons();

	/** Clears the selection */
	void ClearSelection();

	/** Returns true if we are currently editing a valid Identity Pose with a valid Promoted Frame Class that can be instantiated */
	bool IsPoseValid() const;

	/** Returns true if maximum number of promoted frames */
	bool IsPromotedFrameNumberBelowLimit() const;

	/** Returns true if undo operation involved reverting control vertex manipulation only */
	bool UndoControlVertexManipulation(const class FTransaction* InTransaction, const UMetaHumanIdentityPromotedFrame* InSelectedPromotedFrame, bool InIsRedo) const;

	/** Creates a dialog prompting user to set first promoted frame as front */
	bool SetFrontFrameFromDialog();

	/** Returns a reference to the Promoted Frame button at the given index */
	TSharedRef<class SPromotedFrameButton> GetPromotedFrameButton(int32 InIndex) const;

	/** Creates the context menu for the promoted frame of the given index */
	TSharedRef<SWidget> GetPromotedFrameContextMenu(int32 InPromotedFrameIndex);

	/** Called when the unpromote button is clicked */
	void HandleOnRemovePromotedFrameClicked(int32 InPromotedFrameIndex = INDEX_NONE, bool bInBroadcast = true);

	/** Called when one Promoted Frame button is clicked in the UI */
	void HandlePromotedFrameButtonClicked(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InIndex);

	/** Called when the capture source of the pose changes externally */
	void HandleIdentityPoseCaptureDataChanged(bool bInResetRanges);

	/** Called when the camera in the viewport moves. Used to update the camera transform of the selected promoted frame */
	void HandleViewportCameraMoved();

	/** Called when the camera in the viewport stops moving. Used to commit the transaction holding camera transformation changes */
	void HandleViewportCameraStopped();

	/** Called when the viewport settings has changed. Used to store any relevant changes into the select promoted frame */
	void HandleViewportSettingsChanged();

	/** */
	bool HandleShouldUnlockNavigation() const;

	/** Handles a change in the tracking mode of a Promoted Frame */
	void HandlePromotedFrameTrackingModeChanged(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bInTrackOnChange) const;

	/** Updates the viewport camera when the Promoted Frame camera transform changes and this is the currently selected Promoted Frame */
	void HandlePromotedFrameCameraTransformChanged(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const;

	/** Handles changes in the navigation locked state of the given Promoted Frame */
	void HandlePromotedFrameToggleNavigationLocked(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const;

	/** Handles changes in the front view selection for promoted frames. Only 1 promoted frame can be labeled as front */
	void HandleFrontViewToggled(UMetaHumanIdentityPromotedFrame* InPromotedFrame);

	/** Registers the HandlePromotedFrameCameraTransformChanged as a delegate called when the camera transform changes in the Promoted Frame */
	void RegisterPromotedFrameCameraTransformChange(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const;

	/** Stores the current rendering state in the given Promoted Frame */
	void StoreRenderingState(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const;

	/** Loads the rendering state stored in the Promoted Frame and sets it in the viewport */
	void LoadRenderingState(class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const;

private:
	/** The transaction context identifier for transactions done in the Identity Pose being edited */
	static const TCHAR* PromotedFramesTransactionContext;

	// The transaction used to track changes in the camera transform for a given promoted frame
	TUniquePtr<class FScopedTransaction> ScopedTransaction;

	/** Delegate called when the selection state of a Promoted Frame changes */
	FOnPromotedFrameSelectionChanged OnPromotedFrameSelectionChangedDelegate;

	/** Delegate called when the add promoted frame button is clicked */
	FOnPromotedFrameAdded OnPromotedFrameAddedDelegate;

	/** Delegate called when a promoted frame is removed from the pose */
	FOnPromotedFrameRemoved OnPromotedFrameRemovedDelegate;

	/** Delegate called when the navigation locked state of a Promoted Frame changes */
	FOnPromotedFrameNavigationLockedChanged OnPromotedFrameNavigationLockedChangedDelegate;

	/** Delegate called when the tracking mode for a promoted frame changes */
	FOnPromotedFrameTrackingModeChanged OnPromotedFrameTrackingModeChangedDelegate;

	/** A reference to the pose being edited */
	TWeakObjectPtr<class UMetaHumanIdentityPose> IdentityPose;

	/** A reference to the identity the pose belongs to */
	TWeakObjectPtr<class UMetaHumanIdentity> Identity;

	/** A reference to the viewport client where the pose components are being displayed */
	TSharedPtr<class FMetaHumanIdentityViewportClient> ViewportClient;

	/** A container for adding new Promoted Frames */
	TSharedPtr<class SHorizontalBox> PromotedFramesContainer;

	/** The command list with actions associated with this editor */
	TSharedPtr<FUICommandList> CommandList;

	/** An object that holds frame index as UPROPERTY to store promoted frame change in undo stack */
	TObjectPtr<USelectedPromotedFrameIndexHolder> IndexHolder;

	/** An attribute to retrieve the Playback range from the sequencer */
	TAttribute<TRange<int32>> FrameRange;

	/** Attribute used to query if the current promoted frame is being tracked and prevent new ones from being added */
	TAttribute<bool> IsTrackingCurrentFrame;

	/** An attribute to retrieve if the current frame is valid */
	TAttribute<UMetaHumanIdentityPose::ECurrentFrameValid> IsCurrentFrameValid;

	/** A delegate that returns the checked state of the Free Roam camera mode button */
	ECheckBoxState IsFreeRoamingCameraButtonCheckedHandler() const;

	/** A delegate to call when the check state of Free Roaming camera toggle is changed */
	void OnFreeRoamingCameraCheckStateChangedHandler(ECheckBoxState InCheckState);

	/** A delegate to call when the check state of any button in the Promoted Frames Timeline is checked (clears out the selection) */
	void OnCheckStateChangedHandler(enum ECheckBoxState InCheckState);

	/** A brush to use for Promoted Frames Timeline background */
	static const FSlateColorBrush PromotedFramesTimelineBackgroundBrush;

	/** Maximum number of promoted frames for neutral pose */
	static const int32 NeutralPoseFrameLimit;

	/** Maximum number of promoted frames for teeth pose */
	static const int32 TeethPoseFrameLimit;
};