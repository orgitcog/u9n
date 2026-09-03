// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUProfiler.h: Hierarchical GPU Profiler Implementation.
=============================================================================*/

#include "GPUProfiler.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/WildcardString.h"
#include "Misc/CommandLine.h"
#include "RHI.h"
#include "GpuProfilerTrace.h"
#include "Containers/AnsiString.h"
#include "Stats/StatsData.h"

#if !UE_BUILD_SHIPPING
#include "VisualizerEvents.h"
#include "ProfileVisualizerModule.h"
#include "Modules/ModuleManager.h"
#endif

#define LOCTEXT_NAMESPACE "GpuProfiler"

enum class EGPUProfileSortMode
{
	Chronological,
	TimeElapsed,
	NumPrims,
	NumVerts,

	Max
};

static TAutoConsoleVariable<int32> GCVarProfileGPU_Sort(
	TEXT("r.ProfileGPU.Sort"),
	0,
	TEXT("Sorts the TTY Dump independently at each level of the tree in various modes.\n")
	TEXT("0 : Chronological\n")
	TEXT("1 : By time elapsed\n")
	TEXT("2 : By number of prims\n")
	TEXT("3 : By number of verts\n"),
	ECVF_Default);

static TAutoConsoleVariable<FString> GCVarProfileGPU_Root(
	TEXT("r.ProfileGPU.Root"),
	TEXT("*"),
	TEXT("Allows to filter the tree when using ProfileGPU, the pattern match is case sensitive."),
	ECVF_Default);

