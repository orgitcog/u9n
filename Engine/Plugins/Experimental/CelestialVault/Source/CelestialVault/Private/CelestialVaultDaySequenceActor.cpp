// Copyright Epic Games, Inc. All Rights Reserved.


#include "CelestialVaultDaySequenceActor.h"


// Components
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/StaticMeshComponent.h"

// UE Objects
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "DaySequenceSubsystem.h"
#include "DaySequenceCollectionAsset.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Curves/CurveFloat.h"

// Celestial Objects
#include "CelestialMaths.h"
#include "CelestialDataTypes.h"
#include "CelestialVault.h"



// Sets default values
ACelestialVaultDaySequenceActor::ACelestialVaultDaySequenceActor(const FObjectInitializer& Init)
: Super(Init)
{
	ExponentialHeightFogComponent = CreateOptionalDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ExponentialHeightFog"));
	ExponentialHeightFogComponent->SetupAttachment(RootComponent);
	ExponentialHeightFogComponent->bEnableVolumetricFog = true;

	static ConstructorHelpers::FObjectFinder<UCurveFloat> HighlightContrastCurve(TEXT("/CelestialVault/Data/CF_CelestialHighlightContrastCurve.CF_CelestialHighlightContrastCurve"));
	GlobalPostProcessVolume = CreateOptionalDefaultSubobject<UPostProcessComponent>(TEXT("GlobalPostProcessVolume"));
	GlobalPostProcessVolume->SetupAttachment(RootComponent);
	GlobalPostProcessVolume->Settings.bOverride_AutoExposureMinBrightness = true;
	GlobalPostProcessVolume->Settings.AutoExposureMinBrightness = -0.5f;
	GlobalPostProcessVolume->Settings.bOverride_LocalExposureHighlightContrastCurve = true;
	GlobalPostProcessVolume->Settings.LocalExposureHighlightContrastCurve = HighlightContrastCurve.Object;
	GlobalPostProcessVolume->Settings.bOverride_LocalExposureDetailStrength = true;
	GlobalPostProcessVolume->Settings.LocalExposureDetailStrength = 1.2f;
	GlobalPostProcessVolume->Settings.bOverride_LocalExposureBlurredLuminanceBlend = true;
	GlobalPostProcessVolume->Settings.LocalExposureBlurredLuminanceBlend = 0.4f;
	
	
	
	// Components Attached to Root
	SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky Light"));
	SkyLightComponent->SetupAttachment(RootComponent);
	SkyLightComponent->bRealTimeCapture = true;
	SkyLightComponent->bLowerHemisphereIsBlack = false;

	VolumetricCloudComponent = CreateOptionalDefaultSubobject<UVolumetricCloudComponent>(TEXT("Volumetric Cloud"));
	VolumetricCloudComponent->SetupAttachment(RootComponent);
	if (!IsTemplate())
	{
		// We don't want to load this material for the CDO as it will hold on to it forever and it is quite a large asset.
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> VolumetricCloudDefaultMaterialRef(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
		VolumetricCloudComponent->SetMaterial(VolumetricCloudDefaultMaterialRef.Object);
	}

	PlanetCenterComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Planet Center Transform"));
	PlanetCenterComponent->SetupAttachment(RootComponent);

	// Components Attached to Planet Center
	SkyAtmosphereComponent = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky Atmosphere"));
	SkyAtmosphereComponent->SetupAttachment(PlanetCenterComponent);
	SkyAtmosphereComponent->TransformMode = ESkyAtmosphereTransformMode::PlanetCenterAtComponentTransform;


	// Rotating Celestial Vault 
	CelestialVaultComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Rotating Celestial Vault"));
	CelestialVaultComponent->SetupAttachment(PlanetCenterComponent);

	// Components attached to the Celestial Vaul
	
	// Deep Sky background
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkySphereDefaultMesh(TEXT("/CelestialVault/Meshes/SM_CelestialVault.SM_CelestialVault"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkySphereDefaultMaterial(TEXT("/CelestialVault/Materials/MI_CelestialVault.MI_CelestialVault"));
	DeepSkyComponent = CreateOptionalDefaultSubobject<UStaticMeshComponent>(TEXT("Deep Sky"));
	DeepSkyComponent->SetupAttachment(CelestialVaultComponent);
	DeepSkyComponent->SetStaticMesh(SkySphereDefaultMesh.Object);
	DeepSkyComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeepSkyComponent->SetGenerateOverlapEvents(false);
	DeepSkyComponent->SetCastShadow(false);
	DeepSkyComponent->SetAffectDynamicIndirectLighting(false);
	DeepSkyComponent->SetCanEverAffectNavigation(false);
	DeepSkyComponent->SetMaterial(0, SkySphereDefaultMaterial.Object.Get());
	DeepSkyComponent->SetRelativeScale3D(FVector(CelestialVaultDistance*1000.0));

	// Stars ISM
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneXMesh(TEXT("/CelestialVault/Meshes/SM_Plane_FacingX.SM_Plane_FacingX"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StarsDefaultMaterial(TEXT("/CelestialVault/Materials/MI_Stars.MI_Stars"));
	StarsComponent = CreateOptionalDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	StarsComponent->SetupAttachment(CelestialVaultComponent);
	StarsComponent->SetStaticMesh(PlaneXMesh.Object);
	StarsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StarsComponent->SetGenerateOverlapEvents(false);
	StarsComponent->SetCastShadow(false);
	StarsComponent->SetAffectDynamicIndirectLighting(false);
	StarsComponent->SetCanEverAffectNavigation(false);
	StarsComponent->SetMaterial(0, StarsDefaultMaterial.Object);

	// Planets ISM
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlanetsDefaultMaterial(TEXT("/CelestialVault/Materials/MI_SolarSystemPlanets.MI_SolarSystemPlanets"));
	PlanetsComponent = CreateOptionalDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Planets"));
	PlanetsComponent->SetupAttachment(CelestialVaultComponent);
	PlanetsComponent->SetStaticMesh(PlaneXMesh.Object);
	PlanetsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlanetsComponent->SetGenerateOverlapEvents(false);
	PlanetsComponent->SetCastShadow(false);
	PlanetsComponent->SetAffectDynamicIndirectLighting(false);
	PlanetsComponent->SetCanEverAffectNavigation(false);
	PlanetsComponent->SetMaterial(0, PlanetsDefaultMaterial.Object);

	// Moon - Disc
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MoonDiscDefaultMaterial(TEXT("/CelestialVault/Materials/MI_Moon.MI_Moon"));
	MoonDiscComponent = CreateOptionalDefaultSubobject<UStaticMeshComponent>(TEXT("Moon Disk"));
	MoonDiscComponent->SetupAttachment(CelestialVaultComponent);
	MoonDiscComponent->SetStaticMesh(PlaneXMesh.Object);
	MoonDiscComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonDiscComponent->SetGenerateOverlapEvents(false);
	MoonDiscComponent->SetCastShadow(true); // Eclipses?
	MoonDiscComponent->SetAffectDynamicIndirectLighting(false);
	MoonDiscComponent->SetCanEverAffectNavigation(false);
	MoonDiscComponent->SetMaterial(0, MoonDiscDefaultMaterial.Object.Get());
	

	// Moon - Light
	MoonLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon Light"));
	MoonLightComponent->SetupAttachment(CelestialVaultComponent);
	MoonLightComponent->SetAtmosphereSunLightIndex(1);	// Make Moon the secondary directional light that contributes to the sky atmosphere.
	MoonLightComponent->SetForwardShadingPriority(0); // Give Moon forward shading priority.
	MoonLightComponent->SetIntensity(MoonLightIntensity);
	MoonLightComponent->SetUseTemperature(true);
	MoonLightComponent->SetTemperature(9000.f);
	MoonLightComponent->SetWorldRotation(FRotator(-45.f, 0.0f, 0.0f));
	MoonLightComponent->bCastCloudShadows = true; // Otherwise we still have hard shadows with an overcast sky 
	
	// Attach the sunlight relative to the rotating Vault
	SunLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun Light"));
	SunLightComponent->SetupAttachment(CelestialVaultComponent);
	SunLightComponent->SetAtmosphereSunLightIndex(0);	// Make Sun the first directional light that contributes to the sky atmosphere.
	SunLightComponent->SetForwardShadingPriority(1);   // Give Sun forward shading priority.
	SunLightComponent->SetIntensity(SunLightIntensity);
	SunLightComponent->bCastCloudShadows = true; // Otherwise we still have hard shadows with an overcast sky
	
	// Sequence and Data Assets
	if (!IsTemplate())
	{
		// Override the default collection (which animates the moon and sky material)
		static ConstructorHelpers::FObjectFinder<UDaySequenceCollectionAsset> DefaultCollection(TEXT("/CelestialVault/DSCA_CelestialVault.DSCA_CelestialVault"));
		DaySequenceCollections.Add(DefaultCollection.Object.Get());

		static ConstructorHelpers::FObjectFinder<UDataTable> DefaultStarsCatalog(TEXT("/CelestialVault/Data/DT_HYGCatalog_10K.DT_HYGCatalog_10K"));
		CelestialStarCatalog = DefaultStarsCatalog.Object.Get();

		static ConstructorHelpers::FObjectFinder<UDataTable> DefaultPlanetaryBodiesCatalog(TEXT("/CelestialVault/Data/DT_SolarSystemPlanets.DT_SolarSystemPlanets"));
		PlanetsCatalog = DefaultPlanetaryBodiesCatalog.Object.Get();
	}
}

//////// Protected Functions

void ACelestialVaultDaySequenceActor::BeginPlay()
{
	Super::BeginPlay();

	// We don't inherit from a BaseDaySequenceActor (not the same components), so we need to register ourselves
	if (const UWorld* World = GetWorld())
	{
		if (UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			DaySequenceSubsystem->SetDaySequenceActor(this);
		}

		//RebuildAll(); // TODO - is it needed? 
	}
}

void ACelestialVaultDaySequenceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// We don't inherit from a BaseDaySequenceActor (not the same components), so we need to register ourselves
	if (const UWorld* World = GetWorld())
	{
		if (UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			if (DaySequenceSubsystem->GetDaySequenceActor(/* bFindFallbackOnNull */ false) != this)
			{
				DaySequenceSubsystem->SetDaySequenceActor(this);
			}
		}

		// Replace the Moon Material with a MID
		UMaterialInstanceDynamic * MoonDiscMaterial = Cast<UMaterialInstanceDynamic>(MoonDiscComponent->GetMaterial(0));
		if (!MoonDiscMaterial)
		{
			MoonDiscMaterial = MoonDiscComponent->CreateAndSetMaterialInstanceDynamic(0); // Make the material Dynamic to control the Phase
		}
		MoonDiscMaterial->SetScalarParameterValue(FName("MoonAge"), MoonAge);
		
		RebuildAll();  
	}
}

//////// Public Functions
FDateTime ACelestialVaultDaySequenceActor::GetDate() const
{
	if (bUseCurrentDate)
	{
		return FDateTime::Now().GetDate();
	}

	if ( Day > FDateTime::DaysInMonth(Year, Month))
	{
		int32 ClampedDay = FDateTime::DaysInMonth(Year, Month);
		int32 ClampedMonth = FMath::Clamp(Month, 1, 12);
		UE_LOG(LogCelestialVault, Warning, TEXT("Day value (%d) over the number of days in month - Using %d instead"), Day, ClampedDay);
		return FDateTime(Year, ClampedMonth, ClampedDay, 0,0,0);
	}
		
	return FDateTime(Year, Month, Day, 0,0,0);
}

FSunInfo ACelestialVaultDaySequenceActor::GetSunInfo(double JulianDate)
{
	FSunInfo ResultSunInfo = UCelestialMaths::GetSunInformation(JulianDate, Latitude, Longitude);

	// Location
	FVector SunLocation = UCelestialMaths::RADECToXYZ_RH(ResultSunInfo.RA * 15.0, ResultSunInfo.DEC, 1000.0); // TODO - Add the Sun Distance To SunInfo 
	SunLocation.Y *= -100.0; // Convert to UE Frame by inverting Y and scaling to UE Units
	ResultSunInfo.UETransform.SetLocation(SunLocation);
	ResultSunInfo.DirectionTowardEarth = (FVector::ZeroVector - SunLocation).GetSafeNormal();
	return ResultSunInfo;
}

FPlanetaryBodyInfo ACelestialVaultDaySequenceActor::GetMoonInfo(double JulianDate)
{
	double UEDistance = CelestialVaultDistance * 100000.0 * MoonVaultPercentage / 100.0;
	FPlanetaryBodyInfo ResultMoonInfo = GetPlanetaryBodyInfo(FPlanetaryBodyInputData::Moon, JulianDate, UEDistance, MoonScale);

	if (bManualControl)
	{
		// Phase/Age
		ResultMoonInfo.Age = MoonAge;
		ResultMoonInfo.IlluminationPercentage = UCelestialMaths::GetIlluminationPercentage(MoonAge);

		// Location
		FSunInfo SunInfoTemp = GetSunInfo(JulianDate);
		ResultMoonInfo.RA = UCelestialMaths::ModPositive(SunInfoTemp.RA + MoonOffset_RA, 24.0);
		ResultMoonInfo.DEC = UCelestialMaths::ModPositive(SunInfoTemp.DEC + MoonOffset_DEC, 360.0);
		
		// We changed the RA, so we need to update the transform
		ResultMoonInfo.ComputeTranform(UEDistance, MoonScale);
	}
	else
	{
		ResultMoonInfo.Age = UCelestialMaths::GetMoonNormalizedAgeSimple(JulianDate);;
		ResultMoonInfo.IlluminationPercentage = UCelestialMaths::GetIlluminationPercentage(ResultMoonInfo.Age);
	}
	return ResultMoonInfo;
}

void ACelestialVaultDaySequenceActor::SetMoonDiscAge(float InMoonAge)
{
	if (MoonDiscComponent)
	{
		UMaterialInstanceDynamic * MoonDiscMaterial = Cast<UMaterialInstanceDynamic>(MoonDiscComponent->GetMaterial(0));
		if (MoonDiscMaterial)
		{
			MoonDiscMaterial->SetScalarParameterValue(FName("MoonAge"), InMoonAge);
		}
	}

	if (MoonLightComponent)
	{
		MoonLightComponent->SetIntensity(MoonLightIntensity * UCelestialMaths::GetIlluminationPercentage(InMoonAge));
	}
}

bool ACelestialVaultDaySequenceActor::GetClosestStarInfo(FVector ObserverLocation, FVector LookupDirection, double ThresholdAngleDegrees, FStarInfo& FoundStarInfo, FTransform& StarTransform)
{
	StarTransform = FTransform::Identity;
	
	// We query only if we generated the StarInfo  
	if (!bKeepStarsInfo || !StarsComponent)
	{
		return false;
	}
	
	double CosThresholdAngle = FMath::Cos(FMath::DegreesToRadians(ThresholdAngleDegrees));
	LookupDirection.Normalize();
	double MinCos = -1.0;
	int32 ClosestInstanceIndex = -1;
	
	for (int StarInfoIndex = 0; StarInfoIndex < StarsInfo.Num(); StarInfoIndex++)
	{
		FTransform ISMInstanceTransform;
		StarsComponent->GetInstanceTransform(StarsInfo[StarInfoIndex].ISMInstanceIndex, ISMInstanceTransform, true );

		FVector DirectionToInstance = ISMInstanceTransform.GetLocation() - ObserverLocation;
		DirectionToInstance.Normalize();

		double CosDeltaAngle = FVector::DotProduct(LookupDirection, DirectionToInstance);
		if (CosDeltaAngle > CosThresholdAngle && CosDeltaAngle > MinCos)
		{
			// Inside the cone angle, and closer to latest
			MinCos = CosDeltaAngle;
			ClosestInstanceIndex = StarInfoIndex;
		}
	}

	if (ClosestInstanceIndex != -1)
	{
		StarsComponent->GetInstanceTransform(StarsInfo[ClosestInstanceIndex].ISMInstanceIndex, StarTransform, true ); 
		FoundStarInfo = StarsInfo[ClosestInstanceIndex];
		return true;
	}

	return false;
}

bool ACelestialVaultDaySequenceActor::GetClosestPlanetaryBody(FVector ObserverPosition, FVector LookupDirection, double ThresholdAngleDegrees, FPlanetaryBodyInfo& FoundPlanetaryBodyInfo, FTransform& BodyTransform)
{
	BodyTransform = FTransform::Identity;
	
	// We query only if we generated the StarInfo  
	if (!bKeepPlanetsInfos)
	{
		return false;
	}
	
	double CosThresholdAngle = FMath::Cos(FMath::DegreesToRadians(ThresholdAngleDegrees));
	LookupDirection.Normalize();
	double MinCos = -1.0;
	int32 ClosestInstanceIndex = -1;

	for (FPlanetaryBodyInfo BodyInfo : PlanetsInfos)
	{
		if (PlanetsComponent)
		{
			// We need to use the ISM component and query the world transform because the celestial vault has rotated
			FTransform ISMInstanceTransform;
			PlanetsComponent->GetInstanceTransform(BodyInfo.ISMInstanceIndex, ISMInstanceTransform, true );

			FVector DirectionToInstance = ISMInstanceTransform.GetLocation() - ObserverPosition;
			DirectionToInstance.Normalize();

			double CosDeltaAngle = FVector::DotProduct(LookupDirection, DirectionToInstance);
			if (CosDeltaAngle > CosThresholdAngle && CosDeltaAngle > MinCos)
			{
				// Inside the cone angle, and closer to latest
				MinCos = CosDeltaAngle;
				ClosestInstanceIndex = BodyInfo.ISMInstanceIndex;
			}
		}
	}

	// Check for the moon
	bool bIsMoonCloser = false;
	if (MoonDiscComponent)
	{
		// We need to use the ISM component and query the world transform because the celestial vault has rotated
		FVector MoonLocation = MoonDiscComponent->GetComponentLocation();
		FVector DirectionToMoon = MoonLocation - ObserverPosition;
		DirectionToMoon.Normalize();

		double CosDeltaAngle = FVector::DotProduct(LookupDirection, DirectionToMoon);
		if (CosDeltaAngle > CosThresholdAngle && CosDeltaAngle > MinCos)
		{
			// Inside the cone angle, and closer to latest
			bIsMoonCloser = true;
		}
	}

	if (bIsMoonCloser)
	{
		BodyTransform = MoonDiscComponent->GetComponentTransform();
		FoundPlanetaryBodyInfo = MoonBodyInfo;
		return true;
	}
	else if (ClosestInstanceIndex != -1)
	{
		PlanetsComponent->GetInstanceTransform(ClosestInstanceIndex, BodyTransform, true );
		FoundPlanetaryBodyInfo = PlanetsInfos[ClosestInstanceIndex];
		return true;
	}

	return false;
}

bool ACelestialVaultDaySequenceActor::GetPlanetaryBodyByOrbitType(EOrbitType OrbitType, FPlanetaryBodyInfo& FoundPlanetaryBodyInfo, FTransform& BodyTransform)
{
	BodyTransform = FTransform::Identity;
	
	// We query only if we generated the StarInfo  
	if (!bKeepPlanetsInfos)
	{
		return false;
	}
	
	if (OrbitType == EOrbitType::Moon && MoonDiscComponent) 
	{
		BodyTransform = MoonDiscComponent->GetComponentTransform();
		FoundPlanetaryBodyInfo = MoonBodyInfo;
		return true;
	}
	else
	{
		for (FPlanetaryBodyInfo PlanetaryBodyInfo : PlanetsInfos)
		{
			if (PlanetaryBodyInfo.OrbitType == OrbitType)
			{
				if (PlanetsComponent)
				{
					PlanetsComponent->GetInstanceTransform(PlanetaryBodyInfo.ISMInstanceIndex, BodyTransform, true );
				}
				
				FoundPlanetaryBodyInfo = PlanetaryBodyInfo;
				return true;
			}
		}	
	}
	return false;
}

double ACelestialVaultDaySequenceActor::GetDayCelestialVaultAngle() const
{
	double TimeOffset = GMT_TimeZone;
	if (bIsDaylightSavings)
	{
		TimeOffset += 1.0;
	}
	return GMST_AtTOD_0 - TimeOffset * 15.0; 
}

#if WITH_EDITOR

void ACelestialVaultDaySequenceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed  
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	bool bBuildAll = false;
	bool bBuildStars = false;
	bool bBuildPlanetaryBodies = false;
	bool bRebuildSequence = false;

	// Geometry properties
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, CelestialVaultDistance) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, StarsVaultPercentage) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, PlanetsVaultPercentage) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MoonVaultPercentage))
	{
		if (DeepSkyComponent)
		{
			DeepSkyComponent->SetRelativeScale3D(FVector(CelestialVaultDistance*1000.0));	
		}
		
		// TODO UpdateGeoReferencing();

		bBuildAll |= true;
		bRebuildSequence = true;
	}

	// Time properties
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, bUseCurrentDate) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, Year) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, Month) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, Day) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, GMT_TimeZone) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, bIsDaylightSavings) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, Latitude) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, Longitude))
	{
		// DateTime has changed, we need to update the Reference CelestialVault Angle.
		FDateTime DateTime = GetDate();
		GMST_AtTOD_0 = UCelestialMaths::DateTimeToGreenwichMeanSiderealTime(DateTime);
		double CelestialVaultAngle = GetDayCelestialVaultAngle();
		CelestialVaultComponent->SetRelativeRotation( FRotator(0.0, CelestialVaultAngle, 0.0) );

		bBuildAll = true;
		bRebuildSequence = true;
	}

	// Properties impacting the Stars
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, CelestialStarCatalog) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, FictionalStarCatalog) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MaxVisibleMagnitude) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, bKeepStarsInfo))
	{
		bBuildStars |= true;
		bRebuildSequence = true;
	}

	// Properties impacting the Planetary Bodies
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, PlanetsCatalog) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, PlanetsScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, bKeepPlanetsInfos) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MoonScale)||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, bManualControl)||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MoonAge) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MoonLightIntensity) || 
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, SunLightIntensity) || 
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MoonOffset_RA) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACelestialVaultDaySequenceActor, MoonOffset_DEC))
	{
		bBuildPlanetaryBodies |= true;
		bRebuildSequence = true;
	}
	
	if (bBuildAll || bBuildStars)
	{
		InitStars();
	}

	if (bBuildAll || bBuildPlanetaryBodies)
	{
		InitPlanetaryBodies();
	}

	if (bRebuildSequence)
	{
		RootSequence = nullptr;
		UpdateRootSequence();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

//////// Private  Functions

void ACelestialVaultDaySequenceActor::InitStars() 
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ACelestialVaultDaySequenceActor::InitStars);
	
	if (StarsComponent)
	{
		StarsComponent->ClearInstances();
		StarsComponent->SetNumCustomDataFloats(4);
		StarsInfo.Empty();
		
		// Check for the Catalog
		if (!CelestialStarCatalog && !FictionalStarCatalog)
		{
			UE_LOG(LogCelestialVault, Warning, TEXT("Please define at least a Celestial or Fictional StarCatalog"));
			return;
		}

		if (CelestialStarCatalog)
		{
			// Check for the Catalog Data types
			const UScriptStruct* CelestialStarCatalogRowStruct = CelestialStarCatalog->GetRowStruct();
			if (CelestialStarCatalogRowStruct != FCelestialStarInputData::StaticStruct())
			{
				UE_LOG(LogCelestialVault, Warning, TEXT("Invalid DataTable row structure for CelestialStarCatalog! It should be of type %s"), *FCelestialStarInputData::StaticStruct()->GetName() );
			}
			else
			{
				// Generate Celestial Stars
				CelestialStarCatalog->ForeachRow<FCelestialStarInputData>("AEarthDaySequenceActor::InitStars", [&](const FName& Key, const FCelestialStarInputData& CelestialStarInputData)
					{
						if (CelestialStarInputData.Magnitude <= MaxVisibleMagnitude)
						{
							// Location - Convert to UE Left-handed Frame (Invert the Y coordinate)
							FVector StarLocation = UCelestialMaths::RADECToXYZ_RH(CelestialStarInputData.RA * 15.0, CelestialStarInputData.DEC, CelestialVaultDistance * 1000.0 * StarsVaultPercentage / 100.0) *  FVector(100.0, -100.0, 100.0);

							// Color
							FLinearColor StarColor = UCelestialMaths::BVtoLinearColor(CelestialStarInputData.ColorIndex);

							// Create ISM instance
							int32 NewIndex = StarsComponent->AddInstance(FTransform(StarLocation), false);
							TArray<float> NewCustomData;
							NewCustomData.Add(StarColor.R);
							NewCustomData.Add(StarColor.G);
							NewCustomData.Add(StarColor.B);
							NewCustomData.Add(CelestialStarInputData.Magnitude);
							StarsComponent->SetCustomData(NewIndex, NewCustomData);

							// Keep trace of the Star Information for further runtime queries					
							if (bKeepStarsInfo)
							{
								FStarInfo StarInfo;
								StarInfo.RA = CelestialStarInputData.RA;
								StarInfo.DEC = CelestialStarInputData.DEC;
								StarInfo.DistanceInPC = CelestialStarInputData.DistanceInPC;
								StarInfo.Name = CelestialStarInputData.Name;
								StarInfo.Magnitude = CelestialStarInputData.Magnitude;
								StarInfo.Color = StarColor;
								StarInfo.HipparcosID = CelestialStarInputData.HipparcosID;
								StarInfo.HenryDraperID = CelestialStarInputData.HenryDraperID;
								StarInfo.YaleBrightStarID = CelestialStarInputData.YaleBrightStarID;
								StarInfo.ColorIndex = CelestialStarInputData.ColorIndex;
								StarInfo.ISMInstanceIndex = NewIndex;
								// Maybe add other computed values here... 
								StarsInfo.Add(StarInfo);
							}
						}
					}
				);
			}
		}

		if (FictionalStarCatalog)
		{
			const UScriptStruct* FictionalStarCatalogRowStruct = FictionalStarCatalog->GetRowStruct();
			if (FictionalStarCatalogRowStruct != FStarInputData::StaticStruct())
			{
				UE_LOG(LogCelestialVault, Warning, TEXT("Invalid DataTable row structure for FictionalStarCatalog! It should be of type %s"), *FStarInputData::StaticStruct()->GetName() );
			}
			else
			{
				// Generate Fictional Stars
				FictionalStarCatalog->ForeachRow<FStarInputData>("AEarthDaySequenceActor::InitStars / Fictional", [&](const FName& Key, const FStarInputData& StarInputData)
					{
						if (StarInputData.Magnitude <= MaxVisibleMagnitude)
						{
							// Location - Convert to UE Left-handed Frame (Invert the Y coordinate)
							FVector StarLocation = UCelestialMaths::RADECToXYZ_RH(StarInputData.RA * 15.0, StarInputData.DEC, CelestialVaultDistance * 1000.0 * StarsVaultPercentage / 100.0) * FVector(100.0, -100.0, 100.0);

							// Color
							FLinearColor StarColor = StarInputData.Color;

							// Create ISM instance
							int32 NewIndex = StarsComponent->AddInstance(FTransform(StarLocation), false);
							TArray<float> NewCustomData;
							NewCustomData.Add(StarColor.R);
							NewCustomData.Add(StarColor.G);
							NewCustomData.Add(StarColor.B);
							NewCustomData.Add(StarInputData.Magnitude);
							StarsComponent->SetCustomData(NewIndex, NewCustomData);

							// Keep trace of the Star Information for further runtime queries					
							if (bKeepStarsInfo)
							{
								FStarInfo StarInfo;
								StarInfo.RA = StarInputData.RA;
								StarInfo.DEC = StarInputData.DEC;
								StarInfo.DistanceInPC = StarInputData.DistanceInPC;
								StarInfo.Name = StarInputData.Name;
								StarInfo.Magnitude = StarInputData.Magnitude;
								StarInfo.Color = StarColor;
								StarInfo.ISMInstanceIndex = NewIndex;
								StarsInfo.Add(StarInfo);
							}
						}
					}
				);
			}
		}
		
		UE_LOG(LogCelestialVault, Verbose, TEXT("%d Stars added "), StarsComponent->GetInstanceCount());
		StarsComponent->MarkRenderInstancesDirty();
	}		
}

