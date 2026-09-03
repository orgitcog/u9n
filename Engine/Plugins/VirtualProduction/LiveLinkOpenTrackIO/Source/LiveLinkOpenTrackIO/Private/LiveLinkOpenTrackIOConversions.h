// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "LiveLinkLensTypes.h"
#include "LiveLinkOpenTrackIOTypes.h"


/** Conversion helper functions from OpenTrackIO coordinates and units to Unreal */
namespace LiveLinkOpenTrackIOConversions
{
	/** OpenTrackIO coordinate system
	 *
	 * OpenTrackIO is:
	 * o Z-up
	 * o Y-forward
	 * o Right-handed
	 *
	 * Unreal is:
	 * o Z-up
	 * o X-forward
	 * o Left-handed
	 *
	 * To convert translation and scale, swap X and Y, which both updates the forward vector and toggles the handedness.
	 * Translation also needs a factor of 100 because OpenTrackIO units are meters, while Unreal uses centimeters.
	 * To convert rotations, invert the sign of Pan/Yaw. The others can stay the same.
	 * Rotation units are degrees in both OpenTrackIO and Unreal.
	 */

	/** Factor to convert from OpenTrackIO's meters to Unreal's centimeters */
	inline constexpr float MetersToCentimeters = 100.f;


	/** Populate Live Link Lens Frame Data from relevant OpenTrackIO structures */
	inline void ToUnrealLens(
		FLiveLinkLensFrameData& OutUnrealLensData, 
		const FLiveLinkOpenTrackIOLens* InLensData,
		const FLiveLinkOpenTrackIOStaticCamera* InCamera
	)
	{
		// FIZ
		if (InLensData)
		{
			if (InLensData->FocusDistance.IsSet())
			{
				OutUnrealLensData.FocusDistance = InLensData->FocusDistance.GetValue() * MetersToCentimeters;
			}

			if (InLensData->FStop.IsSet())
			{
				OutUnrealLensData.Aperture = InLensData->FStop.GetValue();
			}

			if (InLensData->PinholeFocalLength.IsSet())
			{
				OutUnrealLensData.FocalLength = InLensData->PinholeFocalLength.GetValue();  // Both are in mm
			}
		}

		// Filmback
		if (InCamera)
		{
			if (InCamera->ActiveSensorPhysicalDimensions.Height.IsSet())
			{
				OutUnrealLensData.FilmBackHeight = InCamera->ActiveSensorPhysicalDimensions.Height.GetValue(); // Both are in mm
			}

			if (InCamera->ActiveSensorPhysicalDimensions.Width.IsSet())
			{
				OutUnrealLensData.FilmBackWidth = InCamera->ActiveSensorPhysicalDimensions.Width.GetValue();   // Both are in mm
			}
		}

		// Lens Distortion
		//
		// Only valid if the normalizing parameters are present (OpenTrackIO filmback and focal length, both in mm).
		if (InCamera && InLensData 
			&& InCamera->ActiveSensorPhysicalDimensions.Height.IsSet()
			&& InCamera->ActiveSensorPhysicalDimensions.Width.IsSet()
			&& InLensData->PinholeFocalLength.IsSet()
			)
		{
			const float Width_mm = InCamera->ActiveSensorPhysicalDimensions.Width.GetValue();
			const float Height_mm = InCamera->ActiveSensorPhysicalDimensions.Height.GetValue();
			const float F_mm = InLensData->PinholeFocalLength.GetValue();

			if ((Width_mm > KINDA_SMALL_NUMBER) && (Height_mm > KINDA_SMALL_NUMBER))  // Avoid division by zero.
			{
				// Cx Cy
				{
					// OpenTrackIO DistortionOffset is in mm, Unreal PrincipalPoint is normalized 0..1, centered at 0.5.
					// We normalize DistortionOffset in mm by the filmback dimensions that are also in mm.

					OutUnrealLensData.PrincipalPoint.X = 0.5 + InLensData->DistortionOffset.X / Width_mm;
					OutUnrealLensData.PrincipalPoint.Y = 0.5 + InLensData->DistortionOffset.Y / Height_mm;
				}

				// FxFy
				{
					// OpenTrackIO specifies a singular focal length F. We normalize by the filmback dimensions to get
					// focal length in UV units. That is, if they multiply by 3d coordinates normalized by depth,
					// we get normalized screen coordiantes in the range [0,1].
					OutUnrealLensData.FxFy.X = F_mm / Width_mm;
					OutUnrealLensData.FxFy.Y = F_mm / Height_mm;
				}

				// Distortion parameters
				// Note: The model name was already captured in the Live Link Static Data.
				for (const FLiveLinkOpenTrackIOLens_DistortionCoeff& Distortion : InLensData->Distortion)
				{
					// Copy all parameters, we do not know what they are.
					OutUnrealLensData.DistortionParameters.Append(Distortion.Radial.GetData(), Distortion.Radial.Num());
					OutUnrealLensData.DistortionParameters.Append(Distortion.Tangential.GetData(), Distortion.Tangential.Num());
					OutUnrealLensData.DistortionParameters.Append(Distortion.Custom.GetData(), Distortion.Custom.Num());

					// For now we will pick the first model in the array.
					break;
				}
			}
		}
	}

	/** From OpenTrackIO to Unreal Translation: Swap X <-> Y and convert meters to cm */
	inline FVector ToUnrealTranslation(const FLiveLinkOpenTrackIO_XYZ& InXYZ)
	{
		return FVector(InXYZ.Y, InXYZ.X, InXYZ.Z) * MetersToCentimeters;
	}

	/** From OpenTrackIO to Unreal Scale: Swap X <-> Y (unitless) */
	inline FVector ToUnrealScale(const FLiveLinkOpenTrackIO_XYZ& InXYZ)
	{
		return FVector(InXYZ.Y, InXYZ.X, InXYZ.Z);
	}

	/** From OpenTrackIO to Unreal Translation: Invert sign of Yaw */
	inline FRotator ToUnrealRotation(const FLiveLinkOpenTrackIO_Rotator& InRotator)
	{
		return FRotator(InRotator.Tilt, -InRotator.Pan, InRotator.Roll);
	}

	/** From OpenTrackIO to Unreal FTransform */
	inline FTransform ToUnrealTransform(const FLiveLinkOpenTrackIOTransform& InTransform)
	{
		return FTransform(
			ToUnrealRotation(InTransform.Rotation),
			ToUnrealTranslation(InTransform.Translation),
			ToUnrealScale(InTransform.Scale)
		);
	}
}

