// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UPCGComputeKernel;
class UPCGCopyPointsSettings;
struct FPCGComputeGraphContext;
struct FPCGContext;

namespace PCGCopyPointsKernel
{
	/** Performs data validation common to all copy points kernels. */
	bool IsKernelDataValid(const UPCGComputeKernel* InKernel, const UPCGCopyPointsSettings* InCopyPointSettings, const FPCGComputeGraphContext* InContext);
}
