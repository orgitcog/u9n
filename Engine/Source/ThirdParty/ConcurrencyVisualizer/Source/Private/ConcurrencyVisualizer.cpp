// Copyright Epic Games, Inc. All Rights Reserved.

#include "cvmarkers.h"
#include <vector>

#if WITH_CONCURRENCY_VISUALIZER

PCV_PROVIDER Provider = nullptr;
std::vector<PCV_MARKERSERIES> MarkerSeries;

// Modified during usage
static thread_local std::vector<PCV_SPAN> SpanStack;

uint32_t MaxDepth;

bool ConcurrencyVisualizerInitialize(uint32_t InMaxDepth)
{
	if (FAILED(CvInitProvider(&CvDefaultProviderGuid, &Provider)))
	{
		return false;
	}
		
	MaxDepth = InMaxDepth;
	MarkerSeries.resize(MaxDepth);
	for (uint32_t Depth = 0; Depth < MaxDepth; ++Depth)
	{
		wchar_t Name[64];
		swprintf_s(Name, 64, L"%02d", Depth);
		// We use 0 left padding so we get proper track sorting in Concurrency Viewer
		if (FAILED(CvCreateMarkerSeries(Provider, Name, &MarkerSeries[Depth])))
		{
			return false;
		}
	}

	return true;
}

void ConcurrencyVisualizerStartScopedEvent(const wchar_t* Text)
{
	SpanStack.emplace_back();
	memset(&SpanStack.back(), 0, sizeof(PCV_SPAN));
	if (MarkerSeries.size() > 0 && SpanStack.size() <= MaxDepth)
	{
		CvEnterSpanW(MarkerSeries[SpanStack.size() - 1], &SpanStack.back(), Text);
	}
}

void ConcurrencyVisualizerStartScopedEventA(const char* Text)
{
	SpanStack.emplace_back();
	memset(&SpanStack.back(), 0, sizeof(PCV_SPAN));
	if (MarkerSeries.size() > 0 && SpanStack.size() <= MaxDepth)
	{
		CvEnterSpanA(MarkerSeries[SpanStack.size() - 1], &SpanStack.back(), Text);
	}
}

void ConcurrencyVisualizerEndScopedEvent()
{
	if (SpanStack.size())
	{
		if (SpanStack.back())
		{
			CvLeaveSpan(SpanStack.back());
		}
		SpanStack.pop_back();
	}
}

#else

bool ConcurrencyVisualizerInitialize(uint32_t MaxDepth) { return false; }
void ConcurrencyVisualizerStartScopedEvent(const wchar_t* Text) {}
void ConcurrencyVisualizerStartScopedEventA(const char* Text) {}
void ConcurrencyVisualizerEndScopedEvent() {}

#endif