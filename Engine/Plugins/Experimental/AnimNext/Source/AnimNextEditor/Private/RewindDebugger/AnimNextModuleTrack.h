// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RewindDebugger/AnimNextTrace.h"

#if ANIMNEXT_TRACE_ENABLED
#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerTrack.h"
#include "Textures/SlateIcon.h"
#include "SEventTimelineView.h"
#include "StructUtils/PropertyBag.h"
#include "IDetailsView.h"
#endif // ANIMNEXT_TRACE_ENABLED 

#include "AnimNextModuleTrack.generated.h"

UCLASS()
class UPropertyBagDetailsObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Details, meta=(ShowOnlyInnerProperties))
	FInstancedPropertyBag Properties;
};

#if ANIMNEXT_TRACE_ENABLED
namespace UE::AnimNextEditor
{
	
class FAnimNextModuleTrack : public RewindDebugger::FRewindDebuggerTrack
{
public:
	FAnimNextModuleTrack(uint64 InObjectId);
	FAnimNextModuleTrack(uint64 InObjectId, uint64 InstanceId);
	virtual ~FAnimNextModuleTrack();
	
	TSharedPtr<SEventTimelineView::FTimelineEventData> GetExistenceRange() const { return ExistenceRange; }
private:
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetDetailsViewInternal() override { return DetailsView; }

	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FName GetNameInternal() const override { return "AnimNextModule"; }
	virtual FText GetDisplayNameInternal() const override;
	virtual uint64 GetObjectIdInternal() const override { return ObjectId; }
	virtual void IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction) override;
	virtual bool HandleDoubleClickInternal() override;

	void Initialize();
	UPropertyBagDetailsObject* InitializeDetailsObject();

	TSharedPtr<IDetailsView> DetailsView;
	FSlateIcon Icon;
	uint64 ObjectId;
	uint64 InstanceId = 0; 
	double PreviousScrubTime = -1.0;
	TWeakObjectPtr<UPropertyBagDetailsObject> DetailsObjectWeakPtr;
	TSharedPtr<SEventTimelineView::FTimelineEventData> ExistenceRange;
	mutable FText DisplayNameCache;

	TArray<TSharedPtr<FAnimNextModuleTrack>> Children;
	TArray<FPropertyBagPropertyDesc> PropertyDescriptions;
};


class FAnimNextModuleTrackCreator : public RewindDebugger::IRewindDebuggerTrackCreator
{
private:
	virtual FName GetTargetTypeNameInternal() const;
	virtual FName GetNameInternal() const override { return "AnimNextModule"; }
	virtual void GetTrackTypesInternal(TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const override;
	virtual TSharedPtr<RewindDebugger::FRewindDebuggerTrack> CreateTrackInternal(uint64 ObjectId) const override;
	virtual bool HasDebugInfoInternal(uint64 ObjectId) const override;
};

}
#endif // ANIMNEXT_TRACE_ENABLED
