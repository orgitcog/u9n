// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/BaseAssetToolkit.h"
#include "Misc/NotifyHook.h"
#include "UObject/GCObject.h"
#include "EditorUndoClient.h"

#include "MetaHumanABCommandList.h"

enum class EMapChangeType : uint8;

/**
 * A base toolkit class with common functionality of the MetaHuman asset editors.
 * FMetaHumanToolkitBase provides a toolkit with with a details panel, sequencer and viewport.
 * The viewport has AB capabilities by default with a post process component already in the scene
 * to control exactly how the viewport behaves. Derived classes have the option to provide an extra
 * widget to be displayed in the bottom of the viewport as well as extra entries to the AB view menus
 * to control the visibility of components displayed in the viewport.
 */
class METAHUMANTOOLKIT_API FMetaHumanToolkitBase
	: public FBaseAssetToolkit
	, public FGCObject
	, public FNotifyHook
	, public FSelfRegisteringEditorUndoClient
{
public:
	FMetaHumanToolkitBase(UAssetEditor* InOwningAssetEditor);
	virtual ~FMetaHumanToolkitBase();

public:

	//~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override;
	//~End FGCObject interface

	//~Begin FBaseAssetToolkit interface
	virtual bool IsPrimaryEditor() const override;
	virtual void CreateWidgets() override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void PostInitAssetEditor() override;
	//~End FBaseAssetToolkit interface

	//~Begin FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged) override;
	//~End FNotifyHook interface

	//~Begin FSelfRegisteringEditorUndoClient interface
	virtual void PostUndo(bool bInSuccess) override;
	virtual void PostRedo(bool bInSuccess) override;
	//~End FSelfRegisteringEditorUndoClient interface

protected:

	//~Begin FAssetEditorToolkit interface
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	//~End FAssetEditorToolkit interface

protected:

	/** Override to bind commands that are specific to a MetaHuman toolkit  */
	virtual void BindCommands() {}

	/** Override to return a extra widget to be displayed in the bottom of the viewport */
	virtual TSharedRef<SWidget> GetViewportExtraContentWidget();

	/** Override to customize the menus to control the visibility of components in the views A and B */
	virtual void HandleGetViewABMenuContents(EABImageViewMode InViewMode, class FMenuBuilder& InMenuBuilder) {}

	/** Override to control if timeline widget is enabled */
	virtual bool IsTimelineEnabled() const { return true; }

protected:

	/** Spawns the sequencer tab */
	TSharedRef<SDockTab> SpawnTab_Sequencer(const FSpawnTabArgs& InArgs);

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& InArgs);

	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& InArgs);

	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& InArgs);

	/** Get the current sequencer playback range */
	TRange<int32> GetSequencerPlaybackRange() const;

	/** Returns the current frame number in sequencer */
	FFrameNumber GetCurrentFrameNumber() const;

	enum class EMediaTrackType : uint8
	{
		Colour,
		Depth,
	};

	/** Set or create a media track with the given image sequence */
	void SetMediaTrack(EMediaTrackType InTrackType, TSubclassOf<class UMetaHumanMovieSceneMediaTrack> InTrackClass, class UImgMediaSource* InImageSequence, FTimecode InTimecode, FFrameNumber InStartFrameOffset = 0);
	void SetMediaTrack(TSubclassOf<class UMovieSceneAudioTrack> InTrackClass, class USoundBase* InAudio, FTimecode InTimecode, FFrameNumber InStartFrameOffset = 0);

	/** Removes all media tracks from sequencer */
	void ClearMediaTracks();

	/** Returns true if the given media track has a key in the given frame time */
	bool ChannelContainsKey(class UMetaHumanMovieSceneMediaTrack* InMediaTrack, const FFrameNumber& InFrameTime) const;

	/** Called when the global time in sequencer changes */
	virtual void HandleSequencerGlobalTimeChanged();

	/** Called when sequencer triggers a movie scene data changed event */
	virtual void HandleSequencerMovieSceneDataChanged(enum class EMovieSceneDataChangeType InDataChangeType) {}

	/** Called when a key is added through the sequencer UI */
	virtual void HandleSequencerKeyAdded(struct FMovieSceneChannel* InChannel, const TArray<struct FKeyAddOrDeleteEventItem>& InItems) {}

	/** Called when a key is removed from the sequencer UI */
	virtual void HandleSequencerKeyRemoved(struct FMovieSceneChannel* InChannel, const TArray<struct FKeyAddOrDeleteEventItem>& InItems) {}

	/** Called when footage depth data changes. */
	virtual void HandleFootageDepthDataChanged(float InNear, float InFar) {}

	/** Called when mesh depth data changes. */
	virtual void HandleMeshDepthDataChanged(float InNear, float InFar) {}

	/** Called when mesh depth map visibility changes. */
	virtual void HandleDepthMapVisibilityChanged(bool bInDepthMapVisibility);

