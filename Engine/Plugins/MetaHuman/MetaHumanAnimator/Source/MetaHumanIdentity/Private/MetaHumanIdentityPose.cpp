// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityStyle.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentityLog.h"

#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanFaceFittingSolver.h"

#include "Misc/AssertionMacros.h"
#include "Misc/TransactionObjectEvent.h"
#include "Misc/NamePermissionList.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "ImgMediaSource.h"

#include "Styling/AppStyle.h"

#if WITH_EDITOR
#include "CaptureDataUtils.h"
#include "Dialogs/Dialogs.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanIdentityPose)

#define LOCTEXT_NAMESPACE "MetaHumanIdentityPose"

namespace UE::MetaHuman::Private
{

static TArray<UE::MetaHuman::FSequencedImageTrackInfo> CreateSequencedImageTrackInfos(
	const UFootageCaptureData* InFootageCaptureData,
	const FFrameRate InTargetFrameRate,
	const ETimecodeAlignment InTimecodeAlignment
)
{
	if (!IsValid(InFootageCaptureData))
	{
		return {};
	}

	TRange<FFrameNumber> ProcessingFrameRange(0, 0);
	TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
	TRange<FFrameNumber> MaxFrameRange(0, 0);
	constexpr bool bIncludeAudio = true;
	InFootageCaptureData->GetFrameRanges(InTargetFrameRate, InTimecodeAlignment, bIncludeAudio, MediaFrameRanges, ProcessingFrameRange, MaxFrameRange);

	TArray<const UImgMediaSource*> ImageMediaSources;
	ImageMediaSources.Append(InFootageCaptureData->ImageSequences);
	ImageMediaSources.Append(InFootageCaptureData->DepthSequences);

	TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
	SequencedImageTrackInfos.Reserve(ImageMediaSources.Num());

	for (const UImgMediaSource* ImageMediaSource : ImageMediaSources)
	{
		if (IsValid(ImageMediaSource))
		{
			if (const TRange<FFrameNumber>* MediaRange = MediaFrameRanges.Find(ImageMediaSource))
			{
				SequencedImageTrackInfos.Emplace(ImageMediaSource->FrameRateOverride, *MediaRange);
			}
		}
	}

	return SequencedImageTrackInfos;
}

}

//////////////////////////////////////////////////////////////////////////
// UMetaHumanIdentityPose

FString UMetaHumanIdentityPose::PoseTypeAsString(EIdentityPoseType InPoseType)
{
	return StaticEnum<EIdentityPoseType>()->GetDisplayNameTextByValue(static_cast<int64>(InPoseType)).ToString();
}

UMetaHumanIdentityPose::UMetaHumanIdentityPose()
	: Super{}
	, bFitEyes{ false }
	, PoseTransform{ FTransform::Identity }
	, ManualTeethDepthOffset{ 0 }
{
}

void UMetaHumanIdentityPose::PostInitProperties()
{
	Super::PostInitProperties();

	LoadDefaultTracker();
}

FSlateIcon UMetaHumanIdentityPose::GetPoseIcon() const
{
	const FString PoseTypeName = PoseTypeAsString(PoseType);
	const auto PoseTypeNameAnsiChar = StringCast<ANSICHAR>(*PoseTypeName);

	// Compute the name of the Pose icon based on the PoseType enum
	const FName PoseIconName = FAppStyle::Get().Join("Identity.Pose.", PoseTypeNameAnsiChar.Get());

	FMetaHumanIdentityStyle& Style = FMetaHumanIdentityStyle::Get();
	const FName& StyleSetName = Style.GetStyleSetName();

	return FSlateIcon{ StyleSetName, PoseIconName };
}

FText UMetaHumanIdentityPose::GetPoseTooltip() const
{
	if (PoseType == EIdentityPoseType::Neutral)
	{
		return LOCTEXT("IdentityTreePoseNeutralTooltip", "Neutral Pose\nHolds Capture Data representing a head with the neutral facial expression\nProvides tools for Tracking to produce Marker Curves, which are then used\nby MetaHuman Identity Solve command to conform the Template Mesh\nto the given Capture Data.");
	}
	else if (PoseType == EIdentityPoseType::Teeth)
	{
		return LOCTEXT("IdentityTreePoseBaseTooltip", "Teeth Pose\nHolds Capture Data used for teeth registration and provides tools for tracking the Markers for teeth.\nThe results are used by the Fit Teeth command to adjust the teeth of Skeletal Mesh.\nThe facial expression should show teeth with the jaw closed.\nNOTE: Before using Fit Teeth command, first use Mesh to MetaHuman command to obtain Skeletal Mesh.\nIf teeth are tracked before Mesh to MetaHuman command is used, Fit Teeth will be done automatically");
	}
	return LOCTEXT("IdentityTreePoseCustomTooltip", "Custom Pose for the Face");
}

