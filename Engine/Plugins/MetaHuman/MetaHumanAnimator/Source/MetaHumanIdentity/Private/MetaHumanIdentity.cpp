// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanIdentityViewportSettings.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentityStateValidator.h"
#include "ImgMediaSource.h"
#include "PredictiveSolverInterface.h"

#include "Nodes/ImageUtilNodes.h"
#include "Nodes/HyprsenseNode.h"
#include "Nodes/DepthMapDiagnosticsNode.h"
#include "Pipeline/DataTreeTypes.h"
#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanAuthoringObjects.h"
#include "MetaHumanFaceTrackerInterface.h"

#include "Misc/FileHelper.h"
#include "Templates/SubclassOf.h"
#include "Misc/MessageDialog.h"
#include "DNAUtils.h"
#include "Features/IModularFeatures.h"

#include "FramePathResolverSingleFile.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/Engine.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#endif //WITH_EDITOR

#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Cloud/MetaHumanARServiceRequest.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanIdentity)

#define LOCTEXT_NAMESPACE "MetaHumanIdentity"

const TCHAR* UMetaHumanIdentity::IdentityTransactionContext = TEXT("MetaHumanIdentityTransaction");
const FText UMetaHumanIdentity::AutoRigServiceTitleError = LOCTEXT("ARSErrorTitle", "MetaHuman Service Error");
const FText UMetaHumanIdentity::AutoRigServiceTitleSuccess = LOCTEXT("ARSSuccessTitle", "Mesh to MetaHuman");



/////////////////////////////////////////////////////
// UMetaHumanIdentityThumbnailInfo

UMetaHumanIdentityThumbnailInfo::UMetaHumanIdentityThumbnailInfo()
{
	OverridePromotedFrame = 0;
}

/////////////////////////////////////////////////////
// UMetaHumanIdentity

UMetaHumanIdentity::UMetaHumanIdentity()
{
	ViewportSettings = CreateDefaultSubobject<UMetaHumanIdentityViewportSettings>(TEXT("MetaHuman Identity Viewport Settings"));

	bMetaHumanAuthoringObjectsPresent = FMetaHumanAuthoringObjects::ArePresent();
}

void UMetaHumanIdentity::PostLoad()
{
	Super::PostLoad();

	if (UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (!Face->IsConformalRigValid())
		{
			Parts.Remove(Face);
		}
	}

	if (UMetaHumanIdentityBody* Body = FindPartOfClass<UMetaHumanIdentityBody>())
	{
		Parts.Remove(Body);
	}
}

UMetaHumanIdentityPart* UMetaHumanIdentity::GetOrCreatePartOfClass(TSubclassOf<UMetaHumanIdentityPart> InPartClass)
{
	if (UMetaHumanIdentityPart* Part = FindPartOfClass(InPartClass))
	{
		return Part;
	}
	else
	{
		if (UMetaHumanIdentityPart* NewPart = NewObject<UMetaHumanIdentityPart>(this, InPartClass, NAME_None, RF_Transactional))
		{
			NewPart->Initialize();

			Parts.Add(NewPart);

			return NewPart;
		}
	}

	return nullptr;
}

UMetaHumanIdentityPart* UMetaHumanIdentity::FindPartOfClass(TSubclassOf<UMetaHumanIdentityPart> InPartClass) const
{
	const TObjectPtr<UMetaHumanIdentityPart>* FoundPart = Parts.FindByPredicate([InPartClass](UMetaHumanIdentityPart* Part)
	{
		return Part && Part->IsA(InPartClass);
	});

	if (FoundPart != nullptr)
	{
		return *FoundPart;
	}

	return nullptr;
}

bool UMetaHumanIdentity::CanAddPartOfClass(TSubclassOf<UMetaHumanIdentityPart> InPartClass) const
{
	// Only allow distinct parts to be added
	return FindPartOfClass(InPartClass) == nullptr;
}

bool UMetaHumanIdentity::CanAddPoseOfClass(TSubclassOf<UMetaHumanIdentityPose> InPoseClass, EIdentityPoseType InPoseType) const
{
	if (UMetaHumanIdentityFace* FacePart = FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (InPoseType == EIdentityPoseType::Custom)
		{
			// Can always add a custom pose
			return true;
		}

		// We only support adding poses to the Face
		return FacePart->FindPoseByType(InPoseType) == nullptr;
	}

	return false;
}

