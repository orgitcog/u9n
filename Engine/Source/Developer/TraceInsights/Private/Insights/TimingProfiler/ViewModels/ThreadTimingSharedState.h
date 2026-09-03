// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Containers/ContainerAllocationPolicies.h" // for FDefaultSetAllocator
#include "Containers/Map.h"
#include "Misc/Crc.h" // for TStringPointerMapKeyFuncs_DEPRECATED
#include "Templates/SharedPointer.h"

// TraceInsights
#include "Insights/InsightsManager.h"
#include "Insights/ITimingViewExtender.h"

namespace TraceServices
{
	class IAnalysisSession;
}

namespace UE::Insights::TimingProfiler
{

class FCpuTimingTrack;
class FGpuTimingTrack;
class FGpuQueueTimingTrack;
class FGpuQueueWorkTimingTrack;
class FVerseTimingTrack;
class STimingView;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FThreadTimingSharedState : public Timing::ITimingViewExtender, public TSharedFromThis<FThreadTimingSharedState>
{
private:
	struct FThreadGroup
	{
		const TCHAR* Name; /**< The thread group name; pointer to string owned by ThreadProvider. */
		bool bIsVisible;  /**< Toggle to show/hide all thread timelines associated with this group at once. Used also as default for new thread timelines. */
		uint32 NumTimelines; /**< Number of thread timelines associated with this group. */
		int32 Order; //**< Order index used for sorting. Inherited from last thread timeline associated with this group. **/

		int32 GetOrder() const { return Order; }
	};

	class IThreadSharedStateSetting
	{
	public:
		virtual ~IThreadSharedStateSetting() {}

		virtual bool GetTimingViewShowGpuWorkTracks() const = 0;
		virtual void SetTimingViewShowGpuWorkTracks(bool InValue) = 0;

		virtual bool GetTimingViewShowGpuWorkOverlays() const = 0;
		virtual void SetTimingViewShowGpuWorkOverlays(bool InValue) = 0;

		virtual bool GetTimingViewShowGpuWorkExtendedLines() const = 0;
		virtual void SetTimingViewShowGpuWorkExtendedLines(bool InValue) = 0;

		virtual bool GetTimingViewShowGpuFencesTracks() const = 0;
		virtual void SetTimingViewShowGpuFencesTracks(bool InValue) = 0;

		virtual bool GetTimingViewShowGpuFencesExtendedLines() const = 0;
		virtual void SetTimingViewShowGpuFencesExtendedLines(bool InValue) = 0;

		virtual bool GetTimingViewShowGpuFencesRelations() const = 0;
		virtual void SetTimingViewShowGpuFencesRelations(bool InValue) = 0;
	};

	class FThreadSharedStatePersistentSettings : public IThreadSharedStateSetting
	{
	public:
		virtual bool GetTimingViewShowGpuWorkTracks() const override { return GetInsightsSettings().GetTimingViewShowGpuWorkTracks(); }
		virtual void SetTimingViewShowGpuWorkTracks(bool InValue) override { GetInsightsSettings().SetAndSaveTimingViewShowGpuWorkTracks(InValue); }

		virtual bool GetTimingViewShowGpuWorkOverlays() const override { return GetInsightsSettings().GetTimingViewShowGpuWorkOverlays(); }
		virtual void SetTimingViewShowGpuWorkOverlays(bool InValue) override { GetInsightsSettings().SetAndSaveTimingViewShowGpuWorkOverlays(InValue); }

		virtual bool GetTimingViewShowGpuWorkExtendedLines() const override { return GetInsightsSettings().GetTimingViewShowGpuWorkExtendedLines(); }
		virtual void SetTimingViewShowGpuWorkExtendedLines(bool InValue) override { GetInsightsSettings().SetAndSaveTimingViewShowGpuWorkExtendedLines(InValue); }