void UMetaHumanIdentityPose::SetCaptureData(UCaptureData* InCaptureData)
{
	if (CaptureData != InCaptureData)
	{
		CaptureData = InCaptureData;

		bIsCaptureDataValid = ((CaptureData != nullptr) && CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

		Camera.Reset();
		UFootageCaptureData::PopulateCameraNames(Cast<UFootageCaptureData>(CaptureData), Camera, CameraNames);

		RegisterCaptureDataInternalsChangedDelegate();
		const bool bResetRanges = true;
		HandleCaptureDataChanged(bResetRanges);
	}
}

UCaptureData* UMetaHumanIdentityPose::GetCaptureData() const
{
	return CaptureData;
}

bool UMetaHumanIdentityPose::IsCaptureDataValid() const
{
	return bIsCaptureDataValid;
}

UMetaHumanIdentityPromotedFrame* UMetaHumanIdentityPose::AddNewPromotedFrame(int32& OutPromotedFrameIndex)
{
	if (PromotedFrameClass != nullptr)
	{
		if (UMetaHumanIdentityPromotedFrame* PromotedFrame = NewObject<UMetaHumanIdentityPromotedFrame>(this, PromotedFrameClass, NAME_None, RF_Transactional))
		{
			if (IsDefaultTrackerValid())
			{
				// Use the default tracker set in the Pose itself
				PromotedFrame->ContourTracker = DefaultTracker;
			}

			OutPromotedFrameIndex = PromotedFrames.Add(PromotedFrame);

			return PromotedFrame;
		}
	}

	return nullptr;
}

void UMetaHumanIdentityPose::RemovePromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame)
{
	if (InPromotedFrame != nullptr)
	{
		PromotedFrames.RemoveSingle(InPromotedFrame);
	}
}

bool UMetaHumanIdentityPose::IsDefaultTrackerValid() const
{
	return (DefaultTracker != nullptr) && DefaultTracker->CanProcess();
}

TArray<UMetaHumanIdentityPromotedFrame*> UMetaHumanIdentityPose::GetAllPromotedFramesWithValidContourData() const
{
	TArray<UMetaHumanIdentityPromotedFrame*> ValidPromotedFrames;
	for (UMetaHumanIdentityPromotedFrame* PromotedFrame : PromotedFrames)
	{
		if (PromotedFrame->bUseToSolve && PromotedFrame->FrameContoursContainActiveData())
		{
			ValidPromotedFrames.Add(PromotedFrame);
		}
	}

	return ValidPromotedFrames;
}

TArray<UMetaHumanIdentityPromotedFrame*> UMetaHumanIdentityPose::GetValidContourDataFramesFrontFirst() const
{
	bool bValidFramesFound = false;
	TArray<UMetaHumanIdentityPromotedFrame*> ValidPromotedFrames;

	// If we only have 1 promoted frame that contains contour data - just use that
	if (PromotedFrames.Num() == 1 && PromotedFrames.Last()->bUseToSolve && PromotedFrames.Last()->bIsFrontView && PromotedFrames.Last()->FrameContoursContainActiveData())
	{
		ValidPromotedFrames.Add(PromotedFrames.Last());
		bValidFramesFound = true;
	}
	else if (PromotedFrames.Num() > 0)
	{
		for (UMetaHumanIdentityPromotedFrame* PromotedFrame : PromotedFrames)
		{
			if (PromotedFrame->bUseToSolve && PromotedFrame->FrameContoursContainActiveData() && PromotedFrame->bIsFrontView)
			{
				ValidPromotedFrames.EmplaceAt(0, PromotedFrame);
				bValidFramesFound = true;
				continue;
			}

			if (PromotedFrame->bUseToSolve && PromotedFrame->FrameContoursContainActiveData())
			{
				ValidPromotedFrames.Add(PromotedFrame);
			}
		}
	}

	return bValidFramesFound ? ValidPromotedFrames : TArray<UMetaHumanIdentityPromotedFrame*>();
}

