// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "MovieSceneTrackEditor.h"
#include "SequencerCoreFwd.h"

struct FAssetData;
struct FMovieSceneTimeWarpChannel;
struct FMovieSceneSequenceTransform;

class FMenuBuilder;
class FSequencerSectionPainter;
class UAnimSeqExportOption;
class UAnimSequenceBase;
class UMovieSceneCommonAnimationTrack;
class UMovieSceneSkeletalAnimationSection;
class USkeletalMeshComponent;
class USkeleton;

namespace UE::Sequencer
{

class ITrackExtension;

/**
 * Tools for animation tracks
 */
class MOVIESCENETOOLS_API FCommonAnimationTrackEditor : public FMovieSceneTrackEditor, public FGCObject
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FCommonAnimationTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FCommonAnimationTrackEditor() { }

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCommonAnimationTrackEditor");
	}

	/**
	* Keeps track of how many skeletal animation track editors we have*
	*/
	static int32 NumberActive;

	static USkeletalMeshComponent* AcquireSkeletalMeshFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr);
	static USkeleton* AcquireSkeletonFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr);

public:

	// ISequencerTrackEditor interface

	virtual FText GetDisplayName() const override;
	virtual void BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams) override;
	virtual void OnInitialize() override;
	virtual void OnRelease() override;

protected:

	virtual TSubclassOf<UMovieSceneCommonAnimationTrack> GetTrackClass() const = 0;

	/** Animation sub menu */
	TSharedRef<SWidget> BuildAddAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton, UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::ITrackExtension> TrackModel);
	TSharedRef<SWidget> BuildAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track);
	void AddAnimationSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, USkeleton* Skeleton, UMovieSceneTrack* Track);

	/** Filter only compatible skeletons */
	bool FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton);

	/** Animation sub menu filter function */
	bool ShouldFilterAsset(const FAssetData& AssetData);

	/** Animation asset selected */
	void OnAnimationAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);

	/** Animation asset enter pressed */
	void OnAnimationAssetEnterPressed(const TArray<FAssetData>& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(FFrameNumber KeyTime, UObject* Object, UAnimSequenceBase* AnimSequence, UMovieSceneTrack* Track, int32 RowIndex);
	
	/** Construct the binding menu*/
	void ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings);

	/** Callback to Create the Animation Asset, pop open the dialog */
	void HandleCreateAnimationSequence(USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton, FGuid Binding, bool bCeateSoftLink);

	/** Callback to Creae the Animation Asset after getting the name*/
	bool CreateAnimationSequence(const TArray<UObject*> NewAssets,USkeletalMeshComponent* SkelMeshComp, FGuid Binding, bool bCreateSoftLink);

	/** Open the linked Anim Sequence*/
	void OpenLinkedAnimSequence(FGuid Binding);

	/** Can Open the linked Anim Sequence*/
	bool CanOpenLinkedAnimSequence(FGuid Binding);

	friend class FMovieSceneSkeletalAnimationParamsDetailCustomization;

protected:
	/* Was part of the the section but should be at the track level since it takes the final blended result at the current time, not the section instance value*/
	bool CreatePoseAsset(const TArray<UObject*> NewAssets, FGuid InObjectBinding);
	void HandleCreatePoseAsset(FGuid InObjectBinding);
	bool CanCreatePoseAsset(FGuid InObjectBinding) const;

protected:
	/* For Anim Sequence UI Option with be gc'd*/
	TObjectPtr<UAnimSeqExportOption> AnimSeqExportOption;


protected:
	/* Delegate to handle sequencer changes for auto baking of anim sequences*/
	FDelegateHandle SequencerSavedHandle;
	void OnSequencerSaved(ISequencer& InSequence);
	FDelegateHandle SequencerChangedHandle;
	void OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType);
};


/** Class for animation sections */
class MOVIESCENETOOLS_API FCommonAnimationSection
	: public ISequencerSection
	, public TSharedFromThis<FCommonAnimationSection>
{
public:

	/** Constructor. */
	FCommonAnimationSection( UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FCommonAnimationSection();

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual FText GetSectionToolTip() const override;
	virtual TOptional<FFrameTime> GetSectionTime(FSequencerSectionPainter& InPainter) const override;
	virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const override;
	virtual FMargin GetContentPadding() const override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(FFrameNumber SlipTime) override;
	virtual void CustomizePropertiesDetailsView(TSharedRef<IDetailsView> DetailsView, const FSequencerSectionPropertyDetailsViewCustomizationParams& InParams) const override;
	virtual void BeginDilateSection() override;
	virtual void DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;


protected:
	void FindBestBlendSection(FGuid InObjectBinding);
protected:

	/** The section we are visualizing */
	TWeakObjectPtr<UMovieSceneSkeletalAnimationSection> WeakSection;

	/** Used to draw animation frame, need selection state and local time*/
	TWeakPtr<ISequencer> Sequencer;

	TUniquePtr<FMovieSceneSequenceTransform> InitialDragTransform;
	TUniquePtr<FMovieSceneTimeWarpChannel> PreDilateChannel;
	double PreDilatePlayRate;
};

} // namespace UE::Sequencer