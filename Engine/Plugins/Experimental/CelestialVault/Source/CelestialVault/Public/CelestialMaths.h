// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CelestialDataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CelestialMaths.generated.h"

/**
 *  Units Conventions
 *    Distances: 
 *      Distances are expressed in Astronomical units - Specified by the "AU" mention in the function name
 *
 *    Time:
 *      Local/UTC Time: All function parameters contains either the "Local" or "UTC" prefix in their name to specify if they expect a Local or UTC Time. 

 *      Most Celestial functions are expecting an absolute time expressed using a Julian Date.
 *      When the name "JulianDay" is used, it means the Julian Date when t=0 (midnight, beginning of the day)
 *      By definition, a Julian Date is finishing by 0.5 at Midnight. 
 *
 *    Angles: 
 *      GMST and GAST angle are expressed in Degrees
 *      Some Celestial data involve ArcSeconds (1 Degree = 3600 Arcseconds) - Conversion functions are provided
 */
UCLASS()
class CELESTIALVAULT_API UCelestialMaths : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

#pragma region Colors
    
    /** Returns the RGB normalized components [0..1] from the Color Index (B-V) Value **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Color")
    static FLinearColor BVtoLinearColor(float BV);

#pragma endregion

#pragma region Planetary Bodies

    /**
     * Returns the location of a specific Planetary body, in the FK5 J2000 Coordinate System
     * 
     *   The FK5 is an equatorial coordinate system (coordinate system linked to the Earth) based on its J2000 position.
     *   As any equatorial frame, the FK5-based follows the long-term Earth motion (precession).
     *       
     *   The returned location is expressed in Astronomical Units (AU)
     *   The relativistic effects are ignored (See GetBodyLocation_FK5J2000_AU_Relativistic )
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Planetary bodies")
    static FVector GetBodyLocation_FK5J2000_AU(FPlanetaryBodyInputData PlanetaryBody, double JulianDate);

    /**
     * Returns the location of a specific Planetary body, in the FK5 J2000 Coordinate System
     * 
     *   The FK5 is an equatorial coordinate system (coordinate system linked to the Earth) based on its J2000 position.
     *   As any equatorial frame, the FK5-based follows the long-term Earth motion (precession).
     *       
     *   The returned location is expressed in Astronomical Units (AU)
     *       
     * The returned Location and the Observer Body location are expressed in Astronomical Units (AU)
     * The relativistic effects are considered (time taken for the light to reach the Observer body location)
     * This function therefore returns the location of the Planetary Body as if it was seen from the Observer Body Location
    */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Planetary bodies")
    static FVector GetBodyLocation_FK5J2000_AU_Relativistic(FVector ObserverBodyFK5J2000LocationAU, FPlanetaryBodyInputData PlanetaryBody, double JulianDate);

    /** Return the location of a Planetary Body relative to the Earth, expressed in Celestial Coordinates (RA, DEC, Distance)
     * It requires the Observer location on Earth for more precise computations
     * This function also returns the distance to the Sun, as it can help for Magnitude computations
     * **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Planetary bodies")
    static void GetBodyCelestialCoordinatesAU(double JulianDate, FPlanetaryBodyInputData PlanetaryBody, double ObserverLatitude, double ObserverLongitude, double& RAHours, double& DECDegrees,
        double& DistanceBodyToEarthAU, double& DistanceBodyToSunAU, double& DistanceEarthToSunAU);

    /** Return the Magnitude of a Planetary Body as seen from the Earth **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Planetary bodies")
    static double GetPlanetaryBodyMagnitude(FPlanetaryBodyInputData PlanetaryBody, double DistanceToSunAU, double DistanceToEarthAU, double DistanceEarthToSunAU, double& PhaseAngle);

    /** Returns the Moon Phase for a specific Date
     * 
     *  This is an approximate computation using a number of lunar cycles with a synodic month equals to 29.53059 days
     *  Not very precise over more than 1 centuries before of after 2025
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Planetary bodies")
    static double GetMoonNormalizedAgeSimple(double JulianDate);

	/** return the illumination factor (0..1) of a Body, considering his normalized age and the crescent effects */
	UFUNCTION(BlueprintCallable, Category="Celestial Vault|Planetary bodies")
	static double GetIlluminationPercentage(double NormalizedAge);

#pragma endregion

