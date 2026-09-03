// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialDataVisualization.h"
#include "PCGEditorModule.h"

class UPCGData;

class PCGEDITOR_API FPCGBaseTextureDataVisualization : public IPCGSpatialDataVisualization
{
public:
	// ~Begin IPCGDataVisualization interface
	virtual TArray<TSharedPtr<FStreamableHandle>> LoadRequiredResources(const UPCGData* Data) const override;
	virtual FPCGSetupSceneFunc GetViewportSetupFunc(const UPCGData* Data) const override;
	// ~End IPCGDataVisualization interface
};