FPrimaryAssetId UMetaHumanIdentity::GetPrimaryAssetId() const
{
	// Check if we are an asset or a blueprint CDO

	if (FCoreUObjectDelegates::GetPrimaryAssetIdForObject.IsBound() &&
		(IsAsset() || (HasAnyFlags(RF_ClassDefaultObject) && !GetClass()->HasAnyClassFlags(CLASS_Native)))
		)
	{
		// Call global callback if bound
		return FCoreUObjectDelegates::GetPrimaryAssetIdForObject.Execute(this);
	}

	return FPrimaryAssetId(GetClass()->GetFName(), GetFName());
}

#if WITH_EDITOR

EIdentityErrorCode UMetaHumanIdentity::ImportDNA(TSharedPtr<IDNAReader> InDNAReader, const TArray<uint8>& InBrowsBuffer)
{
	if (UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>())
	{
		FString CompatibilityMsg;
		if (!Face->CheckDNACompatible(InDNAReader.Get(), CompatibilityMsg))
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Provided DNA is incompatible with the Face archetype:\n%s"), *CompatibilityMsg);

			return EIdentityErrorCode::InCompatibleDNA;
		}

		const EIdentityErrorCode AppliedDNA = Face->ApplyDNAToRig(InDNAReader);
		if (AppliedDNA != EIdentityErrorCode::None)
		{
			return AppliedDNA;
		}

		Face->SetBrowsBuffer(InBrowsBuffer);

		if (InDNAReader->GetMLControlCount() > 0)
		{
			return EIdentityErrorCode::MLRig;
		}

		return EIdentityErrorCode::None;
	}

	return EIdentityErrorCode::NoPart;
}

EIdentityErrorCode UMetaHumanIdentity::ImportDNAFile(const FString& InDNAFilePath, EDNADataLayer InDnaDataLayer, const FString& InBrowsFilePath)
{
	if (TSharedPtr<IDNAReader> DNAReader = ReadDNAFromFile(*InDNAFilePath, InDnaDataLayer))
	{
		TArray<uint8> BrowsFileContents;

		if (FFileHelper::LoadFileToArray(BrowsFileContents, *InBrowsFilePath))
		{
			UE_LOG(LogMetaHumanIdentity, Display, TEXT("Applying DNA from files '%s' '%s'"), *InDNAFilePath, *InBrowsFilePath);
			return ImportDNA(DNAReader, BrowsFileContents);
		}
	}

	return EIdentityErrorCode::NoDNA;
}

bool UMetaHumanIdentity::ExportDNADataToFiles(const FString& InDnaPathWithName, const FString& InBrowsPathWithName)
{
	if (UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return Face->ExportDNADataToFiles(InDnaPathWithName, InBrowsPathWithName);
	}
	return false;
}

void UMetaHumanIdentity::SendTelemetryForIdentityAutorigRequest(bool bIsFootageData)
{
	/**
	  * @EventName <Editor.MetaHumanPlugin.AutoRig>
	  * @Trigger <the user has requested auto-rigging from MetaHuman service>
	  * @Type <Client>
	  * @EventParam <CaptureDataType> <"footage", "mesh">
	  * @EventParam <IdentityID> <SHA1 hashed GUID of Identity asset, formed as PrimaryAssetType/PrimaryAssetName>
	  * @Comments <->
	  * @Owner <first.last>
	  */

	TArray< FAnalyticsEventAttribute > EventAttributes;

	//Footage or mesh?
	FString FootageOrMesh = bIsFootageData ? "footage" : "mesh";
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CaptureDataType"), FootageOrMesh));

	//IdentityID
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("IdentityID"), GetHashedIdentityAssetID()));

	FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.MetaHumanPlugin.AutoRig"), EventAttributes);
}

FString UMetaHumanIdentity::GetHashedIdentityAssetID()
{
	//We form an asset ID using asset type and the name, which is guaranteed to be unique per project
	//This is the next best thing to a persistent GUID per asset (invariant to name change), which unfortunately
	//cannot be obtained - generating one and storing it in asset user data doesn't guarantee it won't overlap with another asset's GUID
	//Using names, we might get some false negatives if the user renames the asset in between the operations, but we can live with that
	//We also don't want to send asset names directly as they might contain private user data (even though they are hashed on the server
	//side before being used) so we hash the string before sending just to make sure no asset names are sent to the server
	FPrimaryAssetId IdentityAssetID = GetPrimaryAssetId();
	FString IdentityAssetIDStr = IdentityAssetID.PrimaryAssetType.GetName().ToString() / IdentityAssetID.PrimaryAssetName.ToString();
	FSHA1 IdentityIDSha1;
	IdentityIDSha1.UpdateWithString(*IdentityAssetIDStr, IdentityAssetIDStr.Len());
	FSHAHash IdentityIDHash = IdentityIDSha1.Finalize();
	return IdentityIDHash.ToString();
}