#pragma region Time

    /** Return the UTC for a specific Local Time, using the TimeZone and Daylight Saving Information **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static FDateTime LocalTimeToUTCTime(FDateTime LocalTime, double TimeZoneOffset, bool IsDst);

    /** Return the UTC for a specific Time, using the TimeZone and Daylight Saving Information **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static FDateTime UTCTimeToLocalTime(FDateTime UTCTime, double TimeZoneOffset, bool IsDst);
    
    /** Return the Julian Date for a specific UTC Time **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double UTCDateTimeToJulianDate(FDateTime UTCDateTime);

    /** Return the UTC Time for a specific Julian Date **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static FDateTime JulianDateToUTCDateTime(double JulianDate);

    /**
     * Return the Greenwich Mean Sidereal Time (GMST) for a specific DateTime, in Degrees
     * 
     * By definition, the provided DateTime has to be the DateTime at the Greenwitch Meridian, so it's a UTC DateTime
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double DateTimeToGreenwichMeanSiderealTime(FDateTime UTCDateTime);

    /** Return the Greenwich Mean Sidereal Time (GMST) for a specific Julian Date, In Degrees */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double JulianDateToGreenwichMeanSiderealTime(double JulianDate);

    /** Return the Sidereal Time for a specific Longitude and GMST **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double LocalSideralTime(double LongitudeDegrees, double GreenwichMeanSideralTime);
    
    /**
     * Return the Greenwich Apparent Sidereal Time (GAST) for a specific UTC DateTime, in Degrees.
     * 
     * The Greenwich apparent sidereal time is obtained by adding a correction to the Greenwich mean sidereal time.
     * The correction term is called the nutation in right ascension or the equation of the equinoxes.
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double JulianDateToGreenwichApparentSiderealTime(double JulianDate);

    /**
     * Returns the Julien Centuries
     *
     * Julian Centuries = (JulianDate - 2451545.0) / 36525.0
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double JulianDateToJulianCenturies(double JulianDate);

    /** Returns the Leap Seconds for a specific Julian Date
     *
     * A leap second is a one-second adjustment that is occasionally applied to Coordinated Universal Time (UTC), to accommodate the difference between
     * precise time (International Atomic Time (TAI), as measured by atomic clocks) and imprecise observed solar time (UT1),
     * which varies due to irregularities and long-term slowdown in the Earth's rotation.
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double GetLeapSeconds(double JulianDate);

    /** Returns the Terrestrial Time in SI seconds
     * 
     *  TT = TAI + 32.184 seconds;
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double InternationalAtomicTimeToTerrestrialTime(double TAI);

    /** Returns the International Atomic Time in SI seconds
     *
     *  TAI = GetLeapSeconds(JulianDate) + DaysToSeconds(JulianDate);
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static double JulianDateToInternationalAtomicTime(double JulianDate);

    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static inline double SecondsToDay(double Seconds) { return Seconds / 86400.0; }

    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static inline double DaysToSeconds(double Days) { return Days * 86400.0; }

#pragma endregion

#pragma region Angles
    
    /** Convert Arcseconds to Degrees
     * 
     * 1 Degree = 3600 Arcseconds */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Angles")
    static double ArcsecondsToDegrees(double Arcseconds) { return Arcseconds / 3600.0; };

    /** Convert Arcseconds to Radians
     * 
     * 2 PI Rad = 360 Degrees = 360 * 3600 Arcseconds
     */ 
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Angles")
    static double ArcsecondsToRadians(double Arcseconds) { return Arcseconds / 3600.0 * UE_PI / 180.0; };
    
    /** Convert Degrees to Arcseconds
     * 
     * 1 Degree = 3600 Arcseconds */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Angles")
    static double DegreesToArcseconds(double Degrees) { return Degrees * 3600.0; };

    /** Special Mod function that makes sure to always return positive values */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Angles")
    static double ModPositive(double Value, double Modulo);

    /** Convert Decimal degrees to Hours, Minutes, Seconds ( One Hour equals 15 degrees) */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static void DegreesToHMS(double DecimalDegrees, int32& Hours, int32& Minutes, double& Seconds);

    /** Convert decimal degrees to Degrees, Minutes, Seconds, with the appropriate Sign (True if Positive) **/ 
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Time")
    static void DegreesToDMS(double DecimalDegrees, bool& Sign, int32& Degrees, int32& Minutes, double& Seconds);

#pragma endregion 

