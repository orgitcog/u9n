// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define UE_API MODELINGCOMPONENTS_API

class USplineComponent;
class IToolsContextRenderAPI;

namespace UE {
namespace Geometry {

namespace SplineUtil {

	// Used for DrawSpline()
	struct FDrawSplineSettings
	{
		UE_API FDrawSplineSettings();

		// Defaults to FStyleColors::White
		FColor RegularColor;
		// Defaults to FStyleColors::AccentOrange
		FColor SelectedColor;
		// If non-positive, the spline is drawn just as points and curves. If positive, the
		// orientation and scale are visualized with the given base width.
		double ScaleVisualizationWidth = 0;

		// Keys to use SelectedColor for
		TSet<int32>* SelectedKeys = nullptr;
	};

	MODELINGCOMPONENTS_API void DrawSpline(const USplineComponent& SplineComp, IToolsContextRenderAPI& RenderAPI, const FDrawSplineSettings& Settings);

}
}
}

#undef UE_API
