// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class METAHUMANCORETECH_API FMetaHumanRealtimeCalibration
{
public:
	
	FMetaHumanRealtimeCalibration(const TArray<FName>& InProperties, const TArray<float>& InNeutralFrame, const float InAlpha);

	static TArray<FName> GetDefaultProperties();

	void SetProperties(const TArray<FName>& InProperties);
	void SetAlpha(float Alpha);
	void SetNeutralFrame(const TArray<float>& InNeutralFrame);
	
	bool ProcessFrame(const TArray<FName>& InPropertyNames, TArray<float>& InOutFrame) const;
	
private:

	static TArray<FName> DefaultProperties;
	
	TArray<FName> Properties;
	TArray<float> NeutralFrame;
	float Alpha = 1.0;

};