void ACelestialVaultDaySequenceActor::InitPlanetaryBodies()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ACelestialVaultDaySequenceActor::InitPlanetaryBodies);
	
	// Get the proper Julian day, at Midnight... The Daysequence will rotate the sky vault later 
	FDateTime LocalTimeMidnight = GetDate();
	FDateTime UTCTime = UCelestialMaths::LocalTimeToUTCTime(LocalTimeMidnight, GMT_TimeZone, bIsDaylightSavings);
	double JulianDay = UCelestialMaths::UTCDateTimeToJulianDate(UTCTime);

	// Init the sun
	SunInfo = GetSunInfo(JulianDay);
	
	// Init the Moon
	if (MoonDiscComponent)
	{
		// We don't want to use the GetMoonTransform function here because we also want to keep the Moon information for the day.
		// It has a pitfall because the Moon information will not be adjusted to the current TOD, but that doesn't prevent anyone for doing a manuel query at some point
		MoonBodyInfo = GetMoonInfo(JulianDay);
		SetMoonDiscAge(MoonBodyInfo.Age);
		MoonDiscComponent->SetRelativeTransform(MoonBodyInfo.UETransform);
	}
	
	// Init the Planets from the catalog
	if (PlanetsComponent)
	{
		PlanetsComponent->ClearInstances();
		PlanetsComponent->SetNumCustomDataFloats(2);
		PlanetsInfos.Empty();

		// Safety check on Catalog Data
		if (!PlanetsCatalog)
		{
			UE_LOG(LogCelestialVault, Warning, TEXT("PlanetaryBodiesCatalog is null!"));
			return;
		}

		const UScriptStruct* RowStruct = PlanetsCatalog->GetRowStruct();
		if (RowStruct != FPlanetaryBodyInputData::StaticStruct() )
		{
			UE_LOG(LogCelestialVault, Warning, TEXT("Invalid DataTable row structure for the Planetary Bodies Catalog! It should be of type %s"), *FPlanetaryBodyInputData::StaticStruct()->GetName() );
			return;
		}

		// Init from the DataTable
		PlanetsCatalog->ForeachRow<FPlanetaryBodyInputData>("AEarthDaySequenceActor::InitPlanetaryBodies", [&](const FName& Key, const FPlanetaryBodyInputData& InputPlanetaryBody)
			{
				double UEBodyDistance = CelestialVaultDistance * 100000.0 * PlanetsVaultPercentage / 100.0;
				FPlanetaryBodyInfo BodyInfo = GetPlanetaryBodyInfo(InputPlanetaryBody, JulianDay, UEBodyDistance, PlanetsScale);

				// Add the new ISM Instance
				int32 NewIndex = PlanetsComponent->AddInstance(BodyInfo.UETransform);
				BodyInfo.ISMInstanceIndex = NewIndex;
			
				// Add the Custom data (column index to sample the planets atlas texture)
				TArray<float> NewCustomData;
				NewCustomData.Add(InputPlanetaryBody.TextureColumnIndex);
				NewCustomData.Add(BodyInfo.ApparentMagnitude);
				PlanetsComponent->SetCustomData(NewIndex, NewCustomData);

				// Keep trace of the PlanetaryBody for further queries (the Datatable is readonly, so store in another object)
				if (bKeepPlanetsInfos)
				{
					PlanetsInfos.Add(BodyInfo);	
				}
			}
		);
		PlanetsComponent->MarkRenderInstancesDirty();
	}
}