UMetaHumanIdentityPromotedFrame* UMetaHumanIdentityPose::GetFrontalViewPromotedFrame() const
{
	for (UMetaHumanIdentityPromotedFrame* PromotedFrame : PromotedFrames)
	{
		if (PromotedFrame->bIsFrontView)
		{
			return PromotedFrame;
		}
	}

	return nullptr;
}

const FTransform& UMetaHumanIdentityPose::GetHeadAlignment(int32 InFrameIndex)
{
	check(InFrameIndex < PromotedFrames.Num());
	return PromotedFrames[InFrameIndex]->HeadAlignment;
}

void UMetaHumanIdentityPose::SetHeadAlignment(const FTransform& InTransform, int32 InFrameIndex)
{
	if (InFrameIndex == INDEX_NONE)
	{
		for (UMetaHumanIdentityPromotedFrame* PromotedFrame : PromotedFrames)
		{
			PromotedFrame->bIsHeadAlignmentSet = true;
			PromotedFrame->HeadAlignment = InTransform;
		}
	}
	else
	{
		if (InFrameIndex < PromotedFrames.Num())
		{
			PromotedFrames[InFrameIndex]->bIsHeadAlignmentSet = true;
			PromotedFrames[InFrameIndex]->HeadAlignment = InTransform;
		}
	}
}

void UMetaHumanIdentityPose::LoadDefaultTracker()
{
	if (DefaultTracker == nullptr)
	{
		static constexpr const TCHAR* GenericTrackerPath = TEXT("/" UE_PLUGIN_NAME "/GenericTracker/GenericFaceContourTracker.GenericFaceContourTracker");
		if (UMetaHumanFaceContourTrackerAsset* Tracker = LoadObject<UMetaHumanFaceContourTrackerAsset>(GetTransientPackage(), GenericTrackerPath))
		{
			DefaultTracker = Tracker;
		}
	}
}

void UMetaHumanIdentityPose::UpdateCaptureDataSceneComponent()
{
	if (CaptureData != nullptr && CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full))
	{
#if WITH_EDITOR
		CaptureDataSceneComponent = MetaHumanCaptureDataUtils::CreatePreviewComponent(CaptureData, this);
#else
		CaptureDataSceneComponent = nullptr;
#endif

		if (CaptureData->IsA<UMeshCaptureData>())
		{
			// If updating a mesh capture data, restore the PoseTransform into the CaptureSceneComponent
			// as a new component was created above
			CaptureDataSceneComponent->SetWorldTransform(PoseTransform);
		}

		// Updates the pose transform to whatever is set in the preview component
		HandleCaptureDataSceneComponentTransformChanged();

		// Registers a delegate to update the pose transform if the component transforms changes
		RegisterCaptureDataSceneComponentTransformChanged();
	}
	else if (CaptureDataSceneComponent != nullptr)
	{
		// If the capture data changed to nullptr, it was cleared, so destroy the preview component as well
		CaptureDataSceneComponent->DestroyComponent();
		CaptureDataSceneComponent = nullptr;
	}
}

