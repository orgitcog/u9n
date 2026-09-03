// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Delegates/Delegate.h"
#include "MotionTrailOptions.generated.h"

//if true still use old motion trails for sequencer objects.
extern SEQUENCER_API TAutoConsoleVariable<bool> CVarUseOldSequencerMotionTrails;


class USceneComponent;
class AActor;

UENUM(BlueprintType)
enum class EMotionTrailTrailStyle : uint8
{
	Default = 0,
	Dashed = 1,
	Time = 2,
	HeatMap = 3
};


// TODO: option to make tick size proportional to distance from camera to get a sense of perspective and scale
UCLASS(config = EditorPerProjectUserSettings)
class SEQUENCER_API UMotionTrailToolOptions : public UObject
{
	GENERATED_BODY()

public:
	UMotionTrailToolOptions()
		: bShowTrails(false)
		, bShowSelectedTrails(false)
		, TrailStyle(EMotionTrailTrailStyle::Default)
		, DefaultColor(.22,0.15,1.0)
		, TimePreColor(.22, 0.35, 0.8)
		, TimePostColor(.85, 0.25, 0.1)
		, DashPreColor(0.2, 0.9, 0.3)
		, DashPostColor(0.7, 0.2, 0.7)
		, bShowFullTrail(true)
		, TrailThickness(0.0f)
		, FramesBefore(10)
		, FramesAfter(10)
		, EvalsPerFrame(1)
		, bShowKeys(true)
		, bShowFrameNumber_DEPRECATED(false)
		, KeyColor(1.0, 1.0, 1.0)
		, SelectedKeyColor(0.8,0.8,0.0)
		, KeySize(3.0f)
		, bShowMarks(false)
		, MarkColor(0.25,1.0,0.15)
		, MarkSize(5.0)
		, MaxNumberPinned(10)
		, bLockMarksToFrames_DEPRECATED(true)
		, SecondsPerMark_DEPRECATED(0.1)
	{}

	/** Whether or not to show motion trails */
	UPROPERTY(EditAnywhere, Category = Trail);
	bool bShowTrails;

	/** Whether or not to show selected motion trails */
	UPROPERTY(EditAnywhere, Config, Category = Trail);
	bool bShowSelectedTrails;

	/** How To Show Color*/
	UPROPERTY(EditAnywhere, Config, Category = Trail);
	EMotionTrailTrailStyle TrailStyle;

	/** The color of the motion trail */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	FLinearColor DefaultColor;

	/** The color of the motion trail before current time if show alternating time colors */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	FLinearColor TimePreColor;

	/** The color of the motion trail after current time if show alternating time colors */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	FLinearColor TimePostColor;

	/** The color of the first motion trail color when alternating between frames */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	FLinearColor DashPreColor;

	/** The color of the next motion trail color when alternating between frames */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	FLinearColor DashPostColor;

	/** Whether or not to show the full motion trail */
	UPROPERTY(EditAnywhere, Category = Trail);
	bool bShowFullTrail;

	/* The thickness of the motion trail */
	UPROPERTY(EditAnywhere, Config, Category = Trail, Meta = (ClampMin = "0.0"))
	double TrailThickness;

	/** The number of frames to draw before the start of the trail. Requires not showing the full trail */
	UPROPERTY(EditAnywhere, Config, Category = Trail, Meta = (EditCondition = "!bShowFullTrail", ClampMin = "0"))
	int32 FramesBefore;

	/** The number of frames to draw after the end of the trail. Requires not showing the full trail */
	UPROPERTY(EditAnywhere, Config, Category = Trail, Meta = (EditCondition = "!bShowFullTrail", ClampMin = "0"))
	int32 FramesAfter;

	/** No longer exposed and clamped to 1 The number of evaluations per frame */
	int32 EvalsPerFrame;
	
	/** Whether or not to show keys on the motion trail */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	bool bShowKeys;

	/** Deprecated in 5.6 */
	UPROPERTY()
	bool bShowFrameNumber_DEPRECATED;