		virtual bool GetTimingViewShowGpuFencesTracks() const override { return  GetInsightsSettings().GetTimingViewShowGpuFencesTracks(); }
		virtual void SetTimingViewShowGpuFencesTracks(bool InValue) override { GetInsightsSettings().SetAndSaveTimingViewShowGpuFencesTracks(InValue); }

		virtual bool GetTimingViewShowGpuFencesExtendedLines() const override { return GetInsightsSettings().GetTimingViewShowGpuFencesExtendedLines(); }
		virtual void SetTimingViewShowGpuFencesExtendedLines(bool InValue) override { GetInsightsSettings().SetAndSaveTimingViewShowGpuFencesExtendedLines(InValue); }

		virtual bool GetTimingViewShowGpuFencesRelations() const override { return GetInsightsSettings().GetTimingViewShowGpuFencesRelations(); }
		virtual void SetTimingViewShowGpuFencesRelations(bool InValue) override { GetInsightsSettings().SetAndSaveTimingViewShowGpuFencesRelations(InValue); };

		FInsightsSettings& GetInsightsSettings() { return UE::Insights::FInsightsManager::Get()->GetSettings(); }
		const FInsightsSettings& GetInsightsSettings() const { return UE::Insights::FInsightsManager::Get()->GetSettings(); }
	};

	class FThreadSharedStateLocalSettings : public IThreadSharedStateSetting
	{
	public:
		virtual bool GetTimingViewShowGpuWorkTracks() const override { return bTimingViewShowGpuWorkTracks; }
		virtual void SetTimingViewShowGpuWorkTracks(bool InValue) override { bTimingViewShowGpuWorkTracks = InValue; }

		virtual bool GetTimingViewShowGpuWorkOverlays() const override { return bTimingViewShowGpuWorkOverlays; }
		virtual void SetTimingViewShowGpuWorkOverlays(bool InValue) override { bTimingViewShowGpuWorkOverlays = InValue; }

		virtual bool GetTimingViewShowGpuWorkExtendedLines() const override { return bTimingViewShowGpuWorkExtendedLines; }
		virtual void SetTimingViewShowGpuWorkExtendedLines(bool InValue) override { bTimingViewShowGpuWorkExtendedLines = InValue; }

		virtual bool GetTimingViewShowGpuFencesTracks() const override { return bTimingViewShowGpuFencesTracks; }
		virtual void SetTimingViewShowGpuFencesTracks(bool InValue) override { bTimingViewShowGpuFencesTracks = InValue; }

		virtual bool GetTimingViewShowGpuFencesExtendedLines() const override { return bTimingViewShowGpuFencesExtendedLines; }
		virtual void SetTimingViewShowGpuFencesExtendedLines(bool InValue) override { bTimingViewShowGpuFencesExtendedLines = InValue; }

		virtual bool GetTimingViewShowGpuFencesRelations() const override { return bTimingViewShowGpuFencesRelations; }
		virtual void SetTimingViewShowGpuFencesRelations(bool InValue) override { bTimingViewShowGpuFencesRelations = InValue; };

	private:
		bool bTimingViewShowGpuWorkTracks = true;
		bool bTimingViewShowGpuWorkOverlays = true;
		bool bTimingViewShowGpuWorkExtendedLines = true;

		bool bTimingViewShowGpuFencesTracks = true;
		bool bTimingViewShowGpuFencesExtendedLines = true;
		bool bTimingViewShowGpuFencesRelations = true;
	};

public:
	explicit FThreadTimingSharedState(STimingView* InTimingView);
	virtual ~FThreadTimingSharedState() override = default;

	TSharedPtr<FGpuTimingTrack> GetOldGpu1Track() { return OldGpu1Track; }
	TSharedPtr<FGpuTimingTrack> GetOldGpu2Track() { return OldGpu2Track; }
	TSharedPtr<FGpuQueueTimingTrack> GetGpuTrack(uint32 InQueueId);
	TSharedPtr<FVerseTimingTrack> GetVerseSamplingTrack() { return VerseSamplingTrack; }
	TSharedPtr<FCpuTimingTrack> GetCpuTrack(uint32 InThreadId);
	const TMap<uint32, TSharedPtr<FCpuTimingTrack>> GetAllCpuTracks() { return CpuTracks; }