void UMetaHumanIdentityPose::PostLoad()
{
	Super::PostLoad();

	bIsCaptureDataValid = ((CaptureData != nullptr) && CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

	RegisterCaptureDataInternalsChangedDelegate();
	RegisterCaptureDataSceneComponentTransformChanged();

	// Create new or destroy existing scene component (depending on if there is valid capture data)
	UpdateCaptureDataSceneComponent();

	// Updates the transform of the capture data scene component
	NotifyPoseTransformChanged();

	// Update the display name of the config associated with the capture data
	UpdateCaptureDataConfigName();

	UFootageCaptureData::PopulateCameraNames(Cast<UFootageCaptureData>(CaptureData), Camera, CameraNames);
	UpdateRateMatchingDropFrames();
}

#if WITH_EDITOR

void UMetaHumanIdentityPose::PreEditChange(FEditPropertyChain& InPropertyAboutToChange)
{
	Super::PreEditChange(InPropertyAboutToChange);

	PreviousTimecodeAlignment = TimecodeAlignment;
	PreviousCamera = Camera;
}

void UMetaHumanIdentityPose::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	if (FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = *Property->GetName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, CaptureData))
		{
			bIsCaptureDataValid = ((CaptureData != nullptr) && CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

			Camera.Reset();
			UFootageCaptureData::PopulateCameraNames(Cast<UFootageCaptureData>(CaptureData), Camera, CameraNames);

			RegisterCaptureDataInternalsChangedDelegate();
			const bool bResetRanges = true;
			HandleCaptureDataChanged(bResetRanges);
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, Camera))
		{
			UFootageCaptureData::PopulateCameraNames(Cast<UFootageCaptureData>(CaptureData), Camera, CameraNames);

			bool bCameraChanged = Camera != PreviousCamera;

			if (bCameraChanged && !PromotedFrames.IsEmpty())
			{
				FSuppressableWarningDialog::FSetupInfo Info(
					LOCTEXT("ChangeIdentityCamera", "Changing the camera will delete promoted frames"),
					LOCTEXT("ChangeIdentityCameraTitle", "Change camera"),
					TEXT("ChangeIdentityCameraAlignment"));

				Info.ConfirmText = LOCTEXT("ChangeIdentityCamera_ConfirmText", "Ok");
				Info.CancelText = LOCTEXT("ChangeIdentityCamera_CancelText", "Cancel");

				FSuppressableWarningDialog ShouldChangeCameraDialog(Info);
				FSuppressableWarningDialog::EResult UserInput = ShouldChangeCameraDialog.ShowModal();

				if (UserInput == FSuppressableWarningDialog::EResult::Cancel)
				{
					Camera = PreviousCamera;
					bCameraChanged = false;
				}
			}

			if (bCameraChanged)
			{
				const bool bResetRanges = false;
				HandleCaptureDataChanged(bResetRanges); // Need to do the same things as if this were new capture data, eg clear promoted frames, repopulate timeline
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, PoseTransform))
		{
			NotifyPoseTransformChanged();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, TimecodeAlignment))
		{
			bool bTimecodeAlignmentChanged = true;

			if (!PromotedFrames.IsEmpty())
			{
				FSuppressableWarningDialog::FSetupInfo Info(
					LOCTEXT("ChangeIdentityTimecodeAlignment", "Changing the timecode alignment will delete promoted frames\n"),
					LOCTEXT("ChangeIdentityTimecodeAlignmentTitle", "Change timecode alignment"),
					TEXT("ChangeIdentityTimecodeAlignment"));

				Info.ConfirmText = LOCTEXT("ChangeIdentityTimecode_ConfirmText", "OK");
				Info.CancelText = LOCTEXT("ChangeIdentityTimecode_CancelText", "Cancel");

				FSuppressableWarningDialog ShouldChangeTimecodeAlignmentDialog(Info);
				FSuppressableWarningDialog::EResult UserInput = ShouldChangeTimecodeAlignmentDialog.ShowModal();

				if (UserInput == FSuppressableWarningDialog::EResult::Cancel)
				{
					TimecodeAlignment = PreviousTimecodeAlignment;
					bTimecodeAlignmentChanged = false;
				}
			}

			if (bTimecodeAlignmentChanged)
			{
				const bool bResetRanges = true;
				HandleCaptureDataChanged(bResetRanges); // Need to do the same things as if this were new capture data, eg clear promoted frames, repopulate timeline
			}
		}
	}
}

void UMetaHumanIdentityPose::PostTransacted(const FTransactionObjectEvent& InTransactionEvent)
{
	Super::PostTransacted(InTransactionEvent);

	if (InTransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		const FPermissionListOwners& ChangedProperties = InTransactionEvent.GetChangedProperties();

		if (ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(ThisClass, CaptureData)))
		{
			bIsCaptureDataValid = ((CaptureData != nullptr) && CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

			UFootageCaptureData::PopulateCameraNames(Cast<UFootageCaptureData>(CaptureData), Camera, CameraNames);

			RegisterCaptureDataInternalsChangedDelegate();
			const bool bResetRanges = true;
			HandleCaptureDataChanged(bResetRanges);
		}
		else if (ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(ThisClass, Camera)))
		{
			UFootageCaptureData::PopulateCameraNames(Cast<UFootageCaptureData>(CaptureData), Camera, CameraNames);

			const bool bResetRanges = false;
			HandleCaptureDataChanged(bResetRanges); // Need to do the same things as if this were new capture data, eg clear promoted frames, repopulate timeline
		}
		else if (ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(ThisClass, TimecodeAlignment)))
		{
			const bool bResetRanges = true;
			HandleCaptureDataChanged(bResetRanges); // Need to do the same things as if this were new capture data, eg clear promoted frames, repopulate timeline
		}
		else if (ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(ThisClass, PoseTransform)))
		{
			NotifyPoseTransformChanged();
		}
	}
}