	/** The color of the keys */
	UPROPERTY(EditAnywhere, Category = Keys)
	FLinearColor KeyColor;

	/** The color of the selected keys */
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	FLinearColor SelectedKeyColor;

	/** The size of the keys */
	UPROPERTY(EditAnywhere, Config, Category = Keys, Meta = (ClampMin = "0.0"))
	double KeySize;

	/** Whether or not to show marks along the motion trail */
	UPROPERTY(EditAnywhere, Config, Category = Marks)
	bool bShowMarks;

	/** The color of the marks */
	UPROPERTY(EditAnywhere, Config, Category = Marks)
	FLinearColor MarkColor;

	/** The size of the marks */
	UPROPERTY(EditAnywhere, Config, Category = Marks, Meta = (ClampMin = "0.0"))
	double MarkSize;

	/** Max number of pinned trails*/
	UPROPERTY(EditAnywhere, Config, Category = AdvancedDisplay)
	int32 MaxNumberPinned;

	/** Deprecated in 5.6 */
	UPROPERTY()
	bool bLockMarksToFrames_DEPRECATED;

	/** Deprecated in 5.6 */
	UPROPERTY()
	double SecondsPerMark_DEPRECATED;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override
	{
		FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
		OnDisplayPropertyChanged.Broadcast(PropertyName);
		SaveConfig();
	}

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplayPropertyChanged, FName);
	FOnDisplayPropertyChanged OnDisplayPropertyChanged;

	static UMotionTrailToolOptions* GetTrailOptions()  { return GetMutableDefault<UMotionTrailToolOptions>(); }

	//We put Pinned motion trail information as part of this object so it can be shared by SequencerAnimTools and Control Rig module
	struct FPinnedTrail
	{
		FText TrailName;
		FLinearColor TrailColor;
		bool bHasOffset = false;
		TOptional<FText> SpaceName;
		FGuid TrailGuid;

		bool operator==(const FGuid& InId) const { return TrailGuid == InId; }

	};
	void ResetPinnedItems();
	int32 GetNumPinned() const;
	FPinnedTrail* GetPinnedTrail(int32 Index);

	int32 GetIndexFromGuid(FGuid InGuid) const;
	void PinSelection() const;
	void UnPinSelection();
	void AddPinned(const FPinnedTrail& InPinnedTrail);
	void PinComponent(USceneComponent* InSceneComponent, const FName& InSocketName) const;
	void DeletePinned(int32 Index);
	void DeleteAllPinned();
	void PutPinnnedInSpace(int32 Index, AActor* InActor, const FName& InComponentName);
	void SetLinearColor(int32 Index, const FLinearColor& Color);
	void SetHasOffset(int32 Index, bool bHasOffset);
	//get name and tooltip name for the color types
	TArray<TPair<FText, FText>>& GetTrailStyles();
	void SetTrailStyle(int32 Index);
	int32 GetTrailStyleIndex() const;

	DECLARE_MULTICAST_DELEGATE(FOnPinSelection);
	FOnPinSelection OnPinSelection;

	DECLARE_MULTICAST_DELEGATE(FOnUnPinSelection);
	FOnUnPinSelection OnUnPinSelection;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAddPinned, FGuid);
	FOnAddPinned OnAddPinned;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FPinComponent, USceneComponent*, FName);
	FPinComponent OnPinComponent;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeletePinned, FGuid);
	FOnDeletePinned OnDeletePinned;

	DECLARE_MULTICAST_DELEGATE(FOnDeleteAllPinned);
	FOnDeleteAllPinned OnDeleteAllPinned;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPutPinnedInSpace, FGuid, AActor*, FName);
	FOnPutPinnedInSpace OnPutPinnedInSpace;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSetLinearColor, FGuid, FLinearColor);
	FOnSetLinearColor OnSetLinearColor;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSetHasOffset, FGuid, bool);
	FOnSetHasOffset OnSetHasOffset;

protected:
	TArray<FPinnedTrail> PinnedTrails;
	TArray<TPair<FText, FText>> TrailStylesText;

};



