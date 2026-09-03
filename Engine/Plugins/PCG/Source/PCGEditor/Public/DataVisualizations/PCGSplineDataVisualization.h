// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialDataVisualization.h"
#include "PCGEditorModule.h"

class UPCGData;
struct FPCGContext;

class PCGEDITOR_API IPCGSplineDataVisualization : public IPCGSpatialDataVisualization
{
public:
	// ~Begin IPCGDataVisualization interface
	virtual FPCGTableVisualizerInfo GetTableVisualizerInfoWithDomain(const UPCGData* Data, const FPCGMetadataDomainID& DomainID) const override;
	virtual FPCGSetupSceneFunc GetViewportSetupFunc(const UPCGData* Data) const override;
	// ~End IPCGDataVisualization interface

	// ~Begin IPCGSpatialDataVisualization interface
	/** Overrides collapse behavior to show the spline control points. */
	virtual const UPCGBasePointData* CollapseToDebugBasePointData(FPCGContext* Context, const UPCGData* Data) const override;
	// ~End IPCGSpatialDataVisualization interface
};