#endif

void UMetaHumanIdentityPose::NotifyCaptureDataChanged(bool bInResetRanges)
{
	if (CaptureData != nullptr)
	{
		TSubclassOf<UMetaHumanIdentityPromotedFrame> NewPromotedFrameClass = nullptr;

		if (CaptureData->IsA<UMeshCaptureData>())
		{
			NewPromotedFrameClass = UMetaHumanIdentityCameraFrame::StaticClass();
		}
		else if (CaptureData->IsA<UFootageCaptureData>())
		{
			NewPromotedFrameClass = UMetaHumanIdentityFootageFrame::StaticClass();
		}

		if (PromotedFrameClass != NewPromotedFrameClass)
		{
			PromotedFrameClass = NewPromotedFrameClass;
		}
	}
	else
	{
		// Clear the Promoted Frame class type to prevent new Promoted Frames from being created without a valid CaptureData present
		PromotedFrameClass = nullptr;
	}

	// Clear any existing promoted frames
	PromotedFrames.Empty();

	OnCaptureDataChangedDelegate.Broadcast(bInResetRanges);
}

void UMetaHumanIdentityPose::NotifyPoseTransformChanged()
{
	if (CaptureDataSceneComponent != nullptr)
	{
		CaptureDataSceneComponent->SetWorldTransform(PoseTransform);
		CaptureDataSceneComponent->UpdateComponentToWorld();
		CaptureDataSceneComponent->TransformUpdated.Broadcast(CaptureDataSceneComponent, EUpdateTransformFlags::None, ETeleportType::None);
	}
}

void UMetaHumanIdentityPose::HandleCaptureDataChanged(bool bInResetRanges)
{
	UpdateCaptureDataSceneComponent();

	NotifyCaptureDataChanged(bInResetRanges);

	UpdateCaptureDataConfigName();
	UpdateRateMatchingDropFrames();
}

void UMetaHumanIdentityPose::HandleCaptureDataSceneComponentTransformChanged()
{
	if (CaptureDataSceneComponent != nullptr)
	{
		PoseTransform = CaptureDataSceneComponent->GetComponentTransform();
	}
}

void UMetaHumanIdentityPose::RegisterCaptureDataInternalsChangedDelegate()
{
	if (CaptureData != nullptr)
	{
		if (!CaptureData->OnCaptureDataInternalsChanged().IsBoundToObject(this))
		{
			const bool bResetRanges = true;
			CaptureData->OnCaptureDataInternalsChanged().AddUObject(this, &UMetaHumanIdentityPose::HandleCaptureDataChanged, bResetRanges);
		}
	}
}

void UMetaHumanIdentityPose::RegisterCaptureDataSceneComponentTransformChanged()
{
	if (CaptureDataSceneComponent != nullptr)
	{
		CaptureDataSceneComponent->TransformUpdated.AddWeakLambda(this, [this](class USceneComponent* InRootComponent, EUpdateTransformFlags, ETeleportType)
			{
				if (InRootComponent == CaptureDataSceneComponent)
				{
					HandleCaptureDataSceneComponentTransformChanged();
				}
			});
	}
}

void UMetaHumanIdentityPose::UpdateCaptureDataConfigName()
{
	check(GetOuter()->IsA(UMetaHumanIdentityFace::StaticClass())); // Not a great way of getting the parent part!

	UMetaHumanIdentityFace* Face = Cast<UMetaHumanIdentityFace>(GetOuter());

	if (Face && Face->DefaultSolver)
	{
		Face->DefaultSolver->GetConfigDisplayName(CaptureData, CaptureDataConfig);
	}
	else
	{
		CaptureDataConfig = TEXT("");
	}
}

