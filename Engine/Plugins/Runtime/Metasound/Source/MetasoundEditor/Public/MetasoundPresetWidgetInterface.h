// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Audio/AudioPanelWidgetInterface.h"
#include "MetasoundBuilderSubsystem.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundPresetWidgetInterface.generated.h"

UINTERFACE(Blueprintable, MinimalAPI)
class UMetaSoundPresetWidgetInterface : public UAudioPanelWidgetInterface
{
	GENERATED_BODY()
};

class IMetaSoundPresetWidgetInterface : public IAudioPanelWidgetInterface
{
	GENERATED_BODY()

public:
	// The MetaSounds whose presets are supported by this widget. If Support All Presets is true, this widget is supported by all presets except those in the Excluded array.
	// MetaSounds in the include/exclude arrays can be MetaSound presets or non presets. 
	// If a MetaSound is not a preset, then presets of that MetaSound will be supported/excluded by this widget.
	UFUNCTION(BlueprintImplementableEvent)
	void GetSupportedMetaSounds(bool& bSupportAllPresets, TArray<TScriptInterface<IMetaSoundDocumentInterface>>& ExcludedMetaSounds, TArray<TScriptInterface<IMetaSoundDocumentInterface>>& IncludedMetaSounds) const;

	// Called when the preset widget is constructed, giving the builder of the associated MetaSound preset
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On MetaSoundPreset Widget Constructed"))
	void OnConstructed(UMetaSoundBuilderBase* Builder);
};
