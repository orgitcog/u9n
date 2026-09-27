// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trail.h"

class HHitProxy;
struct FFrameNumber;
class UMotionTrailToolOptions;
class FTrailHierarchy;

namespace UE
{
namespace SequencerAnimTools
{

struct FTrailVisibilityManager
{
	bool IsTrailVisible(const FGuid& Guid, const FTrail* Trail, bool bShowSelected = true) const
	{
		return !InactiveMask.Contains(Guid) && !VisibilityMask.Contains(Guid) && (AlwaysVisible.Contains(Guid) || (bShowSelected == true && (Selected.Contains(Guid) || ControlSelected.Contains(Guid)))
			|| Trail->IsAnythingSelected()) && Guid.IsValid();
	}

	bool IsTrailAlwaysVisible(const FGuid& Guid) const
	{
		return AlwaysVisible.Contains(Guid);
	}
	void SetTrailAlwaysVisible(const FGuid& Guid, bool bSet)
	{
		if (bSet)
		{
			AlwaysVisible.Add(Guid);
		}
		else
		{
			AlwaysVisible.Remove(Guid);
		}
	}

	void Reset()
	{
		InactiveMask.Empty();
		VisibilityMask.Empty();
		AlwaysVisible.Empty();
		Selected.Empty();
		ControlSelected.Empty();
	}

	TSet<FGuid> InactiveMask; // Any trails whose cache state or parent's cache state has been marked as NotUpdated
	TSet<FGuid> VisibilityMask; // Any trails masked out by the user interface, ex bone trails
	TSet<FGuid> AlwaysVisible; // Any trails pinned by the user interface
	TSet<FGuid> Selected; // Any transform or bone trails selected in the user interface
	TSet<FGuid> ControlSelected;// Any control rig trails selected
};

class ITrailHierarchyRenderer
{
public:
	virtual ~ITrailHierarchyRenderer() {}
	virtual void Render(const FSceneView* View, FPrimitiveDrawInterface* PDI) = 0;
	virtual void DrawHUD(const FSceneView* View, FCanvas* Canvas) = 0;
};

class FTrailHierarchyRenderer : public ITrailHierarchyRenderer
{
public:
	FTrailHierarchyRenderer(FTrailHierarchy* InOwningHierarchy, UMotionTrailToolOptions* InOptions)
		: OwningHierarchy(InOwningHierarchy), CachedOptions(InOptions)
	{}

	virtual void Render(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(const FSceneView* View, FCanvas* Canvas) override;

private:
	FTrailHierarchy* OwningHierarchy;
	UMotionTrailToolOptions* CachedOptions;

};

class FTrailHierarchy
{
public:

	FTrailHierarchy()
		:TickViewRange(TRange<FFrameNumber>(0,0))
		,TickEvalRange(TRange<FFrameNumber>(0,0))
		,TicksPerSegment(1)
		,LastTickEvalRange(TRange<FFrameNumber>(0,0))
		,LastTicksPerSegment(1)
		, AllTrails()
		, TimingStats()
		, VisibilityManager()
	{}

	virtual ~FTrailHierarchy() {}

	virtual void Initialize() = 0;
	virtual void Destroy() = 0; // TODO: make dtor?
	virtual ITrailHierarchyRenderer* GetRenderer() const = 0;
	virtual FFrameNumber GetFramesPerFrame() const = 0;
	virtual FFrameNumber GetFramesPerSegment() const = 0;

	//new
	virtual const FCurrentFramesInfo* GetCurrentFramesInfo() const = 0;
	virtual bool IsVisible(const FGuid& InTrailGuid) const;
	virtual bool CheckForChanges() = 0;
	virtual bool IsTrailEvaluating(const FGuid& InTrailGuid, bool bIndirectlyOnly) const = 0;

	//optional methods
	virtual void CalculateEvalRangeArray();
	virtual void Update();
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, FInputClick Click);
	virtual bool IsHitByClick(HHitProxy* HitProx);
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true);
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect = true) ;

	//Get calculated center of all selections into one position, return false if not selected
	virtual bool IsAnythingSelected(FVector& OutVectorPosition)const;
	//Get each selected item's position, return false if not selected. if bAllPositions is true it will get all selected positions from each trail, otherwise will be the average
	virtual bool IsAnythingSelected(TArray<FVector>& OutVectorPositions, bool bAllPositions = false) const;
	virtual bool IsAnythingSelected() const;
	virtual void SelectNone();
	virtual bool IsSelected(const FGuid& Key) const;
	virtual bool IsAlwaysVisible(const FGuid Key) const;

	virtual void AddTrail(const FGuid& Key, TUniquePtr<FTrail>&& TrailPtr);
	virtual void RemoveTrail(const FGuid& Key);

	virtual bool StartTracking();
	virtual bool ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation, bool bApplyToOffset);
	virtual bool EndTracking();

	virtual void TranslateSelectedKeys(bool bRight);
	virtual void DeleteSelectedKeys();

	const TRange<FFrameNumber>& GetViewFrameRange() const { return TickViewRange; }
	FFrameNumber GetTicksPerSegment() const { return TicksPerSegment; }
	const TMap<FGuid, TUniquePtr<FTrail>>& GetAllTrails() const { return AllTrails; }

	const TMap<FString, FTimespan>& GetTimingStats() const { return TimingStats; };
	TMap<FString, FTimespan>& GetTimingStats() { return TimingStats; }

	FTrailVisibilityManager& GetVisibilityManager() { return VisibilityManager; }

protected:
	void RemoveTrailIfNotAlwaysVisible(const FGuid& Key);

	void OpenContextMenu(const FGuid& TrailGuid);
protected:
	TRange<FFrameNumber> TickViewRange;
	TRange<FFrameNumber> TickEvalRange;

	FFrameNumber TicksPerSegment;
	TRange<FFrameNumber> LastTickEvalRange = TRange<FFrameNumber>(0, 0);
	FFrameNumber LastTicksPerSegment = FFrameNumber(1);

	TMap<FGuid, TUniquePtr<FTrail>> AllTrails;

	TMap<FString, FTimespan> TimingStats;

	FTrailVisibilityManager VisibilityManager;
};

} // namespace MovieScene
} // namespace UE
