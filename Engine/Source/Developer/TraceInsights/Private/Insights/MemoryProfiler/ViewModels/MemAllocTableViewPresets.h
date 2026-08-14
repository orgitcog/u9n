// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Templates/SharedPointer.h"

namespace UE::Insights { class ITableTreeViewPreset; }

namespace UE::Insights::MemoryProfiler
{

class SMemAllocTableTreeView;

class FMemAllocTableViewPresets
{
public:
	static TSharedRef<ITableTreeViewPreset> CreateDefaultViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateDetailedViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateHeapViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateSizeViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateTagViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateAssetViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateClassNameViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateCallstackViewPreset(SMemAllocTableTreeView& TableTreeView, bool bIsInverted, bool bIsAlloc);
	static TSharedRef<ITableTreeViewPreset> CreatePlatformPageViewPreset(SMemAllocTableTreeView& TableTreeView);
	static TSharedRef<ITableTreeViewPreset> CreateSwapViewPreset(SMemAllocTableTreeView& TableTreeView);
};

} // namespace UE::Insights::MemoryProfiler
