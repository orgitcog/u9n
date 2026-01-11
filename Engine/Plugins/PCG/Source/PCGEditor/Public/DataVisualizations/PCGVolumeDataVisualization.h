// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialDataVisualization.h"

class UPCGData;
struct FPCGContext;

/** Overrides collapse behavior to target a given number of points generated, to avoid too coarse debug (points way bigger than data) or enormous amount of points (points way smaller than data). */
class PCGEDITOR_API FPCGPrimitiveDataVisualization : public IPCGSpatialDataVisualization
{
public:
	// ~Begin IPCGSpatialDataVisualization interface
	virtual const UPCGBasePointData* CollapseToDebugBasePointData(FPCGContext* Context, const UPCGData* Data) const override;
	FPCGSetupSceneFunc GetViewportSetupFunc(const UPCGData* Data) const override;
	// ~End IPCGSpatialDataVisualization interface

protected:
	virtual void GetComponentsAndTransforms(const UPCGData* InData, TArray<TObjectPtr<UPrimitiveComponent>>& OutComponents, TArray<FTransform>& OutComponentTransforms) const;
};

/** Specialization for volumes that don't always rely on their primitive component */
class FPCGVolumeDataVisualization : public FPCGPrimitiveDataVisualization
{
protected:
	virtual void GetComponentsAndTransforms(const UPCGData* InData, TArray<TObjectPtr<UPrimitiveComponent>>& OutComponents, TArray<FTransform>& OutComponentTransforms) const override;
};

/** Specialization for simple collision shapes */
class FPCGCollisionShapeDataVisualization : public FPCGPrimitiveDataVisualization
{
protected:
	virtual void GetComponentsAndTransforms(const UPCGData* InData, TArray<TObjectPtr<UPrimitiveComponent>>& OutComponents, TArray<FTransform>& OutComponentTransforms) const override;
};

/** Specialization for collision wrappers */
class FPCGCollisionWrapperDataVisualization : public FPCGPrimitiveDataVisualization
{
protected:
	virtual void GetComponentsAndTransforms(const UPCGData* InData, TArray<TObjectPtr<UPrimitiveComponent>>& OutComponents, TArray<FTransform>& OutComponentTransforms) const override;
};