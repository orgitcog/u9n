// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "SoundSourceBusSend.generated.h"

class USoundSourceBus;
class UAudioBus;

UENUM(BlueprintType)
enum class ESourceBusSendLevelControlMethod : uint8
{
	// A send based on linear interpolation between a distance range and send-level range
	Linear,

	// A send based on a supplied curve
	CustomCurve,

	// A manual send level (Uses the specified constant send level value. Useful for 2D sounds.)
	Manual,
};

USTRUCT(BlueprintType)
struct FSoundSourceBusSendInfo
{
	GENERATED_USTRUCT_BODY()

	/*
		Manual: Use Send Level only
		Linear: Interpolate between Min and Max Send Levels based on listener distance (between Min/Max Send Distance)
		Custom Curve: Use the float curve to map Send Level to distance (0.0-1.0 on curve maps to Min/Max Send Distance)
	*/
	UPROPERTY(EditAnywhere, Category = BusSend, meta = (DisplayName = "Send Level Control Method"))
	ESourceBusSendLevelControlMethod SourceBusSendLevelControlMethod;

	// The Source Bus to send the audio to
	UPROPERTY(EditAnywhere, Category = BusSend)
	TObjectPtr<USoundSourceBus> SoundSourceBus;

	// The Audio Bus to send the audio to
	UPROPERTY(EditAnywhere, Category = BusSend)
	TObjectPtr<UAudioBus> AudioBus;

	// Manually set the amount of audio to send to the bus
	UPROPERTY(EditAnywhere, Category = BusSend, meta = (DisplayName = "Manual Send Level", EditCondition = "SourceBusSendLevelControlMethod == ESourceBusSendLevelControlMethod::Manual", EditConditionHides))
	float SendLevel;

	// The amount to send to the bus when sound is located at a distance less than or equal to value specified in the Min Send Distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend, meta = (EditCondition = "SourceBusSendLevelControlMethod == ESourceBusSendLevelControlMethod::Linear", EditConditionHides))
	float MinSendLevel;

	// The amount to send to the bus when sound is located at a distance greater than or equal to value specified in the Max Send Distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend, meta = (EditCondition = "SourceBusSendLevelControlMethod == ESourceBusSendLevelControlMethod::Linear", EditConditionHides))
	float MaxSendLevel;

	/** The distance at which to start mapping between to Min/Max Send Level
	 *  Distances LESS than this will result in a clamped Min Send Level
	 */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend, meta = (EditCondition = "SourceBusSendLevelControlMethod != ESourceBusSendLevelControlMethod::Manual", EditConditionHides))
	float MinSendDistance;

	/** The distance at which to stop mapping between Min/Max Send Level
	 *  Distances GREATER than this will result in a clamped Max Send Level
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend, meta = (EditCondition = "SourceBusSendLevelControlMethod != ESourceBusSendLevelControlMethod::Manual", EditConditionHides))
	float MaxSendDistance;

	// The custom send curve to use for distance-based send level. (0.0-1.0 on the curve's X-axis maps to Min/Max Send Distance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend, meta = (EditCondition = "SourceBusSendLevelControlMethod == ESourceBusSendLevelControlMethod::CustomCurve", EditConditionHides))
	FRuntimeFloatCurve CustomSendLevelCurve;

	FSoundSourceBusSendInfo()
		: SourceBusSendLevelControlMethod(ESourceBusSendLevelControlMethod::Manual)
		, SoundSourceBus(nullptr)
		, AudioBus(nullptr)
		, SendLevel(1.0f)
		, MinSendLevel(0.0f)
		, MaxSendLevel(1.0f)
		, MinSendDistance(100.0f)
		, MaxSendDistance(1000.0f)
	{
	}
};
