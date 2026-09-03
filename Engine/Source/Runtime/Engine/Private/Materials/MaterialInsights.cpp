// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialInsights.h"

FMaterialInsights::FMaterialInsights()
{
}

void FMaterialInsights::Empty()
{
	ConnectionInsights.Empty();
	UniformParameterAllocationInsights.Empty();
	IRString.Empty();
	Legacy_ShaderStringParameters.Empty();
	LegacyHLSLCode.Empty();
	New_ShaderStringParameters.Empty();
	NewHLSLCode.Empty();
}

