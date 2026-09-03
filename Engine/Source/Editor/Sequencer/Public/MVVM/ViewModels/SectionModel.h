// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelHandle.h"
#include "MovieSceneSection.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "MVVM/Extensions/ILayerBarExtension.h"
#include "MVVM/Extensions/ITrackLaneExtension.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IObjectModelExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"
#include "MVVM/Extensions/ISnappableExtension.h"
#include "MVVM/Extensions/IDraggableTrackAreaExtension.h"
#include "MVVM/Extensions/IStretchableExtension.h"
#include "MVVM/Extensions/LinkedOutlinerExtension.h"
#include "MVVM/Extensions/IConditionableExtension.h"
#include "Delegates/DelegateCombinations.h"
#include "EventHandlers/ISignedObjectEventHandler.h"
#include "EventHandlers/ISectionEventHandler.h"

class ISequencerSection;
class UMovieSceneSection;
struct FMovieSceneChannelProxy;

namespace UE
{
namespace Sequencer
{

class ITrackExtension;
class FChannelModel;
struct FOverlappingSections;

/**
 * Model for a sequencer section
 */
class SEQUENCER_API FSectionModel
	: public FViewModel
	, public FLinkedOutlinerExtension
	, public IObjectModelExtension
	, public ILayerBarExtension
	, public ITrackLaneExtension
	, public ISelectableExtension
	, public ISnappableExtension
	, public IDraggableTrackAreaExtension
	, public IStretchableExtension
	, public IConditionableExtension
	, public UE::MovieScene::TIntrusiveEventHandler<UE::MovieScene::ISignedObjectEventHandler>
	, public UE::MovieScene::TIntrusiveEventHandler<UE::MovieScene::ISectionEventHandler>
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FSectionModel, FViewModel
		, FLinkedOutlinerExtension
		, IObjectModelExtension
		, ILayerBarExtension
		, ITrackLaneExtension
		, ISelectableExtension
		, ISnappableExtension
		, IDraggableTrackAreaExtension
		, IStretchableExtension
		, IConditionableExtension
	);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnModelUpdated, FSectionModel*)

	UE_DEPRECATED(5.5, "This member is no longer supported, please subscribe to UMovieSceneSignedObject::OnModifiedDirectly.")
	FOnModelUpdated OnUpdated;

	FSectionModel();
	~FSectionModel();

	void InitializeSection(TSharedPtr<ISequencerSection> InSectionInterface);

	/** Gets the time range of the section */
	TRange<FFrameNumber> GetRange() const;

	/** Gets the section object */
	UMovieSceneSection* GetSection() const;

	/** Gets the section interface */
	TSharedPtr<ISequencerSection> GetSectionInterface() const;

	/** Gets the parent track or track row model */
	TViewModelPtr<ITrackExtension> GetParentTrackModel() const;

	/** Gets the parent track or track row model as an ITrackExtension */
	TViewModelPtr<ITrackExtension> GetParentTrackExtension() const;

	/** Gets an array of sections that underlap the specified section */
	TArray<FOverlappingSections> GetUnderlappingSections();
	/** Gets an array of sections whose easing bounds underlap the specified section */
	TArray<FOverlappingSections> GetEasingSegments();

	int32 GetPreRollFrames() const;
	int32 GetPostRollFrames() const;

	/** Returns whether this section model needs to be rebuilt, ie. the channel proxy is no longer valid */
	bool NeedsLayout() const
	{
		return !PreviousLayoutChannelProxy.Pin().IsValid() || (GetSection() && GetSection()->GetRowIndex() != PreviousLayoutRowIndex);
	}

	/** Set the channel proxy that was most recently used to layout this section model */
	void SetLayoutChannelProxy(TWeakPtr<FMovieSceneChannelProxy> InPreviousLayoutChannelProxy)
	{
		PreviousLayoutChannelProxy = InPreviousLayoutChannelProxy;
	}

	/** Set the row index that was most recently used to layout this section model */
	void SetLayoutRowIndex(int32 InRowIndex)
	{
		PreviousLayoutRowIndex = InRowIndex;
	}

public:

	/*~ IObjectModelExtension Interface */
	virtual void InitializeObject(TWeakObjectPtr<> InWeakObject) override;
	virtual UObject* GetObject() const override;

	/*~ ILayerBarExtension Interface */
	TRange<FFrameNumber> GetLayerBarRange() const override;
	void OffsetLayerBar(FFrameNumber Amount) override;

	/*~ ITrackLaneExtension Interface */
	TSharedPtr<ITrackLaneWidget> CreateTrackLaneView(const FCreateTrackLaneViewParams& InParams) override;
	FTrackLaneVirtualAlignment ArrangeVirtualTrackLaneView() const override;

	/*~ ISelectableExtension Interface */
	ESelectionIntent IsSelectable() const override;

	/*~ ISnappableExtension Interface */
	void AddToSnapField(const ISnapCandidate& Candidate, ISnapField& SnapField) const override;

	/*~ ISignedObjectEventHandler Interface */
	void OnModifiedDirectly(UMovieSceneSignedObject*) override;

	/*~ ISectionEventHandler Interface */
	void OnRowChanged(UMovieSceneSection*) override;

	/*~ IDraggableTrackAreaExtension Interface */
	bool CanDrag() const override;
	void OnBeginDrag(IDragOperation& DragOperation) override;
	void OnEndDrag(IDragOperation& DragOperation) override;

	/*~ IStretchableExtension Interface */
	void OnInitiateStretch(IStretchOperation& StretchOperation, EStretchConstraint Constraint, FStretchParameters* InOutGlobalParameters) override;
	EStretchResult OnBeginStretch(const IStretchOperation& StretchOperation, const FStretchScreenParameters& ScreenParameters, FStretchParameters* InOutParameters) override;
	void OnStretch(const IStretchOperation& StretchOperation, const FStretchScreenParameters& ScreenParameters, FStretchParameters* InOutParameters) override;
	void OnEndStretch(const IStretchOperation& StretchOperation, const FStretchScreenParameters& ScreenParameters, FStretchParameters* InOutParameters) override;

	/*~ IConditionableExtension Interface */
	const UMovieSceneCondition* GetCondition() const override;
	EConditionableConditionState GetConditionState() const override;
	void SetConditionEditorForceTrue(bool bEditorForceTrue) override;

private:

	void UpdateCachedData();

private:

	FViewModelListHead ChannelList;
	TSharedPtr<ISequencerSection> SectionInterface;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	TRange<FFrameNumber> SectionRange;
	TRange<FFrameNumber> LayerBarRange;

	TWeakPtr<FMovieSceneChannelProxy> PreviousLayoutChannelProxy;
	int32 PreviousLayoutRowIndex = -1;
};

struct FOverlappingSections
{
	/** The range for the overlap */
	TRange<FFrameNumber> Range;
	/** The sections that occupy this range, sorted by overlap priority */
	TArray<TWeakPtr<FSectionModel>> Sections;
};

} // namespace Sequencer
} // namespace UE

