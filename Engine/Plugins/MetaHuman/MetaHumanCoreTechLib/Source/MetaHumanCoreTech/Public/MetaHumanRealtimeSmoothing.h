// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetaHumanOneEuroFilter.h"

#include "Engine/DataAsset.h"

#include "MetaHumanRealtimeSmoothing.generated.h"



UENUM()
enum class EMetaHumanRealtimeSmoothingParamMethod : uint8
{
	RollingAverage = 0,
	OneEuro,
};
		
USTRUCT()
struct METAHUMANCORETECH_API FMetaHumanRealtimeSmoothingParam 
{
public:

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data")
	EMetaHumanRealtimeSmoothingParamMethod Method = EMetaHumanRealtimeSmoothingParamMethod::RollingAverage;

	UPROPERTY(EditAnywhere, Category = "Data", DisplayName = "Number of frames", meta = (EditCondition = "Method == EMetaHumanRealtimeSmoothingParamMethod::RollingAverage", EditConditionHides))
	uint8 RollingAverageFrame = 1;

	UPROPERTY(EditAnywhere, Category = "Data", DisplayName = "Slope", meta = (EditCondition = "Method == EMetaHumanRealtimeSmoothingParamMethod::OneEuro", EditConditionHides))
	float OneEuroSlope = 5000;

	UPROPERTY(EditAnywhere, Category = "Data", DisplayName = "Min cutoff", meta = (EditCondition = "Method == EMetaHumanRealtimeSmoothingParamMethod::OneEuro", EditConditionHides))
	float OneEuroMinCutoff = 5;
};

UCLASS(DisplayName = "MetaHuman Realtime Smoothing")
class METAHUMANCORETECH_API UMetaHumanRealtimeSmoothingParams : public UDataAsset
{
public:

	GENERATED_BODY()

	virtual void PostInitProperties() override;

	UPROPERTY(EditAnywhere, Category = "Smoothing")
	TMap<FName, FMetaHumanRealtimeSmoothingParam> Parameters;
};

class METAHUMANCORETECH_API FMetaHumanRealtimeSmoothing
{

public:

	FMetaHumanRealtimeSmoothing(const TMap<FName, FMetaHumanRealtimeSmoothingParam>& InSmoothingParams);

	static TMap<FName, FMetaHumanRealtimeSmoothingParam> GetDefaultSmoothingParams();

	bool ProcessFrame(const TArray<FName>& InPropertyNames, TArray<float>& InOutFrame, double InDeltaTime);

private:

	TMap<FName, FMetaHumanRealtimeSmoothingParam> SmoothingParams;

	static constexpr uint8 DefaultRollingAverageFrameCount = 1;
	static TMap<FName, uint8> DefaultRollingAverage;
	static TMap<FName, TPair<float, float>> DefaultOneEuro;

	uint8 RollingAverageMaxBufferSize = 1;
	TArray<TArray<float>> RollingAverageBuffer;

	TMap<FName, FMetaHumanOneEuroFilter> OneEuroFilters;
	FMetaHumanOneEuroFilter OneEuroYAxis[3];
	FMetaHumanOneEuroFilter OneEuroXAxis[3];

	bool IsOrientation(const FName& InProperty) const;
};
