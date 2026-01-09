// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "Views/DashboardViewFactory.h"

class USoundSubmix;

namespace UE::Audio::Insights
{
	class FSubmixAudioAnalyzerRack;

	class FAudioAnalyzerRackDashboardViewFactory : public IDashboardViewFactory, public TSharedFromThis<FAudioAnalyzerRackDashboardViewFactory>
	{
	public:
		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual TSharedRef<SWidget> MakeWidget(TSharedRef<SDockTab> OwnerTab, const FSpawnTabArgs& SpawnTabArgs) override;

	private:
		void HandleOnActiveAudioDeviceChanged();
		void HandleOnSubmixSelectionChanged(const TWeakObjectPtr<USoundSubmix> InSoundSubmix);

		TSharedPtr<FSubmixAudioAnalyzerRack> SubmixAudioAnalyzerRack;
		TObjectPtr<USoundSubmix> MainSubmix;
	};
} // namespace UE::Audio::Insights