#endif //WITH_EDITOR

UCaptureData* UMetaHumanIdentity::GetPoseCaptureData(EIdentityPoseType InPoseType) const
{
	if(UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>())
	{
		for (UMetaHumanIdentityPose* Pose : Face->GetPoses())
		{
			if (Pose->PoseType == InPoseType)
			{
				UCaptureData* CaptureData = Pose->GetCaptureData();
				return CaptureData;
			}
		}
	}
	return nullptr;
}

void UMetaHumanIdentity::StartFrameTrackingPipeline(const TArray<FColor>& InImageData, int32 InWidth, int32 InHeight, const FString& InDepthFramePath, 
	UMetaHumanIdentityPose* InPose, UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bInShowProgress)
{
	UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>();
	verify(Face);

	UMetaHumanFaceContourTrackerAsset* FaceContourTracker = InPromotedFrame->ContourTracker;

	// Depth related tracking diagnostics can only run if the Depth Processing plugin is enabled
	const bool bSkipDiagnostics = Face->bSkipDiagnostics || !IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName());

	// get the calibrations 
	TArray<FCameraCalibration> Calibrations = Face->GetCalibrationsForPoseAndFrame(InPose, InPromotedFrame);

	// get the full camera name; if this is M2MH the camera name is left blank as we only perform diagnostics for F2MH
	FString FullCameraName = FString{};
	if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame))
	{
		FullCameraName = Face->GetFullCameraName(InPose, InPromotedFrame, InPose->Camera);
	}

	if (bBlockingProcessing)
	{
		bool bTrackersLoaded = FaceContourTracker->LoadTrackersSynchronous();

		if (!bTrackersLoaded)
		{
			UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to load trackers"));
		}

		StartPipeline(InImageData, InWidth, InHeight, InDepthFramePath, Calibrations, FullCameraName, InPromotedFrame, bInShowProgress, bSkipDiagnostics);
	}
	else
	{
		FaceContourTracker->LoadTrackers(true, [this, InImageData, InWidth, InHeight, InDepthFramePath, Calibrations, FullCameraName, InPromotedFrame, bInShowProgress, bSkipDiagnostics](bool bTrackersLoaded)
		{
			if (!bTrackersLoaded)
			{
				UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to load trackers"));
			}

			StartPipeline(InImageData, InWidth, InHeight, InDepthFramePath, Calibrations, FullCameraName, InPromotedFrame, bInShowProgress, bSkipDiagnostics);
		});
	}
}

