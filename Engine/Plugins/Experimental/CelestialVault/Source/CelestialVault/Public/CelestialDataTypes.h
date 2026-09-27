// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CelestialDataTypes.generated.h"


/** 
 * Any celestial body with an Elliptic Orbit type will have to provide the elliptic parameters
 * Other will use the solar system VSOP87 computations for their location
 */
UENUM(BlueprintType) 
enum class EOrbitType : uint8
{
	Elliptic = 0, 
	Mercury,
	Venus,
	Earth,
	Mars,
	Jupiter,
	Saturn,
	Uranus ,
	Neptune ,
	Moon 
};

/**
 * TableRow Base type to describe the Creation parameters of a planetary body (Planet, Moon)
 * Will only be used at creation time, from a proper Data Table
 */
USTRUCT(BlueprintType)
struct FPlanetaryBodyInputData : public FTableRowBase
{
	GENERATED_BODY()

	FPlanetaryBodyInputData(const FString& InName, EOrbitType InOrbitType, double InRadius) : Name(InName), OrbitType(InOrbitType), Radius(InRadius) {}
	FPlanetaryBodyInputData() {}
public:
	/** The body Name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	FString Name = "";

	/** One of the predefined Orbit types (Solar system planets), any custom Elliptic ones for fantasy planets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	EOrbitType OrbitType = EOrbitType::Elliptic;
	
	/** The Body Radius in Kilometers */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Kilometers"))
	double Radius = 1000.0;

	/** The planetary body Material expects Bodies textures in a single row - This is the 0-based index of the texture to use */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	int32 TextureColumnIndex = 0;

	// TODO_Beta
	// Add the Elliptic parameters for other Moon/Planets
	// Ellipsoid - Minor/Major axes + offset to X axis
	// Revolution period (year) + angle offset to time
	// Phase if we want to fake it

public: 
	static FPlanetaryBodyInputData Earth; // Earth Preset
	static FPlanetaryBodyInputData Moon; // Moon Preset
};

/**
 * Runtime structure to store the computed properties of a Planetary Body, for any Query. 
 */
USTRUCT(BlueprintType)
struct FPlanetaryBodyInfo
{
	GENERATED_BODY()
public:
	/** One of the predefined body types (Solar system planets), any custom one for fantasy planets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	EOrbitType OrbitType = EOrbitType::Elliptic;

	/** The body Name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	FString Name = "";
	
	/** The Body Radius in Kilometers */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Kilometers"))
	double Radius = 1000.0;

	/** The Body Right Ascension in the Celestial Frame - In hours! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Hours"))
	double RA = 0.0;

	/** The Body Declination in the Celestial Frame - In Degrees */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Degrees"))
	double DEC  = 0.0;

	/** The Body distance to Earth - In Astronomical Units! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	double DistanceInAU = 0.0;

	/** The True apparent diameter of the body seen from Earth (Angular size, in degrees) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault",  meta = (ForceUnits="Degrees"))
	double ApparentDiameterDegrees = 0.0;

	/**
	 * The Scaled apparent diameter of the body seen from Earth (Angular size, in degrees) 
	 *   Takes the fake scaling factor into consideration, so useful for camera FOV tracking. 
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault",  meta = (ForceUnits="Degrees"))
	double ScaledApparentDiameterDegrees = 0.0;
	
	/** The Magnitude of the body seen from Earth */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	double ApparentMagnitude = 0.0;

	/** indication of the lunar age. 0 = New Moon, 0.25 = First quarter, 0.5 = Full Moon, 1 = Next New Moon */ 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	double Age = 0.5;

	/** indication of percentage of a full moon illumination */ 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	double IlluminationPercentage = 1.0;

	/** Internal value of the ISM Instance corresponding to this planet */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	int32 ISMInstanceIndex = 0; 

	/** Keep track of the Vault-relative transform, at ToD = 0 for animated bodies (Moon) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	FTransform UETransform;

	/** Keep track of the location toward the Earth */ 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	FVector DirectionTowardEarth = -FVector::ZeroVector;

	// Can't use a UFUNCTION inside a struct --> Use the Celestial Maths Blueprint Function Library
	FString ToString() const;

	
	void ComputeTranform(double UEDistance, double BodyScale);
};

/**
 * Runtime structure to store the computed properties of the Sun, for any Query. 
 */
USTRUCT(BlueprintType)
struct FSunInfo
{
	GENERATED_BODY()
public:
	/** The Sun Right Ascension in the Celestial Frame - In hours! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Hours", DisplayName="Right Ascension", MakeStructureDefaultValue="0.000000"))
	double RA = 0.0;

	/** The Sun Declination in the Celestial Frame - In Degrees */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Degrees", DisplayName="Declination", MakeStructureDefaultValue="0.000000"))
	double DEC = 0.0;

	/** Keep track of the Vault-relative transform, at ToD = 0 for the Sun */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	FTransform UETransform;