void UMetaHumanIdentityPose::UpdateRateMatchingDropFrames()
{
	using namespace UE::MetaHuman;
	using namespace UE::MetaHuman::Private;

	RateMatchingDropFrameRanges = {};

	const UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(CaptureData);

	if (!IsValid(FootageCaptureData) || FootageCaptureData->ImageSequences.IsEmpty() || !FootageCaptureData->ImageSequences[0])
	{
		return;
	}

	const FFrameRate TargetFrameRate = FootageCaptureData->ImageSequences[0]->FrameRateOverride;

	TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos = CreateSequencedImageTrackInfos(FootageCaptureData, TargetFrameRate, TimecodeAlignment);

	if (SequencedImageTrackInfos.IsEmpty())
	{
		return;
	}

	const bool bTracksHaveDifferentFrameRates = TracksHaveDifferentFrameRates(SequencedImageTrackInfos);
	const bool bTracksHaveCompatibleFrameRates = TracksHaveCompatibleFrameRates(SequencedImageTrackInfos);

	if (bTracksHaveDifferentFrameRates && bTracksHaveCompatibleFrameRates)
	{
		const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(TargetFrameRate, MoveTemp(SequencedImageTrackInfos));
		RateMatchingDropFrameRanges = PackIntoFrameRanges(RateMatchingDropFrames);

		UE_LOG(
			LogMetaHumanIdentity,
			Warning,
			TEXT(
				"Detected mismatch in image media frame rates. We need to exclude some frames to make sure everything is paired up correctly, "
				"you will not be able to promote these frames in the identity (%s)"
			),
			*GetPathName()
		);
	}
}

UMetaHumanIdentityPose::ECurrentFrameValid UMetaHumanIdentityPose::GetIsFrameValid(int32 InFrameNumber, const TRange<FFrameNumber>& InProcessingFrameRange, const TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>>& InMediaFrameRanges) const
{
	using namespace UE::MetaHuman;

	if (UFootageCaptureData* FootageData = Cast<UFootageCaptureData>(CaptureData))
	{
		if (!FootageData->ImageSequences.IsEmpty() && FootageData->ImageSequences[0] && !InMediaFrameRanges.IsEmpty())
		{
			if (InFrameNumber >= InProcessingFrameRange.GetLowerBoundValue().Value && InFrameNumber < InProcessingFrameRange.GetUpperBoundValue().Value)
			{
				TArray<FFrameRange> ExcludedFrames = FootageData->CaptureExcludedFrames;
				ExcludedFrames += RateMatchingDropFrameRanges;

				if (!FFrameRange::ContainsFrame(InFrameNumber, ExcludedFrames))
				{
					return ECurrentFrameValid::Valid;
				}
				else
				{
					return ECurrentFrameValid::Invalid_Excluded;
				}
			}
			else
			{
				return ECurrentFrameValid::Invalid_NoRGBOrDepth;
			}
		}
		else
		{
			return ECurrentFrameValid::Invalid_NoFootage;
		}
	}
	else if (UMeshCaptureData* MeshData = Cast<UMeshCaptureData>(CaptureData))
	{
		return ECurrentFrameValid::Valid;
	}

	return ECurrentFrameValid::Invalid_NoCaptureData;
}

TArray<FFrameRange> UMetaHumanIdentityPose::GetRateMatchingExcludedFrameRanges() const
{
	return RateMatchingDropFrameRanges;
}

UMetaHumanIdentityPose::ECurrentFrameValid UMetaHumanIdentityPose::GetIsFrameValid(int32 InFrameNumber) const
{
	if (UFootageCaptureData* FootageData = Cast<UFootageCaptureData>(CaptureData))
	{
		const FFrameRate FrameRate = FootageData->ImageSequences[0]->FrameRateOverride;

		TRange<FFrameNumber> ProcessingFrameRange = TRange<FFrameNumber>(0, 0);
		TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
		TRange<FFrameNumber> MaxFrameRange;
		FootageData->GetFrameRanges(FrameRate, TimecodeAlignment, false, MediaFrameRanges, ProcessingFrameRange, MaxFrameRange);

		return GetIsFrameValid(InFrameNumber, ProcessingFrameRange, MediaFrameRanges);
	}
	else if (UMeshCaptureData* MeshData = Cast<UMeshCaptureData>(CaptureData))
	{
		return ECurrentFrameValid::Valid;
	}

	return ECurrentFrameValid::Invalid_NoCaptureData;
}

#undef LOCTEXT_NAMESPACE