void UMetaHumanIdentity::StartPipeline(const TArray<FColor>& InImageData, int32 InWidth, int32 InHeight, const FString & InDepthFramePath,
	const TArray<FCameraCalibration> & InCalibrations, const FString & InCamera,
	UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bInShowProgress, bool bInSkipDiagnostics)
{
	const TSharedPtr<UE::MetaHuman::Pipeline::FFColorToUEImageNode> UEImage = TrackPipeline.MakeNode<UE::MetaHuman::Pipeline::FFColorToUEImageNode>("RenderTarget");
	const TSharedPtr<UE::MetaHuman::Pipeline::FHyprsenseNode> GenericTracker = TrackPipeline.MakeNode<UE::MetaHuman::Pipeline::FHyprsenseNode>("GenericTracker");

	UEImage->Samples = InImageData;
	UEImage->Width = InWidth;
	UEImage->Height = InHeight;

	UMetaHumanFaceContourTrackerAsset* FaceContourTracker = InPromotedFrame->ContourTracker;

	bool bSetTrackersSuccessfully = GenericTracker->SetTrackers(FaceContourTracker->FullFaceTracker,
		FaceContourTracker->FaceDetector,
		FaceContourTracker->BrowsDenseTracker,
		FaceContourTracker->EyesDenseTracker,
		FaceContourTracker->MouthDenseTracker,
		FaceContourTracker->LipzipDenseTracker,
		FaceContourTracker->NasioLabialsDenseTracker,
		FaceContourTracker->ChinDenseTracker,
		FaceContourTracker->TeethDenseTracker,
		FaceContourTracker->TeethConfidenceTracker);

	if (!bSetTrackersSuccessfully)
	{
		// a standard pipeline 'Failed to start' error will be triggered but we display this information in the log 
		// so that the user can act (for example if a custom tracker asset has not been set up correctly)
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("%s"), *GenericTracker->GetErrorMessage());
	}

	const FString TrackingResultsPinName = GenericTracker->Name + ".Contours Out";
	FString DepthMapDiagnosticsResultsPinName;

	TrackPipeline.MakeConnection(UEImage, GenericTracker);

	// only run diagnostics if the flag is set, there is a depth frame path (ie this is a footage 2 MetaHuman case)
	// and depth processing plugin is enabled
	if (!bInSkipDiagnostics && !InDepthFramePath.IsEmpty())
	{
		TSharedPtr<UE::MetaHuman::Pipeline::FDepthLoadNode> Depth = TrackPipeline.MakeNode<UE::MetaHuman::Pipeline::FDepthLoadNode>("LoadDepth");
		Depth->FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolverSingleFile>(InDepthFramePath);

		TSharedPtr<UE::MetaHuman::Pipeline::FDepthMapDiagnosticsNode> DepthMapDiagnostics = TrackPipeline.MakeNode<UE::MetaHuman::Pipeline::FDepthMapDiagnosticsNode>("DepthMapDiagnostics");
		DepthMapDiagnostics->Calibrations = InCalibrations;
		DepthMapDiagnostics->Camera = InCamera;
		DepthMapDiagnosticsResultsPinName = DepthMapDiagnostics->Name + ".DepthMap Diagnostics Out";
		TrackPipeline.MakeConnection(GenericTracker, DepthMapDiagnostics);
		TrackPipeline.MakeConnection(Depth, DepthMapDiagnostics);
	}

	UE::MetaHuman::Pipeline::FFrameComplete OnFrameComplete;
	UE::MetaHuman::Pipeline::FProcessComplete OnProcessComplete;

	OnFrameComplete.AddLambda([this, InPromotedFrame, TrackingResultsPinName, bInSkipDiagnostics, InDepthFramePath, DepthMapDiagnosticsResultsPinName](const TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
	{
		const FFrameTrackingContourData& TrackedData = InPipelineData->GetData<FFrameTrackingContourData>(TrackingResultsPinName);
		InPromotedFrame->UpdateContourDataFromFrameTrackingContours(TrackedData, true);

		if (!bInSkipDiagnostics && !InDepthFramePath.IsEmpty())
		{
			TMap<FString, FDepthMapDiagnosticsResult> CurDepthMapDiagnosticsResult(InPipelineData->MoveData<TMap<FString, FDepthMapDiagnosticsResult>>(DepthMapDiagnosticsResultsPinName));
			check(CurDepthMapDiagnosticsResult.Num() == 1); // currently only supporting a single depthmap so should only be one result per frame
			InPromotedFrame->DepthMapDiagnostics = CurDepthMapDiagnosticsResult.begin()->Value;
		}
	});

	OnProcessComplete.AddLambda([this, bInSkipDiagnostics, InDepthFramePath, InPromotedFrame](TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
	{

		if (InPipelineData->GetExitStatus() != UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok)
		{
			UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Tracking process failed"));
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PromotedFrameProcessingFailed", "The promoted frame processing failed."), LOCTEXT("PromotedFrameProcessingFailedTitle", "Promoted frame processing failed"));
		}
		else if (!bInSkipDiagnostics && !InDepthFramePath.IsEmpty()) 
		{
			UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>();
			FText DiagnosticsWarningMessage;
			if (InPromotedFrame->DiagnosticsIndicatesProcessingIssue(Face->MinimumDepthMapFaceCoverage, Face->MinimumDepthMapFaceWidth,  DiagnosticsWarningMessage))
			{
				FMessageDialog::Open(EAppMsgType::Ok, DiagnosticsWarningMessage, LOCTEXT("IdentityContourTrackingDiagnosticsWarningTitle", "Frame Contour Tracking Diagnostics Warning"));
				UE_LOG(LogMetaHumanIdentity, Warning, TEXT("The frame contour tracking diagnostics check found a potential issue with the data: %s"), *DiagnosticsWarningMessage.ToString());
			}
		}

		TrackPipeline.Reset();
	});

	UE::MetaHuman::Pipeline::FPipelineRunParameters PipelineRunParameters;
	PipelineRunParameters.SetStartFrame(0);
	PipelineRunParameters.SetEndFrame(1);
	PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
	PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
	PipelineRunParameters.SetGpuToUse(UE::MetaHuman::Pipeline::FPipeline::PickPhysicalDevice());
	if (bBlockingProcessing)
	{
		PipelineRunParameters.SetMode(UE::MetaHuman::Pipeline::EPipelineMode::PushSyncNodes);
	}

	TrackPipeline.Run(PipelineRunParameters);
}

void UMetaHumanIdentity::SetBlockingProcessing(bool bInBlockingProcessing)
{
	bBlockingProcessing = bInBlockingProcessing;
}

bool UMetaHumanIdentity::IsFrameTrackingPipelineProcessing() const
{
	return TrackPipeline.IsRunning();
}

void UMetaHumanIdentity::LogInToAutoRigService()
{
}

bool UMetaHumanIdentity::IsLoggedInToService()
{
	return true;
}

bool UMetaHumanIdentity::IsAutoRiggingInProgress()
{
	return bIsAutorigging;
}

void UMetaHumanIdentity::CreateDNAForIdentity(bool bInLogOnly)
{
#if WITH_EDITOR
	UE::MetaHuman::FTargetSolveParameters Params;
	// Note no teeth mesh available
	if (IdentityIsReadyForAutoRig(Params.ConformedFaceVertices, 
		Params.ConformedLeftEyeVertices, 
		Params.ConformedRightEyeVertices, 
		bInLogOnly))
	{
		bIsAutorigging = true;

		// Analytics are only available in the editor at the moment

		if (GEngine->AreEditorAnalyticsEnabled() && FEngineAnalytics::IsAvailable())
		{
			UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>();
			UCaptureData* CaptureData = Face->FindPoseByType(EIdentityPoseType::Neutral)->GetCaptureData();
			bool bIsFootageData = CaptureData->IsA<UFootageCaptureData>();
			SendTelemetryForIdentityAutorigRequest(bIsFootageData);
		}

		Params.ModelIdentifier = GetName();
		Params.ExportLayers = UE::MetaHuman::EExportLayers::Rbf;
		Params.HighFrequency = -1;

		TSharedRef<UE::MetaHuman::FAutoRigServiceRequest> Request = 
			UE::MetaHuman::FAutoRigServiceRequest::CreateRequest(Params);

		Request->OnMetaHumanServiceRequestBeginDelegate.BindLambda([this]()
		{
			// Notify the user the Mesh To MetaHuman task has started
			FNotificationInfo Info(LOCTEXT("AutoRigProgressText", "Waiting for MetaHuman backend..."));
			Info.bFireAndForget = false;

			AutoRigProgressNotification = FSlateNotificationManager::Get().AddNotification(Info);
			if (AutoRigProgressNotification.IsValid())
			{
				AutoRigProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
			}
		});

		Request->OnMetaHumanServiceRequestFailedDelegate.BindLambda([this, bInLogOnly](EMetaHumanServiceRequestResult InRequestResult)
		{
			HandleAutoRigServiceError(InRequestResult, bInLogOnly);

			bIsAutorigging = false;
			AutoRigProgressEnd(false);
			OnAutoRigServiceFinishedDynamicDelegate.Broadcast(false);
			OnAutoRigServiceFinishedDelegate.Broadcast(false);
		});

		Request->AutorigRequestCompleteDelegate.BindLambda([this, bInLogOnly](const UE::MetaHuman::FAutorigResponse& InResponse)
		{
			if (!InResponse.IsValid())
			{
				return;
			}

			UE_LOG(LogMetaHumanIdentity, Display, TEXT("Autorigging service executed"));

			// The DNA cannot be applied after we get the result from the AR service on non-editor builds because 
			// it depends on USkelMeshDNAUtils which are only available in the editor
			bool bServiceSuccess = false;
			if (InResponse.Dna != nullptr)
			{
				UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>();
				const EIdentityErrorCode AppliedDNA = Face->ApplyCombinedDNAToRig(InResponse.Dna);
				UMetaHumanIdentity::HandleError(AppliedDNA);
				bServiceSuccess = AppliedDNA == EIdentityErrorCode::None;
			}

			AutoRigSolveFinished(bServiceSuccess, bInLogOnly);

			bIsAutorigging = false;
			AutoRigProgressEnd(true);

			OnAutoRigServiceFinishedDynamicDelegate.Broadcast(bServiceSuccess);
			OnAutoRigServiceFinishedDelegate.Broadcast(bServiceSuccess);
		});

		Request->RequestSolveAsync();
	}
#endif // WITH_EDITOR
}

void UMetaHumanIdentity::AutoRigProgressEnd(bool bSuccess) const
{
#if WITH_EDITOR
	if (AutoRigProgressNotification.IsValid())
	{
		if (bSuccess)
		{
			AutoRigProgressNotification.Pin()->SetText(LOCTEXT("AutoRigProgressComplete", "Mesh to MetaHuman complete!"));
		}
		else
		{
			AutoRigProgressNotification.Pin()->SetText(LOCTEXT("AutoRigProgressFailed", "Mesh to MetaHuman failed!"));
		}
		AutoRigProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
		AutoRigProgressNotification.Pin()->ExpireAndFadeout();
	}
#endif // WITH_EDITOR
}

bool UMetaHumanIdentity::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	bool bDiagnosticsIndicatesProcessingIssue = false;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		FText CurDiagnosticsWarningMessage;
		bool bCurDiagnosticsIndicatesProcessingIssue = Parts[Index]->DiagnosticsIndicatesProcessingIssue(CurDiagnosticsWarningMessage);
		if (bCurDiagnosticsIndicatesProcessingIssue)
		{
			bDiagnosticsIndicatesProcessingIssue = true;
			OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n\n") + CurDiagnosticsWarningMessage.ToString());
		}
	}
	return bDiagnosticsIndicatesProcessingIssue;
}