	/** Keep track of the location toward the Earth */ 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault")
	FVector DirectionTowardEarth = -FVector::ZeroVector;;

	// Can't use a UFUNCTION inside a struct --> Celestial Maths Blueprint Function Library
	FString ToString() const;

	// TODO_Beta
	// Compute and store additional properties for the Sun
	//   Elevation
	//   CorrectedElevation - Sun Elevation, corrected for atmospheric diffraction
	//   Azimuth
	//   SunriseTime
	//   SunsetTime
	//   SolarNoon
};

/**
 * Runtime structure to store the computed properties of a Star, for any Query. 
 */
USTRUCT(BlueprintType)
struct FStarInfo
{
	GENERATED_BODY()

public:
	/** The Star Right Ascension in the Celestial Frame - In hours! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Hours", DisplayName="Right Ascension", MakeStructureDefaultValue="0.000000"))
	double RA = 0.0;

	/** The Star Declination in the Celestial Frame - In Degrees */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Degrees", DisplayName="Declination", MakeStructureDefaultValue="0.000000"))
	double DEC = 0.0;

	/** Earth to Star distance (in Parsecs)  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Distance (in parsecs)", MakeStructureDefaultValue="100.000000"))
	double DistanceInPC = 100.0;
	
	/** Star Name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Name"))
	FString Name = "";

	/** Star Magnitude */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Magnitude", MakeStructureDefaultValue="1.000000"))
	double Magnitude = 1.0;

	/** Star RGB Color - Can be computed from the B-V value if the star is from an official Catalog */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Color"))
	FLinearColor Color = FLinearColor(1.0f,1.0f,1.0f,1.0f);

	/** Star Hipparcos ID if present in the Hipparcos Catalog */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Hipparcos Catalog ID", MakeStructureDefaultValue="0"))
	int32 HipparcosID = 0;

	/** Star Henry Draper ID if present in the Henry Draper Catalog */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Henry Draper Catalog ID", MakeStructureDefaultValue="0"))
	int32 HenryDraperID = 0;

	/** Star YaleBrightStar ID if present in the Yale Bright Star Catalog */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Yale Bright Star Catalog ID", MakeStructureDefaultValue="0"))
	int32 YaleBrightStarID = 0;
	
	/** Star Color Index, also named B-V */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="ColorIndex", MakeStructureDefaultValue="0.000000"))
	double ColorIndex = 0.0;

	/** Internal value of the ISM Instance corresponding to this Star */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault",  meta = (HideInDataTable))
	int32 ISMInstanceIndex = 0;

	// Can't use a UFUNCTION inside a struct --> Celestial Maths Blueprint Function Library
	FString ToString() const;
};


/**
 * TableRow Base type to describe the Creation parameters of a Basic Star
 * Will only be used at creation time, from a proper Data Table
 * This Struct contains the minimal needed data for a Fictional Star
 */
USTRUCT(BlueprintType)
struct FStarInputData : public FTableRowBase 
{
	GENERATED_BODY()
public:
	/** The Star Right Ascension in the Celestial Frame - In hours! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Hours", DisplayName="Right Ascension", MakeStructureDefaultValue="0.000000"))
	double RA = 0.0;

	/** The Star Declination in the Celestial Frame - In Degrees */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(ForceUnits="Degrees", DisplayName="Declination", MakeStructureDefaultValue="0.000000"))
	double DEC = 0.0;

	/** Earth to Star distance (in Parsecs)  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Distance (in parsecs)", MakeStructureDefaultValue="100.000000"))
	double DistanceInPC = 100.0;
	
	/** Star Name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Name"))
	FString Name = "";

	/** Star Magnitude */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Magnitude", MakeStructureDefaultValue="1.000000"))
	double Magnitude = 1.0;

	/** Star RGB Color - Useless if computed from the B-V */ 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Color"))
	FLinearColor Color = FLinearColor(1.0f,1.0f,1.0f,1.0f);
};

/**
 * TableRow Base type to describe the Creation parameters of a Catalog-based Star
 * Will only be used at creation time, from a proper Data Table
 * This Struct extends the FStarInputData class with additional Catalog Properties
 */
USTRUCT(BlueprintType)
struct FCelestialStarInputData : public FStarInputData 
{
	GENERATED_BODY()
public:
	/** Star Hipparcos ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Hipparcos Catalog ID", MakeStructureDefaultValue="0"))
	int32 HipparcosID = -1;

	/** Star Henry Draper ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Henry Draper Catalog ID", MakeStructureDefaultValue="0"))
	int32 HenryDraperID = -1;

	/** Star YaleBrightStar ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="Yale Bright Star Catalog ID", MakeStructureDefaultValue="0"))
	int32 YaleBrightStarID = -1;
	
	/** Star Color Index, also named B-V */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category= "Celestial Vault", meta=(DisplayName="ColorIndex", MakeStructureDefaultValue="0.000000"))
	double ColorIndex = -1.0;
};