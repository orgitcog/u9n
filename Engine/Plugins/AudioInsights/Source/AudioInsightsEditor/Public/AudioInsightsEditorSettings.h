// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSpectrogram.h"
#include "AudioSpectrumAnalyzer.h"
#include "Engine/DeveloperSettings.h"

#include "AudioInsightsEditorSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings)
class AUDIOINSIGHTSEDITOR_API UAudioInsightsEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;

	/** Whether to automatically set the first PIE client in Audio Insights World Filter. */
	UPROPERTY(Config, EditAnywhere, Category="World Filter")
	bool bWorldFilterDefaultsToFirstClient = false;

	/** Settings for analyzer rack spectrogram widget */
	UPROPERTY(EditAnywhere, config, Category = Spectrogram, meta = (ShowOnlyInnerProperties))
	FSpectrogramRackUnitSettings SpectrogramSettings;

	/** Settings for analyzer rack spectrum analyzer widget */
	UPROPERTY(EditAnywhere, config, Category = SpectrumAnalyzer, meta = (ShowOnlyInnerProperties))
	FSpectrumAnalyzerRackUnitSettings SpectrumAnalyzerSettings;
};
