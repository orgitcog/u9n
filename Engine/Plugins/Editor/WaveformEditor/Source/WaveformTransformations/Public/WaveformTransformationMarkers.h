// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWaveformTransformation.h"

#include "WaveformTransformationMarkers.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWaveformTransformationMarkers, Log, All);

class USoundWave;

enum class ELoopModificationControls : uint8
{
	None = 0,
	LeftHandleIncrement,
	LeftHandleDecrement,
	RightHandleIncrement,
	RightHandleDecrement,
	IncreaseIncrement,
	DecreaseIncrement,
	SelectNextLoop,
	SelectPreviousLoop,
};

//Used to make CuePoints work with PropertyHandles
UCLASS(BlueprintType, DefaultToInstanced)
class WAVEFORMTRANSFORMATIONS_API UWaveCueArray : public UObject
{
	GENERATED_BODY()
public:
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	
	//If uninitialized, init Marker array
	void InitMarkersIfNotSet(const TArray<FSoundWaveCuePoint>& InMarkers);
	//Uninitialize and empty Marker array
	void Reset();

	void EnableLoopRegion(FSoundWaveCuePoint* OutSoundWaveCue);

	UPROPERTY(EditAnywhere, Category = "Markers")
	TArray<FSoundWaveCuePoint> CuesAndLoops;
	int32 SelectedCue = INDEX_NONE;

	DECLARE_DELEGATE_OneParam(ModifyMarkerLoopRegionDelegate, ELoopModificationControls);
	ModifyMarkerLoopRegionDelegate ModifyMarkerLoop;

	DECLARE_DELEGATE_OneParam(CycleMarkerLoopRegionDelegate, ELoopModificationControls);
	CycleMarkerLoopRegionDelegate CycleMarkerLoop;

	// To minimize complexity while supporting all common editing cases, loops have a min length of 10 frames
	static constexpr int64 MinLoopSize = 10;

	DECLARE_DELEGATE(OnCueChange);
	OnCueChange CueChanged;

private:
	UPROPERTY()
	bool bIsInitialized = false;
};

class WAVEFORMTRANSFORMATIONS_API FWaveTransformationMarkers : public Audio::IWaveTransformation
{
public:
	explicit FWaveTransformationMarkers(double InStartLoopTime, double InEndLoopTime);
	virtual void ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const override;

	virtual constexpr Audio::ETransformationPriority FileChangeLengthPriority() const override { return Audio::ETransformationPriority::Low; }

private:
	double StartLoopTime = 0.0;
	double EndLoopTime = 0.0;
};

UCLASS()
class WAVEFORMTRANSFORMATIONS_API UWaveformTransformationMarkers : public UWaveformTransformationBase
{
	GENERATED_BODY()
public:
	UWaveformTransformationMarkers(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	UPROPERTY(VisibleAnywhere, Category = "Markers")
	TObjectPtr<UWaveCueArray> Markers;

	// These properties are hidden in editor by an IPropertyTypeCustomization
	UPROPERTY(EditAnywhere, Category = "Loop Preview", meta = (ClampMin = 0.0))
	double StartLoopTime = 0.0;
	// When EndLoopTime is < 0 it skips processing the audio in FWaveTransformationMarkers::ProcessAudio 
	UPROPERTY(EditAnywhere, Category = "Loop Preview")
	double EndLoopTime = -1.0;
	UPROPERTY(EditAnywhere, Category = "Loop Preview")
	bool bIsPreviewingLoopRegion = false;

	
	virtual Audio::FTransformationPtr CreateTransformation() const override;

	void UpdateConfiguration(FWaveTransformUObjectConfiguration& InOutConfiguration) override;
	void OverwriteTransformation() override;
	virtual constexpr Audio::ETransformationPriority GetTransformationPriority() const { return Audio::ETransformationPriority::Low; }

	void ModifyMarkerLoopRegion(ELoopModificationControls Modification) const;
	void CycleMarkerLoopRegion(ELoopModificationControls Modification) const;

	void ResetLoopPreviewing();

#if WITH_EDITOR
	void OverwriteSoundWaveData(USoundWave& InOutSoundWave) override;
	void GetTransformationInfo(FWaveformTransformationInfo& InOutTransformationInfo) const override;
#endif //WITH_EDITOR

private:
	float SampleRate = 0.f;
	float AvailableWaveformDuration = -1.f;

	/* Made a UPROPERTY so that it is captured when the soundwave is duplicated during export
	 * Given these specifiers to prevent a crash in the property handle caching system used for transformations
	 * Note: hidden in the details panel by the IPropertyTypeCustomization
	 */
	UPROPERTY(VisibleAnywhere, Category = "Markers")
	int64 StartFrameOffset = 0;

	bool bCachedIsPreviewingLoopRegion = false;
	bool bCachedSoundWaveLoopState = false;
};