	bool IsOldGpu1TrackVisible() const;
	bool IsOldGpu2TrackVisible() const;
	bool IsAnyGpuTrackVisible() const;
	bool IsGpuTrackVisible(uint32 InQueueId) const;
	bool IsVerseSamplingTrackVisible() const;
	bool IsCpuTrackVisible(uint32 InThreadId) const;

	void GetVisibleGpuQueues(TSet<uint32>& OutSet) const;
	void GetVisibleCpuThreads(TSet<uint32>& OutSet) const;
	void GetVisibleTimelineIndexes(TSet<uint32>& OutSet) const;

	//////////////////////////////////////////////////
	// ITimingViewExtender interface

	virtual void OnBeginSession(Timing::ITimingViewSession& InSession) override;
	virtual void OnEndSession(Timing::ITimingViewSession& InSession) override;
	virtual void Tick(Timing::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
	virtual void ExtendGpuTracksFilterMenu(Timing::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) override;
	virtual void ExtendCpuTracksFilterMenu(Timing::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) override;

	//////////////////////////////////////////////////

	void BindCommands();

	bool IsAllGpuTracksToggleOn() const { return bShowHideAllGpuTracks; }
	void SetAllGpuTracksToggle(bool bOnOff);
	void ShowAllGpuTracks() { SetAllGpuTracksToggle(true); }
	void HideAllGpuTracks() { SetAllGpuTracksToggle(false); }
	void ShowHideAllGpuTracks() { SetAllGpuTracksToggle(!IsAllGpuTracksToggleOn()); }

	bool IsAllVerseTracksToggleOn() const { return bShowHideAllVerseTracks; }
	void SetAllVerseTracksToggle(bool bOnOff);
	void ShowAllVerseTracks() { SetAllVerseTracksToggle(true); }
	void HideAllVerseTracks() { SetAllVerseTracksToggle(false); }
	void ShowHideAllVerseTracks() { SetAllVerseTracksToggle(!IsAllVerseTracksToggleOn()); }

	bool IsAllCpuTracksToggleOn() const { return bShowHideAllCpuTracks; }
	void SetAllCpuTracksToggle(bool bOnOff);
	void ShowAllCpuTracks() { SetAllCpuTracksToggle(true); }
	void HideAllCpuTracks() { SetAllCpuTracksToggle(false); }
	void ShowHideAllCpuTracks() { SetAllCpuTracksToggle(!IsAllCpuTracksToggleOn()); }

	bool AreOverlaysVisibleInGpuQueueTracks() const { return Settings->GetTimingViewShowGpuWorkOverlays(); }
	bool AreExtendedLinesVisibleInGpuQueueTracks() const { return Settings->GetTimingViewShowGpuWorkExtendedLines(); }

	bool AreGpuWorkTracksVisible() const { return Settings->GetTimingViewShowGpuWorkTracks(); }
	void SetGpuWorkTracksVisibility(bool bOnOff);

	bool AreGpuFencesTracksVisible() const { return Settings->GetTimingViewShowGpuFencesTracks(); }
	void SetGpuFencesTracksVisibility(bool bOnOff);

	bool AreGpuFencesExtendedLinesVisible() const { return Settings->GetTimingViewShowGpuFencesExtendedLines(); }
	bool AreGpuFenceRelationsVisible() const { return Settings->GetTimingViewShowGpuFencesRelations(); }

	TSharedPtr<const ITimingEvent> FindMaxEventInstance(uint32 TimerId, double StartTime, double EndTime);
	TSharedPtr<const ITimingEvent> FindMinEventInstance(uint32 TimerId, double StartTime, double EndTime);

private:
	void CreateThreadGroupsMenu(FMenuBuilder& MenuBuilder);

	bool ToggleTrackVisibilityByGroup_IsChecked(const TCHAR* InGroupName) const;
	void ToggleTrackVisibilityByGroup_Execute(const TCHAR* InGroupName);

	void Command_ShowGpuWorkTracks_Execute() { SetGpuWorkTracksVisibility(!Settings->GetTimingViewShowGpuWorkTracks()); }
	bool Command_ShowGpuWorkTracks_CanExecute() { return GpuTracks.Num() > 0; }
	bool Command_ShowGpuWorkTracks_IsChecked() { return AreGpuWorkTracksVisible(); }

	void Command_ShowGpuWorkOverlays_Execute() { Settings->SetTimingViewShowGpuWorkOverlays(!Settings->GetTimingViewShowGpuWorkOverlays()); }
	bool Command_ShowGpuWorkOverlays_CanExecute() { return AreGpuWorkTracksVisible() && GpuTracks.Num() > 0; }
	bool Command_ShowGpuWorkOverlays_IsChecked() { return AreOverlaysVisibleInGpuQueueTracks(); }

	void Command_ShowGpuWorkExtendedLines_Execute() { Settings->SetTimingViewShowGpuWorkExtendedLines(!Settings->GetTimingViewShowGpuWorkExtendedLines()); }
	bool Command_ShowGpuWorkExtendedLines_CanExecute() { return AreGpuWorkTracksVisible() && GpuTracks.Num() > 0; }
	bool Command_ShowGpuWorkExtendedLines_IsChecked() { return AreExtendedLinesVisibleInGpuQueueTracks(); }

	void Command_ShowGpuFencesExtendedLines_Execute() { Settings->SetTimingViewShowGpuFencesExtendedLines(!Settings->GetTimingViewShowGpuFencesExtendedLines()); }
	bool Command_ShowGpuFencesExtendedLines_CanExecute() { return AreGpuFencesTracksVisible() && GpuTracks.Num() > 0; }
	bool Command_ShowGpuFencesExtendedLines_IsChecked() { return AreGpuFencesExtendedLinesVisible(); }

	void Command_ShowGpuFencesRelations_Execute();
	bool Command_ShowGpuFencesRelations_CanExecute() { return GpuTracks.Num() > 0; }
	bool Command_ShowGpuFencesRelations_IsChecked() { return AreGpuFenceRelationsVisible(); }

	void Command_ShowGpuFencesTracks_Execute() { SetGpuFencesTracksVisibility(!Settings->GetTimingViewShowGpuFencesTracks()); }
	bool Command_ShowGpuFencesTracks_CanExecute() { return GpuTracks.Num() > 0; }
	bool Command_ShowGpuFencesTracks_IsChecked() { return AreGpuFencesTracksVisible(); }

	void AddGpuWorkChildTracks();
	void RemoveGpuWorkChildTracks();

	void AddGpuFencesChildTracks();
	void RemoveGpuFencesChildTracks();

	void OnTimingEventSelected(TSharedPtr<const ITimingEvent> InSelectedEvent);

private:
	STimingView* TimingView = nullptr;

	bool bShowHideAllGpuTracks = false;
	bool bShowHideAllVerseTracks = false;
	bool bShowHideAllCpuTracks = false;

	TSharedPtr<FGpuTimingTrack> OldGpu1Track;
	TSharedPtr<FGpuTimingTrack> OldGpu2Track;

	/** Maps GPU queue id to track pointer. */
	TMap<uint32, TSharedPtr<FGpuQueueTimingTrack>> GpuTracks;

	TSharedPtr<FVerseTimingTrack> VerseSamplingTrack;

	/** Maps CPU thread id to track pointer. */
	TMap<uint32, TSharedPtr<FCpuTimingTrack>> CpuTracks;

	/** Maps thread group name to thread group info. */
	TMap<const TCHAR*, FThreadGroup, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, FThreadGroup>> ThreadGroups;

	uint64 TimingProfilerTimelineCount = 0;
	uint64 LoadTimeProfilerTimelineCount = 0;

	TSharedRef<IThreadSharedStateSetting> Settings;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace UE::Insights::TimingProfiler