void UMetaHumanIdentity::AutoRigSolveFinished(bool bSuccess, bool bInLogOnly)
{
	FText DiagnosticsWarningMessage;
	bool bDiagnosticsWarning = false;
	if (bSuccess) 
	{
		bDiagnosticsWarning = DiagnosticsIndicatesProcessingIssue(DiagnosticsWarningMessage);
		if (bDiagnosticsWarning)
		{
			UE_LOG(LogMetaHumanIdentity, Warning, TEXT("The Identity creation diagnostics check found a potential issue with the data: %s"), *DiagnosticsWarningMessage.ToString());
		}
	}

	if (bSuccess && !bInLogOnly)
	{
		FText AutoRigResponse = LOCTEXT("SkeletalMeshAvailableNotification", "Skeletal Mesh with an embedded MetaHuman DNA is now available in your Content Browser.");

		if (bDiagnosticsWarning)
		{
			AutoRigResponse = FText::FromString(AutoRigResponse.ToString() + TEXT("\n\n") + DiagnosticsWarningMessage.ToString());
		}

		FMessageDialog::Open(EAppMsgType::Ok, AutoRigResponse, UMetaHumanIdentity::AutoRigServiceTitleSuccess);
	}
	else if (!bSuccess)
	{
		FText ErrorText = LOCTEXT("ARSInvalidInputData", "Error while trying to process data obtained from MetaHuman service");
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Autorigging service parse bytes error: '%s'"), *ErrorText.ToString());

		if (bInLogOnly)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorText, UMetaHumanIdentity::AutoRigServiceTitleError);
		}
	}
}