FPlanetaryBodyInfo ACelestialVaultDaySequenceActor::GetPlanetaryBodyInfo(const FPlanetaryBodyInputData& InputPlanetaryBody, double JulianDay, double UEDistance, double BodyScale) const
{
	FPlanetaryBodyInfo BodyInfo;
	BodyInfo.OrbitType = InputPlanetaryBody.OrbitType;
	BodyInfo.Name = InputPlanetaryBody.Name;
	BodyInfo.Radius = InputPlanetaryBody.Radius;

	// Compute location	
	double RAHours, DECDegrees, DistanceToEarthAU, DistanceToSunAU, DistanceEarthToSunAU;
	UCelestialMaths::GetBodyCelestialCoordinatesAU(JulianDay, InputPlanetaryBody, Latitude, Longitude, RAHours, DECDegrees, DistanceToEarthAU, DistanceToSunAU, DistanceEarthToSunAU);
	BodyInfo.RA = RAHours;
	BodyInfo.DEC = DECDegrees;
	BodyInfo.DistanceInAU = DistanceToEarthAU;

	// Compute Magnitude
	double Phase; // TODO - Check the PhaseAngle Computations. 
	BodyInfo.ApparentMagnitude = UCelestialMaths::GetPlanetaryBodyMagnitude(InputPlanetaryBody, DistanceToEarthAU, DistanceToSunAU, DistanceEarthToSunAU, Phase);
	BodyInfo.Age = Phase;
	
	// Compute the True and the Scaled apparent Diameters
	BodyInfo.ApparentDiameterDegrees = FMath::RadiansToDegrees(FMath::Atan2(InputPlanetaryBody.Radius * 1000.0, UCelestialMaths::AstronomicalUnitsToMeters(DistanceToEarthAU)) * 2.0);
	BodyInfo.ScaledApparentDiameterDegrees = FMath::RadiansToDegrees(FMath::Atan2(InputPlanetaryBody.Radius * 1000.0 * BodyScale, UCelestialMaths::AstronomicalUnitsToMeters(DistanceToEarthAU)) * 2.0);


	//
	BodyInfo.ComputeTranform(UEDistance, BodyScale);
	
	return BodyInfo;
}

