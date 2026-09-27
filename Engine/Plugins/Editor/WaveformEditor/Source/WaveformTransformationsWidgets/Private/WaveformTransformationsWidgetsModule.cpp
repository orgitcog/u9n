// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationsWidgetsModule.h"

#include "Modules/ModuleManager.h"
#include "WaveformTransformationRendererMapper.h"
#include "WaveformTransformationTrimFade.h"
#include "WaveformTransformationTrimFadeRenderer.h"
#include "WaveformTransformationMarkers.h"
#include "WaveformTransformationMarkersRenderer.h"

void FWaveformTransformationsWidgetsModule::StartupModule()
{
	FWaveformTransformationRendererMapper& RendererMapper = FWaveformTransformationRendererMapper::Get();
	RendererMapper.RegisterRenderer<FWaveformTransformationTrimFadeRenderer>(UWaveformTransformationTrimFade::StaticClass());
	RendererMapper.RegisterRenderer<FWaveformTransformationMarkerRenderer>(UWaveformTransformationMarkers::StaticClass()); 
}

void FWaveformTransformationsWidgetsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FWaveformTransformationsWidgetsModule, WaveformTransformationsWidgets)