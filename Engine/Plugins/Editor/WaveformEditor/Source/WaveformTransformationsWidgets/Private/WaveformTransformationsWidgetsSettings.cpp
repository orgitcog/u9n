// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationsWidgetsSettings.h"

UWaveformTransformationsWidgetsSettings::UWaveformTransformationsWidgetsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MarkerColor(WaveformTransformationWidgetSharedDefaults::DefaultMarkerColor)
	, LoopColors(WaveformTransformationWidgetSharedDefaults::DefaultLoopColors)
	, LabelTextColor(WaveformTransformationWidgetSharedDefaults::DefaultLabelTextColor)
	, LabelFontSize(WaveformTransformationWidgetSharedDefaults::DefaultLabelFontSize)
{
}

FName UWaveformTransformationsWidgetsSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UWaveformTransformationsWidgetsSettings::GetSectionName() const
{
	return TEXT("Waveform Transformations Display");
}

#if WITH_EDITOR
FText UWaveformTransformationsWidgetsSettings::GetSectionText() const
{
	return NSLOCTEXT("WaveformTransformationsDisplay", "WaveformTransformationsDisplaySettingsSection", "Waveform Transformations Display");
}

void UWaveformTransformationsWidgetsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != nullptr)
	{
		SettingsChangedDelegate.Broadcast(PropertyChangedEvent.GetPropertyName(), this);
	}
}

FOnWaveformTransformationsWidgetsSettingsChanged& UWaveformTransformationsWidgetsSettings::OnSettingChanged()
{
	return SettingsChangedDelegate;
}

FOnWaveformTransformationsWidgetsSettingsChanged UWaveformTransformationsWidgetsSettings::SettingsChangedDelegate;
#endif// WITH_EDITOR