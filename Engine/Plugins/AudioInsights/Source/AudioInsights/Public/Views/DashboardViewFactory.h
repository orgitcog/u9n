// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceProviderBase.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"
#include "Widgets/SWidget.h"

class FSpawnTabArgs;
class SDockTab;

namespace UE::Audio::Insights
{
	enum class EDefaultDashboardTabStack : uint8
	{
		Viewport,
		Log,
		Analysis,
		AudioMeters,
		AudioAnalyzerRack,
	};

	class IDashboardViewFactory
	{
	public:
		virtual ~IDashboardViewFactory() = default;

		virtual EDefaultDashboardTabStack GetDefaultTabStack() const = 0;
		virtual FText GetDisplayName() const = 0;
		virtual FName GetName() const = 0;
		virtual FSlateIcon GetIcon() const = 0;
		virtual TSharedRef<SWidget> MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs) = 0;
	};

	class FTraceDashboardViewFactoryBase : public IDashboardViewFactory
	{
	public:
		const TArray<TSharedPtr<FTraceProviderBase>>& GetProviders() const
		{
			return Providers;
		}

		template <typename ProviderType>
		TSharedPtr<ProviderType> FindProvider(bool bEnsureIfMissing = true) const
		{
			for (const TSharedPtr<FTraceProviderBase>& Provider : Providers)
			{
				if (Provider->GetName() == ProviderType::GetName_Static())
				{
					return StaticCastSharedPtr<ProviderType>(Provider);
				}
			}

			if (bEnsureIfMissing)
			{
				ensureMsgf(false, TEXT("Failed to find associated provider '%s'"), *ProviderType::GetName_Static().ToString());
			}

			return TSharedPtr<ProviderType>();
		}

	protected:
		TArray<TSharedPtr<FTraceProviderBase>> Providers;
	};
} // namespace UE::Audio::Insights
