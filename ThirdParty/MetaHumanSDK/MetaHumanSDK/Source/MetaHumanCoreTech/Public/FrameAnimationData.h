// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetaHumanMeshData.h"

#include "FrameAnimationData.generated.h"

UENUM()
enum class EFrameAnimationQuality : uint8
{
	Undefined,
	Preview,
	Final,
	PostFiltered
};

UENUM()
enum class EAudioProcessingMode : uint8
{
	Undefined,
	FullFace,
	TongueTracking,
	MouthOnly
};

USTRUCT(BlueprintType)
struct METAHUMANCORETECH_API FFrameAnimationData
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform Pose = FTransform(FQuat(ForceInitToZero), FVector(ForceInitToZero), FVector(ForceInitToZero));

	UPROPERTY(Transient)
	TArray<float> RawPoseData;

	UPROPERTY(BlueprintReadOnly, Category = "AnimationData")
	TMap<FString, float> AnimationData;

	UPROPERTY(Transient)
	TMap<FString, float> RawAnimationData;

	UPROPERTY(Transient)
	FMetaHumanMeshData MeshData;

	UPROPERTY()
	EFrameAnimationQuality AnimationQuality = EFrameAnimationQuality::Undefined;

	UPROPERTY()
	EAudioProcessingMode AudioProcessingMode = EAudioProcessingMode::Undefined;

	bool ContainsData() const;

	friend FArchive& operator<<(FArchive& Ar, FFrameAnimationData& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	void Serialize(FArchive& Ar);
};
