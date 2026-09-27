// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CelestialDataTypes.h"
#include "DaySequenceActor.h"
#include "CelestialVaultDaySequenceActor.generated.h"

// Celestial Classes
class UStarsDataTable;

// UE Classes
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;


UCLASS(Blueprintable, HideCategories=(Tags, Networking, LevelInstance))
class CELESTIALVAULT_API ACelestialVaultDaySequenceActor
	: public ADaySequenceActor
{
	GENERATED_BODY()

public:
	ACelestialVaultDaySequenceActor(const FObjectInitializer& Init);
 
public: // Properties

#pragma region Components 
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category= "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PlanetCenterComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category= "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> CelestialVaultComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category= "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDirectionalLightComponent> SunLightComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category= "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDirectionalLightComponent> MoonLightComponent;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphereComponent;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkyLightComponent> SkyLightComponent;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExponentialHeightFogComponent> ExponentialHeightFogComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPostProcessComponent> GlobalPostProcessVolume;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVolumetricCloudComponent> VolumetricCloudComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DeepSkyComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MoonDiscComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> StarsComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Celestial Vault", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> PlanetsComponent;

#pragma endregion

#pragma region DateTime related Properties

	/* If true, ignore the Year Month Day value and use the current system Date */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Date & Location")
	bool bUseCurrentDate = false;

	/** Current Year*/ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Date & Location", meta = (EditConditionHides, EditCondition = "bUseCurrentDate==false", ClampMin = "1", ClampMax = "9999"))
	int Year = 2025;

	/** Current Month*/ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Date & Location", meta = (EditConditionHides, EditCondition = "bUseCurrentDate==false", ClampMin = "1", ClampMax = "12"))
	int Month = 1;

	/** Current Day*/ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Date & Location", meta = (EditConditionHides, EditCondition = "bUseCurrentDate==false", ClampMin = "1", ClampMax = "31"))
	int Day = 7;

	/** Current Time Zone*/ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Date & Location", meta = (ClampMin = "-11", ClampMax = "14"))
	double GMT_TimeZone = -5.0;

	/** Set to true if your current date is during the DST period */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Vault|Date & Location")
	bool bIsDaylightSavings = false;
	
	/** Latitude of Level Origin on planet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Vault|Date & Location", meta = (ForceUnits="Degrees", ClampMin = "-90", ClampMax = "90"))
	double Latitude = 45.0;

	/** Longitude of Level Origin on planet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Vault|Date & Location", meta = (ForceUnits="Degrees", ClampMin = "-180", ClampMax = "180"))
	double Longitude = -73.0;
	
	// Greenwich Mean Sidereal Time at corresponding to a 0 Time of Day (midnight in the morning) for the selected Date. 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Celestial Vault|Date & Location")
	double GMST_AtTOD_0 = 0.0;

	// Transform to apply to the planet to have it located tangent to the Origin
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Celestial Vault|Date & Location")
	FTransform PlanetCenterTransform;
	
#pragma endregion

#pragma region Celestial Vault related Properties

	/** We generate the sky elements the "Platon" way, using a sphere surrounding the Earth. This is the radius of this sphere. Make sure it's not too small to avoid parallax effects */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Celestial Vault", meta=( ForceUnits="Kilometers", ClampMin = "0", ClampMax = "500000"))
	double CelestialVaultDistance = 400000.0;

	/** Percentage of the CelestialVaultDistance at which the Stars are created */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Celestial Vault", meta=( ForceUnits="Percent", ClampMin = "0.1", ClampMax = "100"))
	double StarsVaultPercentage = 99.0;

	/** Percentage of the CelestialVaultDistance at which the Planets are created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Celestial Vault", meta=( ForceUnits="Percent", ClampMin = "0.1", ClampMax = "100"))
	double PlanetsVaultPercentage = 97.0;
	
	/** Percentage of the CelestialVaultDistance at which the Moons are created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Celestial Vault", meta=( ForceUnits="Percent", ClampMin = "0.1", ClampMax = "100"))
	double MoonVaultPercentage = 95.0;
	
#pragma endregion

#pragma region Stars related Properties
	
	/** A Datatable containing a Celestial Star Catalog data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Stars")
	TObjectPtr<UDataTable> CelestialStarCatalog = nullptr;

	/** A Datatable containing a Fictional Star Catalog data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Stars")
	TObjectPtr<UDataTable> FictionalStarCatalog = nullptr;
	
	/** All stars from the catalog with a Magnitude dimmer than this threshold won't be generated - Usually 6 is the naked eye visibility limit*/ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Stars", meta = (ClampMin = "-30", ClampMax = "30"))
	float MaxVisibleMagnitude = 6.0f;

	/** If true, the Stars information will be kept in memory and queryable at runtime */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Stars")
	bool bKeepStarsInfo = false;
	
	/** Array of the created Stars information - Only populated if KeepStarsInfo is true */ 
	UPROPERTY(BlueprintReadOnly, Category="Celestial Vault|Stars")
	TArray<FStarInfo> StarsInfo;
	
#pragma endregion
	
#pragma region Planets related Properties

	/** The Data Catalog containing all Planets data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Planets")
	TObjectPtr<UDataTable> PlanetsCatalog = nullptr;

	/** Factor to artificially increase the Planetary bodies size */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Planets", meta = (ClampMin = "0"))
	float PlanetsScale = 1.0f;

	/** If true, the Stars information will be kept in memory and queryable at runtime */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Planets")
	bool bKeepPlanetsInfos = false;
	
	/** Array of the created planetary bodies, with all their computed information - Only populated if KeepPlanetsInfos is true */ 
	UPROPERTY(BlueprintReadOnly, Category="Celestial Vault|Planets")
	TArray<FPlanetaryBodyInfo> PlanetsInfos;

#pragma endregion

#pragma region Moon / Sun related Properties

	/** Factor to artificially increase the Moon size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Celestial Vault|Moons", meta = (ClampMin = "0"))
	float MoonScale = 2.0f;

	/** Celestial Info for the Moon, at the beginning of the Day */  
	UPROPERTY(BlueprintReadOnly, Category="Celestial Vault|Moons")
	FPlanetaryBodyInfo MoonBodyInfo;

	/** If true, the moon Age (Phase) and location can be overriden */  
	UPROPERTY(EditAnywhere, Category="Celestial Vault|Moons")
	bool bManualControl = false;
	
	/** Lunar age. 0 = New Moon, 0.25 = First quarter, 0.5 = Full Moon, 1 = Next New Mo */  
	UPROPERTY(EditAnywhere, Category="Celestial Vault|Moons", meta = (EditConditionHides, EditCondition = "bManualControl==true", ClampMin = "0", ClampMax = "1"))
	float MoonAge = 0.2f;
	
	/** When faking the moon location, we need to give a location relative to the sun
	 * This is a way to control this "Horizontally" using an offset in Right Ascension. 
	 * 
	 * Ex: if MoonOffset_RA is 4, the moon will set 4 hours after the sun, so will be visible only the 4 first hours of the night.
	 * Be mindful, it should normally be correlated with the phase.
	 *   (if HoursBehindSun < 12, we are ~1st quarter, so Age should be < 0.5, and if 12< < 24 age should be > 0.5) 
	 * Any combination is possible, but manual control can lead to inconsistencies. 
	 */  
	UPROPERTY(EditAnywhere, Category="Celestial Vault|Moons", meta = (EditConditionHides, DisplayName="Moon Offset to Sun's Right Ascension", EditCondition = "bManualControl==true", ForceUnits="Hours", ClampMin = "0", ClampMax = "24"))
	float MoonOffset_RA = 12.0f;

	/** When faking the moon location, we need to give a location relative to the sun
	 * This is a way to control this "Vertically" using an offset in Declination. */ 
	UPROPERTY(EditAnywhere, Category="Celestial Vault|Moons", meta = (EditConditionHides, DisplayName="Moon Offset to Sun's Declination", EditCondition = "bManualControl==true", ForceUnits="Degrees", ClampMin = "-45", ClampMax = "45"))
	float MoonOffset_DEC = 15.0f;

	/** Celestial Info for the Sun, at the beginning of the Day */  
	UPROPERTY(BlueprintReadOnly, Category="Celestial Vault|Sun")
	FSunInfo SunInfo; 

	/** Base Sun Intensity
	 * Typically 120000 Lux
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Celestial Vault", meta=( ForceUnits="Lux", ClampMin = "0", ClampMax = "200000"))
	float SunLightIntensity = 120000.0f;
	
	/** Base Moonlight Intensity (for Full Moon)
	 * Typically 0.1 Lux, up to 0.32 Lux when the moon is at it's perigee (SuperMoon)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Celestial Vault", meta=( ForceUnits="Lux", ClampMin = "0", ClampMax = "200000"))
	float MoonLightIntensity = 0.1f;

	
#pragma endregion 

protected: // Functions

	/** BeginPlay and OnConstruction overrides auto-register this actor with the DaySequenceSubsystem. */
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
public: // Functions

	/** Returns the current defined day, without any Time, because the Time will be controlled by the DaySequence Time of day - Uses "Now" or the Year/Month/Day properties */ 
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Date & Location")
	FDateTime GetDate() const;

	/** Returns the Celestial Info for the Sun, at a specific Julian Date */  
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Sun")
	FSunInfo GetSunInfo(double JulianDate);
	
	/** Returns the Celestial Info for the Moon, at a specific Julian Date */  
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Queries")
	FPlanetaryBodyInfo GetMoonInfo(double JulianDate);

	/** Manually set the Moon Age (Phase) */  
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Moons")
	void SetMoonDiscAge(float InMoonAge);
	
// Data Queries
	
	/** Return the Celestial Information of the Star closest to a specific direction, within an angle threshold */
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Queries")
	bool GetClosestStarInfo(FVector ObserverLocation, FVector LookupDirection, double ThresholdAngleDegree, FStarInfo& FoundStarInfo, FTransform& StarTransform);
	
	/** Return the Celestial Information of the Planetary Body (moon, planet) closest to a specific direction, within an angle threshold */
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Queries")
	bool GetClosestPlanetaryBody(FVector StartPosition, FVector LookupDirection, double ThresholdAngleDegree, FPlanetaryBodyInfo& FoundPlanetaryBodyInfo, FTransform& BodyTransform);

	/** Return the Celestial Information of a specific Planetary Body (by its orbit type) */
	UFUNCTION(BlueprintCallable, Category = "Celestial Vault|Queries")
	bool GetPlanetaryBodyByOrbitType(EOrbitType OrbitType, FPlanetaryBodyInfo& FoundPlanetaryBodyInfo, FTransform& BodyTransform);

	/** Returns the Celestial vault rotation angle for the current Date at t=0 midnight */
	double GetDayCelestialVaultAngle() const;

#if WITH_EDITOR
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void InitStars();
	void InitPlanetaryBodies();
	FPlanetaryBodyInfo GetPlanetaryBodyInfo(const FPlanetaryBodyInputData& InputPlanetaryBody, double JulianDay, double UEDistance, double BodyScale) const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category= "Celestial Vault")
	void RebuildAll();

	FDelegateHandle DrawDebugDelegateHandle;
};
