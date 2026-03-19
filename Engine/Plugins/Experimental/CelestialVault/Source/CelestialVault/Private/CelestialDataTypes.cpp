// Copyright Epic Games, Inc. All Rights Reserved.


#include "CelestialDataTypes.h"
#include "CelestialMaths.h"

// Earth Preset
FPlanetaryBodyInputData FPlanetaryBodyInputData::Earth(TEXT("Earth"), EOrbitType::Earth, 6378.0);
// Moon Preset
FPlanetaryBodyInputData FPlanetaryBodyInputData::Moon(TEXT("Moon"), EOrbitType::Moon, 1737.4);


FString FPlanetaryBodyInfo::ToString() const
{
	FString PlanetaryBodyOrbitTypeName = FString("Invalid");
	if (UEnum* EnumClass = StaticEnum<EOrbitType>())
	{
		PlanetaryBodyOrbitTypeName = EnumClass->GetNameStringByValue(static_cast<int64>(OrbitType));
	}

	return FString::Printf(TEXT(
			"Planetary Body: %s\n"
			"Orbit Type: %s\n"
			"RightAscensionHours: %s\n"
			"Declination: %s\n"
			"Distance: %.0f AU\n"
			"Radius: %.0f km\n"
			"Apparent Diameter: %s / %.8f°\n"
			"Apparent Magnitude: %.2f\n"
			"Age: %.2f (%.2f days) since last New Moon\n"
			"IlluminationPercentage: %.2f\n"
			"-------\n"
			"Scaled Apparent Diameter: %.8f°\n"
			),
			*Name,
			*PlanetaryBodyOrbitTypeName,
			*UCelestialMaths::Conv_RightAscensionToString(RA),
			*UCelestialMaths::Conv_DeclinationToString(DEC),
			DistanceInAU,
			Radius,
			*UCelestialMaths::Conv_DeclinationToString(ApparentDiameterDegrees), ApparentDiameterDegrees,
			ApparentMagnitude,
			Age, UCelestialMaths::SynodicMonthAverage * Age, 
			IlluminationPercentage,
			ScaledApparentDiameterDegrees
		); 
	
}

void FPlanetaryBodyInfo::ComputeTranform(double UEDistance, double BodyScale)
{
	// Location
	FVector BodyLocation = UCelestialMaths::RADECToXYZ_RH(RA * 15, DEC, UEDistance);
	BodyLocation.Y *= -1; // Convert to UE Frame by inverting Y

	// Rotation - Orient the Body mesh towards the earth -
	// Use Look at the UE Origin, this is an acceptable approximation if the vault is big enough - We could use the Earth center if needed
	FVector BodyToEarth = (FVector::ZeroVector - BodyLocation).GetSafeNormal();
	FQuat BodyRotation = FQuat(BodyToEarth.Rotation());

	// Scale
	// The mesh plane is 100 UE Units (1m), and located at PlanetMeshDistanceKm. Use Thales theorem to compute its effective scale at this distance to ensure the right apparent diameter
	double UERadius = (UEDistance * Radius * 1000.0) / UCelestialMaths::AstronomicalUnitsToMeters(DistanceInAU);
	double Scale = UERadius * BodyScale / 50.0; // The plane half-length is 50 UE Units
	FVector BodyScale3D = FVector(Scale, Scale, Scale);

	UETransform = FTransform(BodyRotation, BodyLocation, BodyScale3D);

	// Direction
	DirectionTowardEarth = BodyToEarth;
}

FString FSunInfo::ToString() const
{
	return FString::Printf(TEXT(
			"RightAscensionHours: %s\n"
			"Declination: %s\n"
			),
			*UCelestialMaths::Conv_RightAscensionToString(RA),
			*UCelestialMaths::Conv_DeclinationToString(DEC)
		);
}


FString FStarInfo::ToString() const
{
	return FString::Printf(TEXT(
			"Name: %s\n"
			"RightAscensionHours: %s\n"
			"Declination: %s\n"
			"Distance: %.0f PC\n"
			"Magnitude: %.2f\n"
			),
			*Name,
			*UCelestialMaths::Conv_RightAscensionToString(RA),
			*UCelestialMaths::Conv_DeclinationToString(DEC),
			DistanceInPC,
			Magnitude
		);
	
}
