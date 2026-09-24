// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Templates/SharedPointer.h"

namespace UE::Audio::Insights
{
	class AUDIOINSIGHTS_API IDashboardDataViewEntry : public TSharedFromThis<IDashboardDataViewEntry>
	{
	public:
		virtual ~IDashboardDataViewEntry() = default;

		virtual bool IsValid() const = 0;
	};

	class AUDIOINSIGHTS_API IDashboardDataTreeViewEntry : public TSharedFromThis<IDashboardDataTreeViewEntry>
	{
	public:
		virtual ~IDashboardDataTreeViewEntry() = default;

		virtual bool IsValid() const = 0;
		virtual bool ShouldInitExpandChildren() const = 0;
		virtual void ResetShouldInitExpandChildren() = 0;

		TArray<TSharedPtr<IDashboardDataTreeViewEntry>> Children;
		bool bIsExpanded = false;
	};
} // namespace UE::Audio::Insights
