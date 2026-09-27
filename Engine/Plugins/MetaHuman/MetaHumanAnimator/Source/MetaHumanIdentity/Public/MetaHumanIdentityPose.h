// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/////////////////////////////////////////////////////
// UMetaHumanIdentityPose

#include "Misc/EnumRange.h"
#include "Engine/EngineTypes.h"
#include "Textures/SlateIcon.h"
#include "CaptureData.h"

#include "SequencedImageTrackInfo.h"

#include "MetaHumanIdentityPose.generated.h"

UENUM()
enum class EIdentityPoseType : uint8
{
	Invalid = 0,
	Neutral,
	Teeth,
	Custom,
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EIdentityPoseType, EIdentityPoseType::Count);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCaptureDataChanged, bool bInResetRanges)

/**
 * A Pose describes the appearance of the MetaHuman Identity in some way. It could be a mesh or footage showing the
 * Identity's teeth or its neutral expression.
 */
	UCLASS(BlueprintType, HideCategories = ("Preview"))
	class METAHUMANIDENTITY_API UMetaHumanIdentityPose
	: public UObject
{
	GENERATED_BODY()

public:
	/** Utility function to convert the EIdentityPoseType to a FString */
	static FString PoseTypeAsString(EIdentityPoseType InPoseType);

	UMetaHumanIdentityPose();

	/** Returns an icon that represents this pose */
	FSlateIcon GetPoseIcon() const;

	/** Returns an tooltip for this pose */
	FText GetPoseTooltip() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Target")
	void SetCaptureData(class UCaptureData* InCaptureData);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Target")
	class UCaptureData* GetCaptureData() const;

	/** Returns true if the capture is initialized */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Target")
	bool IsCaptureDataValid() const;

	/**
	 * @brief Creates a new promoted frame suitable for this pose.
	 *	The type of the promoted frame will be based on PromotedFrameClass.
	 *	It also sets the default tracker for the frame to be the same as DefaultTracker.
	 *
	 * @param OutPromotedFrameIndex The index of the newly created promoted frame in the internal array
	 * @return A new UMetaHumanIdentityPromotedFrame based on PromotedFrameClass
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Frame Promotion")
	UMetaHumanIdentityPromotedFrame* AddNewPromotedFrame(int32& OutPromotedFrameIndex);

	/** Removes the given promoted frame from this pose */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Frame Promotion")
	void RemovePromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame);

	FOnCaptureDataChanged& OnCaptureDataChanged()
	{
		return OnCaptureDataChangedDelegate;
	}

	/** Returns true iff the default tracker is a valid object that is ready to track */
	bool IsDefaultTrackerValid() const;

	/** Returns all Promoted Frames that contain contour data */
	TArray<class UMetaHumanIdentityPromotedFrame*> GetAllPromotedFramesWithValidContourData() const;

	/** If front view is set, returns promoted frames with valid contours with front view as first entry */
	TArray<class UMetaHumanIdentityPromotedFrame*> GetValidContourDataFramesFrontFirst() const;

	/** Returns the promoted frame tagged as front view */
	UMetaHumanIdentityPromotedFrame* GetFrontalViewPromotedFrame() const;

	/** Returns the head alignment transform for a given promoted frame */
	const FTransform& GetHeadAlignment(int32 InFrameIndex = 0);

	/**
	 * Sets the head alignment transform for a promoted frame.
	 * If InFrameIndex is not specified, sets the same transform to all promoted frames
	 */
	void SetHeadAlignment(const FTransform& InTransform, int32 InFrameIndex = INDEX_NONE);

	/** Sets the default tracker based on the PoseType. Only changes it DefaultTracker is not currently set */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Trackers")
	void LoadDefaultTracker();

	/** Adds or destroys the capture data scene component depending if it has valid capture data */
	void UpdateCaptureDataSceneComponent();

	/** Update the capture data config name */
	void UpdateCaptureDataConfigName();

	/** List of all RGB cameras (views) in the footage capture data */
	TArray<TSharedPtr<FString>> CameraNames;

public:
	// UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PreEditChange(FEditPropertyChain& InPropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& InTransactionEvent) override;
#endif