bool UMetaHumanIdentity::IdentityIsReadyForAutoRig(TArray<FVector>& OutConformedFaceVertices, 
												   TArray<FVector>& OutConformedLeftEyeVertices, 
												   TArray<FVector>& OutConformedRightEyeVertices, 
												   bool bInLogOnly)
{
	EAutoRigIdentityValidationError ErrorCode = EAutoRigIdentityValidationError::None;

	if (UMetaHumanIdentityFace* Face = FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->bIsConformed)
		{
			Face->GetConformalVerticesForAutoRigging(OutConformedFaceVertices, OutConformedLeftEyeVertices, OutConformedRightEyeVertices);

			if (OutConformedFaceVertices.IsEmpty())
			{
				ErrorCode = EAutoRigIdentityValidationError::EmptyConformalMesh;
			}
		}
		else
		{
			ErrorCode = EAutoRigIdentityValidationError::MeshNotConformed;
		}
	}
	else
	{
		ErrorCode = EAutoRigIdentityValidationError::NoFacePart;
	}

	HandleIdentityForAutoRigValidation(ErrorCode, bInLogOnly);
	return ErrorCode == EAutoRigIdentityValidationError::None;
}

bool UMetaHumanIdentity::HandleError(EIdentityErrorCode InErrorCode, bool bInLogOnly)
{
	FText Message;

	switch (InErrorCode)
	{
	case EIdentityErrorCode::None:
		break;

	case EIdentityErrorCode::CreateRigFromDNA:
		Message = LOCTEXT("DuplicateError", "Failed to create a template skeletal mesh");
		break;

	case EIdentityErrorCode::LoadBrows:
		Message = LOCTEXT("LoadBrowsError", "Failed to load the brows data");
		break;

	case EIdentityErrorCode::NoDNA:
		Message = LOCTEXT("NoDNAError", "MetaHuman Identity rig has no DNA asset!");
		break;

	case EIdentityErrorCode::NoTemplate:
		Message = LOCTEXT("NoTemplateError", "Failed to load template rig");
		break;

	case EIdentityErrorCode::CreateDebugFolder:
		Message = LOCTEXT("CreateDebugFolderError", "Failed to create folder to save debugging data during mesh fitting");
		return false;

	case EIdentityErrorCode::CalculatePCAModel:
		Message = LOCTEXT("PcaModelFromDnaRigError", "Failed to calculate solver model");
		break;

	case EIdentityErrorCode::Initialization:
		Message = LOCTEXT("InitializationError", "Initialization error");
		break;

	case EIdentityErrorCode::CameraParameters:
		Message = LOCTEXT("CameraParametersError", "Failed to set camera parameters");
		break;

	case EIdentityErrorCode::ScanInput:
		Message = LOCTEXT("ScanInputError", "Failed to set scan input data");
		break;

	case EIdentityErrorCode::DepthInput:
		Message = LOCTEXT("DepthInputError", "Failed to set depth input data");
		break;

	case EIdentityErrorCode::TeethSource:
		Message = LOCTEXT("TeethSourceError", "Failed to update teeth source");
		break;

	case EIdentityErrorCode::FitRigid:
		Message = LOCTEXT("FitRigidError", "Rigid MetaHuman Identity fit failed");
		break;

	case EIdentityErrorCode::FitPCA:
		Message = LOCTEXT("FitPCAError", "Non-rigid MetaHuman Identity fit failed");
		break;

	case EIdentityErrorCode::FitTeethFailed:
		Message = LOCTEXT("FitTeethError", "Teeth fitting failed: please check that depth map data is complete in the teeth region and MetaHuman Identity teeth marker curves are correct");
		break;

	case EIdentityErrorCode::TeethDepthDelta:
		Message = LOCTEXT("TeethDepthDeltaError", "Failed to calculate teeth depth delta");
		break;

	case EIdentityErrorCode::UpdateRigWithTeeth:
		Message = LOCTEXT("UpdateRigWithTeethMeshVerticesError", "Failed to update rig with teeth mesh");
		break;

	case EIdentityErrorCode::InvalidDNA:
		Message = LOCTEXT("InvalidDNAError", "Cannot apply invalid DNA and delta DNA to MetaHuman Identity rig!");
		break;

	case EIdentityErrorCode::ApplyDeltaDNA:
		Message = LOCTEXT("ApplyDeltaDNAError", "Cannot apply delta DNA");
		break;

	case EIdentityErrorCode::RefineTeeth:
		Message = LOCTEXT("RefineTeethError", "Failed to refine teeth position");
		break;

	case EIdentityErrorCode::ApplyScaleToDNA:
		Message = LOCTEXT("ApplyScaleToDNAError", "Cannot apply scale to DNA");
		break;

	case EIdentityErrorCode::NoPart:
		Message = LOCTEXT("NoPartError", "No part");
		break;

	case EIdentityErrorCode::InCompatibleDNA:
		Message = LOCTEXT("IncompatibleDNAError", "Incompatible DNA");
		break;

	case EIdentityErrorCode::CaptureDataInvalid:
		Message = LOCTEXT("CaptureDataInvalidError", "CaptureData for Pose is not valid");
		break;

	case EIdentityErrorCode::SolveFailed:
		Message = LOCTEXT("SolveIdentityError", "Failed to solve MetaHuman Identity: please check the depth map data is complete and the MetaHuman Identity marker curves are correct");
		break;

	case EIdentityErrorCode::FitEyesFailed:
		Message = LOCTEXT("FitEyesError", "Eye fitting failed: please check that depth map data is complete in the eye region and MetaHuman Identity eye marker curves are correct");
		break;

	case EIdentityErrorCode::BrowsFailed:
		Message = LOCTEXT("BrowsError", "Failed to generate brow location information for the neutral frame marked as frontal");
		break;

	case EIdentityErrorCode::NoPose:
		Message = LOCTEXT("NeutralError", "Neutral Pose was not found for MetaHuman Identity Face");
		break;

	case EIdentityErrorCode::BadInputMeshTopology:
		Message = LOCTEXT("BadInputMeshTopology", "Failed to conform input mesh to MetaHuman topology."
			"\nPlease make sure the input mesh has common vertices for adjacent triangles merged.");
		break;

	default:
		Message = LOCTEXT("UnknownError", "Unknown error");
		break;
	};

	if (InErrorCode != EIdentityErrorCode::None)
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("%s"), *Message.ToString());

		if (!bInLogOnly)
		{
			const FText ErrorDialogTitle = LOCTEXT("IdentityError", "MetaHuman Identity Error");
			FMessageDialog::Open(EAppMsgType::Ok, Message, ErrorDialogTitle);
		}

		return false;
	}

	return true;
}