void ACelestialVaultDaySequenceActor::RebuildAll()
{
	if (const UWorld* World = GetWorld())
	{
		// Make sure the actor is properly located at the Origin
		SetActorTransform(FTransform::Identity);
		
		// DateTime has changed, we need to update the Reference CelestialVault Angle.
		FDateTime DateTime = GetDate();
		GMST_AtTOD_0 = UCelestialMaths::DateTimeToGreenwichMeanSiderealTime(DateTime);

		// Get the transformation in proper Celestial World units (meters, right handed)
		FTransform ECEFFrameToWorldFrame = UCelestialMaths::GetPlanetCenterTransform(Latitude, Longitude, 0);

		// UE Frame are expressed in Left-handed coordinates, and units are in meters - Convert to UE Transform 
		FMatrix WorldFrameToUEFrame = FMatrix( 
			FVector(1.0, 0.0, 0.0),		// Easting (X) is UE World X
			FVector(0.0, -1.0, 0.0),	// Northing (Y) is UE World -Y because of left-handed convention
			FVector(0.0, 0.0, 1.0),		// Up (Z) is UE World Z 
			FVector(0.0, 0.0, 0.0));	// No Origin offset
		FMatrix UEFrameToWorldFrame = WorldFrameToUEFrame.Inverse();

		// Update the rotation part
		FMatrix TransformMatrix = UEFrameToWorldFrame * ECEFFrameToWorldFrame.ToMatrixNoScale() * WorldFrameToUEFrame;
		// Get Origin, and convert UE units to meters
		FVector UEOrigin = TransformMatrix.GetOrigin() * FVector(100.0, 100.0, 100.0);
		TransformMatrix.SetOrigin(UEOrigin);

		// Apply the transform
		PlanetCenterTransform = FTransform(TransformMatrix);
		PlanetCenterComponent->SetWorldTransform(PlanetCenterTransform);

		// Rebuild Sky and Sequence
		InitStars();
		InitPlanetaryBodies();

		double CelestialVaultAngle = GetDayCelestialVaultAngle();
		CelestialVaultComponent->SetRelativeRotation( FRotator(0.0, CelestialVaultAngle, 0.0) );
	}
}