public:
	/** The display name of the pose. This can be edited for custom poses */
	UPROPERTY(EditAnywhere, Category = "Pose", meta = (EditCondition = "PoseType == EIdentityPoseType::Custom"))
	FText PoseName;

	/** The type this pose represents */
	UPROPERTY(BlueprintReadWrite, Category = "Pose")
	EIdentityPoseType PoseType;

	/** Whether or not to use the data driven approach to fit the eyes in the template mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", DisplayName = "Use Data Driven Eyes", meta = (EditCondition = "PoseType == EIdentityPoseType::Neutral"))
	bool bFitEyes;

	/** The transform for the pose if changed in the viewport */
	UPROPERTY(EditAnywhere, Category = "Pose")
	FTransform PoseTransform;

	/** The default tracker that should be used for tracking a Promoted Frame of this pose. This can still be customized in a per-frame basis */
	UPROPERTY(EditAnywhere, Category = "Trackers")
	TObjectPtr<class UMetaHumanFaceContourTrackerAsset> DefaultTracker; // TODO: We need a common interface for trackers

	/** The class the defines the Promoted Frame type for this pose */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Frame Promotion")
	TSubclassOf<class UMetaHumanIdentityPromotedFrame> PromotedFrameClass;

	/** The array of Promoted Frames for this pose */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced, Category = "Frame Promotion", meta = (EditFixedOrder))
	TArray<TObjectPtr<class UMetaHumanIdentityPromotedFrame>> PromotedFrames;

	/** The scene component that represents the capture data for this pose */
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<class USceneComponent> CaptureDataSceneComponent;

	/** Additional offset depth translation for the teeth */
	UPROPERTY(EditAnywhere, Category = "Pose", DisplayName = "Manual Teeth Depth Offset", meta = (EditCondition = "PoseType == EIdentityPoseType::Teeth",
		ToolTip = "Unit: cm",
		ClampMin = "-1.0", ClampMax = "1.0",
		UIMin = "-1.0", UIMax = "1.0"))
	float ManualTeethDepthOffset;

private:
	void NotifyCaptureDataChanged(bool bInResetRanges);
	void NotifyPoseTransformChanged();

	void HandleCaptureDataChanged(bool bInResetRanges);
	void HandleCaptureDataSceneComponentTransformChanged();

	void RegisterCaptureDataInternalsChangedDelegate();
	void RegisterCaptureDataSceneComponentTransformChanged();

	void UpdateRateMatchingDropFrames();

	TArray<FFrameRange> RateMatchingDropFrameRanges;

private:

	/** Delegated called when the capture data associated with the Pose changes */
	FOnCaptureDataChanged OnCaptureDataChangedDelegate;

	/** Source data for this pose, this could be a mesh or footage */
	UPROPERTY(EditAnywhere, Category = "Target")
	TObjectPtr<class UCaptureData> CaptureData;
	
	/** A cached check on if capture data is valid */
	bool bIsCaptureDataValid = false;

	/** Display name of the config to use with the capture data */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Target")
	FString CaptureDataConfig;

	/** The value of timecode alignment used previously (used to detect when timecode alignment changes between particular modes) */
	ETimecodeAlignment PreviousTimecodeAlignment = ETimecodeAlignment::None;

	/** The value of camera used previously (used to detect when camera changes) */
	FString PreviousCamera;

public:

	/** Name of camera (view) in the footage capture data to use for display and processing */
	UPROPERTY(EditAnywhere, Category = "Target")
	FString Camera;

	/** Controls alignment of media tracks via their timecode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	ETimecodeAlignment TimecodeAlignment = ETimecodeAlignment::Relative;

	/** Determine if specified frame is valid, ie has RGB and depth data, not excluded etc */
	enum class ECurrentFrameValid : uint8
	{
		Valid = 0,
		Invalid_NoCaptureData,
		Invalid_NoFootage,
		Invalid_NoRGBOrDepth,
		Invalid_Excluded,
	};

	ECurrentFrameValid GetIsFrameValid(int32 InFrameNumber) const;
	ECurrentFrameValid GetIsFrameValid(int32 InFrameNumber, const TRange<FFrameNumber>& InProcessingFrameRange, const TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>>& InMediaFrameRanges) const;

	TArray<FFrameRange> GetRateMatchingExcludedFrameRanges() const;
};