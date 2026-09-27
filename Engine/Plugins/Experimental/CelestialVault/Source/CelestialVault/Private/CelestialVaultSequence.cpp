// Copyright Epic Games, Inc. All Rights Reserved.


#include "CelestialVaultSequence.h"

#include "CelestialMaths.h"
#include "CelestialVault.h"
#include "CelestialVaultDaySequenceActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"


void FCelestialVaultSequence::BuildSequence(UProceduralDaySequenceBuilder* InBuilder)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCelestialVaultSequence::BuildSequence);
	
	ACelestialVaultDaySequenceActor* EarthDaySequenceActor = Cast<ACelestialVaultDaySequenceActor>(WeakTargetActor.Get());
	if (!EarthDaySequenceActor)
	{
		UE_LOG(LogCelestialVault, Warning, TEXT("This ProceduralDaySequence applies only on AEarthDaySequenceActors"));
		return;
	}

	// Use the reference time for Midnight, and build the keys from here
	FDateTime LocalTimeMidnight = EarthDaySequenceActor->GetDate();

	
	// Create the Celestial Vault Animation
	if (USceneComponent* CelestialVaultComponent = EarthDaySequenceActor->CelestialVaultComponent)
	{
		InBuilder->SetActiveBoundObject(CelestialVaultComponent);
		
		const double NormalizedTimeIncrement = 1.0 / FMath::Max(KeyCount - 1, static_cast<unsigned>(1));
		for (unsigned int Key = 0; Key < KeyCount; ++Key)
		{
			double CelestialVaultAngleAtMidnight = EarthDaySequenceActor->GetDayCelestialVaultAngle(); 

			const double KeyTime = Key * NormalizedTimeIncrement;
			InBuilder->AddRotationKey(KeyTime, FRotator(0.0, CelestialVaultAngleAtMidnight + KeyTime * 360.0, 0.0));
		}
	}

	// Create the Moon Disc Animation
	UStaticMeshComponent* MoonDiscComponent = EarthDaySequenceActor->MoonDiscComponent;
	UDirectionalLightComponent* MoonLightComponent = EarthDaySequenceActor->MoonLightComponent;

	if (MoonDiscComponent && MoonLightComponent)
	{
		const double NormalizedTimeIncrement = 1.0 / FMath::Max(KeyCount - 1, static_cast<unsigned>(1));
		for (unsigned int Key = 0; Key < KeyCount; ++Key)
		{
			const double KeyTime = Key * NormalizedTimeIncrement;

			FDateTime KeyDateTime = LocalTimeMidnight + FTimespan::FromSeconds(KeyTime * 24.0 * 60.0 * 60.0);
			FDateTime UTCTime = UCelestialMaths::LocalTimeToUTCTime(KeyDateTime, EarthDaySequenceActor->GMT_TimeZone,
				EarthDaySequenceActor->bIsDaylightSavings);
			double JulianDay = UCelestialMaths::UTCDateTimeToJulianDate(UTCTime);

			FPlanetaryBodyInfo MoonInfo = EarthDaySequenceActor->GetMoonInfo(JulianDay);

			if (Key == 0)
			{
				EarthDaySequenceActor->SetMoonDiscAge(MoonInfo.Age);
			}


			// Animate Disc
			InBuilder->SetActiveBoundObject(MoonDiscComponent);
			InBuilder->AddTransformKey(KeyTime, MoonInfo.UETransform);

			// Animate Light
			InBuilder->SetActiveBoundObject(MoonLightComponent);
			InBuilder->AddRotationKey(KeyTime, FRotationMatrix::MakeFromX(MoonInfo.DirectionTowardEarth).Rotator());
		}
	}

	// Create the SunLight Animation
	if (UDirectionalLightComponent* SunLightComponent = EarthDaySequenceActor->SunLightComponent)
	{
		InBuilder->SetActiveBoundObject(SunLightComponent);

		const double NormalizedTimeIncrement = 1.0 / FMath::Max(KeyCount - 1, static_cast<unsigned>(1));
		for (unsigned int Key = 0; Key < KeyCount; ++Key)
		{
			const double KeyTime = Key * NormalizedTimeIncrement;

			FDateTime KeyDateTime = LocalTimeMidnight + FTimespan::FromSeconds(KeyTime * 24 * 60 * 60);
			FDateTime UTCTime = UCelestialMaths::LocalTimeToUTCTime(KeyDateTime, EarthDaySequenceActor->GMT_TimeZone,
				EarthDaySequenceActor->bIsDaylightSavings);
			double JulianDay = UCelestialMaths::UTCDateTimeToJulianDate(UTCTime);

			FSunInfo SunInfo = EarthDaySequenceActor->GetSunInfo(JulianDay);
			// Animate Light
			InBuilder->AddRotationKey(KeyTime, FRotationMatrix::MakeFromX(SunInfo.DirectionTowardEarth).Rotator());
		}
	}
	
	FProceduralDaySequence::BuildSequence(InBuilder);
}
