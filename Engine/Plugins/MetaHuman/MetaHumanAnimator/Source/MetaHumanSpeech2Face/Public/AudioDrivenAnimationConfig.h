// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrameAnimationData.h"

#include "AudioDrivenAnimationConfig.generated.h"

USTRUCT(BlueprintType)
struct METAHUMANSPEECH2FACE_API FAudioDrivenAnimationModels
{
	GENERATED_BODY()

	FAudioDrivenAnimationModels();

	// The model which will be used for audio encoding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties, meta = (AllowedClasses = "/Script/NNE.NNEModelData"))
	FSoftObjectPath AudioEncoder;

	// The model which will be used for decoding the animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties, meta = (AllowedClasses = "/Script/NNE.NNEModelData"))
	FSoftObjectPath AnimationDecoder;
};

UENUM(BlueprintType)
enum class EAudioDrivenAnimationOutputControls : uint8
{
	FullFace UMETA(DisplayName = "Default (Full Face)"),
	MouthOnly,
};

UENUM(BlueprintType)
enum class EAudioDrivenAnimationMood : uint8
{
	// Auto Detect (255)
	AutoDetect = 255,
	// Neutral (0)
	Neutral = 0,
	// Happy (1)
	Happy = 1,
	// Sad (2)
	Sad = 2,
	// Disgust (3)
	Disgust = 3,
	// Anger (4)
	Anger = 4,
	// Surprise (5)
	Surprise = 5,
	// Fear (6)
	Fear = 6,
};

USTRUCT(BlueprintType)
struct METAHUMANSPEECH2FACE_API FAudioDrivenAnimationSolveOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties)
	EAudioDrivenAnimationMood Mood = EAudioDrivenAnimationMood::AutoDetect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Properties, Meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 1.0, ClampMax = 1.0, Delta = 0.01))
	float MoodIntensity = 1.0;
};
