// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDAccelerationStructureVisualizationSettings.h"

#include "ChaosVDSettingsManager.h"
#include "Utils/ChaosVDUserInterfaceUtils.h"


void UChaosVDAccelerationStructureVisualizationSettings::SetDataVisualizationFlags(EChaosVDAccelerationStructureDataVisualizationFlags NewFlags)
{
	if (UChaosVDAccelerationStructureVisualizationSettings* Settings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDAccelerationStructureVisualizationSettings>())
	{
		Settings->AccelerationStructureDataVisualizationFlags = static_cast<uint32>(NewFlags);
		Settings->BroadcastSettingsChanged();
	}
}

EChaosVDAccelerationStructureDataVisualizationFlags UChaosVDAccelerationStructureVisualizationSettings::GetDataVisualizationFlags()
{
	if (UChaosVDAccelerationStructureVisualizationSettings* Settings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDAccelerationStructureVisualizationSettings>())
	{
		return static_cast<EChaosVDAccelerationStructureDataVisualizationFlags>(Settings->AccelerationStructureDataVisualizationFlags);
	}

	return EChaosVDAccelerationStructureDataVisualizationFlags::None;
}

bool UChaosVDAccelerationStructureVisualizationSettings::CanVisualizationFlagBeChangedByUI(uint32 Flag)
{
	return Chaos::VisualDebugger::Utils::ShouldVisFlagBeEnabledInUI(Flag, AccelerationStructureDataVisualizationFlags, EChaosVDAccelerationStructureDataVisualizationFlags::EnableDraw);
}