void UMetaHumanIdentity::HandleIdentityForAutoRigValidation(EAutoRigIdentityValidationError InErrorCode, bool bInLogOnly)
{
	FText Message;

	switch (InErrorCode)
	{
	case EAutoRigIdentityValidationError::None:
		break;

	case EAutoRigIdentityValidationError::BodyNotSelected:
		Message = LOCTEXT("ARNoBody", "Mesh to MetaHuman requires the addition of a Body Part, and a Body Type Preset selection.");
		break;

	case EAutoRigIdentityValidationError::BodyIndexInvalid:
		Message = LOCTEXT("ARNoBodyType", "No Body Type Preset is selected in the Body Part. Please select a Body Type Preset to continue.");
		break;

	case EAutoRigIdentityValidationError::MeshNotConformed:
		Message = FText::Format(LOCTEXT("MeshNotConformed", "Error submitting to autorig. Face mesh was not conformed in the MetaHuman Identity {0}"), FText::FromString(GetName()));
		break;

	case EAutoRigIdentityValidationError::EmptyConformalMesh:
		Message = FText::Format(LOCTEXT("EmptyConformalMesh", "Error submitting to autorig. ConformalMesh has no vertices to submit in the MetaHuman Identity %s"), FText::FromString(GetName()));
		break;

	case EAutoRigIdentityValidationError::NoFacePart:
		Message = FText::Format(LOCTEXT("NoFacePart", "Error submitting to autorig. Face Part not found in the MetaHuman Identity %s"), FText::FromString(GetName()));
		break;

	default:
		Message = LOCTEXT("UnknownError", "Unknown error");
		break;
	}

	if (InErrorCode != EAutoRigIdentityValidationError::None)
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("%s"), *Message.ToString());

		if (!bInLogOnly)
		{
			FMessageDialog::Open(EAppMsgType::Ok, Message, UMetaHumanIdentity::AutoRigServiceTitleError);
		}
	}
}

