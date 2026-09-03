// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIFwd.h"

class FRDGBuilder;

namespace UE::Color::ACES
{
	/*
	* Get the ACES 2.0 transform table resources.
	* 
	* @param GraphBuilder			Frame render dependency graph.
	* @param InPeakLuminance		Peak luminance value in nits (cd/m^2).
	* @param OutReachMTable			Reach M value table resource output.
	* @param OutGamutCuspTable		Gamut cusp table resource output.
	* @param OutUpperHullGammaTable	Upper hull gamma table resource output.
	*/
	void GetTransformResources(
		FRDGBuilder& GraphBuilder,
		float InPeakLuminance,
		FRHIShaderResourceView*& OutReachMTable,
		FRHIShaderResourceView*& OutGamutCuspTable,
		FRHIShaderResourceView*& OutUpperHullGammaTable
	);
} // namespace UE::Color::ACES
