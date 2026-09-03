// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRLoadingScreen.h"
#include "TickableObjectRenderThread.h"
#include "XRThreadUtils.h"
#include "IXRTrackingSystem.h"
#include "Tickable.h"

/**
* Base utility class for implementations of the IXRLoadingScreen interface
*/
template<typename SplashType>
class XRBASE_API TXRLoadingScreenBase : public IXRLoadingScreen, protected FTickableGameObject
{
public: 

	TXRLoadingScreenBase(class IXRTrackingSystem* InTrackingSystem)
		: TrackingSystem(InTrackingSystem)
		, HMDOrientation(FQuat::Identity)
		, bShowing(false)
		, SystemDisplayInterval(1 / 90.0f)
		, LastTimeInSeconds(FPlatformTime::Seconds())
	{
		check(TrackingSystem);
	}

	/* IXRLoadingScreen interface */
	virtual void ClearSplashes() override
	{
		if (bShowing)
		{
			for (SplashType& Splash : Splashes)
			{
				DoDeleteSplash(Splash);
			}
		}
		Splashes.Reset();
	}

	virtual void AddSplash(const FSplashDesc& Splash) override
	{
		Splashes.Emplace(Splash);
		DoAddSplash(Splashes.Last());
	}

	virtual void ShowLoadingScreen() override
	{
		{
			FQuat TMPOrientation = FQuat::Identity;
			FVector TMPPosition;
			TrackingSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, TMPOrientation, TMPPosition);

			// Only use the Yaw of the HMD orientation
			FRotator TMPRotation(TMPOrientation);
			TMPRotation.Pitch = 0;
			TMPRotation.Roll = 0;

			HMDOrientation = FQuat(TMPRotation);
			HMDOrientation.Normalize();
		}

		for (SplashType& Splash : Splashes)
		{
			DoShowSplash(Splash);
		}

		if (!bShowing)
		{
			bShowing = true;
			SetTickableTickType(ETickableTickType::Always);
			LastTimeInSeconds = FPlatformTime::Seconds();
		}
	}

	virtual void HideLoadingScreen() override
	{
		if (!bShowing)
		{
			return;
		}

		for (SplashType& Splash : Splashes)
		{
			DoHideSplash(Splash);
		}

		SetTickableTickType(ETickableTickType::Never);

		bShowing = false;
	}

	virtual bool IsShown() const override { return bShowing; }
	virtual bool IsPlayingLoadingMovie() const = 0;


protected:

	/* FTickableGameObject interface */
	virtual void Tick(float DeltaTime) override
	{
		const double TimeInSeconds = FPlatformTime::Seconds();
		const double DeltaTimeInSeconds = TimeInSeconds - LastTimeInSeconds;

		if (DeltaTimeInSeconds > 2.f * SystemDisplayInterval)
		{
			for (SplashType& Splash : Splashes)
			{
				ApplyDeltaRotation(Splash);
			}
			LastTimeInSeconds = TimeInSeconds;
		}
	}

	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FDefaultXRLoadingScreen, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return bShowing ? ETickableTickType::Always : ETickableTickType::Never; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }

	virtual void DoShowSplash(SplashType& Splash) = 0;
	virtual void DoHideSplash(SplashType& Splash) = 0;
	virtual void DoDeleteSplash(SplashType& Splash) = 0;
	virtual void DoAddSplash(SplashType& Splash) = 0;
	virtual void ApplyDeltaRotation(const SplashType& Splash) = 0;

	TArray<SplashType> Splashes;

	class IXRTrackingSystem* TrackingSystem;
	FQuat HMDOrientation;
	bool bShowing;

	float SystemDisplayInterval;
	double LastTimeInSeconds;
};