protected:

	/** Handles an Undo or Redo transaction. The base implementation does nothing by default */
	virtual void HandleUndoOrRedoTransaction(const class FTransaction* InTransaction) {}

	/** Creates the depth mesh visualization component using the information from the given camera calibration */
	void CreateDepthMeshComponent(class UCameraCalibration* InCameraCalibration);

	/** Uses the given texture as input for the depth mesh component */
	void SetDepthMeshTexture(class UTexture* InDepthTexture);

	/** Destroys the depth mesh component */
	void DestroyDepthMeshComponent();

	/** Called when depth mesh material is compiled. Used to invalidate the scene capture components */
	void HandleDepthMeshMaterialCompiled(UMaterialInterface* InMaterialInterface);

	/** Called when level is changed */
	void HandleMapChanged(UWorld* InNewWorld, EMapChangeType InMapChangeType);

private:

	/** Creates the preview scene that will be displayed in the viewport */
	void CreatePreviewScene();

	/** Creates the sequencer objects */
	void CreateSequencerTimeline();

	/** Returns the viewport client as a FMetaHumanEditorViewportClient */
	TSharedRef<class FMetaHumanEditorViewportClient> GetMetaHumanEditorViewportClient() const;

protected:

	/** The name of the sequencer tab */
	static const FName TimelineTabId;

	/** The name of the preview settings tab */
	static const FName PreviewSettingsTabId;

protected:

	/** The command list with actions to be performed by views A or B */
	FMetaHumanABCommandList ABCommandList;

	/** A reference to the preview scene we are seeing in the viewport */
	TSharedPtr<class FAdvancedPreviewScene> PreviewScene;

	/** The PreviewScene manages the lifetime of this object */
	TObjectPtr<class AActor> PreviewActor;

	/** The depth mesh component used to display depth data as a 3D mesh */
	TObjectPtr<class UMetaHumanDepthMeshComponent> DepthMeshComponent;

	/** The object that represents Sequencer */
	TSharedPtr<class ISequencer> TimelineSequencer;

	/** The playback context used for sequencer to play audio tracks */
	TSharedPtr<class FMetaHumanSequencerPlaybackContext> PlaybackContext;

	/** The Sequence we are currently visualizing in the timeline */
	TObjectPtr<class UMetaHumanSceneSequence> Sequence;

	/** The main colour media track displayed in sequencer */
	TObjectPtr<class UMetaHumanMovieSceneMediaTrack> ColourMediaTrack;

	/** The main depth media track displayed in sequencer */
	TObjectPtr<class UMetaHumanMovieSceneMediaTrack> DepthMediaTrack;

	/** The main audio track displayed in sequencer */
	TObjectPtr<class UMovieSceneAudioTrack> AudioMediaTrack;

	/** The media texture representing the colour track */
	TObjectPtr<class UMediaTexture> ColourMediaTexture;

	/** The media texture representing the depth track */
	TObjectPtr<class UMediaTexture> DepthMediaTexture;

private:
	/** Increases the display rate of the movie scene if the supplied rate is higher */
	void RatchetMovieSceneDisplayRate(FFrameRate InFrameRate);

	/** 
	  Resets the display rate of the movie scene to a low value, so that the ratcheting 
	  function will increase the value as new tracks get added
	*/
	void ResetMovieSceneDisplayRate();
};