static TAutoConsoleVariable<float> GCVarProfileGPU_ThresholdPercent(
	TEXT("r.ProfileGPU.ThresholdPercent"),
	0.0f,
	TEXT("Percent of the total execution duration the event needs to be larger than to be printed."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_UnicodeOutput(
	TEXT("r.ProfileGPU.UnicodeOutput"),
	true,
	TEXT("When enabled, the output results will be formatted in a unicode table."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowLeafEvents(
	TEXT("r.ProfileGPU.ShowLeafEvents"),
	true,
	TEXT("Allows profileGPU to display event-only leaf nodes with no draws associated."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowHeader(
	TEXT("r.ProfileGPU.ShowHeader"),
	true,
	TEXT("When true, prints a summary of the profileGPU settings before the report table in the log."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowEmptyQueues(
	TEXT("r.ProfileGPU.ShowEmptyQueues"),
	true,
	TEXT("When true, GPU queues without any registered work are still displayed in the report tables."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowStats(
	TEXT("r.ProfileGPU.ShowStats"),
	true,
	TEXT("When true, additional stat columns are shown in the report (numbers of draws, dispatches, vertices and primitives)."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowPercentColumn(
	TEXT("r.ProfileGPU.ShowPercentColumn"),
	true,
	TEXT("When true, a column showing the relative portion of time each stat takes as a percentage is displayed, including a visual unicode bar when unicode output is enabled."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowInclusive(
	TEXT("r.ProfileGPU.ShowInclusive"),
	true,
	TEXT("When true, inclusive GPU times are shown."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowExclusive(
	TEXT("r.ProfileGPU.ShowExclusive"),
	true,
	TEXT("When true, exclusive GPU times are shown."),
	ECVF_Default);

static TAutoConsoleVariable<bool> GCVarProfileGPU_ShowUI(
	TEXT("r.ProfileGPU.ShowUI"),
	true,
	TEXT("Whether the user interface profiler should be displayed after profiling the GPU.\n")
	TEXT("The results will always go to the log/console."),
	ECVF_Default);

static TAutoConsoleVariable<int> CVarGPUCsvStatsEnabled(
	TEXT("r.GPUCsvStatsEnabled"),
	0,
	TEXT("Enables or disables GPU stat recording to CSVs"));

#if (RHI_NEW_GPU_PROFILER == 0)

static TAutoConsoleVariable<FString> GProfileGPUPatternCVar(
	TEXT("r.ProfileGPU.Pattern"),
	TEXT("*"),
	TEXT("Allows to filter the entries when using ProfileGPU, the pattern match is case sensitive.\n")
	TEXT("'*' can be used in the end to get all entries starting with the string.\n")
	TEXT("    '*' without any leading characters disables the pattern matching and uses a time threshold instead (default).\n")
	TEXT("'?' allows to ignore one character.\n")
	TEXT("e.g. AmbientOcclusionSetup, AmbientOcclusion*, Ambient???lusion*, *"),
	ECVF_Default);

static TAutoConsoleVariable<int32> GProfileShowEventHistogram(
	TEXT("r.ProfileGPU.ShowEventHistogram"),
	0,
	TEXT("Whether the event histogram should be shown."),
	ECVF_Default);

TAutoConsoleVariable<int32> GProfileGPUTransitions(
	TEXT("r.ProfileGPU.ShowTransitions"),
	0,
	TEXT("Allows profileGPU to display resource transition events."),
	ECVF_Default);

// Should we print a summary at the end?
static TAutoConsoleVariable<int32> GProfilePrintAssetSummary(
	TEXT("r.ProfileGPU.PrintAssetSummary"),
	0,
	TEXT("Should we print a summary split by asset (r.ShowMaterialDrawEvents is strongly recommended as well).\n"),
	ECVF_Default);

// Should we print a summary at the end?
static TAutoConsoleVariable<FString> GProfileAssetSummaryCallOuts(
	TEXT("r.ProfileGPU.AssetSummaryCallOuts"),
	TEXT(""),
	TEXT("Comma separated list of substrings that deserve special mention in the final summary (e.g., \"LOD,HeroName\"\n")
	TEXT("r.ProfileGPU.PrintAssetSummary must be true to enable this feature"),
	ECVF_Default);

static TAutoConsoleVariable<int32> GSaveScreenshotAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.Screenshot"),
	1,
	TEXT("Whether a screenshot should be taken when profiling the GPU. 0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> GGPUHitchThresholdCVar(
	TEXT("RHI.GPUHitchThreshold"),
	100.0f,
	TEXT("Threshold for detecting hitches on the GPU (in milliseconds).")
);

static TAutoConsoleVariable<int32> CVarGPUCrashDataCollectionEnable(
	TEXT("r.gpucrash.collectionenable"),
	1,
	TEXT("Stores GPU crash data from scoped events when a applicable crash debugging system is available."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGPUCrashDataDepth(
	TEXT("r.gpucrash.datadepth"),
	-1,
	TEXT("Limits the amount of marker scope depth we record for GPU crash debugging to the given scope depth."),
	ECVF_RenderThreadSafe);

namespace RHIConfig
{
	bool ShouldSaveScreenshotAfterProfilingGPU()
	{
		return GSaveScreenshotAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	bool ShouldShowProfilerAfterProfilingGPU()
	{
		return GCVarProfileGPU_ShowUI.GetValueOnAnyThread() != 0;
	}

	float GetGPUHitchThreshold()
	{
		return GGPUHitchThresholdCVar.GetValueOnAnyThread() * 0.001f;
	}
}

/** Recursively generates a histogram of nodes and stores their timing in TimingResult. */
static void GatherStatsEventNode(FGPUProfilerEventNode* Node, int32 Depth, TMap<FString, FGPUProfilerEventNodeStats>& EventHistogram)
{
	if (Node->NumDraws > 0 || Node->NumDispatches > 0 || Node->Children.Num() > 0)
	{
		Node->TimingResult = Node->GetTiming() * 1000.0f;
		Node->NumTotalDraws = Node->NumDraws;
		Node->NumTotalDispatches = Node->NumDispatches;
		Node->NumTotalPrimitives = Node->NumPrimitives;
		Node->NumTotalVertices = Node->NumVertices;

		FGPUProfilerEventNode* Parent = Node->Parent;		
		while (Parent)
		{
			Parent->NumTotalDraws += Node->NumDraws;
			Parent->NumTotalDispatches += Node->NumDispatches;
			Parent->NumTotalPrimitives += Node->NumPrimitives;
			Parent->NumTotalVertices += Node->NumVertices;

			Parent = Parent->Parent;
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			// Traverse children
			GatherStatsEventNode(Node->Children[ChildIndex], Depth + 1, EventHistogram);
		}

		FGPUProfilerEventNodeStats* FoundHistogramBucket = EventHistogram.Find(Node->Name);
		if (FoundHistogramBucket)
		{
			FoundHistogramBucket->NumDraws += Node->NumTotalDraws;
			FoundHistogramBucket->NumPrimitives += Node->NumTotalPrimitives;
			FoundHistogramBucket->NumVertices += Node->NumTotalVertices;
			FoundHistogramBucket->TimingResult += Node->TimingResult;
			FoundHistogramBucket->NumEvents++;
		}
		else
		{
			FGPUProfilerEventNodeStats NewNodeStats;
			NewNodeStats.NumDraws = Node->NumTotalDraws;
			NewNodeStats.NumPrimitives = Node->NumTotalPrimitives;
			NewNodeStats.NumVertices = Node->NumTotalVertices;
			NewNodeStats.TimingResult = Node->TimingResult;
			NewNodeStats.NumEvents = 1;
			EventHistogram.Add(Node->Name, NewNodeStats);
		}
	}
}

struct FGPUProfileInfoPair
{
	int64 Triangles;
	int32 DrawCalls;

	FGPUProfileInfoPair()
		: Triangles(0)
		, DrawCalls(0)
	{
	}

	void AddDraw(int64 InTriangleCount)
	{
		Triangles += InTriangleCount;
		++DrawCalls;
	}
};

struct FGPUProfileStatSummary
{
	TMap<FString, FGPUProfileInfoPair> TrianglesPerMaterial;
	TMap<FString, FGPUProfileInfoPair> TrianglesPerMesh;
	TMap<FString, FGPUProfileInfoPair> TrianglesPerNonMesh;

	int32 TotalNumNodes;
	int32 TotalNumDraws;

	bool bGatherSummaryStats;
	bool bDumpEventLeafNodes;

	FGPUProfileStatSummary()
		: TotalNumNodes(0)
		, TotalNumDraws(0)
		, bGatherSummaryStats(false)
		, bDumpEventLeafNodes(false)
	{
		bDumpEventLeafNodes = GCVarProfileGPU_ShowLeafEvents.GetValueOnRenderThread() != 0;
		bGatherSummaryStats = GProfilePrintAssetSummary.GetValueOnRenderThread() != 0;
	}

	void ProcessMatch(FGPUProfilerEventNode* Node)
	{
		if (bGatherSummaryStats && (Node->NumTotalPrimitives > 0) && (Node->NumTotalVertices > 0) && (Node->Children.Num() == 0))
		{
			FString MaterialPart;
			FString AssetPart;
			if (Node->Name.Split(TEXT(" "), &MaterialPart, &AssetPart, ESearchCase::CaseSensitive))
			{
				TrianglesPerMaterial.FindOrAdd(MaterialPart).AddDraw(Node->NumTotalPrimitives);
				TrianglesPerMesh.FindOrAdd(AssetPart).AddDraw(Node->NumTotalPrimitives);
			}
			else
			{
				TrianglesPerNonMesh.FindOrAdd(Node->Name).AddDraw(Node->NumTotalPrimitives);
			}
		}
	}

	void PrintSummary()
	{
		UE_LOG(LogRHI, Log, TEXT("Total Nodes %u Draws %u"), TotalNumNodes, TotalNumDraws);
		UE_LOG(LogRHI, Log, TEXT(""));
		UE_LOG(LogRHI, Log, TEXT(""));

		if (bGatherSummaryStats)
		{
			// Sort the lists and print them out
			TrianglesPerMesh.ValueSort([](const FGPUProfileInfoPair& A, const FGPUProfileInfoPair& B){ return A.Triangles > B.Triangles; });
			UE_LOG(LogRHI, Log, TEXT(""));
			UE_LOG(LogRHI, Log, TEXT("MeshList,TriangleCount,DrawCallCount"));
			for (auto& Pair : TrianglesPerMesh)
			{
				UE_LOG(LogRHI, Log, TEXT("%s,%" INT64_FMT ",%d"), *Pair.Key, Pair.Value.Triangles, Pair.Value.DrawCalls);
			}

			TrianglesPerMaterial.ValueSort([](const FGPUProfileInfoPair& A, const FGPUProfileInfoPair& B){ return A.Triangles > B.Triangles; });
			UE_LOG(LogRHI, Log, TEXT(""));
			UE_LOG(LogRHI, Log, TEXT("MaterialList,TriangleCount,DrawCallCount"));
			for (auto& Pair : TrianglesPerMaterial)
			{
				UE_LOG(LogRHI, Log, TEXT("%s,%" INT64_FMT ",%d"), *Pair.Key, Pair.Value.Triangles, Pair.Value.DrawCalls);
			}

			TrianglesPerNonMesh.ValueSort([](const FGPUProfileInfoPair& A, const FGPUProfileInfoPair& B){ return A.Triangles > B.Triangles; });
			UE_LOG(LogRHI, Log, TEXT(""));
			UE_LOG(LogRHI, Log, TEXT("MiscList,TriangleCount,DrawCallCount"));
			for (auto& Pair : TrianglesPerNonMesh)
			{
				UE_LOG(LogRHI, Log, TEXT("%s,%" INT64_FMT ",%d"), *Pair.Key, Pair.Value.Triangles, Pair.Value.DrawCalls);
			}

			// See if we want to call out any particularly interesting matches
			TArray<FString> InterestingSubstrings;
			GProfileAssetSummaryCallOuts.GetValueOnRenderThread().ParseIntoArray(InterestingSubstrings, TEXT(","), true);

			if (InterestingSubstrings.Num() > 0)
			{
				UE_LOG(LogRHI, Log, TEXT(""));
				UE_LOG(LogRHI, Log, TEXT("Information about specified mesh substring matches (r.ProfileGPU.AssetSummaryCallOuts)"));
				for (const FString& InterestingSubstring : InterestingSubstrings)
				{
					int32 InterestingNumDraws = 0;
					int64 InterestingNumTriangles = 0;

					for (auto& Pair : TrianglesPerMesh)
					{
						if (Pair.Key.Contains(InterestingSubstring))
						{
							InterestingNumDraws += Pair.Value.DrawCalls;
							InterestingNumTriangles += Pair.Value.Triangles;
						}
					}

					UE_LOG(LogRHI, Log, TEXT("Matching '%s': %d draw calls, with %" INT64_FMT " tris (%.2f M)"), *InterestingSubstring, InterestingNumDraws, InterestingNumTriangles, InterestingNumTriangles * 1e-6);
				}
				UE_LOG(LogRHI, Log, TEXT(""));
			}
		}
	}
};

/** Recursively dumps stats for each node with a depth first traversal. */
static void DumpStatsEventNode(FGPUProfilerEventNode* Node, float RootResult, int32 Depth, const FWildcardString& WildcardFilter, bool bParentMatchedFilter, float& ReportedTiming, FGPUProfileStatSummary& Summary)
{
	Summary.TotalNumNodes++;
	ReportedTiming = 0;

	if (Node->NumDraws > 0 || Node->NumDispatches > 0 || Node->Children.Num() > 0 || Summary.bDumpEventLeafNodes)
	{
		Summary.TotalNumDraws += Node->NumDraws;
		// Percent that this node was of the total frame time
		const float Percent = Node->TimingResult * 100.0f / (RootResult * 1000.0f);
		const float PercentThreshold = GCVarProfileGPU_ThresholdPercent.GetValueOnRenderThread();
		const int32 EffectiveDepth = FMath::Max(Depth - 1, 0);
		const bool bDisplayEvent = (bParentMatchedFilter || WildcardFilter.IsMatch(Node->Name)) && (Percent > PercentThreshold || Summary.bDumpEventLeafNodes);

		if (bDisplayEvent)
		{
			FString NodeStats = TEXT("");

			if (Node->NumTotalDraws > 0)
			{
				NodeStats = FString::Printf(TEXT("%u %s %u prims %u verts "), Node->NumTotalDraws, Node->NumTotalDraws == 1 ? TEXT("draw") : TEXT("draws"), Node->NumTotalPrimitives, Node->NumTotalVertices);
			}

			if (Node->NumTotalDispatches > 0)
			{
				NodeStats += FString::Printf(TEXT("%u %s"), Node->NumTotalDispatches, Node->NumTotalDispatches == 1 ? TEXT("dispatch") : TEXT("dispatches"));
			
				// Cumulative group stats are not meaningful, only include dispatch stats if there was one in the current node
				if (Node->GroupCount.X > 0 && Node->NumDispatches == 1)
				{
					NodeStats += FString::Printf(TEXT(" %u"), Node->GroupCount.X);

					if (Node->GroupCount.Y > 1)
					{
						NodeStats += FString::Printf(TEXT("x%u"), Node->GroupCount.Y);
					}

					if (Node->GroupCount.Z > 1)
					{
						NodeStats += FString::Printf(TEXT("x%u"), Node->GroupCount.Z);
					}

					NodeStats += TEXT(" groups");
				}
			}

			// Print information about this node, padded to its depth in the tree
			UE_LOG(LogRHI, Log, TEXT("%s%4.1f%%%5.2fms   %s %s"), 
				*FString(TEXT("")).LeftPad(EffectiveDepth * 3), 
				Percent,
				Node->TimingResult,
				*Node->Name,
				*NodeStats
				);

			ReportedTiming = Node->TimingResult;
			Summary.ProcessMatch(Node);
		}

		struct FCompareGPUProfileNode
		{
			EGPUProfileSortMode SortMode;
			FCompareGPUProfileNode(EGPUProfileSortMode InSortMode)
				: SortMode(InSortMode)
			{}
			FORCEINLINE bool operator()(const FGPUProfilerEventNode* A, const FGPUProfilerEventNode* B) const
			{
				switch (SortMode)
				{
					case EGPUProfileSortMode::NumPrims:
						return B->NumTotalPrimitives < A->NumTotalPrimitives;
					case EGPUProfileSortMode::NumVerts:
						return B->NumTotalVertices < A->NumTotalVertices;
					case EGPUProfileSortMode::TimeElapsed:
					default:
						return B->TimingResult < A->TimingResult;
				}
			}
		};

		EGPUProfileSortMode SortMode = (EGPUProfileSortMode)FMath::Clamp(GCVarProfileGPU_Sort.GetValueOnRenderThread(), 0, ((int32)EGPUProfileSortMode::Max - 1));
		if (SortMode != EGPUProfileSortMode::Chronological)
		{
			Node->Children.Sort(FCompareGPUProfileNode(SortMode));
		}

		float TotalChildTime = 0;
		uint32 TotalChildDraws = 0;
		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			FGPUProfilerEventNode* ChildNode = Node->Children[ChildIndex];

			// Traverse children			
			const int32 PrevNumDraws = Summary.TotalNumDraws;
			float ChildReportedTiming = 0;
			DumpStatsEventNode(Node->Children[ChildIndex], RootResult, Depth + 1, WildcardFilter, bDisplayEvent, ChildReportedTiming, Summary);
			const int32 NumChildDraws = Summary.TotalNumDraws - PrevNumDraws;

			TotalChildTime += ChildReportedTiming;
			TotalChildDraws += NumChildDraws;
		}

		const float UnaccountedTime = FMath::Max(Node->TimingResult - TotalChildTime, 0.0f);
		const float UnaccountedPercent = UnaccountedTime * 100.0f / (RootResult * 1000.0f);

		// Add an 'Other Children' node if necessary to show time spent in the current node that is not in any of its children
		if (bDisplayEvent && Node->Children.Num() > 0 && TotalChildDraws > 0 && (UnaccountedPercent > 2.0f || UnaccountedTime > .2f))
		{
			UE_LOG(LogRHI, Log, TEXT("%s%4.1f%%%5.2fms   Other Children"), 
				*FString(TEXT("")).LeftPad((EffectiveDepth + 1) * 3), 
				UnaccountedPercent,
				UnaccountedTime);
		}
	}
}

#if !UE_BUILD_SHIPPING

/**
 * Converts GPU profile data to Visualizer data
 *
 * @param InProfileData GPU profile data
 * @param OutVisualizerData Visualizer data
 */
static TSharedPtr< FVisualizerEvent > CreateVisualizerDataRecursively( const TRefCountPtr< class FGPUProfilerEventNode >& InNode, TSharedPtr< FVisualizerEvent > InParentEvent, const double InStartTimeMs, const double InTotalTimeMs )
{
	TSharedPtr< FVisualizerEvent > VisualizerEvent( new FVisualizerEvent( InStartTimeMs / InTotalTimeMs, InNode->TimingResult / InTotalTimeMs, InNode->TimingResult, 0, InNode->Name ) );
	VisualizerEvent->ParentEvent = InParentEvent;

	double ChildStartTimeMs = InStartTimeMs;
	for( int32 ChildIndex = 0; ChildIndex < InNode->Children.Num(); ChildIndex++ )
	{
		TRefCountPtr< FGPUProfilerEventNode > ChildNode = InNode->Children[ ChildIndex ];
		TSharedPtr< FVisualizerEvent > ChildEvent = CreateVisualizerDataRecursively( ChildNode, VisualizerEvent, ChildStartTimeMs, InTotalTimeMs );
		VisualizerEvent->Children.Add( ChildEvent );

		ChildStartTimeMs += ChildNode->TimingResult;
	}

	return VisualizerEvent;
}

/**
 * Converts GPU profile data to Visualizer data
 *
 * @param InProfileData GPU profile data
 * @param OutVisualizerData Visualizer data
 */
static TSharedPtr< FVisualizerEvent > CreateVisualizerData( const TArray<TRefCountPtr<class FGPUProfilerEventNode> >& InProfileData )
{
	// Calculate total time first
	double TotalTimeMs = 0.0;
	for( int32 Index = 0; Index < InProfileData.Num(); ++Index )
	{
		TotalTimeMs += InProfileData[ Index ]->TimingResult;
	}
	
	// Assumption: InProfileData contains only one (root) element. Otherwise an extra FVisualizerEvent root event is required.
	TSharedPtr< FVisualizerEvent > DummyRoot;
	// Recursively create visualizer event data.
	TSharedPtr< FVisualizerEvent > StatEvents( CreateVisualizerDataRecursively( InProfileData[0], DummyRoot, 0.0, TotalTimeMs ) );
	return StatEvents;
}

#endif

void FGPUProfilerEventNodeFrame::DumpEventTree()
{
	if (EventTree.Num() > 0)
	{
		float RootResult = GetRootTimingResults();

		FString ConfigString;

		if (GCVarProfileGPU_Root.GetValueOnRenderThread() != TEXT("*"))
		{
			ConfigString += FString::Printf(TEXT("Root filter: %s "), *GCVarProfileGPU_Root.GetValueOnRenderThread());
		}

		if (GCVarProfileGPU_ThresholdPercent.GetValueOnRenderThread() > 0.0f)
		{
			ConfigString += FString::Printf(TEXT("Threshold: %.2f%% "), GCVarProfileGPU_ThresholdPercent.GetValueOnRenderThread());
		}

		if (ConfigString.Len() > 0)
		{
			ConfigString = FString(TEXT(", ")) + ConfigString;
		}

		UE_LOG(LogRHI, Log, TEXT("Perf marker hierarchy, total GPU time %.2fms%s"), RootResult * 1000.0f, *ConfigString);
		UE_LOG(LogRHI, Log, TEXT(""));

		// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled
		FText VsyncEnabledWarningText = FText::GetEmpty();
		static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		if (CVSyncVar->GetInt() != 0 && !PlatformDisablesVSync())
		{
			VsyncEnabledWarningText = LOCTEXT("GpuProfileVsyncEnabledWarning", "WARNING: This GPU profile was captured with v-sync enabled.  V-sync wait time may show up in any bucket, and as a result the data in this profile may be skewed. Please profile with v-sync disabled to obtain the most accurate data.");
			UE_LOG(LogRHI, Log, TEXT("%s"), *(VsyncEnabledWarningText.ToString()));
		}

		LogDisjointQuery();

		TMap<FString, FGPUProfilerEventNodeStats> EventHistogram;
		for (int32 BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			GatherStatsEventNode(EventTree[BaseNodeIndex], 0, EventHistogram);
		}

		FString RootWildcardString = GCVarProfileGPU_Root.GetValueOnRenderThread();
		FWildcardString RootWildcard(RootWildcardString);

		FGPUProfileStatSummary Summary;
		for (int32 BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			float Unused = 0;
			DumpStatsEventNode(EventTree[BaseNodeIndex], RootResult, 0, RootWildcard, false, Unused, /*inout*/ Summary);
		}
		Summary.PrintSummary();

		const bool bShowHistogram = GProfileShowEventHistogram.GetValueOnRenderThread() != 0;

		if (RootWildcardString == TEXT("*") && bShowHistogram)
		{
			struct FNodeStatsCompare
			{
				/** Sorts nodes by descending durations. */
				FORCEINLINE bool operator()(const FGPUProfilerEventNodeStats& A, const FGPUProfilerEventNodeStats& B) const
				{
					return B.TimingResult < A.TimingResult;
				}
			};

			// Sort descending based on node duration
			EventHistogram.ValueSort( FNodeStatsCompare() );

			// Log stats about the node histogram
			UE_LOG(LogRHI, Log, TEXT("Node histogram %u buckets"), EventHistogram.Num());

			// bad: reading on render thread but we don't support ECVF_RenderThreadSafe on strings yet
			// It's very unlikely to cause a problem as the cvar is only changes by the user.
			FString WildcardString = GProfileGPUPatternCVar.GetValueOnRenderThread();

			FGPUProfilerEventNodeStats Sum;

			const float ThresholdInMS = 5.0f;

			if(WildcardString == FString(TEXT("*")))
			{
				// disable Wildcard functionality
				WildcardString.Empty();
			}

			if(WildcardString.IsEmpty())
			{
				UE_LOG(LogRHI, Log, TEXT(" r.ProfileGPU.Pattern = '*' (using threshold of %g ms)"), ThresholdInMS);
			}
			else
			{
				UE_LOG(LogRHI, Log, TEXT(" r.ProfileGPU.Pattern = '%s' (not using time threshold)"), *WildcardString);
			}

			FWildcardString Wildcard(WildcardString);

			int32 NumNotShown = 0;
			for (TMap<FString, FGPUProfilerEventNodeStats>::TIterator It(EventHistogram); It; ++It)
			{
				const FGPUProfilerEventNodeStats& NodeStats = It.Value();

				bool bDump = NodeStats.TimingResult > RootResult * ThresholdInMS;

				if(!Wildcard.IsEmpty())
				{
					// if a Wildcard string was specified, we want to always dump all entries
					bDump = Wildcard.IsMatch(*It.Key());
				}

				if (bDump)
				{
					UE_LOG(LogRHI, Log, TEXT("   %.2fms   %s   Events %u   Draws %u"), NodeStats.TimingResult, *It.Key(), NodeStats.NumEvents, NodeStats.NumDraws);
					Sum += NodeStats;
				}
				else
				{
					NumNotShown++;
				}
			}

			UE_LOG(LogRHI, Log, TEXT("   Total %.2fms   Events %u   Draws %u,    %u buckets not shown"), 
				Sum.TimingResult, Sum.NumEvents, Sum.NumDraws, NumNotShown);
		}

#if !UE_BUILD_SHIPPING
		// Create and display profile visualizer data
		if (RHIConfig::ShouldShowProfilerAfterProfilingGPU())
		{

		// execute on main thread
			{
				struct FDisplayProfilerVisualizer
				{
					void Thread( TSharedPtr<FVisualizerEvent> InVisualizerData, const FText InVsyncEnabledWarningText )
					{
						static FName ProfileVisualizerModule(TEXT("ProfileVisualizer"));			
						if (FModuleManager::Get().IsModuleLoaded(ProfileVisualizerModule))
						{
							IProfileVisualizerModule& ProfileVisualizer = FModuleManager::GetModuleChecked<IProfileVisualizerModule>(ProfileVisualizerModule);
							// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled (otherwise InVsyncEnabledWarningText is empty)
							ProfileVisualizer.DisplayProfileVisualizer( InVisualizerData, TEXT("GPU"), InVsyncEnabledWarningText, FLinearColor::Red );
						}
					}
				} DisplayProfilerVisualizer;

				TSharedPtr<FVisualizerEvent> VisualizerData = CreateVisualizerData( EventTree );

				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DisplayProfilerVisualizer"),
					STAT_FSimpleDelegateGraphTask_DisplayProfilerVisualizer,
					STATGROUP_TaskGraphTasks);

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateRaw(&DisplayProfilerVisualizer, &FDisplayProfilerVisualizer::Thread, VisualizerData, VsyncEnabledWarningText),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DisplayProfilerVisualizer), nullptr, ENamedThreads::GameThread
				);
			}

			
		}
#endif
	}
}

void FGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	if (bTrackingEvents)
	{
		check(StackDepth >= 0);
		StackDepth++;

		check(IsInRenderingThread() || IsInRHIThread());
		if (CurrentEventNode)
		{
			// Add to the current node's children
			CurrentEventNode->Children.Add(CreateEventNode(Name, CurrentEventNode));
			CurrentEventNode = CurrentEventNode->Children.Last();
		}
		else
		{
			// Add a new root node to the tree
			CurrentEventNodeFrame->EventTree.Add(CreateEventNode(Name, NULL));
			CurrentEventNode = CurrentEventNodeFrame->EventTree.Last();
		}

		check(CurrentEventNode);
		// Start timing the current node
		CurrentEventNode->StartTiming();
	}
}

void FGPUProfiler::PopEvent()
{
	if (bTrackingEvents)
	{
		check(StackDepth >= 1);
		StackDepth--;

		check(CurrentEventNode && (IsInRenderingThread() || IsInRHIThread()));
		// Stop timing the current node and move one level up the tree
		CurrentEventNode->StopTiming();
		CurrentEventNode = CurrentEventNode->Parent;
	}
}

/** Whether GPU timing measurements are supported by the driver. */
bool FGPUTiming::GIsSupported = false;

/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
TStaticArray<uint64, MAX_NUM_GPUS> FGPUTiming::GTimingFrequency(InPlace, 0);

/**
* Two timestamps performed on GPU and CPU at nearly the same time.
* This can be used to visualize GPU and CPU timing events on the same timeline.
*/
TStaticArray<FGPUTimingCalibrationTimestamp, MAX_NUM_GPUS> FGPUTiming::GCalibrationTimestamp;

/** Whether the static variables have been initialized. */
bool FGPUTiming::GAreGlobalsInitialized = false;

#else

namespace UE::RHI::GPUProfiler
{
	RHI_API FRHIOnProfileGPU OnProfileGPU;

	TLockFreePointerListUnordered<void, PLATFORM_CACHE_LINE_SIZE> FEventStream::FChunk::MemoryPool;

	static TArray<FEventSink*>& GetSinks()
	{
		static TArray<FEventSink*> Sinks;
		return Sinks;
	}

	FEventSink::FEventSink()
	{
		GetSinks().Add(this);
	}

	FEventSink::~FEventSink()
	{
		GetSinks().RemoveSingle(this);
	}

	void ProcessEvents(TArrayView<FEventStream> EventStreams)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::RHI::GPUProfiler::ProcessEvents);

		TArray<TSharedRef<FEventStream>> SharedStreams;
		SharedStreams.Reserve(EventStreams.Num());

		for (FEventStream& Stream : EventStreams)
		{
			if (!Stream.IsEmpty())
			{
				SharedStreams.Emplace(MakeShared<FEventStream>(MoveTemp(Stream)));
			}
		}

		if (SharedStreams.Num())
		{
			for (FEventSink* Sink : GetSinks())
			{
				Sink->ProcessStreams(SharedStreams);
			}
		}
	}

	void InitializeQueues(TConstArrayView<FQueue> Queues)
	{
		for (FEventSink* Sink : GetSinks())
		{
			Sink->InitializeQueues(Queues);
		}
	}

#if WITH_PROFILEGPU
	template <uint32 Width>
	struct TUnicodeHorizontalBar
	{
		TCHAR Text[Width + 1];

		// 0 <= Value <= 1
		TUnicodeHorizontalBar(double Value)
		{
			TCHAR* Output = Text;
			int32 Solid, Partial, Blank;
			{
				double Integer;
				double Remainder = FMath::Modf(FMath::Clamp(Value, 0.0, 1.0) * Width, &Integer);

				Solid = (int32)Integer;
				Partial = (int32)FMath::Floor(Remainder * 8);
				Blank = (Width - Solid - (Partial > 0 ? 1 : 0));
			}

			// Solid characters
			for (int32 Index = 0; Index < Solid; ++Index)
			{
				*Output++ = TEXT('█');
			}

			// Partially filled character
			if (Partial > 0)
			{
				static constexpr TCHAR const Data[] = TEXT("▏▎▍▌▋▊▉");
				*Output++ = Data[Partial - 1];
			}

			// Blank Characters to pad out the width
			for (int32 Index = 0; Index < Blank; ++Index)
			{
				*Output++ = TEXT(' ');
			}

			*Output++ = 0;
			check(uintptr_t(Output) == (uintptr_t(Text) + sizeof(Text)));
		}
	};

	struct FNode
	{
		FString Name;

		FNode* Parent = nullptr;
		FNode* Next = nullptr;

		TArray<FNode*> Children;

		struct FStats
		{
			uint32 NumDraws      = 0;
			uint32 NumDispatches = 0;
			uint32 NumPrimitives = 0;
			uint32 NumVertices   = 0;

			uint64 BusyCycles    = 0;
			uint64 IdleCycles    = 0;
			uint64 WaitCycles    = 0;

			double GetBusyMilliseconds() const
			{
				return FPlatformTime::ToMilliseconds64(BusyCycles);
			}

			bool HasWork() const
			{
				return NumDraws > 0 || NumDispatches > 0;
			}

			FStats& operator += (FStats const& Stats)
			{
				NumDraws      += Stats.NumDraws;
				NumDispatches += Stats.NumDispatches;
				NumPrimitives += Stats.NumPrimitives;
				NumVertices   += Stats.NumVertices;
				BusyCycles    += Stats.BusyCycles;
				IdleCycles    += Stats.IdleCycles;
				WaitCycles    += Stats.WaitCycles;
				return *this;
			}

			FStats& operator += (FEvent::FStats const& Stats)
			{
				NumDraws      += Stats.NumDraws;
				NumDispatches += Stats.NumDispatches;
				NumPrimitives += Stats.NumPrimitives;
				NumVertices   += Stats.NumVertices;

				return *this;
			}

			void Accumulate(uint64 Busy, uint64 Wait, uint64 Idle)
			{
				BusyCycles += Busy;
				IdleCycles += Idle;
				WaitCycles += Wait;
			}
		};

		// Exclusive stats for this node
		FStats Exclusive;

		// Sum of stats including all children
		FStats Inclusive;

		FNode(FString&& Name)
			: Name(MoveTemp(Name))
		{}
	};

	struct FTable
	{
		bool const bUnicodeOutput;
		bool const bShowHeader;
		bool const bShowEmptyQueues;
		bool const bShowStats;
		bool const bShowPercent;
		bool const bShowInclusive;
		bool const bShowExclusive;

		FTable()
			: bUnicodeOutput  (GCVarProfileGPU_UnicodeOutput    .GetValueOnAnyThread())
			, bShowHeader     (GCVarProfileGPU_ShowHeader       .GetValueOnAnyThread())
			, bShowEmptyQueues(GCVarProfileGPU_ShowEmptyQueues  .GetValueOnAnyThread())
			, bShowStats      (GCVarProfileGPU_ShowStats        .GetValueOnAnyThread())
			, bShowPercent    (GCVarProfileGPU_ShowPercentColumn.GetValueOnAnyThread())
			, bShowInclusive  (GCVarProfileGPU_ShowInclusive    .GetValueOnAnyThread())
			, bShowExclusive  (GCVarProfileGPU_ShowExclusive    .GetValueOnAnyThread())
		{}

		enum class EColumn : uint32
		{
			Exclusive_NumDraws,
			Exclusive_NumDispatches,
			Exclusive_NumPrimitives,
			Exclusive_NumVertices,
			Exclusive_Percent,
			Exclusive_Time,

			Inclusive_NumDraws,
			Inclusive_NumDispatches,
			Inclusive_NumPrimitives,
			Inclusive_NumVertices,
			Inclusive_Percent,
			Inclusive_Time,

			Events,

			Num
		};

		uint32 GetColumnMinimumWidth(EColumn Column) const
		{
			switch (Column)
			{
			case EColumn::Events:
				return 6;
			}

			return 0;
		}

		TCHAR const* GetColumnHeader(EColumn Column) const
		{
			switch (Column)
			{
			case EColumn::Exclusive_NumDraws:
			case EColumn::Inclusive_NumDraws:
				return TEXT("Draws");

			case EColumn::Exclusive_NumDispatches:
			case EColumn::Inclusive_NumDispatches:
				return TEXT("Dsptch");
				
			case EColumn::Exclusive_NumPrimitives:
			case EColumn::Inclusive_NumPrimitives:
				return TEXT("Prim");

			case EColumn::Exclusive_NumVertices:
			case EColumn::Inclusive_NumVertices:
				return TEXT("Vert");

			case EColumn::Exclusive_Percent:
			case EColumn::Inclusive_Percent:
				return TEXT("Percent");
				
			case EColumn::Exclusive_Time:
			case EColumn::Inclusive_Time:
				return TEXT("Time");
			}

			return TEXT("");
		}

		uint32 GetColumnGroup(EColumn Column) const
		{
		switch (Column)
			{
			case EColumn::Exclusive_NumDraws:
			case EColumn::Exclusive_NumDispatches:
			case EColumn::Exclusive_NumPrimitives:
			case EColumn::Exclusive_NumVertices:
			case EColumn::Exclusive_Percent:
			case EColumn::Exclusive_Time:
				return 0;

			case EColumn::Inclusive_NumDraws:
			case EColumn::Inclusive_NumDispatches:
			case EColumn::Inclusive_NumPrimitives:
			case EColumn::Inclusive_NumVertices:
			case EColumn::Inclusive_Percent:
			case EColumn::Inclusive_Time:
				return 1;

			default:
			case EColumn::Events:
				return 2;
			}
		}

		TCHAR const* GetGroupName(uint32 GroupIndex) const
		{
			switch (GroupIndex)
			{
			case 0: return TEXT("Exclusive");
			case 1: return TEXT("Inclusive");
			case 2: return TEXT("Events");
			}
			return TEXT("");
		}

		uint32 NumRows = 0;
		TStaticArray<TArray<FString>, uint32(EColumn::Num)> Columns { InPlace };
		TArray<bool> RowBreaks;

		FString& Col(EColumn Column)
		{
			return Columns[uint32(Column)].Emplace_GetRef();
		}

		bool HasRows() const
		{
			return NumRows > 0;
		}

		void AddRow(FNode* Root, FNode::FStats const& Inclusive, FNode::FStats const& Exclusive, FString const& Name, uint32 Level)
		{
			double ExclusivePercent = double(Exclusive.BusyCycles) / Root->Inclusive.BusyCycles;
			double InclusivePercent = double(Inclusive.BusyCycles) / Root->Inclusive.BusyCycles;

			static constexpr uint32 BarWidth = 8;
			TUnicodeHorizontalBar<BarWidth> ExclusiveBar = ExclusivePercent;
			TUnicodeHorizontalBar<BarWidth> InclusiveBar = InclusivePercent;

			static constexpr TCHAR const BarSeparator[] = TEXT(" ┊ ");

			if (bShowExclusive)
			{
				if (bShowStats)
				{
					Col(EColumn::Exclusive_NumDraws     ) = FString::Printf(TEXT("%d"), Exclusive.NumDraws);
					Col(EColumn::Exclusive_NumDispatches) = FString::Printf(TEXT("%d"), Exclusive.NumDispatches);
					Col(EColumn::Exclusive_NumPrimitives) = FString::Printf(TEXT("%d"), Exclusive.NumPrimitives);
					Col(EColumn::Exclusive_NumVertices  ) = FString::Printf(TEXT("%d"), Exclusive.NumVertices);
				}

				if (bShowPercent)
				{
					Col(EColumn::Exclusive_Percent) = FString::Printf(TEXT("%.1f%%%s%s"), ExclusivePercent * 100.0, bUnicodeOutput ? BarSeparator : TEXT(""), bUnicodeOutput ? ExclusiveBar.Text : TEXT(""));
				}

				Col(EColumn::Exclusive_Time) = FString::Printf(TEXT("%.3f ms"), FPlatformTime::ToMilliseconds64(Exclusive.BusyCycles));
			}

			if (bShowInclusive)
			{
				if (bShowStats)
				{
					Col(EColumn::Inclusive_NumDraws     ) = FString::Printf(TEXT("%d"), Inclusive.NumDraws);
					Col(EColumn::Inclusive_NumDispatches) = FString::Printf(TEXT("%d"), Inclusive.NumDispatches);
					Col(EColumn::Inclusive_NumPrimitives) = FString::Printf(TEXT("%d"), Inclusive.NumPrimitives);
					Col(EColumn::Inclusive_NumVertices  ) = FString::Printf(TEXT("%d"), Inclusive.NumVertices);
				}

				if (bShowPercent)
				{
					Col(EColumn::Inclusive_Percent) = FString::Printf(TEXT("%.1f%%%s%s"), InclusivePercent * 100.0, bUnicodeOutput ? BarSeparator : TEXT(""), bUnicodeOutput ? InclusiveBar.Text : TEXT(""));
				}

				Col(EColumn::Inclusive_Time) = FString::Printf(TEXT("%.3f ms"), FPlatformTime::ToMilliseconds64(Inclusive.BusyCycles));
			}

			static constexpr uint32 SpacesPerIndent = 3;
			Col(EColumn::Events) = FString::Printf(TEXT("%*s"), Name.Len() + (Level * SpacesPerIndent), *Name);

			// Insert a horizontal rule before each root level row.
			RowBreaks.Add(Level == 0);

			NumRows++;
		}

		struct FChars
		{
			TCHAR const* Left;
			TCHAR const* GroupSeparator;
			TCHAR const* LastGroupSeparator;
			TCHAR const* Right;
			TCHAR const* CellSeparator;
		};

		struct FFormat
		{
			TCHAR const* LineMajor;
			TCHAR const* LineMinor;
			TCHAR const* Indent;

			FChars const TopRow;
			FChars const GroupNameRow;
			FChars const GroupBorderRow;
			FChars const ValueRow;
			FChars const DividorRow;
			FChars const BottomRow;
		};

		FString ToString() const
		{
			if (bUnicodeOutput)
			{
				static constexpr FFormat Unicode = 
				{
					.LineMajor  = TEXT("━"),
					.LineMinor  = TEXT("─"),
					.Indent     = TEXT("    "),

					//                 Left     GrpSep     LastGrp     Right     CellSep
					.TopRow        { TEXT("┏"), TEXT("┳"), TEXT("┳"), TEXT("┓"), TEXT(" ") },
					.GroupNameRow  { TEXT("┃"), TEXT("┃"), TEXT("┃"), TEXT("┃"), TEXT(" ") },
					.GroupBorderRow{ TEXT("┠"), TEXT("╂"), TEXT("┨"), TEXT("┃"), TEXT("┬") },
					.ValueRow      { TEXT("┃"), TEXT("┃"), TEXT("┃"), TEXT("┃"), TEXT("│") },
					.DividorRow    { TEXT("┠"), TEXT("╂"), TEXT("╂"), TEXT("┨"), TEXT("┼") },
					.BottomRow     { TEXT("┗"), TEXT("┻"), TEXT("┻"), TEXT("┛"), TEXT("┷") },
				};

				return ToStringInner(Unicode);
			}
			else
			{
				static constexpr FFormat Ascii = 
				{
					.LineMajor  = TEXT("-"),
					.LineMinor  = TEXT("-"),
					.Indent = TEXT("    "),

					//                 Left     GrpSep     LastGrp     Right     CellSep
					.TopRow        { TEXT("+"), TEXT("+"), TEXT("+"), TEXT("+"), TEXT(" ") },
					.GroupNameRow  { TEXT("|"), TEXT("|"), TEXT("|"), TEXT("|"), TEXT(" ") },
					.GroupBorderRow{ TEXT("+"), TEXT("+"), TEXT("+"), TEXT("|"), TEXT("+") },
					.ValueRow      { TEXT("|"), TEXT("|"), TEXT("|"), TEXT("|"), TEXT("|") },
					.DividorRow    { TEXT("+"), TEXT("+"), TEXT("+"), TEXT("+"), TEXT("+") },
					.BottomRow     { TEXT("+"), TEXT("+"), TEXT("+"), TEXT("+"), TEXT("+") },
				};

				return ToStringInner(Ascii);
			}
		}

		FString ToStringInner(FFormat const& Format) const
		{
			struct FGroup  { uint32 Index, Width; };
			struct FColumn { uint32 Index, Width; };

			static constexpr uint32 NumGroups = 3;
			static constexpr uint32 CellPadding = 1;

			// Auto-size column widths to their contents
			TStaticArray<int32, uint32(EColumn::Num)> ColumnWidths{ InPlace, 0 };
			for (uint32 ColumnIndex = 0; ColumnIndex < uint32(EColumn::Num); ++ColumnIndex)
			{
				if (Columns[ColumnIndex].Num() == 0)
					continue;

				check(Columns[ColumnIndex].Num() == NumRows);

				int32& Width = ColumnWidths[ColumnIndex];

				// Auto-size column width
				Width = GetColumnMinimumWidth(EColumn(ColumnIndex));
				Width = FMath::Max(Width, FCString::Strlen(GetColumnHeader(EColumn(ColumnIndex))));

				for (FString const& Cell : Columns[ColumnIndex])
				{
					Width = FMath::Max(Width, Cell.Len());
				}
			}

			FString Result;

			auto EmitGroupRow = [&](FChars const& Chars, TUniqueFunction<void(FGroup)> GroupCallback)
			{
				uint32 const CellSeparatorLength = FCString::Strlen(Chars.CellSeparator);

				Result += Format.Indent;
				Result += Chars.Left;

				uint32 GroupWidth = 0;
				uint32 GroupIndex = 0;

				for (uint32 ColumnIndex = 0; ColumnIndex < uint32(EColumn::Num); ++ColumnIndex)
				{
					if (Columns[ColumnIndex].Num() == 0)
						continue;

					GroupWidth += ColumnWidths[ColumnIndex] + CellPadding * 2;
					GroupIndex = GetColumnGroup(EColumn(ColumnIndex));

					if (GroupIndex != GetColumnGroup(EColumn(ColumnIndex + 1)))
					{ 
						// Group Change
						GroupCallback({ GroupIndex, GroupWidth });

						// Add the group separator character
						Result += GroupIndex < NumGroups - 2
							? Chars.GroupSeparator
							: Chars.LastGroupSeparator;

						GroupWidth = 0;
					}
					else if (ColumnIndex < uint32(EColumn::Num) - 1)
					{
						// Same group. Count the (missing) cell division
						GroupWidth += CellSeparatorLength;
					}
				}

				// Emit final group
				GroupCallback({ GroupIndex, GroupWidth });

				// Close the row
				Result += Chars.Right;
				Result += TEXT("\n");
			};

			auto EmitValueRow = [&](bool bGroupBorderRow, FChars const& Chars, TUniqueFunction<void(FColumn)> CellCallback)
			{
				Result += Format.Indent;

				uint32 FirstColumnIndex = 0;

				if (bGroupBorderRow)
				{
					// Find first visible column
					while (FirstColumnIndex < uint32(EColumn::Num) && Columns[FirstColumnIndex].Num() == 0)
					{
						++FirstColumnIndex;
					}

					Result += FirstColumnIndex < uint32(EColumn::Events)
						? Chars.Left
						: Chars.Right;
				}
				else
				{
					Result += Chars.Left;
				}

				for (uint32 ColumnIndex = FirstColumnIndex; ColumnIndex < uint32(EColumn::Num); ++ColumnIndex)
				{
					if (Columns[ColumnIndex].Num() == 0)
						continue;

					CellCallback({ ColumnIndex, ColumnWidths[ColumnIndex] + (CellPadding * 2) });

					if (ColumnIndex < uint32(EColumn::Num) - 1)
					{
						// Find next visible column
						uint32 NextColumnIndex = ColumnIndex + 1;
						while (NextColumnIndex < uint32(EColumn::Num) && Columns[NextColumnIndex].Num() == 0)
						{
							++NextColumnIndex;
						}

						uint32 CurrentGroupIndex = GetColumnGroup(EColumn(ColumnIndex));
						uint32 NextGroupIndex    = GetColumnGroup(EColumn(NextColumnIndex));

						if (CurrentGroupIndex != NextGroupIndex)
						{
							// Group change, add the group separator
							Result += NextGroupIndex < NumGroups - 1
								? Chars.GroupSeparator
								: Chars.LastGroupSeparator;
						}
						else
						{
							// Same group, add the cell separator
							Result += Chars.CellSeparator;
						}
					}
				}

				// Close the row
				Result += Chars.Right;
				Result += TEXT("\n");
			};

			auto AlignCenter = [&](TCHAR const* Str, uint32 Width)
			{
				int32 PaddingLeft = FMath::Max(0, int32(Width) - FCString::Strlen(Str));
				int32 PaddingRight = (PaddingLeft / 2) + (PaddingLeft & 1);
				PaddingLeft /= 2;

				Result += FString::Printf(TEXT("%*s%s%*s"), PaddingLeft, TEXT(""), Str, PaddingRight, TEXT(""));
			};

			// Top Border
			EmitGroupRow(Format.TopRow, [&](FGroup Group)
			{
				while (Group.Width--)
				{
					Result += Format.LineMajor;
				}
			});

			// Exclusive / Inclusive Group Row
			EmitGroupRow(Format.GroupNameRow, [&](FGroup Group)
			{
				TCHAR const* Str = Group.Index != GetColumnGroup(EColumn::Events)
					? GetGroupName(Group.Index)
					: TEXT("");

				AlignCenter(Str, Group.Width);
			});

			// Events Group Row
			EmitValueRow(true, Format.GroupBorderRow, [&](FColumn Column)
			{
				if (Column.Index == uint32(EColumn::Events))
				{
					AlignCenter(GetGroupName(GetColumnGroup(EColumn::Events)), Column.Width);
				}
				else
				{
					while (Column.Width--)
					{
						Result += Format.LineMinor;
					}
				}
			});

			// Header Row
			EmitValueRow(false, Format.ValueRow, [&](FColumn Column)
			{
				AlignCenter(GetColumnHeader(EColumn(Column.Index)), Column.Width);
			});

			// Header Border Row
			EmitValueRow(false, Format.DividorRow, [&](FColumn Column)
			{
				while (Column.Width--)
				{
					Result += Format.LineMinor;
				}
			});

			// Value rows
			for (uint32 RowIndex = 0; RowIndex < NumRows; ++RowIndex)
			{
				if (RowIndex > 0 && RowBreaks[RowIndex])
				{
					// Add a horizontal rule
					EmitValueRow(false, Format.DividorRow, [&](FColumn Column)
					{
						while (Column.Width--)
						{
							Result += Format.LineMinor;
						}
					});
				}

				EmitValueRow(false, Format.ValueRow, [&](FColumn Column)
				{
					int32 Width = Column.Width - (CellPadding * 2);
					if (EColumn(Column.Index) == EColumn::Events)
					{
						Width = -Width; // Align left
					}

					FString const& Cell = Columns[Column.Index][RowIndex];
					Result += FString::Printf(TEXT("%*s%*s%*s")
						, CellPadding, TEXT("")
						, Width, *Cell
						, CellPadding, TEXT(""));
				});
			}

			// Bottom Border
			EmitValueRow(false, Format.BottomRow, [&](FColumn Column)
			{
				while (Column.Width--)
				{
					Result += Format.LineMajor;
				}
			});

			return Result;
		}
	};
#endif

#if HAS_GPU_STATS
	// Per queue GPU stats

	// Total busy time on the current queue. StatName == "Unaccounted" is used by the Csv profiler
	static FGPUStat GPUStat_Total(TEXT("Unaccounted"), TEXT("Queue Total"));
#endif

#if STATS
	TCHAR const* FGPUStat::GetTypeString(EType Type)
	{
		switch (Type)
		{
		default: checkNoEntry(); [[fallthrough]];
		case EType::Busy: return TEXT("Busy");
		case EType::Wait: return TEXT("Wait");
		case EType::Idle: return TEXT("Idle");
		}
	}

	FString FGPUStat::GetIDString(FQueue Queue, bool bFriendly)
	{
		if (bFriendly)
		{
			return FString::Printf(TEXT("GPU %d %s Queue %d")
				, Queue.GPU
				, Queue.GetTypeString()
				, Queue.Index
			);
		}
		else
		{
			return FString::Printf(TEXT("GPU%d_%s%d")
				, Queue.GPU
				, Queue.GetTypeString()
				, Queue.Index
			);
		}
	}

	FGPUStat::FStatInstance::FInner& FGPUStat::GetStatInstance(FQueue Queue, EType Type)
	{
		FStatInstance& Instance = Instances.FindOrAdd(Queue);

		switch (Type)
		{
		default: checkNoEntry(); [[fallthrough]];
		case EType::Busy: return Instance.Busy; break;
		case EType::Wait: return Instance.Wait; break;
		case EType::Idle: return Instance.Idle; break;
		}
	}

	TMap<FQueue, TUniquePtr<FGPUStat::FStatCategory>> FGPUStat::FStatCategory::Categories;

	FGPUStat::FStatCategory::FStatCategory(FQueue Queue)
		: GroupName(FString::Printf(TEXT("STATGROUP_%s"), *GetIDString(Queue, false)))
		, GroupDesc(FString::Printf(TEXT("%s Timing"), *GetIDString(Queue, true)))
	{}

	TStatId FGPUStat::GetStatId(FQueue Queue, EType Type)
	{
		FStatInstance::FInner& Instance = GetStatInstance(Queue, Type);

		if (!Instance.Stat)
		{
			TUniquePtr<FStatCategory>& Category = FStatCategory::Categories.FindOrAdd(Queue);
			if (!Category)
			{
				Category = MakeUnique<FStatCategory>(Queue);
			}

			// Encode the stat type in the FName number
			Instance.StatName = FName(*FString::Printf(TEXT("STAT_%s_%s"), *GetIDString(Queue, false), DisplayName), int32(Type));

			Instance.Stat = MakeUnique<FDynamicStat>(
				Instance.StatName,
				DisplayName,
				*Category->GroupName,
				FStatNameAndInfo::GpuStatCategory,
				*Category->GroupDesc,
				true, // IsDefaultEnabled
				true, // IsClearEveryFrame
				EStatDataType::ST_double,
				false, // IsCycleStat
				false, // SortByName
				FPlatformMemory::MCR_Invalid
			);
		}

		return Instance.Stat->GetStatId();
	}

#endif

	// Handles computing the "stat unit" GPU time, "stat gpu" stats, and "profilegpu".
	struct FGPUProfilerSink_StatSystem final : public FEventSink
	{
		class FTimestampStream
		{
		private:
			TArray<uint64> Values;

		public:
			struct FState
			{
				FTimestampStream const& Stream;
				int32 TimestampIndex = 0;
				uint64 BusyCycles = 0;

				FState(FTimestampStream const& Stream)
					: Stream(Stream)
				{}

				uint64 GetCurrentTimestamp (uint64 Anchor) const { return Stream.Values[TimestampIndex] - Anchor; }
				uint64 GetPreviousTimestamp(uint64 Anchor) const { return Stream.Values[TimestampIndex - 1] - Anchor; }

				bool HasMoreTimestamps() const { return TimestampIndex < Stream.Values.Num(); }
				bool IsStartingWork   () const { return (TimestampIndex & 0x01) == 0x00; }
				void AdvanceTimestamp () { TimestampIndex++; }
			};

			void AddTimestamp(uint64 Value, bool bBegin)
			{
				if (bBegin)
				{
					if (!Values.IsEmpty() && Value <= Values.Last())
					{
						//
						// The Begin TOP event is sooner than the last End BOP event.
						// The markers overlap, and the GPU was not idle.
						// 
						// Remove the previous End event, and discard this Begin event.
						//
						Values.RemoveAt(Values.Num() - 1, EAllowShrinking::No);
					}
					else
					{
						// GPU was idle. Keep this timestamp.
						Values.Add(Value);
					}
				}
				else
				{
					Values.Add(Value);
				}
			}

			static uint64 ComputeUnion(TArrayView<FTimestampStream::FState> Streams)
			{
				// The total number of cycles where at least one GPU pipe was busy.
				uint64 UnionBusyCycles = 0;

				uint64 LastMinCycles = 0;
				int32 BusyPipes = 0;
				bool bFirst = true;

				uint64 Anchor = 0; // @todo - handle possible timestamp wraparound

				// Process the time ranges from each pipe.
				while (true)
				{
					// Find the next minimum timestamp
					FTimestampStream::FState* NextMin = nullptr;
					for (auto& Current : Streams)
					{
						if (Current.HasMoreTimestamps() && (!NextMin || Current.GetCurrentTimestamp(Anchor) < NextMin->GetCurrentTimestamp(Anchor)))
						{
							NextMin = &Current;
						}
					}

					if (!NextMin)
						break; // No more timestamps to process

					if (!bFirst)
					{
						if (BusyPipes > 0 && NextMin->GetCurrentTimestamp(Anchor) > LastMinCycles)
						{
							// Accumulate the union busy time across all pipes
							UnionBusyCycles += NextMin->GetCurrentTimestamp(Anchor) - LastMinCycles;
						}

						if (!NextMin->IsStartingWork())
						{
							// Accumulate the busy time for this pipe specifically.
							NextMin->BusyCycles += NextMin->GetCurrentTimestamp(Anchor) - NextMin->GetPreviousTimestamp(Anchor);
						}
					}

					LastMinCycles = NextMin->GetCurrentTimestamp(Anchor);

					BusyPipes += NextMin->IsStartingWork() ? 1 : -1;
					check(BusyPipes >= 0);

					NextMin->AdvanceTimestamp();
					bFirst = false;
				}

				check(BusyPipes == 0);

				return UnionBusyCycles;
			}
		};

		struct FStatState
		{
			struct
			{
				uint64 BusyCycles = 0;
				uint64 IdleCycles = 0;
				uint64 WaitCycles = 0;

				void Accumulate(uint64 Busy, uint64 Wait, uint64 Idle)
				{
					BusyCycles += Busy;
					IdleCycles += Idle;
					WaitCycles += Wait;
				}
			} Exclusive, Inclusive;

			FStatState() = default;
			FStatState(FStatState const&) = default;

			FStatState(FStatState&& Other)
				: FStatState(Other)
			{
				Other.Exclusive = {};
				Other.Inclusive = {};
			}

		#if HAS_GPU_STATS
			void EmitResults(FQueue Queue, FGPUStat& GPUStat
			#if STATS
				, FEndOfPipeStats* Stats
			#endif
			#if CSV_PROFILER_STATS
				, FCsvProfiler* CsvProfiler
			#endif
			) const
			{
			#if STATS
				Stats->AddMessage(GPUStat.GetStatId(Queue, FGPUStat::EType::Busy).GetName(), EStatOperation::Set, FPlatformTime::ToMilliseconds64(Inclusive.BusyCycles));
				Stats->AddMessage(GPUStat.GetStatId(Queue, FGPUStat::EType::Idle).GetName(), EStatOperation::Set, FPlatformTime::ToMilliseconds64(Inclusive.IdleCycles));
				Stats->AddMessage(GPUStat.GetStatId(Queue, FGPUStat::EType::Wait).GetName(), EStatOperation::Set, FPlatformTime::ToMilliseconds64(Inclusive.WaitCycles));
			#endif

			#if CSV_PROFILER_STATS
				if (CsvProfiler && Queue.Type == FQueue::EType::Graphics && Queue.Index == 0)
				{
					if (!GPUStat.CsvStat.IsSet())
					{
						static TArray<TUniquePtr<FCsvCategory>> CsvGPUCategories;
						if (!CsvGPUCategories.IsValidIndex(Queue.GPU))
						{
							CsvGPUCategories.SetNum(Queue.GPU + 1);
						}

						TUniquePtr<FCsvCategory>& Category = CsvGPUCategories[Queue.GPU];
						if (!Category)
						{
							Category = Queue.GPU > 0
								? MakeUnique<FCsvCategory>(*FString::Printf(TEXT("GPU%d"), Queue.GPU + 1), true)
								: MakeUnique<FCsvCategory>(TEXT("GPU"), true);
						}

						GPUStat.CsvStat.Emplace(GPUStat.StatName, Category->Index);
					}

					uint64 TotalCycles = Exclusive.BusyCycles + Exclusive.WaitCycles;
					CsvProfiler->RecordEndOfPipeCustomStat(GPUStat.CsvStat->Name, GPUStat.CsvStat->CategoryIndex, FPlatformTime::ToMilliseconds64(TotalCycles), ECsvCustomStatOp::Set);
				}
			#endif
			}
		#endif
		};

		struct FQueueTimestamps
		{
			FTimestampStream Queue;
			FStatState WholeQueueStat;

			uint64 CPUFrameBoundary = 0;

			// Used to override the GPU time calculation for this queue, if an FFrameTime event is in the stream
			TOptional<uint64> TotalBusyCycles;

		#if WITH_RHI_BREADCRUMBS
			TMap<FRHIBreadcrumbData_Stats, FStatState> Stats;
		#endif
		};

		struct FResolvedWait
		{
			uint64 GPUTimestampTOP = 0;
			uint64 CPUTimestamp = 0;
		};

		struct FResolvedSignal
		{
			uint64 GPUTimestampBOP = 0;
			uint64 Value = 0;
		};

		struct FFrameState : TMap<FQueue, FQueueTimestamps>
		{
		#if STATS
			TOptional<int64> StatsFrame;
		#endif
		};

		struct FQueueState
		{
			FQueue const Queue;
			TSpscQueue<FEventSink::FIterator> PendingStreams;

			// Array of fence signal history. Events are kept until all queues have processed events
			// later than the CPU timestamps of these signals. The old events are then trimmed.
			TArray<FResolvedSignal> Signals;

			// The value of the latest signaled fence on this queue.
			FResolvedSignal MaxSignal;

			// The GPU timestamp of the last event processed.
			uint64 LastGPUCycles = 0;

			FQueueTimestamps Timestamps;

			bool bBusy = false;

			bool bWasTraced = false;

		#if WITH_RHI_BREADCRUMBS
			TMap<FRHIBreadcrumbData_Stats, int32> ActiveStats;
			TArray<FRHIBreadcrumbData_Stats> ActiveStatsStack;
			FRHIBreadcrumbNode* Breadcrumb = nullptr;
		#endif

		#if WITH_PROFILEGPU
			struct
			{
				TArray<TUniquePtr<FNode>> Nodes;
				FNode* Current = nullptr;
				FNode* Prev = nullptr;
				FNode* First = nullptr;
				bool bProfileFrame = false;

				void PushNode(FString&& Name)
				{
					FNode* Parent = Current;
					Current = Nodes.Emplace_GetRef(MakeUnique<FNode>(MoveTemp(Name))).Get();
					Current->Parent = Parent;

					if (!First)
					{
						First = Current;
					}

					if (Parent)
					{
						Parent->Children.Add(Current);
					}

					if (Prev)
					{
						Prev->Next = Current;
					}
					Prev = Current;
				}

				void PopNode()
				{
					check(Current && Current->Parent);
					Current = Current->Parent;
				}

				void LogTree(FQueueState const& QueueState, uint32 FrameNumber) const
				{
					FTable Table;

					EGPUProfileSortMode SortMode = (EGPUProfileSortMode)FMath::Clamp(GCVarProfileGPU_Sort.GetValueOnAnyThread(), 0, ((int32)EGPUProfileSortMode::Max - 1));
					FWildcardString RootWildcard(GCVarProfileGPU_Root.GetValueOnAnyThread());
					const bool bShowEmptyNodes = GCVarProfileGPU_ShowLeafEvents.GetValueOnAnyThread();
					const double PercentThreshold = FMath::Clamp(GCVarProfileGPU_ThresholdPercent.GetValueOnAnyThread(), 0.0f, 100.0f);

					if (SortMode != EGPUProfileSortMode::Chronological)
					{
						for (FNode* Node = First; Node; Node = Node->Next)
						{
							Node->Children.Sort([SortMode](FNode const& A, FNode const& B)
							{
								switch (SortMode)
								{
								default:
								case EGPUProfileSortMode::TimeElapsed: return B.Inclusive.BusyCycles    < A.Inclusive.BusyCycles;
								case EGPUProfileSortMode::NumPrims   : return B.Inclusive.NumPrimitives < A.Inclusive.NumPrimitives;
								case EGPUProfileSortMode::NumVerts   : return B.Inclusive.NumVertices   < A.Inclusive.NumVertices;
								}
							});
						}
					}

					auto Recurse = [&](auto& Recurse, FNode* Root, FNode* CurrentNode, bool bParentMatchedFilter, int32 Level) -> bool
					{
						// Percent that this node was of the total frame time
						const double Percent = Root ? (CurrentNode->Inclusive.GetBusyMilliseconds() / Root->Inclusive.GetBusyMilliseconds()) * 100.0 : 100.0;

						// Filter nodes according to cvar settings
						const bool bAboveThreshold = Percent >= PercentThreshold;
						const bool bNameMatches = bParentMatchedFilter || RootWildcard.IsMatch(CurrentNode->Name);
						const bool bHasWork = bShowEmptyNodes || CurrentNode->Inclusive.HasWork();

						const bool bDisplayEvent = bNameMatches && bHasWork && bAboveThreshold;
			
						if (bDisplayEvent)
						{
							if (Root == nullptr)
							{
								Root = CurrentNode;
							}

							Table.AddRow(
								Root,
								CurrentNode->Inclusive,
								CurrentNode->Exclusive,
								CurrentNode->Name,
								Level
							);
						}

						FNode::FStats OtherChildrenInclusive;
						FNode::FStats OtherChildrenExclusive;
						uint32 NumHiddenChildren = 0;

						for (FNode* Child : CurrentNode->Children)
						{
							bool bChildShown = Recurse(Recurse, Root, Child, bDisplayEvent, bDisplayEvent ? Level + 1 : Level);
							if (!bChildShown)
							{
								OtherChildrenInclusive += Child->Inclusive;
								OtherChildrenExclusive += Child->Exclusive;

								NumHiddenChildren++;
							}
						}

						if (bDisplayEvent && NumHiddenChildren > 0)
						{
							// Don't show the "other children" node if their total inclusive time is still below the percent threshold
							if ((double(OtherChildrenInclusive.BusyCycles) / Root->Inclusive.BusyCycles) >= PercentThreshold)
							{
								Table.AddRow(
									Root,
									OtherChildrenInclusive,
									OtherChildrenExclusive,
									FString::Printf(TEXT("%d Other %s"), NumHiddenChildren, NumHiddenChildren >= 2 ? TEXT("Children") : TEXT("Child")),
									Level + 1
								);
							}
						}
			
						return bDisplayEvent;
					};

					// Skip building the table if there was no useful work
					if (First && First->Inclusive.BusyCycles > 0)
					{
						Recurse(Recurse, nullptr, First, false, 0);
					}

					FString Header;
					if (Table.bShowHeader)
					{
						Header = FString::Printf(
							TEXT("    - %-30s: %.2fms\n")
							TEXT("    - %-30s: \"%s\"\n")
							TEXT("    - %-30s: %.2f%%\n")
							TEXT("    - %-30s: %s\n")
							TEXT("\n")
							, TEXT("Frame Time")
							, First ? First->Inclusive.GetBusyMilliseconds() : 0.0
							, *IConsoleManager::Get().FindConsoleObjectName(GCVarProfileGPU_Root.AsVariable())
							, *RootWildcard
							, *IConsoleManager::Get().FindConsoleObjectName(GCVarProfileGPU_ThresholdPercent.AsVariable())
							, PercentThreshold
							, *IConsoleManager::Get().FindConsoleObjectName(GCVarProfileGPU_ShowLeafEvents.AsVariable())
							, bShowEmptyNodes ? TEXT("true") : TEXT("false")
						);
					}

					FString GPUString = FString::Printf(TEXT("Frame %d - %s %d - GPU %d")
						, FrameNumber
						, QueueState.Queue.GetTypeString()
						, QueueState.Queue.Index
						, QueueState.Queue.GPU
					);

					FString Final = FString::Printf(
						TEXT("\n")
						TEXT("GPU Profile for %s\n")
						TEXT("\n")
						TEXT("%s")
						TEXT("%s")
						, *GPUString
						, *Header
						, Table.HasRows() ? *Table.ToString() : TEXT("    No recorded work for this queue.\n")
					);

					if (Table.HasRows() || Table.bShowEmptyQueues)
					{
						TArray<FString> Lines;
						Final.ParseIntoArrayLines(Lines, false);

						for (FString const& Line : Lines)
						{
							UE_LOG(LogRHI, Display, TEXT("%s"), *Line);
						}
					}

				#if !UE_BUILD_SHIPPING
					// Create and display profile visualizer data for the graphics queue
					if (GCVarProfileGPU_ShowUI.GetValueOnAnyThread() && QueueState.Queue.Type == FQueue::EType::Graphics && Table.HasRows())
					{
						// Count the total number of exclusive cycles in the frame. Needed to draw the horizontal bars in the visualizer.
						uint64 TotalBusyCycles = 0;
						{
							auto CountCycles = [&TotalBusyCycles](auto& CountCycles, FNode* CurrentNode) -> void
							{
								TotalBusyCycles += CurrentNode->Exclusive.BusyCycles;
								for (FNode* Child : CurrentNode->Children)
								{
									CountCycles(CountCycles, Child);
								}
							};
							CountCycles(CountCycles, First);
						}

						// Recursive function to build the visualizer data structs from our nodes.
						uint64 StartCycles = 0;
						auto BuildData = [&](auto& BuildData, FNode* CurrentNode, TSharedPtr<FVisualizerEvent> const& Parent) -> TSharedPtr<FVisualizerEvent>
						{
							TSharedPtr<FVisualizerEvent> VisualizerEvent = MakeShared<FVisualizerEvent>(
								double(StartCycles) / TotalBusyCycles,
								double(CurrentNode->Inclusive.BusyCycles) / TotalBusyCycles,
								CurrentNode->Inclusive.GetBusyMilliseconds(),
								0,
								CurrentNode->Name
							);
							VisualizerEvent->ParentEvent = Parent;

							StartCycles += CurrentNode->Exclusive.BusyCycles;

							for (FNode* ChildNode : CurrentNode->Children)
							{
								VisualizerEvent->Children.Add(BuildData(BuildData, ChildNode, VisualizerEvent));
							}

							return VisualizerEvent;
						};

						// Launch the visualizer on the game thread
						FFunctionGraphTask::CreateAndDispatchWhenReady([
							Title = GPUString,
							VisualizerData = BuildData(BuildData, First, nullptr)
						]
						{
							static FName ProfileVisualizerModule(TEXT("ProfileVisualizer"));
							if (FModuleManager::Get().IsModuleLoaded(ProfileVisualizerModule))
							{
								IProfileVisualizerModule& ProfileVisualizer = FModuleManager::GetModuleChecked<IProfileVisualizerModule>(ProfileVisualizerModule);
								// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled (otherwise InVsyncEnabledWarningText is empty)
								ProfileVisualizer.DisplayProfileVisualizer(VisualizerData, *Title);
							}
						}, TStatId(), nullptr, ENamedThreads::GameThread);
					}
				#endif
				}
			} Profile;
		#endif

			FQueueState(FQueue const& Queue)
				: Queue(Queue)
			{}

			void ResolveSignal(FEvent::FSignalFence const& Event)
			{
				FResolvedSignal& Result = Signals.Emplace_GetRef();
		
				//
				// Take the max between the previous GPU EndWork event and the CPU timestamp. The signal cannot have happened on the GPU until the CPU has submitted the command to the driver.
				// 
				// An example would be a GPU queue that completes work and goes idle at time T. Later, the CPU issues a Signal without other prior work at time T + 100ms.
				// The fence signal cannot have happened until time T + 100ms because the CPU hadn't instructed the GPU to do so until then.
				// LastGPUCycles would still be set to time T, since that was the time of the preceeding EndWork event.
				//
				Result.GPUTimestampBOP = FMath::Max(LastGPUCycles, Event.CPUTimestamp);
				Result.Value = Event.Value;

				FGpuProfilerTrace::SignalFence(Queue.Value, Result.GPUTimestampBOP, Event.Value);

				//
				// Fences signals *MUST* be sequential, to remove ambiguity caused by trimming the Signals array.
				// 
				// To explain why, assume non-sequential signals are allowed, and consider the following example events on an arbitrary queue:
				// 
				//		     [Signal 2]
				//		-- Frame Boundary --
				//		     [Signal 4]
				//
				// Assume, after trimming events earlier than the frame boundary, that only [Signal 4] remains in the Signals array.
				// Then, some other queue attempts to [Wait 3]. We need to compute when [Wait 3] is resolved with only the information about [Signal 4].
				// 
				// Given that fences resolve waits as soon as the signalled value is >= the wait value, we could assume the fence was resolved at [Signal 4].
				// However, we don't know if the fence was already signalled to value 3 before the frame boundary and the trimming.
				// 
				// Without this information, it is ambiguous whether [Wait 3] is already resolved by a [Signal 3] before the frame boundary that is no longer
				// in the Signals array, or won't be resolved until [Signal 4]. We could have had this sequence of events:
				// 
				//		     [Signal 2]
				//		     [Signal 3]
				//		-- Frame Boundary --
				//		     [Signal 4]
				// 
				// Requiring that fences are always signalled in sequential order solves this.
				// If the awaited value is less than the first Signal, the fence has already been signalled before the frame boundary.
				//
				checkf(Result.Value == MaxSignal.Value + 1, TEXT("Fence signals must be sequential. Result.Value: %llu, MaxSignal.Value + 1: %llu"), Result.Value, (MaxSignal.Value + 1));

				// Signals should always advance in time
				checkf(Result.GPUTimestampBOP >= MaxSignal.GPUTimestampBOP, TEXT("Signals should always advance in time. Result.GPUTimestampBOP: %llu, MaxSignal.GPUTimestampBOP: %llu"), Result.GPUTimestampBOP, MaxSignal.GPUTimestampBOP);

				MaxSignal = Result;
			}

			void AccumulateTime(uint64 Busy, uint64 Wait, uint64 Idle)
			{
			#if WITH_RHI_BREADCRUMBS
				// Apply the timings to all active stats
				for (auto const& [Stat, RefCount] : ActiveStats)
				{
					FStatState& State = Timestamps.Stats.FindChecked(Stat);
					State.Inclusive.Accumulate(Busy, Wait, Idle);

					if (ActiveStatsStack.Num() > 0 && ActiveStatsStack.Last() == Stat)
					{
						State.Exclusive.Accumulate(Busy, Wait, Idle);
					}
				}

				if (ActiveStatsStack.Num() == 0)
			#endif
				{
					Timestamps.WholeQueueStat.Exclusive.Accumulate(Busy, Wait, Idle);
				}

				Timestamps.WholeQueueStat.Inclusive.Accumulate(Busy, Wait, Idle);

			#if WITH_PROFILEGPU
				for (FNode* Node = Profile.Current; Node; Node = Node->Parent)
				{
					Node->Inclusive.Accumulate(Busy, Wait, Idle);

					if (Node == Profile.Current)
					{
						Node->Exclusive.Accumulate(Busy, Wait, Idle);
					}
				}
			#endif
			}

			void BeginWork(FEvent::FBeginWork const& Event)
			{
				Timestamps.Queue.AddTimestamp(Event.GPUTimestampTOP, true);

				uint64 Idle = Event.CPUTimestamp > LastGPUCycles
					? Event.CPUTimestamp - LastGPUCycles
					: 0;

				AccumulateTime(0, 0, Idle);

				FGpuProfilerTrace::BeginWork(Queue.Value, Event.GPUTimestampTOP, Event.CPUTimestamp);

				LastGPUCycles = FMath::Max(LastGPUCycles, Event.GPUTimestampTOP);
			}

			void EndWork(FEvent::FEndWork const& Event)
			{
				Timestamps.Queue.AddTimestamp(Event.GPUTimestampBOP, false);

				uint64 Busy = Event.GPUTimestampBOP > LastGPUCycles
					? Event.GPUTimestampBOP - LastGPUCycles
					: 0;

				AccumulateTime(Busy, 0, 0);

				FGpuProfilerTrace::EndWork(Queue.Value, Event.GPUTimestampBOP);

				LastGPUCycles = FMath::Max(LastGPUCycles, Event.GPUTimestampBOP);
			}

		#if WITH_RHI_BREADCRUMBS
			void BeginBreadcrumb(FEvent::FBeginBreadcrumb const& Event)
			{
				uint64 Busy = Event.GPUTimestampTOP > LastGPUCycles
					? Event.GPUTimestampTOP - LastGPUCycles
					: 0;

				AccumulateTime(Busy, 0, 0);
				LastGPUCycles = FMath::Max(LastGPUCycles, Event.GPUTimestampTOP);

				FRHIBreadcrumbData_Stats const& Stat = Event.Breadcrumb->Data;
				if (Stat.ShouldComputeStat())
				{
					// Disregard the stat if it is nested within itself (i.e. its already in the ActiveStats map with a non-zero ref count).
					// Only the outermost stat will count the busy time, otherwise we'd be double-counting the nested time.
					int32 RefCount = ActiveStats.FindOrAdd(Stat)++;
					if (RefCount == 0)
					{
						Timestamps.Stats.FindOrAdd(Stat);
					}

					ActiveStatsStack.Add(Stat);
				}

				Breadcrumb = Event.Breadcrumb;
				Breadcrumb->TraceBeginGPU(Queue.Value, Event.GPUTimestampTOP);

			#if WITH_PROFILEGPU
				if (Profile.bProfileFrame)
				{
					FRHIBreadcrumb::FBuffer Buffer;
					const TCHAR* Name = Event.Breadcrumb->GetTCHAR(Buffer);

					// Push a new node
					Profile.PushNode(Name);
				}
			#endif
			}

			void EndBreadcrumb(FEvent::FEndBreadcrumb const& Event)
			{
				uint64 Busy = Event.GPUTimestampBOP > LastGPUCycles
					? Event.GPUTimestampBOP - LastGPUCycles
					: 0;

				AccumulateTime(Busy, 0, 0);
				LastGPUCycles = FMath::Max(LastGPUCycles, Event.GPUTimestampBOP);

				FRHIBreadcrumbData_Stats const& Stat = Event.Breadcrumb->Data;
				if (Stat.ShouldComputeStat())
				{
					// Pop the stat when the refcount hits zero.
					int32 RefCount = --ActiveStats.FindChecked(Stat);
					if (RefCount == 0)
					{
						ActiveStats.FindAndRemoveChecked(Stat);
					}

					check(ActiveStatsStack.Last() == Stat);
					ActiveStatsStack.RemoveAt(ActiveStatsStack.Num() - 1, EAllowShrinking::No);
				}
				
				Breadcrumb->TraceEndGPU(Queue.Value, Event.GPUTimestampBOP);

				Breadcrumb = Event.Breadcrumb->GetParent();


			#if WITH_PROFILEGPU
				if (Profile.bProfileFrame)
				{
					Profile.PopNode();
				}
			#endif
			}
		#endif

			void Stats(FEvent::FStats const& Event)
			{
			#if WITH_PROFILEGPU
				if (Profile.Current)
				{
					Profile.Current->Exclusive += Event;

					for (FNode* Node = Profile.Current; Node; Node = Node->Parent)
					{
						Node->Inclusive += Event;
					}
				}
			#endif
				FGpuProfilerTrace::Stats(Queue.Value, Event.NumDraws, Event.NumPrimitives);
			}

			void Wait(FResolvedWait const& ResolvedWait, const FEvent::FWaitFence& WaitFence)
			{
				// Time the queue was idle between the last EndWork event, and the Wait command being submitted to the GPU driver.
				uint64 Idle = ResolvedWait.CPUTimestamp > LastGPUCycles
					? ResolvedWait.CPUTimestamp - LastGPUCycles
					: 0;

				uint64 WaitStart = FMath::Max(ResolvedWait.CPUTimestamp, LastGPUCycles);

				FGpuProfilerTrace::WaitFence(Queue.Value, ResolvedWait.GPUTimestampTOP, WaitFence.Queue.Value, WaitFence.Value);

				// Time the queue spent waiting for the fence to signal on another queue.
				uint64 Wait = 0;
				if (ResolvedWait.GPUTimestampTOP > WaitStart)
				{
					Wait = ResolvedWait.GPUTimestampTOP - WaitStart;
					FGpuProfilerTrace::TraceWait(Queue.Value, WaitStart, ResolvedWait.GPUTimestampTOP);
				}

				// Bring the last GPU busy end time forwards to where the wait is resolved.
				LastGPUCycles = ResolvedWait.GPUTimestampTOP;

				AccumulateTime(0, Wait, Idle);
			}

			void TrimSignals(uint64 CPUTimestamp)
			{
				// Remove all signals that occured on the GPU timeline before this frame boundary on the CPU.
				int32 Index = Algo::LowerBoundBy(Signals, CPUTimestamp, [](FResolvedSignal const& Signal) { return Signal.GPUTimestampBOP; });
				if (Index >= 0)
				{
					Signals.RemoveAt(0, Index, EAllowShrinking::No);
				}
			}

			void FrameTime(uint64 TotalGPUTime)
			{
				Timestamps.TotalBusyCycles = TotalGPUTime;
			}

			void FrameBoundary(FEvent::FFrameBoundary const& Event, FFrameState& FrameState, uint32 FrameNumber)
			{
				check(!bBusy);
				Timestamps.CPUFrameBoundary = Event.CPUTimestamp;

				FGpuProfilerTrace::FrameBoundary(Queue.Value, Event.FrameNumber);

			#if WITH_PROFILEGPU
				if (Profile.bProfileFrame)
				{
					Profile.LogTree(*this, Event.FrameNumber);
					Profile = {};
				}
			#endif

				FrameState.Emplace(Queue, MoveTemp(Timestamps));

			#if WITH_RHI_BREADCRUMBS
				// Reinsert timestamp streams for the current active stats on 
				// this queue, since these got moved into the frame state.
				for (auto& [Stat, RefCount] : ActiveStats)
				{
					Timestamps.Stats.FindOrAdd(Stat);
				}
			#endif

			#if WITH_PROFILEGPU
				if (FrameNumber == Event.FrameNumber + 1)
				{
					Profile.bProfileFrame = true;

					// Build the node tree 
					Profile.PushNode(TEXT("<root>"));

				#if WITH_RHI_BREADCRUMBS
					auto Recurse = [&](auto& Recurse, FRHIBreadcrumbNode* Current) -> void
					{
						if (!Current)
						{
							return;
						}

						Recurse(Recurse, Current->GetParent());

						FRHIBreadcrumb::FBuffer Buffer;
						Profile.PushNode(Current->GetTCHAR(Buffer));
					};
					Recurse(Recurse, Event.Breadcrumb);
				#endif // WITH_RHI_BREADCRUMBS
				}
			#endif
			}
		};

		std::atomic<bool> bTriggerProfile{ false };
		uint32 ProfileFrameNumber = 0;
		uint32 MaxFrameNumber = 0;

		TMap<uint32, FFrameState> Frames;
		TMap<FQueue, TUniquePtr<FQueueState>> QueueStates;

		// Attempts to retrieve the CPU and GPU timestamps of when a fence wait is resolved by a signal on another queue.
		TOptional<FResolvedWait> ResolveWait(FQueueState& LocalQueue, FEvent::FWaitFence const& WaitFenceEvent)
		{
			FQueueState const& RemoteQueue = static_cast<FQueueState const&>(*QueueStates.FindChecked(WaitFenceEvent.Queue));

			if (RemoteQueue.MaxSignal.Value < WaitFenceEvent.Value)
			{
				// Fence has not yet been signalled on the remote queue
				return {};
			}
			else
			{
				// Fence has been signalled, but it may be in the future.

				FResolvedWait Result;
				Result.CPUTimestamp = WaitFenceEvent.CPUTimestamp;

				//
				// The wait cannot be resolved any earlier than:
				//
				//		1) The wait command was issued to the driver (WaitFenceEvent.CPUTimestamp)
				//		2) The GPU completed prior work on this queue (LocalQueue.LastGPUCycles)
				//
				Result.GPUTimestampTOP = FMath::Max(WaitFenceEvent.CPUTimestamp, LocalQueue.LastGPUCycles);
				//
				//      3) The wait maybe be further delayed by the remote queue the GPU is awaiting.
				//
				int32 Index = Algo::LowerBoundBy(RemoteQueue.Signals, WaitFenceEvent.Value, [](FResolvedSignal const& Signal) { return Signal.Value; });
				if (RemoteQueue.Signals.IsValidIndex(Index))
				{
					FResolvedSignal const& Signal = RemoteQueue.Signals[Index];

					//
					// Only consider this signal's timestamp if the fence was not already signalled at the previous frame boundary.
					// See comment in ResolveSignal() for details.
					//
					if (!(Index == 0 && WaitFenceEvent.Value < Signal.Value))
					{
						Result.GPUTimestampTOP = FMath::Max(Result.GPUTimestampTOP, Signal.GPUTimestampBOP);
					}
				}

				return Result;
			}
		}

		void InitializeQueues(TConstArrayView<FQueue> Queues) override
		{
			FGpuProfilerTrace::Initialize();

			for (FQueue Queue : Queues)
			{
				TUniquePtr<FQueueState>& Ptr = QueueStates.FindOrAdd(Queue);
				if (!Ptr.IsValid())
				{
					Ptr = MakeUnique<FQueueState>(Queue);
				}
			}
		}

		bool ProcessQueue(FQueueState& QueueState, FIterator& Iterator)
		{
			if (FGpuProfilerTrace::IsAvailable() && !QueueState.bWasTraced)
			{
				FGpuProfilerTrace::InitializeQueue(QueueState.Queue.Value, QueueState.Queue.GetTypeString());
				QueueState.bWasTraced = true;
			}

			while (FEvent const* Event = Iterator.Peek())
			{
				switch (Event->GetType())
				{
				case FEvent::EType::BeginWork:
					{
						check(!QueueState.bBusy);
						QueueState.bBusy = true;
						QueueState.BeginWork(Event->Value.Get<FEvent::FBeginWork>());
					}
					break;

				case FEvent::EType::EndWork:
					{
						check(QueueState.bBusy);
						QueueState.bBusy = false;
						QueueState.EndWork(Event->Value.Get<FEvent::FEndWork>());
					}
					break;

			#if WITH_RHI_BREADCRUMBS
				case FEvent::EType::BeginBreadcrumb:
					{
						check(QueueState.bBusy);
						QueueState.BeginBreadcrumb(Event->Value.Get<FEvent::FBeginBreadcrumb>());
					}
					break;

				case FEvent::EType::EndBreadcrumb:
					{
						check(QueueState.bBusy);
						QueueState.EndBreadcrumb(Event->Value.Get<FEvent::FEndBreadcrumb>());
					}
					break;
			#endif // WITH_RHI_BREADCRUMBS

			#if WITH_PROFILEGPU
				case FEvent::EType::Stats:
					{
						check(QueueState.bBusy);
						QueueState.Stats(Event->Value.Get<FEvent::FStats>());
					}
					break;
			#endif // WITH_PROFILEGPU

				case FEvent::EType::SignalFence:
					{
						check(!QueueState.bBusy);
						QueueState.ResolveSignal(Event->Value.Get<FEvent::FSignalFence>());
					}
					break;

				case FEvent::EType::WaitFence:
					{
						check(!QueueState.bBusy);
						const FEvent::FWaitFence& WaitFence= Event->Value.Get<FEvent::FWaitFence>();
						TOptional<FResolvedWait> ResolvedWait = ResolveWait(QueueState, Event->Value.Get<FEvent::FWaitFence>());

						if (!ResolvedWait.IsSet())
						{
							// Unresolved fence, pause processing
							return false;
						}

						QueueState.Wait(*ResolvedWait, WaitFence);
					}
					break;

				case FEvent::EType::FrameTime:
					{
						const FEvent::FFrameTime& FrameTime = Event->Value.Get<FEvent::FFrameTime>();
						QueueState.FrameTime(FrameTime.TotalGPUTime);
					}
					break;

				case FEvent::EType::FrameBoundary:
					{
						FEvent::FFrameBoundary const& FrameBoundary = Event->Value.Get<FEvent::FFrameBoundary>();
						FFrameState& FrameState = Frames.FindOrAdd(FrameBoundary.FrameNumber);

					#if STATS
						FrameState.StatsFrame = FrameBoundary.bStatsFrameSet
							? FrameBoundary.StatsFrame
							: TOptional<int64>();
					#endif

					#if WITH_PROFILEGPU
						// Latch the index of the next frame to profile
						MaxFrameNumber = FMath::Max(FrameBoundary.FrameNumber, MaxFrameNumber);
						if (bTriggerProfile.exchange(false))
						{
							ProfileFrameNumber = MaxFrameNumber + 1;
						}
					#endif // WITH_PROFILEGPU

						QueueState.FrameBoundary(FrameBoundary, FrameState, ProfileFrameNumber);

						if (FrameState.Num() == QueueStates.Num())
						{
							// Trim the Signals array in each queue, up to the lowest frame boundary CPU timestamp.
							{
								uint64 MinFrameBoundary = TNumericLimits<uint64>::Max();
								for (auto& [Queue, QueueTimestamps] : FrameState)
								{
									MinFrameBoundary = FMath::Min(MinFrameBoundary, QueueTimestamps.CPUFrameBoundary);
								}

								for (auto& [Queue, LocalQueueState] : QueueStates)
								{
									LocalQueueState.Get()->TrimSignals(MinFrameBoundary);
								}
							}

							// All registered queues have reported their frame boundary event.
							// We have a full set of data to compute the total frame GPU stats.
							ProcessFrame(FrameState);

							Frames.Remove(FrameBoundary.FrameNumber);
						}
					}
					break;
				}

				Iterator.Pop();
			}

			return true;
		}

		void ProcessFrame(FFrameState& FrameState)
		{
		#if STATS
			FEndOfPipeStats* Stats = FEndOfPipeStats::Get();
			if (FrameState.StatsFrame.IsSet())
			{
				Stats->AddMessage(FStatConstants::AdvanceFrame.GetEncodedName(), EStatOperation::AdvanceFrameEventEndOfPipe, *FrameState.StatsFrame);
			}
		#endif

		#if CSV_PROFILER_STATS
			const bool bCsvStatsEnabled = !!CVarGPUCsvStatsEnabled.GetValueOnAnyThread();
			FCsvProfiler* CsvProfiler = FCsvProfiler::Get();
			CsvProfiler->BeginFrameEOP();
		#else
			const bool bCsvStatsEnabled = false;
		#endif

			TOptional<uint64> MaxQueueBusyCycles;

			for (auto const& [Queue, QueueTimestamps] : FrameState)
			{
			#if WITH_RHI_BREADCRUMBS && HAS_GPU_STATS
				// Compute the individual GPU stats
				for (auto const& [Stat, StatState] : QueueTimestamps.Stats)
				{
					StatState.EmitResults(Queue, *Stat.GPUStat
					#if STATS
						, Stats
					#endif
					#if CSV_PROFILER_STATS
						, bCsvStatsEnabled ? CsvProfiler : nullptr
					#endif
					);
				}
			#endif // WITH_RHI_BREADCRUMBS && HAS_GPU_STATS

				// Set the whole-frame per queue stat
			#if HAS_GPU_STATS
				QueueTimestamps.WholeQueueStat.EmitResults(Queue, GPUStat_Total
				#if STATS
					, Stats
				#endif
				#if CSV_PROFILER_STATS
					, bCsvStatsEnabled ? CsvProfiler : nullptr
				#endif
				);
			#endif

				if (QueueTimestamps.TotalBusyCycles.IsSet())
				{
					uint64 CurrentMax = MaxQueueBusyCycles ? *MaxQueueBusyCycles : 0;
					MaxQueueBusyCycles = FMath::Max(CurrentMax, *QueueTimestamps.TotalBusyCycles);
				}
			}

			if (MaxQueueBusyCycles.IsSet())
			{
				// Set the total GPU time stat according to the value directly provided by the platform RHI
				GRHIGPUFrameTimeHistory.PushFrameCycles(1.0 / FPlatformTime::GetSecondsPerCycle64(), *MaxQueueBusyCycles);
			}
			else
			{
				// Compute the whole-frame total GPU time.
				TArray<FTimestampStream::FState, TInlineAllocator<GetRHIPipelineCount() * MAX_NUM_GPUS>> StreamPointers;
				for (auto const& [Queue, State] : FrameState)
				{
					StreamPointers.Emplace(State.Queue);
				}
				uint64 WholeFrameUnion = FTimestampStream::ComputeUnion(StreamPointers);

				// Update the global GPU frame time stats
				GRHIGPUFrameTimeHistory.PushFrameCycles(1.0 / FPlatformTime::GetSecondsPerCycle64(), WholeFrameUnion);
			}

			// @todo set global csv GPU time
			//RHISetGPUStatTotals(bCsvStatsEnabled, FPlatformTime::ToMilliseconds64(WholeFrameUnion));

		#if STATS
			Stats->Flush();
		#endif
		}

		void ProcessAllQueues()
		{
			// Process the queue as far as possible
			bool bProgress;
			do
			{
				bProgress = false;
				for (auto& [Queue, QueueState] : QueueStates)
				{
					while (FIterator* Iterator = QueueState->PendingStreams.Peek())
					{
						FEvent const* Start = Iterator->Peek();

						bool bPaused = !ProcessQueue(*QueueState.Get(), *Iterator);

						FEvent const* End = Iterator->Peek();
						bProgress |= End != Start;

						if (bPaused)
						{
							// The queue was paused by a Wait event
							check(End);
							break;
						}

						if (!End)
						{
							// This stream has been fully processed.
							QueueState->PendingStreams.Dequeue();
						}
					}
				}
			} while (bProgress);
		}

		void ProcessStreams(TConstArrayView<TSharedRef<FEventStream>> EventStreams) override
		{
			for (TSharedRef<FEventStream> const& Stream : EventStreams)
			{
				FQueueState& QueueState = *QueueStates.FindChecked(Stream->Queue);
				QueueState.PendingStreams.Enqueue(FIterator(Stream));
			}

			ProcessAllQueues();
		}
	} GGPUProfilerSink_StatSystem;

#if WITH_PROFILEGPU
	static FAutoConsoleCommand GCommand_ProfileGPU(
		TEXT("ProfileGPU"),
		TEXT("Captures statistics about a frame of GPU work and prints the results to the log."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			GGPUProfilerSink_StatSystem.bTriggerProfile = true;
		}));
#endif // WITH_PROFILEGPU
}

#endif // RHI_NEW_GPU_PROFILER

RHI_API FRHIGPUFrameTimeHistory GRHIGPUFrameTimeHistory;

FRHIGPUFrameTimeHistory::EResult FRHIGPUFrameTimeHistory::FState::PopFrameCycles(uint64& OutCycles64)
{
	return GRHIGPUFrameTimeHistory.PopFrameCycles(*this, OutCycles64);
}

FRHIGPUFrameTimeHistory::EResult FRHIGPUFrameTimeHistory::PopFrameCycles(FState& State, uint64& OutCycles64)
{
	FScopeLock Lock(&CS);

	if (State.NextIndex == NextIndex)
	{
		OutCycles64 = 0;
		return EResult::Empty;
	}
	else
	{
		uint64 MinHistoryIndex = NextIndex >= MaxLength ? NextIndex - MaxLength : 0;

		if (State.NextIndex < MinHistoryIndex)
		{
			State.NextIndex = MinHistoryIndex;
			OutCycles64 = History[State.NextIndex++ % MaxLength];
			return EResult::Disjoint;
		}
		else
		{
			OutCycles64 = History[State.NextIndex++ % MaxLength];
			return EResult::Ok;
		}
	}
}

void FRHIGPUFrameTimeHistory::PushFrameCycles(double GPUFrequency, uint64 GPUCycles)
{
	double Seconds = double(GPUCycles) / GPUFrequency;
	double Cycles32 = Seconds / FPlatformTime::GetSecondsPerCycle();
	double Cycles64 = Seconds / FPlatformTime::GetSecondsPerCycle64();

	{
		FScopeLock Lock(&CS);
		History[NextIndex++ % MaxLength] = uint64(Cycles64);
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPlatformAtomics::InterlockedExchange(reinterpret_cast<volatile int32*>(&GGPUFrameTime), int32(Cycles32));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

RHI_API uint32 RHIGetGPUFrameCycles(uint32 GPUIndex)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return (uint32)FPlatformAtomics::AtomicRead(reinterpret_cast<volatile int32*>(&GGPUFrameTime));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#undef LOCTEXT_NAMESPACE
