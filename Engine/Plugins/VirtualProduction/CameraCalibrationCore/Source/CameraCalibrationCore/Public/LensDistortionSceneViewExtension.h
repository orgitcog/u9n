// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

#include "LensFileRendering.h"

class UCameraComponent;

/**
 * View extension drawing distortion/undistortion displacement maps
 */
class FLensDistortionSceneViewExtension : public FSceneViewExtensionBase
{
public:
	/** The distortion model to use when creating distortion maps */
	enum class EDistortionModel : uint8
	{
		None,
		
		/** Distortion maps generated from distortion parameters with a spherical distortion equation */
		SphericalDistortion,

		/** Distortion maps generated from distortion parameters with an anamorphic distortion equation */
		AnamorphicDistortion,

		/** Distortion maps generated from UV-based ST maps */
		STMap,
		MAX
	};
	
public:

	FLensDistortionSceneViewExtension(const FAutoRegister& AutoRegister);

	//~ Begin ISceneViewExtension interface	
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	//~End ISceneVIewExtension interface

public:
	/** Update the distortion state and blending params for the input camera */
	void UpdateDistortionState_AnyThread(ACameraActor* CameraActor, FDisplacementMapBlendingParams DistortionState, ULensDistortionModelHandlerBase* LensDistortionHandler = nullptr);

	/** Remove the distortion state and blending params for the input camera */
	void ClearDistortionState_AnyThread(ACameraActor* CameraActor);

private:
	/** Use the input distortion state to draw a distortion displacement map */
	void DrawDisplacementMap_RenderThread(FRDGBuilder& GraphBuilder, const FLensDistortionState& CurrentState, EDistortionModel DistortionModel, float InverseOverscan, float CameraOverscan, const FVector2D& SensorSize, FRDGTextureRef& OutDistortionMapWithOverscan);

	/** Use the input blend parameters to draw multiple displacement maps and blend them together into a final distortion displacement map */
	void BlendDisplacementMaps_RenderThread(FRDGBuilder& GraphBuilder, const FDisplacementMapBlendingParams& BlendState, EDistortionModel DistortionModel, float InverseOverscan, float CameraOverscan, const FVector2D& SensorSize, FRDGTextureRef& OutDistortionMapWithOverscan);

	/** Crop the input overscanned distortion map to the original requested resolution */
	void CropDisplacementMap_RenderThread(FRDGBuilder& GraphBuilder, const FRDGTextureRef& InDistortionMapWithOverscan, FRDGTextureRef& OutDistortionMap);

	/** Invert the input distortion map to generate a matching undistortion map (with no overscan) */
	void InvertDistortionMap_RenderThread(FRDGBuilder& GraphBuilder, const FRDGTextureRef& InDistortionMap, float InInverseOverscan, FRDGTextureRef& OutUndistortionMap);

	/** Performs a warp grid inverse of the specified distortion map to fill up its inverse to the specified overscan amount */
	void FillSTDisplacementMap_RenderThread(FRDGBuilder& GraphBuilder, const FRDGTextureRef& InUndisplacementMap, float InOverscan, FRDGTextureRef& OutFilledDisplacementMap);

	/** Recenters the specified distortion map within an overscanned map, filling the overscanned region with zeros */
	void RecenterSTDisplacementMap_RenderThread(FRDGBuilder& GraphBuilder, const FRDGTextureRef& InDisplacementMap, float InOverscan, FRDGTextureRef& OutRecenteredDisplacementMap);
	
	/** Gets whether the specified distortion model is forward distorting (generating the distort map first, then inverting for the undistort) or inverse distorting */
	bool IsDistortionModelForwardDistorting(EDistortionModel InDistortionModel) const;

	/** Computes the overscan needed to invert the distortion map */
	float GetInverseOverscan(ULensDistortionModelHandlerBase* LensDistortionHandler, EDistortionModel InDistortionModel) const;
	
private:

	struct FCameraDistortionProxy
	{
		FDisplacementMapBlendingParams Params = {};
		float CameraOverscan = 1.0f;
		FCameraFilmbackSettings FilmbackSettings = {};
		TWeakObjectPtr<ULensDistortionModelHandlerBase> LensDistortionHandler = nullptr;
	};
	/** Map of cameras to their associated distortion state and blending parameters, used to determine if and how displacement maps should be rendered for a specific view */
	TMap<uint32, FCameraDistortionProxy> DistortionStateMap;

	/** Critical section to lock access to the distortion state map when potentially being accessed from multiple threads */
	mutable FCriticalSection DistortionStateMapCriticalSection;
};