#pragma region Earth

    /** Retrurn the Earth Rotation Angle (In Degrees) as measured by GMST (Greenwich Mean Sidereal Time)
     *
     * It refers to the angle of Earth's rotation relative to the fixed stars, specifically the hour angle of the mean vernal equinox as observed from the Greenwich meridian.
     * It represents how far Earth has rotated since the mean equinox crossed the Greenwich meridian;
     * It is essentially a way to measure Earth's rotation in angular terms based on a celestial reference point.
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Earth")
    static double GetEarthRotationAngle(double JulianDate);

    /** Convert Geodetic Lat Lon to Geocentric XYZ position vector in ECEF coordinates, for the WGS84 Ellipsoid.
     * 
     *   Be careful, XYZ coordinates are
     *    - ECEF Coordinates in the ECEF Right-Handed Frame (not the Left-handed UE ones in UE Units)
     *    - Expressed in Astronomical Units (AU)  
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Earth")
    static FVector GeodeticLatLonToECEFXYZAU(double Latitude, double Longitude, double Altitude);

    /** Return the Geocentric position of an observer located at the Earth surface, considering the rotation at this specific JulianDate, using the Greenwich Apparent Didereal Time.
     *
     *  The position is expressed relatively to the earth center, but on the solar system reference frame.
     *  Coordinates are Expressed in Astronomical Units (AU)
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Earth")
    static FVector GetObserverGeocentricLocationAU(double Latitude, double Longitude, double Altitude, double JulianDate);

    /** Return the nutation in right ascension ( aka the equation of the equinoxes) in Degrees
     * 
     * This correction term is used when computing the Greenwich apparent sidereal from the Greenwich mean sidereal time
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Earth")
    static double EquationOfTheEquinoxes(double JulianDate);

    /** Approximation of the IAU2000A/B nutation model used in the Equation Of The Equinoxes, accurate enough for VSOP87 computations  */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Earth")
    static void Nutation2000BTruncated(double JulianDate, double& DeltaPsi, double& DeltaEpsilon);

    /** Return the Transformation to apply to a WGS84 Ellipsoid model so that its location in Lat,long,Altitude is tangent to the Origin
     * It's used to locate the Rotating Celestial Vault for a specific UE Origin
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Earth")
    static FTransform GetPlanetCenterTransform(double Latitude, double Longitude, double Altitude);
    
#pragma endregion
    
#pragma region Sun

    /** Compute all Sun Properties for a speficic JulianDate */ 
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Sun")
    static FSunInfo GetSunInformation(double JulianDate, double ObserverLatitude, double ObserverLongitude);
        
#pragma endregion

#pragma region Utilities

    /** Returns the Speed of Light 299 792 458 (m/s) **/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Utilities")
    static double GetSpeedOfLight() { return UCelestialMaths::SpeedOfLightMetersPerSeconds; }

    
    /** Convert Astronomical Unit (UA) to meters
     *
     * 1 AU = 149 597 870 700 m 
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Utilities")
    static double AstronomicalUnitsToMeters(double AU) {return AU * AstronomicalUnitsMeters; };

    /** Convert meters to Astronomical Unit (UA)
     *
     * 1 AU = 149 597 870 700 m
     */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Utilities")
    static double MetersToAstronomicalUnits(double Meters) {return Meters / AstronomicalUnitsMeters; };

    /** Convert Cartesian Coordinates to Polar Coordinates, using a Righ-Handed Frame */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Utilities")
    static void XYZToRADEC_RH(FVector XYZ, double& RADegrees, double& DECDegrees, double& Radius);

    /** Convert Polar Coordinates to Cartesian Coordinates, using a Righ-Handed Frame */
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Utilities")
    static FVector RADECToXYZ_RH(double RADegrees, double DECDegrees, double Radius);

    /** Returns a String displaying a vector with a large number of digits*/
    UFUNCTION(BlueprintCallable, Category="Celestial Vault|Utilities")
    static FString GetPreciseVectorString(FVector Vector, int32 MinimumFractionalDigits = 10);

    /** StarInfo String Builder */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (StarInfo)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Celestial|Utilities")
    static FString Conv_StarInfoToString(const FStarInfo& StarInfo);

    /** PlanetaryBodyInfo String Builder */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (PlanetaryBodyInfo)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Celestial|Utilities")
    static FString Conv_PlanetaryBodyInfoToString(const FPlanetaryBodyInfo& PlanetaryBodyInfo);

    /** SunInfo ToString String Builder */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (SunInfo)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Celestial|Utilities")
    static FString Conv_SunInfoToString(const FSunInfo& SunInfo);
	
    /** Right Ascension String Builder */ 
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Right Ascension in hours)"), Category = "Celestial|Utilities")
    static FString Conv_RightAscensionToString(const double& RightAscensionHours);

    /** Declination String Builder */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Declination in Degrees)"), Category = "Celestial|Utilities")
    static FString Conv_DeclinationToString(const double& DecDegrees);

    /** HMS String Builder */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Angle in Hours Minutes Seconds)"), Category = "Celestial|Utilities")
    static FString Conv_HMSToString(int32 Hours, int32 Minutes, double Seconds);

    /** DMS String Builder */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Signed Angle in Degrees, Minutes, Seconds )"), Category = "Celestial|Utilities")
    static FString Conv_DMSToString(bool Sign, int32 Degrees, int32 Minutes, double Seconds);
    
#pragma endregion
public:
    static double SynodicMonthAverage;
    
private:

    /** Because the Dates are expressed around Jan 1, 4713 BC, we need a special Floor function to have the right years continuity
     *
     * Special "Math.floor()" function used by dateToJulianDate()
     */
    static int32 FloorForJulianDate(double JulianDate);

private:
    // Static const Members
    static FMatrix VSOPToJ2000;
    static double SpeedOfLightMetersPerSeconds;
    static double AstronomicalUnitsMeters;
    static double NewMoonReferenceJulianDate;
    
};
