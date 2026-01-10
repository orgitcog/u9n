// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevLimiterMotorSimComponent.h"
#include "AudioMotorSimTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RevLimiterMotorSimComponent)

void URevLimiterMotorSimComponent::RevLimitToTarget(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo)
{
	if (RuntimeInfo.Rpm >= LimiterMaxRpm && !bActive)
	{
		bActive = true;
		TimeRemaining = LimitTime;
		OnRevLimiterHit.Broadcast();
		OnRevLimiterStateChanged.Broadcast(bActive);
	}

	if(RuntimeInfo.Rpm <= RecoverRPM && bActive)
	{
		bActive = false;
		OnRevLimiterStateChanged.Broadcast(bActive);

	}
		
	Input.Throttle = bActive ? 0.0f : Input.Throttle;
}

void URevLimiterMotorSimComponent::Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo)
{
	
	if(bRevLimitOnClutchEngaged && Input.bClutchEngaged)
	{
		RevLimitToTarget(Input, RuntimeInfo);
		return;
	}
	
	if (Input.bDriving && Input.bGrounded && Input.SideSpeed < SideSpeedThreshold)
	{
		TimeRemaining = 0.f;
		TimeInAir = 0.0f;

		if(bActive)
		{
			bActive = false;
			OnRevLimiterStateChanged.Broadcast(bActive);
		}

		Super::Update(Input, RuntimeInfo);
		return;
	}

	if(bActive == false)
	{
		bActive = true;
		OnRevLimiterStateChanged.Broadcast(bActive);
	}
	
	Input.bCanShift = false;

	// We've hit the limiter
	if (RuntimeInfo.Rpm >= LimiterMaxRpm)
	{
		TimeRemaining = LimitTime;
		RuntimeInfo.Rpm = LimiterMaxRpm;
		OnRevLimiterHit.Broadcast();
	}

	if (TimeRemaining > 0.0f)
	{
		Input.Throttle = 0.0f;

		TimeRemaining -= Input.DeltaTime;
		Input.bClutchEngaged = true;
	}
	else if (Input.bDriving == false)
	{
		Input.bClutchEngaged = true;
	}

	if (Input.bGrounded)
	{
		TimeInAir = 0.0f;
		Super::Update(Input, RuntimeInfo);
		return;
	}

	Input.bClutchEngaged = true;

	if (TimeRemaining > 0.0f)
	{
		TimeInAir += Input.DeltaTime;
	}

	if (TimeInAir >= AirMaxThrottleTime)
	{
		Input.Throttle = 0.0f;
	}

	Super::Update(Input, RuntimeInfo);
}

void URevLimiterMotorSimComponent::Reset()
{
	Super::Reset();

	TimeRemaining = 0.f;
	TimeInAir = 0.f;
}

