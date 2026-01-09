// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDrivenAnimationConfig.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EditorAnimUtils.h"
#include "Misc/EnumClassFlags.h"
#include "MetaHumanBatchOperation.generated.h"

struct FScopedSlowTask;
class USkeleton;
class USkeletalMesh;
class UMetaHumanPerformance;
class USoundWave;

UENUM(BlueprintType)
enum class EBatchOperationStepsFlags : uint8
{
	None            = 0,
	SoundWaveToPerformance =	(1 << 0), // Create a MetaHuman performance from sound wave asset and set up ready for processing
	ProcessPerformance =		(1 << 1), // Process the MetaHuman performance
	ExportAnimSequence =		(1 << 2), // Export Anim Sequence from processed performance.
	ExportLevelSequence =		(1 << 3), // Export Level Sequence from processed performance.
};
ENUM_CLASS_FLAGS(EBatchOperationStepsFlags);


// Data needed to run a batch operation on a set of speech audio assets to animation
struct FMetaHumanBatchOperationContext
{
public:
	// The source assets to process
	TArray<TWeakObjectPtr<UObject>> AssetsToProcess;

	// Processing steps to be performed on assets
	EBatchOperationStepsFlags BatchStepsFlags = EBatchOperationStepsFlags::None;

	// Rename rules for duplicated assets
	EditorAnimUtils::FNameDuplicationRule PerformanceNameRule;
	EditorAnimUtils::FNameDuplicationRule ExportedAssetNameRule;

	// Set to override existing output assets, otherwise a unique asset name is created
	bool bOverrideAssets = false;

	// Processing options
	bool bGenerateBlinks = true;
	bool bMixAudioChannels = true;
	uint32 AudioChannelIndex = 0;
	FAudioDrivenAnimationSolveOverrides AudioDrivenAnimationSolveOverrides;
	EAudioDrivenAnimationOutputControls AudioDrivenAnimationOutputControls = EAudioDrivenAnimationOutputControls::FullFace;

	// Export options
	bool bEnableHeadMovement = false;
	TEnumAsByte<ERichCurveInterpMode> CurveInterpolation = ERichCurveInterpMode::RCIM_Linear;

	// Skeleton or SkelMesh used for exported anim sequence
	TSoftObjectPtr<UObject> TargetSkeletonOrSkeletalMesh;

	// Level sequence export options
	bool bExportAudioTrack = true;
	bool bExportCamera = true;
	TSoftObjectPtr<class UBlueprint> TargetMetaHuman;
	
	// Is the data configured in such a way that we could process
	bool IsValid() const;

	bool ValidatePerformanceNameRule() const;
	bool ValidateExportAssetNameRule() const;
};

// Encapsulate ability to process performances from SoundWave assets and into animation
UCLASS()
class UMetaHumanBatchOperation : public UObject
{
	GENERATED_BODY()

public:

	// Run the process from audio to animation
	void RunProcess(FMetaHumanBatchOperationContext& InContext);

private:

	TObjectPtr<UMetaHumanPerformance> CreatePerformanceFromSoundWave(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSoundWave, FScopedSlowTask& InProgress);
	TObjectPtr<UMetaHumanPerformance> GetTransientPerformance(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSoundWave);
	void SetupPerformance(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSoundWave, TObjectPtr<UMetaHumanPerformance> InPerformance);
	bool ProcessPerformanceAsset(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<UMetaHumanPerformance> InPerformance, FScopedSlowTask& InProgress);
	void ExportAnimationSequence(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSourceSoundWave, TObjectPtr<UMetaHumanPerformance> InPerformance, FScopedSlowTask& InProgress);
	void ExportLevelSequence(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSourceSoundWave, TObjectPtr<UMetaHumanPerformance> InPerformance, FScopedSlowTask& InProgress);

	// Overwrite existing assets
	void OverwriteExistingAssets(const FMetaHumanBatchOperationContext& InContext, FScopedSlowTask& InProgress);
	// Notify user of results
	void NotifyResults(const FMetaHumanBatchOperationContext& InContext, FScopedSlowTask& InProgress, bool bInErrorOccurred);
	// If user cancelled half way, cleanup all new created assets
	void CleanupIfCancelled(const FScopedSlowTask& Progress) const;

	TMap<FAssetData, FAssetData> CreatedAssets;	
	TObjectPtr<UMetaHumanPerformance> TransientPerformance;

	TObjectPtr<USkeleton> ExportSkeleton;
	TObjectPtr<USkeletalMesh> ExportSkeletalMesh;
	TObjectPtr<class UBlueprint> ExportMetaHuman; 
};