#if WITH_EDITOR
void UMetaHumanIdentity::HandleAutoRigServiceError(EMetaHumanServiceRequestResult InServiceError, bool bInLogOnly)
{
	FText ErrorText;
	switch (InServiceError)
	{
		case EMetaHumanServiceRequestResult::Busy:
			ErrorText = LOCTEXT("ARSBusy", "The MetaHuman Service is busy, try again later");
			break;
		case EMetaHumanServiceRequestResult::Unauthorized:
			ErrorText = LOCTEXT("ARSUnauthorized", "You are not authorized to use the Mesh to MetaHuman Service");
			break;
		case EMetaHumanServiceRequestResult::EulaNotAccepted:
			ErrorText = LOCTEXT("ARSEulaNotAccepted", "MetaHuman EULA was not accepted");
			break;
		case EMetaHumanServiceRequestResult::InvalidArguments:
			ErrorText = LOCTEXT("ARSInvalidArguments", "MetaHuman Service invoked with invalid arguments");
			break;
		case EMetaHumanServiceRequestResult::ServerError:
			ErrorText = LOCTEXT("ARSServerError", "Error while interacting with the MetaHuman Service");
			break;
		case EMetaHumanServiceRequestResult::LoginFailed:
			ErrorText = LOCTEXT("ARSServerLoginError", "Unable to log in successfully");
			break;
		case EMetaHumanServiceRequestResult::Timeout:
			ErrorText = LOCTEXT("ARSServerTimeoutError", "Timeout on the Request to the MetaHuman Service");
			break;
		case EMetaHumanServiceRequestResult::GatewayError:
			ErrorText = LOCTEXT("ARSServerGatewayError", "Gateway error when interacting with MetaHuman service");
			break;
		default:
			ErrorText = LOCTEXT("ARSUnknownError", "Unknown error while interacting with the MetaHuman Service");
			break;
	}
	UE_LOG(LogMetaHumanIdentity, Error, TEXT("Autorigging service returned an error: '%s'"), *ErrorText.ToString());
	if (!bInLogOnly)
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorText, UMetaHumanIdentity::AutoRigServiceTitleError);
	}
}
#endif

bool UMetaHumanIdentity::GetMetaHumanAuthoringObjectsPresent() const
{
	return bMetaHumanAuthoringObjectsPresent;
}

#undef LOCTEXT_NAMESPACE
