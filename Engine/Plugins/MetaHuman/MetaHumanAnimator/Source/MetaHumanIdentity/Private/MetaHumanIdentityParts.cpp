// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentityStyle.h"
#include "MetaHumanTemplateMeshComponent.h"
#include "MetaHumanConfig.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityCustomVersion.h"
#include "MetaHumanIdentityPose.h"
#include "DNAUtilities.h"
#include "CaptureData.h"
#include "TrackingPathUtils.h"

#include "MetaHumanFaceFittingSolver.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "MetaHumanConformer.h"
#include "MetaHumanFaceTrackerInterface.h"

#include "DNAReader.h"
#include "DNAAsset.h"
#include "DNAUtils.h"
#include "SkelMeshDNAUtils.h"
#include "DNAToSkelMeshMap.h"
#include "SkelMeshDNAReader.h"

#include "Algo/AllOf.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/MeshNormals.h"
#include "MeshDescription.h"
#include "DynamicMeshToMeshDescription.h"
#include "StaticMeshAttributes.h"
#include "DynamicMesh/MeshTransforms.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/TransactionObjectEvent.h"
#include "OpenCVHelperLocal.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/GCObjectScopeGuard.h"
#include "AssetExportTask.h"
#include "Materials/Material.h"
#include "Styling/AppStyle.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImgMediaSource.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageSequenceUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Features/IModularFeatures.h"
#include "AnimationRuntime.h"

#if WITH_EDITOR
#include "ControlRigBlueprint.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "LODUtilities.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "Exporters/StaticMeshExporterOBJ.h"
#include "ObjectTools.h"
#include "Dialogs/Dialogs.h"
#endif

#include "SkeletalMeshAttributes.h"

#include "FramePathResolver.h"

#include "DNAInterchangeModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanIdentityParts)

#define LOCTEXT_NAMESPACE "MetaHumanIdentityParts"

namespace
{
	TAutoConsoleVariable<bool> CVarEnableExportMeshes{
		TEXT("mh.Identity.ExportMeshes"),
		false,
		TEXT("Enables exporting MetaHuman Identity meshes as OBJs and other debugging data"),
		ECVF_Default
	};

	TAutoConsoleVariable<bool> CVarTrainPreviewSolvers{
		TEXT("mh.Identity.TrainPreviewSolvers"),
		true,
		TEXT("If set to true, Preparing for Performance will train the full preview solvers, otherwise only the teeth solver will be trained"),
		ECVF_Default
	};

	static constexpr TCHAR DepthSuffix[] = TEXT("Depth");

	// Code copied from the Engine\Plugins\MetaHuman\MetaHumanCharacter\Source\MetaHumanCharacterEditor\Private\MetaHumanCharacterSkelMeshUtils.cpp
	// FSkelMeshDNAUtils::UpdateJoints gives slightly wrong result
	static void UpdateJoints(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap)
	{
		{	// Scoping of RefSkelModifier
			FReferenceSkeletonModifier RefSkelModifier(InSkelMesh->GetRefSkeleton(), InSkelMesh->GetSkeleton());

			// copy here
			TArray<FTransform> RawBonePose = InSkelMesh->GetRefSkeleton().GetRawRefBonePose();

			// calculate component space ahead of current transform
			TArray<FTransform> ComponentTransforms;
			FAnimationRuntime::FillUpComponentSpaceTransforms(InSkelMesh->GetRefSkeleton(), RawBonePose, ComponentTransforms);

			const TArray<FMeshBoneInfo>& RawBoneInfo = InSkelMesh->GetRefSkeleton().GetRawRefBoneInfo();

			// Skipping root joint (index 0) to avoid blinking of the mesh due to bounding box issue
			for (uint16 JointIndex = 0; JointIndex < InDNAReader->GetJointCount(); JointIndex++)
			{
				int32 BoneIndex = InDNAToSkelMeshMap->GetUEBoneIndex(JointIndex);

				FTransform DNATransform = FTransform::Identity;

				// Updating bind pose affects just translations.
				FVector Translate = InDNAReader->GetNeutralJointTranslation(JointIndex);
				FVector RotationVector = InDNAReader->GetNeutralJointRotation(JointIndex);
				FRotator Rotation(RotationVector.X, RotationVector.Y, RotationVector.Z);

				if (InDNAReader->GetJointParentIndex(JointIndex) == JointIndex) // This is the highest joint of the dna - not necessarily the UE root bone  
				{
					FQuat YUpToZUpRotation = FQuat(FRotator(0, 0, 90));
					FQuat ComponentRotation = YUpToZUpRotation * FQuat(Rotation);

					DNATransform.SetTranslation(FVector(Translate.X, Translate.Z, -Translate.Y));
					DNATransform.SetRotation(ComponentRotation);

					ComponentTransforms[BoneIndex] = DNATransform;
				}
				else
				{
					DNATransform.SetTranslation(Translate);
					DNATransform.SetRotation(Rotation.Quaternion());

					if (ensure(RawBoneInfo[BoneIndex].ParentIndex != INDEX_NONE))
					{
						ComponentTransforms[BoneIndex] = DNATransform * ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex];
					}
				}

				ComponentTransforms[BoneIndex].NormalizeRotation();
			}

			for (uint16 BoneIndex = 0; BoneIndex < RawBoneInfo.Num(); BoneIndex++)
			{
				FTransform LocalTransform;

				if (BoneIndex == 0)
				{
					LocalTransform = ComponentTransforms[BoneIndex];
				}
				else
				{
					LocalTransform = ComponentTransforms[BoneIndex].GetRelativeTransform(ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex]);
				}

				LocalTransform.NormalizeRotation();

				RefSkelModifier.UpdateRefPoseTransform(BoneIndex, LocalTransform);
			}
		}

		InSkelMesh->GetRefBasesInvMatrix().Reset();
		InSkelMesh->CalculateInvRefMatrices(); // Needs to be called after RefSkelModifier is destroyed
	}
}


//////////////////////////////////////////////////////////////////////////
// UMetaHumanIdentityFace

const TArray<FString> UMetaHumanIdentityFace::CurveNamesForEyeFitting
{
	TEXT("crv_iris_r"),
	TEXT("crv_iris_l"),
	TEXT("crv_eyelid_lower_l"),
	TEXT("crv_eyelid_lower_r"),
	TEXT("crv_eyelid_upper_l"),
	TEXT("crv_eyelid_upper_r"),
};

UMetaHumanIdentityFace::UMetaHumanIdentityFace()
	: Super{}
	, bIsConformed{ false }
	, bHasFittedEyes{ false }
{
	// Even though this is deprecated the object still needs to be created so older Identities that have been conformed can be loaded
	// The data stored in this component will be transferred to the new Template Mesh Component on PostLoad
	ConformalMeshComponent_DEPRECATED = CreateDefaultSubobject<UMetaHumanTemplateMesh>(TEXT("Conformal Mesh Component"));

	TemplateMeshComponent = CreateDefaultSubobject<UMetaHumanTemplateMeshComponent>(TEXT("Template Mesh Component"));
	SetTemplateMeshTransform(GetTemplateMeshInitialTransform());

	RigComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Rig Component"));

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		LoadDefaultFaceFittingSolvers();
	}
}

FText UMetaHumanIdentityFace::GetPartName() const
{
	return LOCTEXT("IdentityFacePartName", "Face");
}

FText UMetaHumanIdentityFace::GetPartDescription() const
{
	return LOCTEXT("IdentityFacePartDescription", "The Face Part of the MetaHuman Identity. This creates a new static mesh asset representing the Template Mesh");
}

FSlateIcon UMetaHumanIdentityFace::GetPartIcon(const FName& InPropertyName) const
{
	FMetaHumanIdentityStyle& Style = FMetaHumanIdentityStyle::Get();
	const FName& StyleSetName = Style.GetStyleSetName();

	if (!InPropertyName.IsNone())
	{
		if (InPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityFace, TemplateMeshComponent))
		{
			return FSlateIcon{ StyleSetName, "Identity.Face.ConformalMesh" };
		}
		else if (InPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityFace, RigComponent))
		{
			return FSlateIcon{ StyleSetName, "Identity.Face.Rig" };
		}
	}

	return FSlateIcon{ StyleSetName, "Identity.Face.Part" };
}

FText UMetaHumanIdentityFace::GetPartTooltip(const FName& InPropertyName) const
{
	if (!InPropertyName.IsNone())
	{
		if (InPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityFace, TemplateMeshComponent))
		{
			return LOCTEXT( "IdentityTreeTemplateMeshTooltip", "Template Mesh Component of Face Part\nA head mesh template, on MetaHuman topology.\nConformed to the Capture Data from Neutral Pose\nusing MetaHuman Identity Solve command.");
		}
		else if (InPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityFace, RigComponent))
		{
			return LOCTEXT("IdentityTreeSkeletalMeshTooltip", "Skeletal Mesh Component of Face Part\nA Skeletal Mesh of the head, on MetaHuman topology.\nFitted to the Template Mesh through MetaHuman Service using Mesh to MetaHuman command.\nCan be used to solve animation in Performance asset.\nIt can also be further edited in MetaHuman Creator and downloaded as a full MetaHuman\nNOTE: Downloaded MetaHuman should not be used for solving animation.");
		}
	}

	return LOCTEXT("IdentityTreePartTooltip", "Face Part of Identity\nClick on sub-nodes to inspect different components\nand select them in the Viewport");
}

bool UMetaHumanIdentityFace::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	bool bDiagnosticsIndicatesProcessingIssue = false;
	if (!bSkipDiagnostics)
	{
		if (DNAScale_DEPRECATED > (1.0f + MaximumScaleDifferenceFromAverage / 100.0f) || DNAScale_DEPRECATED < (1.0f - MaximumScaleDifferenceFromAverage / 100.0f))
		{
			OutDiagnosticsWarningMessage = FText::Format(LOCTEXT("IdentityFaceProcessingDiagnosticsWarning1", "Identity face scale is {0}x that of an average MetaHuman, which may indicate an issue with the input data."), DNAScale_DEPRECATED);

			if (IPluginManager::Get().FindEnabledPlugin("MetaHumanDepthProcessing").IsValid())
			{
				bool bUsesFootage = false;
				for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
				{
					if (Poses[PoseIndex]->GetCaptureData()->IsA<UFootageCaptureData>())
					{
						bUsesFootage = true;
						break;
					}
				}
				if (bUsesFootage)
				{
					OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n")
						+ LOCTEXT("IdentityFaceProcessingDiagnosticsWarning2",
							"Please check your footage camera calibrations are correct and that calibration distances are expressed in cm.").ToString());
				}
				else
				{
					OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n")
						+ FText::Format(LOCTEXT("IdentityFaceProcessingDiagnosticsWarning3",
							"Please check that the input face mesh uses units of cm and is less than {0}% away from the scale of an average MetaHuman face."),
							MaximumScaleDifferenceFromAverage).ToString());
				}
			}

			bDiagnosticsIndicatesProcessingIssue = true;
		}
	}
	return bDiagnosticsIndicatesProcessingIssue;
}

void UMetaHumanIdentityFace::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR

	if (TemplateMeshComponent != nullptr && TemplateMeshComponent->HeadMeshComponent->GetDynamicMesh()->IsEmpty())
	{
		TemplateMeshComponent->LoadMeshAssets();
	}

	if (ConformalMeshComponent_DEPRECATED != nullptr && ConformalMeshComponent_DEPRECATED->GetDynamicMesh() != nullptr && !ConformalMeshComponent_DEPRECATED->GetDynamicMesh()->IsEmpty())
	{
		// ConformalMeshComponent_DEPRECATED contains data and this Identity has already been conformed it means it was created before the new Template Mesh Component existed
		// so we transfer the mesh from the old conformal mesh component to the new Template Mesh Component
		// Both neutral and teeth poses are initialized with the existing data as well as the eyes
		// Also, fit teeth if possible so we can obtain the show teeth mesh and store it in the new template mesh component

		if (bIsConformed)
		{
			// Copy the deprecated mesh into both poses of the new template mesh component
			TemplateMeshComponent->PoseHeadMeshes[EIdentityPoseType::Neutral]->SetMesh(ConformalMeshComponent_DEPRECATED->GetDynamicMesh()->GetMeshRef());
			TemplateMeshComponent->PoseHeadMeshes[EIdentityPoseType::Teeth]->SetMesh(ConformalMeshComponent_DEPRECATED->GetDynamicMesh()->GetMeshRef());
			TemplateMeshComponent->HeadMeshComponent->GetDynamicMesh()->SetMesh(TemplateMeshComponent->GetPoseHeadMesh(EIdentityPoseType::Neutral)->GetMeshRef());

			// Convert the Left and Right eye meshes so we can set it in the template mesh component
			auto ConvertToFVector3f = [](const FVector& InVector)
			{
				return FVector3f(InVector);
			};

			TArray<FVector3f> LeftEyeMesh;
			TArray<FVector3f> RightEyeMesh;
			LeftEyeMesh.Reserve(ConformalVertsLeftEyeRigSpace_DEPRECATED.Num());
			RightEyeMesh.Reset(ConformalVertsRightEyeRigSpace_DEPRECATED.Num());
			Algo::Transform(ConformalVertsLeftEyeRigSpace_DEPRECATED, LeftEyeMesh, ConvertToFVector3f);
			Algo::Transform(ConformalVertsRightEyeRigSpace_DEPRECATED, RightEyeMesh, ConvertToFVector3f);

			TemplateMeshComponent->SetEyeMeshesVertices(LeftEyeMesh, RightEyeMesh, ETemplateVertexConversion::None);

			if (CanFitTeeth())
			{
				// For an identity that has already been conformed, bake UEToRigSpaceTransform in the teeth
				// meshes so we can position the original teeth mesh correctly. If we don't do this
				// the original teeth mesh will be placed upside down.
				TemplateMeshComponent->BakeTeethMeshTransform(UMetaHumanTemplateMeshComponent::UEToRigSpaceTransform);

				// Run teeth fitting again if we can to get the fitted teeth mesh and store it in the template mesh component
				const EIdentityErrorCode FittedTeeth = FitTeeth();
				if (FittedTeeth != EIdentityErrorCode::None)
				{
					UMetaHumanIdentity::HandleError(FittedTeeth);
					return;
				}
			}

			// Use the neutral pose as the default pose of the template mesh
			TemplateMeshComponent->ShowHeadMeshForPose(EIdentityPoseType::Neutral);

			// Finally empty the left and right eye meshes arrays
			ConformalVertsLeftEyeRigSpace_DEPRECATED.Empty();
			ConformalVertsRightEyeRigSpace_DEPRECATED.Empty();
		}

		// Transfer the transform stored in the deprecated conformal mesh component to the new one
		ConformalMeshComponent_DEPRECATED->UpdateComponentToWorld();
		TemplateMeshComponent->SetWorldTransform(ConformalMeshComponent_DEPRECATED->GetComponentTransform());

		// Set it to null once we transfer the data to the new template mesh component
		ConformalMeshComponent_DEPRECATED->GetDynamicMesh()->Reset();
		ConformalMeshComponent_DEPRECATED->MarkAsGarbage();
		ConformalMeshComponent_DEPRECATED = nullptr;
	}

	if (bIsAutoRigged && !CheckRigCompatible())
	{
		// Since we're using bIsAutoRigged for both M2MH and imported DNA state,
		// we can unset it here for optimization purposes (e.g. we don't want to
		// calculate rig compatibility every tick in Performance to update Process
		// button state).
		bIsAutoRigged = false;
	}
#endif

	if (TemplateMeshComponent != nullptr)
	{
		TemplateMeshComponent->UpdateComponentToWorld();
	}

	RawDNABuffer_DEPRECATED.Empty();
	RawDeltaDNABuffer_DEPRECATED.Empty();
	RawCombinedDNABuffer_DEPRECATED.Empty();
	DNABuffer_DEPRECATED.Empty();
	PCARig_DEPRECATED.Empty();
	BrowsBuffer_DEPRECATED.Empty();
	PredictiveSolvers_DEPRECATED.Empty();
	PredictiveWithoutTeethSolver_DEPRECATED.Empty();

	UpdateCaptureDataConfigName();

	if (DefaultSolver)
	{
		DefaultSolver->OnInternalsChanged().AddUObject(this, &UMetaHumanIdentityFace::UpdateCaptureDataConfigName);
	}

	if (IsConformalRigValid())
	{
		// Get rig of the physics asset as they interfere with the skeletal mesh bounding box
		RigComponent->GetSkeletalMeshAsset()->SetPhysicsAsset(nullptr);
		RigComponent->UpdateBounds();
	}
}

#if WITH_EDITOR

void UMetaHumanIdentityFace::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	if (FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = *Property->GetName();

		const bool bDefaultSolverChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, DefaultSolver);

		if (bDefaultSolverChanged)
		{
			UpdateCaptureDataConfigName();

			if (DefaultSolver)
			{
				DefaultSolver->OnInternalsChanged().AddUObject(this, &UMetaHumanIdentityFace::UpdateCaptureDataConfigName);
			}
		}
	}
}

void UMetaHumanIdentityBody::PostTransacted(const FTransactionObjectEvent& InTransactionEvent)
{
	Super::PostTransacted(InTransactionEvent);

	if (InTransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		OnMetaHumanIdentityBodyChangedDelegate.Broadcast();
	}
}

#endif

void UMetaHumanIdentityFace::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FMetaHumanIdentityCustomVersion::GUID);

	const int32 IdentityVersion = Ar.CustomVer(FMetaHumanIdentityCustomVersion::GUID);

	if (Ar.IsLoading() && IdentityVersion < FMetaHumanIdentityCustomVersion::EditorBulkDataUpdate)
	{
		// If we are loading an Identity before the BulkData update just transfer the contents
		// of each dna buffer to the corresponding bulk data for storage

		SetRawDNABuffer(RawDeltaDNABuffer_DEPRECATED);
		SetRawDeltaDNABuffer(RawDeltaDNABuffer_DEPRECATED);
		SetCombinedDNABuffer(RawCombinedDNABuffer_DEPRECATED);
		SetDNABuffer(DNABuffer_DEPRECATED);
		SetPCARig(PCARig_DEPRECATED);
		SetBrowsBuffer(BrowsBuffer_DEPRECATED);
		SetPredictiveSolvers(PredictiveSolvers_DEPRECATED);
		SetPredictiveWithoutTeethSolver(PredictiveWithoutTeethSolver_DEPRECATED);
	}
	else
	{
		RawDNABufferBulkData.Serialize(Ar, this);
		RawDeltaDNABufferBulkData.Serialize(Ar, this);
		RawCombinedDNABufferBulkData.Serialize(Ar, this);
		DNABufferBulkData.Serialize(Ar, this);
		PCARigBulkData.Serialize(Ar, this);
		BrowsBufferBulkData.Serialize(Ar, this);
		PredictiveSolversBulkData.Serialize(Ar, this);
		PredictiveWithoutTeethSolverBulkData.Serialize(Ar, this);
	}
}

bool UMetaHumanIdentityFace::CanConform() const
{
	if (!TemplateMeshComponent->GetPoseHeadMesh(EIdentityPoseType::Neutral))
	{
		return false;
	}

	if (UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral))
	{
		const TArray<UMetaHumanIdentityPromotedFrame*> ValidPromotedFrames = NeutralPose->GetValidContourDataFramesFrontFirst();
		if (ValidPromotedFrames.IsEmpty())
		{
			return false;
		}

		if (!NeutralPose->IsCaptureDataValid())
		{
			return false;
		}

		if (DefaultSolver == nullptr || !DefaultSolver->CanProcess())
		{
			return false;
		}

		if (NeutralPose->bFitEyes)
		{
			const UMetaHumanIdentityPromotedFrame* FrontalFrame = ValidPromotedFrames[0];

			// Check if all required eye curves are active in the frontal frame
			const bool bAllEyeCurvesActive = Algo::AllOf(CurveNamesForEyeFitting, [FrontalFrame](const FString& InCurveName)
			{
				if (const FTrackingContour* Contour = FrontalFrame->GetFrameTrackingContourData()->TrackingContours.Find(InCurveName))
				{
					return Contour->State.bActive;
				}

				return false;
			});

			return bAllEyeCurvesActive;
		}
		else
		{
			// If not fitting eyes conforming can happen
			return true;
		}
	}

	return false;
}

bool UMetaHumanIdentityFace::CanSubmitToAutorigging() const
{
	if (UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral))
	{
		return NeutralPose->IsCaptureDataValid() && bIsConformed;
	}

	return false;
}

bool UMetaHumanIdentityFace::IsConformalRigValid() const
{
	return RigComponent != nullptr && RigComponent->GetSkeletalMeshAsset() != nullptr;
}

void UMetaHumanIdentityFace::ExportTemplateMesh(const FString& InPackageName, const FString& InAssetName)
{
	UDynamicMesh* Head = TemplateMeshComponent->GetPoseHeadMesh(EIdentityPoseType::Neutral);
	UE::Geometry::FDynamicMesh3 DynMesh = Head->GetMeshRef();

	constexpr bool bReverseOrientationIfNeeded = true;
	const UE::Geometry::FTransformSRT3d TemplateMeshTransform = TemplateMeshComponent->GetRelativeTransform();
	MeshTransforms::ApplyTransform(DynMesh, TemplateMeshTransform, bReverseOrientationIfNeeded);

	FMeshDescription MeshDescription;
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();

	FDynamicMeshToMeshDescription Converter;
	Converter.Convert(&DynMesh, MeshDescription, true);

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(CreatePackage(*InPackageName), *InAssetName, RF_Public | RF_Standalone);
	StaticMesh->InitResources();
	StaticMesh->BuildFromMeshDescriptions({ &MeshDescription });
}

FString UMetaHumanIdentityFace::GetDeviceDNAToPCAConfig(UCaptureData* InCaptureData) const
{
	return DefaultSolver->FaceAnimationSolver->GetSolverPCAFromDNAData(InCaptureData);
}

EIdentityErrorCode UMetaHumanIdentityFace::Conform(EConformType InConformType)
{
	const int32 NumTasksForProgressBar = 3;

	FScopedSlowTask ConformTask(NumTasksForProgressBar, LOCTEXT("ConformProgressText", "Running MetaHuman Identity Solve..."));
	ConformTask.MakeDialog();

	ConformTask.EnterProgressFrame();

	UE::Wrappers::FMetaHumanConformer Conformer;

	if (UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral))
	{
		FString TemplateDescriptionJson = DefaultSolver->GetFittingTemplateData(NeutralPose->GetCaptureData());
		FString FittingConfigurationJson = DefaultSolver->GetFittingConfigData(NeutralPose->GetCaptureData());
		FString IdentityModelJson = DefaultSolver->GetFittingIdentityModelData(NeutralPose->GetCaptureData());

		if (!Conformer.Init(TemplateDescriptionJson, IdentityModelJson, FittingConfigurationJson))
		{
			return EIdentityErrorCode::Initialization;
		}

		if (!NeutralPose->IsCaptureDataValid())
		{
			return EIdentityErrorCode::CaptureDataInvalid;
		}

		UCaptureData* CaptureData = NeutralPose->GetCaptureData();

		double ConformDuration = 0.0f;

		EIdentityErrorCode ConformerResult = EIdentityErrorCode::None;

		if (CaptureData->IsA<UMeshCaptureData>())
		{
			const FScopedDurationTimer Timer{ ConformDuration };

			if (InConformType == EConformType::Solve)
			{
				// Run the Mesh Fitting Conformer

				ConformTask.EnterProgressFrame();

				if (!SetConformerCameraParameters(NeutralPose, Conformer))
				{
					return EIdentityErrorCode::CameraParameters;
				}

				bool bInvalidMeshTopology = true;
				if (!SetConformerScanInputData(NeutralPose, Conformer, bInvalidMeshTopology))
				{
					return EIdentityErrorCode::ScanInput;
				}
				if (bInvalidMeshTopology)
				{
					return EIdentityErrorCode::BadInputMeshTopology;
				}

				ConformTask.EnterProgressFrame();

				ConformerResult = RunMeshConformer(NeutralPose, Conformer);
			}
			else if (InConformType == EConformType::Copy)
			{
#if WITH_EDITOR
				CopyMeshVerticesFromExistingMesh(CaptureData);
#endif
			}
		}
		else if (CaptureData->IsA<UFootageCaptureData>())
		{
			const FScopedDurationTimer Timer{ ConformDuration };

			// Run the Footage Fitting Conformer

			ConformTask.EnterProgressFrame();

			if (!SetConformerCameraParameters(NeutralPose, Conformer))
			{
				return EIdentityErrorCode::CameraParameters;
			}

			if (!SetConformerDepthInputData(NeutralPose, Conformer))
			{
				return EIdentityErrorCode::DepthInput;
			}

			ConformTask.EnterProgressFrame();

			ConformerResult = RunMeshConformer(NeutralPose, Conformer);
		}

		if (!bIsConformed)
		{
			return ConformerResult;
		}

		if (InConformType != EConformType::Copy)
		{
			const FScopedDurationTimer Timer{ ConformDuration };

			bIsConformed = false;

			const TArray<UMetaHumanIdentityPromotedFrame*> PromotedFrames = NeutralPose->GetValidContourDataFramesFrontFirst();

			for (int32 FrameIndex = 0; FrameIndex < PromotedFrames.Num(); ++FrameIndex)
			{
				UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFrames[FrameIndex];

				if (PromotedFrame->bIsFrontView)
				{
					TArray<uint8> BrowsBuffer;
					bIsConformed = Conformer.GenerateBrowMeshLandmarks(CombineFrameNameAndCameraViewName(GetFrameNameForConforming(PromotedFrame, FrameIndex), NeutralPose->Camera), BrowsBuffer);

					// Ensure valid, non-empty, results. 1000 is arbitrary value chosen to ensure buffer contains the expect
					// amount of data, ie its not an empty json string like "{}" but actually contains the tracking data.
					// Genuine json data returned from this function is about 10k in size.
					bIsConformed &= BrowsBuffer.Num() > 1000;

					if (bIsConformed)
					{
						SetBrowsBuffer(BrowsBuffer);
					}

					break;
				}
			}

			if (!bIsConformed)
			{
				return EIdentityErrorCode::BrowsFailed;
			}
		}

		UE_LOG(LogMetaHumanIdentity, Display, TEXT("Conforming took %lf seconds"), ConformDuration);

		return EIdentityErrorCode::None;
	}
	else
	{
		return EIdentityErrorCode::NoPose;
	}
}

#if WITH_EDITOR

void UMetaHumanIdentityFace::ResetRigComponent(bool bInCreateNewRigComponent)
{
	bIsConformed = false;
	bIsAutoRigged = false;
	bHasFittedEyes = false;

	ClearDNABuffer();
	ClearRawDeltaDNABuffer();
	ClearCombinedDNABuffer();

	ClearPCARig();
	ClearPredictiveSolvers();
	ClearPredictiveWithoutTeethSolver();

	ClearBrowsBuffer();

	ResetTemplateMesh();

	if (bInCreateNewRigComponent && RigComponent != nullptr)
	{
		RigComponent->SetSkeletalMeshAsset(nullptr);

		const EIdentityErrorCode Initialized = InitializeRig();
		if (Initialized != EIdentityErrorCode::None)
		{
			UMetaHumanIdentity::HandleError(Initialized);
			return;
		}
	}
}

void UMetaHumanIdentityFace::ResetTemplateMesh()
{
	if (TemplateMeshComponent != nullptr)
	{
		TemplateMeshComponent->ResetMeshes();
	}

	ResetTemplateMeshTransform();
}

EIdentityErrorCode UMetaHumanIdentityFace::ApplyCombinedDNAToRig(TSharedPtr<class IDNAReader> InDNAReader)
{
	if (InDNAReader == nullptr)
	{
		return EIdentityErrorCode::InvalidDNA;
	}

	const TArray<uint8> RawDNABuffer = ReadStreamFromDNA(InDNAReader.Get(), EDNADataLayer::All);

	SetRawDNABuffer(RawDNABuffer);
	// RawDNABuffer from the new AR Service is already DNA and Delta DNA combined
	SetCombinedDNABuffer(RawDNABuffer);

	bShouldUpdateRigComponent = true;

	const EIdentityErrorCode FittedTeeth = FitTeeth();

	bIsAutoRigged = (FittedTeeth == EIdentityErrorCode::None);

	return FittedTeeth;
}

bool UMetaHumanIdentityFace::CheckDNACompatible(class IDNAReader* InDNAReader) const
{
	FString CompatibilityMsg;

	return CheckDNACompatible(InDNAReader, CompatibilityMsg);
}

bool UMetaHumanIdentityFace::CheckDNACompatible(class IDNAReader* InDNAReader, FString& OutCompatibilityMsg) const
{
	if (InDNAReader != nullptr)
	{
		if (TSharedPtr<IDNAReader> ArchetypeDnaReader = UMetaHumanIdentityFace::GetPluginArchetypeDNAReader())
		{
			// Note we are checking the embedded DNA rather than the skel mesh. So even though the mesh can be created 
			// With only LOD0 The embedded DNA could still contain all LOD's and the LOD check vs the archetype will pass
			return FDNAUtilities::CheckCompatibility(ArchetypeDnaReader.Get(), InDNAReader, EDNARigCompatiblityFlags::All, OutCompatibilityMsg);
		}
	}

	return false;
}

bool UMetaHumanIdentityFace::CheckRigCompatible() const
{
	FString CompatibilityMsg;

	return CheckRigCompatible(CompatibilityMsg);
}

bool UMetaHumanIdentityFace::CheckRigCompatible(FString& OutCompatibilityMsg) const
{
	if (RigComponent != nullptr && RigComponent->GetSkeletalMeshAsset() != nullptr)
	{
		if (UDNAAsset* SkelMeshDnaAsset = USkelMeshDNAUtils::GetMeshDNA(RigComponent->GetSkeletalMeshAsset()))
		{
			FSkelMeshDNAReader SkelMeshDnaReader{ SkelMeshDnaAsset };

			return CheckDNACompatible(&SkelMeshDnaReader, OutCompatibilityMsg);
		}
	}

	// If there is no DNA, then we can say that the rig is compatible
	return true;
}

bool UMetaHumanIdentityFace::CanFitTeeth() const
{
	if (HasCombinedDNABuffer() && DefaultSolver && DefaultSolver->CanProcess())
	{
		if (UMetaHumanIdentityPose* TeethPose = FindPoseByType(EIdentityPoseType::Teeth))
		{
			const TArray<UMetaHumanIdentityPromotedFrame*> ValidPromotedFrames = TeethPose->GetValidContourDataFramesFrontFirst();
			return !ValidPromotedFrames.IsEmpty();
		}
	}

	return false;
}

bool UMetaHumanIdentityFace::HasValidPromotedFramesForPose( EIdentityPoseType InPoseType ) const
{
	if (UMetaHumanIdentityPose* Pose = FindPoseByType(InPoseType))
	{
		const TArray<UMetaHumanIdentityPromotedFrame*> ValidPromotedFrames = Pose->GetValidContourDataFramesFrontFirst();
		return !ValidPromotedFrames.IsEmpty();
	}
	return false;
}

EIdentityErrorCode UMetaHumanIdentityFace::FitTeeth()
{
	bool bCanFitTeeth = false;
	if (UMetaHumanIdentityPose* TeethPose = FindPoseByType(EIdentityPoseType::Teeth))
	{
		if (!TeethPose->GetValidContourDataFramesFrontFirst().IsEmpty())
		{
			bCanFitTeeth = true;

			ClearDNABuffer();

			FScopedSlowTask FitTeethProgress(100, LOCTEXT("FitTeeth", "Fitting teeth position..."));
			FitTeethProgress.MakeDialog();

			UE::Wrappers::FMetaHumanConformer Conformer;

			FitTeethProgress.EnterProgressFrame(10); // 10 total

			FString TemplateDescriptionJson = DefaultSolver->GetFittingTemplateData(TeethPose->GetCaptureData());
			FString FittingConfigurationJson = DefaultSolver->GetFittingConfigTeethData(TeethPose->GetCaptureData());
			FString IdentityModelJson = DefaultSolver->GetFittingIdentityModelData(TeethPose->GetCaptureData());

			if (!Conformer.Init(TemplateDescriptionJson, IdentityModelJson, FittingConfigurationJson))
			{
				return EIdentityErrorCode::Initialization;
			}

			FitTeethProgress.EnterProgressFrame(10); // 20 total

			if (!SetConformerCameraParameters(TeethPose, Conformer))
			{
				return EIdentityErrorCode::CameraParameters;
			}

			UCaptureData* CaptureData = TeethPose->GetCaptureData();

			if (CaptureData)
			{
				if (CaptureData->IsA<UMeshCaptureData>())
				{
					bool bInvalidMeshTopology = true;
					if (!SetConformerScanInputData(TeethPose, Conformer, bInvalidMeshTopology))
					{
						return EIdentityErrorCode::ScanInput;
					}

					if (bInvalidMeshTopology)
					{
						return EIdentityErrorCode::BadInputMeshTopology;
					}
				}
				else if (CaptureData->IsA<UFootageCaptureData>())
				{
					if (!SetConformerDepthInputData(TeethPose, Conformer))
					{
						return EIdentityErrorCode::DepthInput;
					}
				}
			}

			FitTeethProgress.EnterProgressFrame(10); // 30 total

			TArray<uint8> IntermediatePCARig;
			if (!UE::Wrappers::FMetaHumanConformer::CalculatePcaModelFromDnaRig(GetDeviceDNAToPCAConfig(TeethPose->GetCaptureData()), GetCombinedDNABuffer(), IntermediatePCARig))
			{
				return EIdentityErrorCode::CalculatePCAModel;
			}

			FitTeethProgress.EnterProgressFrame(30); // 60 total

			FString DebuggingFolder = "";
			if (CVarEnableExportMeshes.GetValueOnAnyThread())
			{
				FString DebuggingFolderBase = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName());
				DebuggingFolder = DebuggingFolderBase / TeethPose->GetName();

				// note that we don't check whether saving debugging data has been successful
				SaveDebuggingData(TeethPose, Conformer, DebuggingFolderBase);
			}

			// update the teeth source in the conformer to use the teeth from the neutral DNA
			if (!Conformer.UpdateTeethSource(GetCombinedDNABuffer()))
			{
				return EIdentityErrorCode::TeethSource;
			}


			TArray<float> FaceVertices, StackedToScanTransforms, StackedToScanScales;
			if (!Conformer.FitRigid(FaceVertices, StackedToScanTransforms, StackedToScanScales, 10))
			{
				return EIdentityErrorCode::FitRigid;
			}

			FitTeethProgress.EnterProgressFrame(20); // 80 total


			if (!Conformer.FitPcaRig(IntermediatePCARig, GetCombinedDNABuffer(), FaceVertices, StackedToScanTransforms, StackedToScanScales, DebuggingFolder))
			{
				return EIdentityErrorCode::FitPCA;
			}

			const TConstArrayView<FVector3f> ConformalVertsFaceView((FVector3f*)FaceVertices.GetData(), FaceVertices.Num() / 3);
			TemplateMeshComponent->SetPoseHeadMeshVertices(EIdentityPoseType::Teeth, ConformalVertsFaceView, ETemplateVertexConversion::ConformerToUE);

			FitTeethProgress.EnterProgressFrame(10); // 90 total

			TArray<float> TeethVertices;
			if (!Conformer.FitTeeth(TeethVertices, DebuggingFolder))
			{
				return EIdentityErrorCode::FitTeethFailed;
			}

			const TConstArrayView<FMatrix44f> StackedTransformsView((FMatrix44f*)StackedToScanTransforms.GetData(), StackedToScanTransforms.Num() / 16);


			float Dx, Dy, Dz;
			if (!Conformer.CalcTeethDepthDelta(TeethPose->ManualTeethDepthOffset, Dx, Dy, Dz))
			{
				return EIdentityErrorCode::TeethDepthDelta;
			}

			// TODO: going forward this should be done as a method of the UMetaHumanTemplateMeshComponent class
			// Also, it is probably better to expose the teeth transform in the template mesh component rather than in the teeth pose
			// for the full implementation
			for (int32 TeethVert = 0; TeethVert < TeethVertices.Num(); TeethVert += 3)
			{
				TeethVertices[TeethVert] += Dx;
				TeethVertices[TeethVert + 1] += Dy;
				TeethVertices[TeethVert + 2] += Dz;
			}


			const TConstArrayView<FVector3f> TeethVertsView((FVector3f*)TeethVertices.GetData(), TeethVertices.Num() / 3);
			TemplateMeshComponent->SetTeethMeshVertices(TeethVertsView, ETemplateVertexConversion::ConformerToUE);

			FitTeethProgress.EnterProgressFrame(10); // 100 total

			TArray<uint8> DNABuffer;
			if (!Conformer.UpdateRigWithTeethMeshVertices(GetCombinedDNABuffer(), TeethVertices, DNABuffer))
			{
				return EIdentityErrorCode::UpdateRigWithTeeth;
			}

			SetDNABuffer(DNABuffer);

			SetHeadAlignmentForPose(TeethPose, StackedTransformsView, StackedToScanScales);
		}
	}

	if (!bCanFitTeeth)
	{
		SetDNABuffer(GetCombinedDNABuffer());
	}

	if (CVarEnableExportMeshes.GetValueOnAnyThread())
	{
		FString PathToDNAFile;
		if (bCanFitTeeth)
		{
			PathToDNAFile = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName()) / FString::Format(TEXT("{0}_Teeth_DNA.dna"), { GetOuter()->GetName() });
		}
		else
		{
			PathToDNAFile = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName()) / FString::Format(TEXT("{0}_Neutral_DNA.dna"), { GetOuter()->GetName() });
		}
		TArray<uint8> DNABuffer = GetDNABuffer();
		TSharedPtr<IDNAReader> DNAReader = ReadDNAFromBuffer(&DNABuffer);
		WriteDNAToFile(DNAReader.Get(), EDNADataLayer::All, PathToDNAFile);
	}

	return Finalize();
}

EIdentityErrorCode UMetaHumanIdentityFace::Finalize()
{
	ClearPCARig();
	ClearPredictiveSolvers();
	ClearPredictiveWithoutTeethSolver();

	FScopedSlowTask FinalizingProgress(2, LOCTEXT("FinalizingIdentity", "Finalizing current operation..."));
	FinalizingProgress.MakeDialog();

	FinalizingProgress.EnterProgressFrame();

	TArray<uint8> DNABuffer = GetDNABuffer();
	TArray<uint8> PCARig;

	// convert the DNA to a PCA model. If the rig has been built from poses, use the neutral pose capture data to specify the DNA to PCA config, otherwise
	// indicate a general Mesh 2 MetaHuman Use Case by passing a nullptr
	FString DebuggingFolder = "";
	if (CVarEnableExportMeshes.GetValueOnAnyThread())
	{
		DebuggingFolder = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName());
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*DebuggingFolder))
		{
			bool bCreatedFolder = PlatformFile.CreateDirectory(*DebuggingFolder);
			if (!bCreatedFolder)
			{
				return EIdentityErrorCode::CreateDebugFolder;
			}
		}
	}
	if (UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral))
	{
		if (!UE::Wrappers::FMetaHumanConformer::CalculatePcaModelFromDnaRig(GetDeviceDNAToPCAConfig(NeutralPose->GetCaptureData()), DNABuffer, PCARig, DebuggingFolder))
		{
			return EIdentityErrorCode::CalculatePCAModel;
		}
	}
	else
	{
		if (!UE::Wrappers::FMetaHumanConformer::CalculatePcaModelFromDnaRig(GetDeviceDNAToPCAConfig(nullptr), DNABuffer, PCARig, DebuggingFolder))
		{
			return EIdentityErrorCode::CalculatePCAModel;
		}
	}

	SetPCARig(PCARig);

	FinalizingProgress.EnterProgressFrame();

	ApplyDNAToRigComponent(ReadDNAFromBuffer(&DNABuffer));

	return EIdentityErrorCode::None;
}

bool UMetaHumanIdentityFace::ExportDNADataToFiles(const FString& InDnaPathWithName, const FString& InBrowsPathWithName)
{
	bool bSuccess = false;
	if (FFileHelper::SaveArrayToFile(GetBrowsBuffer(), *InBrowsPathWithName))
	{
		TArray<uint8> DNABuffer = GetDNABuffer();
		TSharedPtr<IDNAReader> DNAReader = ReadDNAFromBuffer(&DNABuffer);
		WriteDNAToFile(DNAReader.Get(), EDNADataLayer::All, InDnaPathWithName);

		bSuccess = FPaths::FileExists(*InDnaPathWithName);
	}

	return bSuccess;
}

#endif // WITH_EDITOR

static TArray<uint8> GetBulkDataPayload(const UE::Serialization::FEditorBulkData& InBulkData)
{
	TArray<uint8> PayloadData;

	if (InBulkData.HasPayloadData())
	{
		TFuture<FSharedBuffer> PayloadFuture = InBulkData.GetPayload();

		if (PayloadFuture.Get().GetSize() > TNumericLimits<int32>::Max()) // Blocking call. Max that can be stored in a TArray
		{
			UE_LOG(LogMetaHumanIdentity, Fatal, TEXT("Payload size too large"));
			return PayloadData;
		}

		PayloadData.Append((const uint8*)PayloadFuture.Get().GetData(), PayloadFuture.Get().GetSize());
	}

	return PayloadData;
}

static void SetBulkDataPayload(UE::Serialization::FEditorBulkData& InBulkData, TConstArrayView<uint8> InBuffer)
{
	InBulkData.UpdatePayload(FSharedBuffer::Clone(InBuffer.GetData(), InBuffer.Num()));
}

void UMetaHumanIdentityFace::SetRawDNABuffer(TConstArrayView<uint8> InRawDNABuffer)
{
	SetBulkDataPayload(RawDNABufferBulkData, InRawDNABuffer);
}

TArray<uint8> UMetaHumanIdentityFace::GetRawDNABuffer() const
{
	return GetBulkDataPayload(RawDNABufferBulkData);
}

bool UMetaHumanIdentityFace::HasRawDNABuffer() const
{
	return RawDNABufferBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearRawDNABuffer()
{
	RawDNABufferBulkData.Reset();
}

void UMetaHumanIdentityFace::SetRawDeltaDNABuffer(TConstArrayView<uint8> InRawDeltaDNABuffer)
{
	SetBulkDataPayload(RawDeltaDNABufferBulkData, InRawDeltaDNABuffer);
}

TArray<uint8> UMetaHumanIdentityFace::GetRawDeltaDNABuffer() const
{
	return GetBulkDataPayload(RawDeltaDNABufferBulkData);
}

bool UMetaHumanIdentityFace::HasRawDeltaDNABuffer() const
{
	return RawDeltaDNABufferBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearRawDeltaDNABuffer()
{
	RawDeltaDNABufferBulkData.Reset();
}

void UMetaHumanIdentityFace::SetCombinedDNABuffer(TConstArrayView<uint8> InRawCombinedDNABuffer)
{
	SetBulkDataPayload(RawCombinedDNABufferBulkData, InRawCombinedDNABuffer);
}

TArray<uint8> UMetaHumanIdentityFace::GetCombinedDNABuffer() const
{
	return GetBulkDataPayload(RawCombinedDNABufferBulkData);
}

bool UMetaHumanIdentityFace::HasCombinedDNABuffer() const
{
	return RawCombinedDNABufferBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearCombinedDNABuffer()
{
	return RawCombinedDNABufferBulkData.Reset();
}

void UMetaHumanIdentityFace::SetDNABuffer(TConstArrayView<uint8> InDNABuffer)
{
	SetBulkDataPayload(DNABufferBulkData, InDNABuffer);
}

TArray<uint8> UMetaHumanIdentityFace::GetDNABuffer() const
{
	return GetBulkDataPayload(DNABufferBulkData);
}

bool UMetaHumanIdentityFace::HasDNABuffer() const
{
	return DNABufferBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearDNABuffer()
{
	DNABufferBulkData.Reset();
}

void UMetaHumanIdentityFace::SetPCARig(TConstArrayView<uint8> InPCARig)
{
	SetBulkDataPayload(PCARigBulkData, InPCARig);
}

TArray<uint8> UMetaHumanIdentityFace::GetPCARig() const
{
	return GetBulkDataPayload(PCARigBulkData);
}

bool UMetaHumanIdentityFace::HasPCARig() const
{
	return PCARigBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearPCARig()
{
	PCARigBulkData.Reset();
}

void UMetaHumanIdentityFace::SetBrowsBuffer(TConstArrayView<uint8> InBrowsBuffer)
{
	SetBulkDataPayload(BrowsBufferBulkData, InBrowsBuffer);
}

TArray<uint8> UMetaHumanIdentityFace::GetBrowsBuffer() const
{
	return GetBulkDataPayload(BrowsBufferBulkData);
}

bool UMetaHumanIdentityFace::HasBrowsBuffer() const
{
	return BrowsBufferBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearBrowsBuffer()
{
	BrowsBufferBulkData.Reset();
}

void UMetaHumanIdentityFace::SetPredictiveSolvers(TConstArrayView<uint8> InPredictiveSolvers)
{
	SetBulkDataPayload(PredictiveSolversBulkData, InPredictiveSolvers);
}

TArray<uint8> UMetaHumanIdentityFace::GetPredictiveSolvers() const
{
	return GetBulkDataPayload(PredictiveSolversBulkData);
}

bool UMetaHumanIdentityFace::HasPredictiveSolvers() const
{
	return PredictiveSolversBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearPredictiveSolvers()
{
	PredictiveSolversBulkData.Reset();
}

void UMetaHumanIdentityFace::SetPredictiveWithoutTeethSolver(TConstArrayView<uint8> InPredictiveWithoutTeethSolver)
{
	SetBulkDataPayload(PredictiveWithoutTeethSolverBulkData, InPredictiveWithoutTeethSolver);
}

TArray<uint8> UMetaHumanIdentityFace::GetPredictiveWithoutTeethSolver() const
{
	return GetBulkDataPayload(PredictiveWithoutTeethSolverBulkData);
}

bool UMetaHumanIdentityFace::HasPredictiveWithoutTeethSolver() const
{
	return PredictiveWithoutTeethSolverBulkData.HasPayloadData();
}

void UMetaHumanIdentityFace::ClearPredictiveWithoutTeethSolver()
{
	PredictiveWithoutTeethSolverBulkData.Reset();
}

#if WITH_EDITOR

// WIP: Temporary local function which will be moved to DNASkelMeshUtils later.
void UMetaHumanIdentityFace::UpdateSourceData(USkeletalMesh* SkelMesh, IDNAReader* DNAReader, FDNAToSkelMeshMap* DNAToSkelMeshMap)
{
	FSkeletalMeshModel* ImportedModel = SkelMesh->GetImportedModel();
	const int32 LODCount = ImportedModel->LODModels.Num();
	const TArray<FTransform>& RawBonePose = SkelMesh->GetRefSkeleton().GetRawRefBonePose();
	for (int32 LODIndex = 0; LODIndex < LODCount; LODIndex++)
	{
		// Update vertices.
		FString PointsBefore;
		FString PointsAfter;
		const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];

		FMeshDescription* MeshDescription = SkelMesh->GetMeshDescription(LODIndex);
		FSkeletalMeshImportData ImportData = FSkeletalMeshImportData::CreateFromMeshDescription(*MeshDescription);

		const int32 LODMeshVtxCount = LODModel.MeshToImportVertexMap.Num();
		TArray<FSoftSkinVertex> LODVertices;
		LODModel.GetVertices(LODVertices);

		TArray<SkeletalMeshImportData::FRawBoneInfluence> NewInfluences;
		TArray<bool> HasOverlappingVertices;
		HasOverlappingVertices.AddZeroed(LODMeshVtxCount);
		for (int32 LODMeshVtxIndex = 0; LODMeshVtxIndex < LODMeshVtxCount; LODMeshVtxIndex++)
		{
			// Update points.
			int32 FbxVertexIndex = LODModel.MeshToImportVertexMap[LODMeshVtxIndex];
			if (!HasOverlappingVertices[FbxVertexIndex])
			{
				HasOverlappingVertices[FbxVertexIndex] = true;
				if (FbxVertexIndex <= LODModel.MaxImportVertex)
				{
					ImportData.Points[FbxVertexIndex] = LODVertices[LODMeshVtxIndex].Position;
				}

				// Update influences.
				int32 SectionIdx;
				int32 VertexIdx;
				LODModel.GetSectionFromVertexIndex(LODMeshVtxIndex, SectionIdx, VertexIdx);
				if (LODModel.Sections[SectionIdx].SoftVertices[VertexIdx].Color.B != 0)
				{
					int32 DNAMeshIndex = DNAToSkelMeshMap->ImportVtxToDNAMeshIndex[LODIndex][LODMeshVtxIndex];
					int32 DNAVertexIndex = DNAToSkelMeshMap->ImportVtxToDNAVtxIndex[LODIndex][LODMeshVtxIndex];

					if (DNAVertexIndex >= 0)
					{
						TArrayView<const float> DNASkinWeights = DNAReader->GetSkinWeightsValues(DNAMeshIndex, DNAVertexIndex);
						TArrayView<const uint16> DNASkinJoints = DNAReader->GetSkinWeightsJointIndices(DNAMeshIndex, DNAVertexIndex);
						uint16 SkinJointNum = DNASkinJoints.Num();
						for (uint16 InfluenceIndex = 0; InfluenceIndex < SkinJointNum; ++InfluenceIndex)
						{
							float InfluenceWeight = DNASkinWeights[InfluenceIndex];
							int32 UpdatedBoneId = DNAToSkelMeshMap->GetUEBoneIndex(DNASkinJoints[InfluenceIndex]);

							SkeletalMeshImportData::FRawBoneInfluence Influence;
							Influence.VertexIndex = FbxVertexIndex;
							Influence.BoneIndex = UpdatedBoneId;
							Influence.Weight = InfluenceWeight;
							NewInfluences.Add(Influence);
						}
						ImportData.Influences.RemoveAll([FbxVertexIndex](const SkeletalMeshImportData::FRawBoneInfluence& BoneInfluence) { return FbxVertexIndex == BoneInfluence.VertexIndex; });
					}
				}
			}
		}
		ImportData.Influences.Append(NewInfluences);
		// Sort influences by vertex index.
		FLODUtilities::ProcessImportMeshInfluences(ImportData.Wedges.Num(), ImportData.Influences, SkelMesh->GetPathName());

		// Update reference pose.
		const int32 JointCount = LODModel.RequiredBones.Num();
		if (ImportData.RefBonesBinary.Num() == JointCount)
		{
			for (int32 JointIndex = 0; JointIndex < JointCount; JointIndex++)
			{
				const int32 OriginalBoneIndex = LODModel.RequiredBones[JointIndex];
				const FTransform& UpdatedTransform = FTransform(RawBonePose[OriginalBoneIndex]);
				ImportData.RefBonesBinary[OriginalBoneIndex].BonePos.Transform = FTransform3f(UpdatedTransform);
			}
		}

		// Update morph targets. 
		const int32 MorphTargetCount = SkelMesh->GetMorphTargets().Num();
		ImportData.MorphTargetModifiedPoints.Empty(MorphTargetCount);
		ImportData.MorphTargetNames.Empty(MorphTargetCount);
		ImportData.MorphTargets.Empty(MorphTargetCount);
		if (LODIndex == 0)
		{
			// Blend shapes are used only in LOD0.
			for (int32 MorphIndex = 0; MorphIndex < MorphTargetCount; MorphIndex++)
			{
				UMorphTarget* MorphTarget = SkelMesh->GetMorphTargets()[MorphIndex];
				// Add Morph target name.
				ImportData.MorphTargetNames.Add(MorphTarget->GetName());
				FSkeletalMeshImportData MorphTargetImportDeltas;
				FMorphTargetLODModel& MorphLODModel = MorphTarget->GetMorphLODModels()[LODIndex];

				// Init deltas and vertices for the current morph target.
				int32 NumDeltas = MorphLODModel.Vertices.Num();
				MorphTargetImportDeltas.Points.Reserve(NumDeltas);
				TSet<uint32> MorphTargetImportVertices;
				MorphTargetImportVertices.Reserve(NumDeltas);

				FMorphTargetDelta* Deltas = MorphLODModel.Vertices.GetData();
				for (int32 DeltaIndex = 0; DeltaIndex < NumDeltas; DeltaIndex++)
				{
					const uint32 SourceIndex = LODModel.MeshToImportVertexMap[Deltas[DeltaIndex].SourceIdx];
					MorphTargetImportDeltas.Points.Add(ImportData.Points[SourceIndex] + Deltas[DeltaIndex].PositionDelta);
					MorphTargetImportVertices.Add(SourceIndex);
				}
				ImportData.MorphTargetModifiedPoints.Add(MorphTargetImportVertices);
				ImportData.MorphTargets.Add(MorphTargetImportDeltas);
			}

		}

		ImportData.GetMeshDescription(SkelMesh, &SkelMesh->GetLODInfo(LODIndex)->BuildSettings, *MeshDescription);
		SkelMesh->CommitMeshDescription(LODIndex);
	}
}

#endif // WITH_EDITOR

void UMetaHumanIdentityFace::SetHeadAlignmentForPose(UMetaHumanIdentityPose* InPose, TConstArrayView<FMatrix44f> InStackedTransforms, TConstArrayView<float> InStackedScales)
{
	check(InStackedTransforms.Num() == InStackedScales.Num());

	if (InPose != nullptr)
	{
		for (int32 TransformIndex = 0; TransformIndex < InStackedTransforms.Num(); ++TransformIndex)
		{
			FMatrix44f TransformMatrix = InStackedTransforms[TransformIndex];
			FTransform HeadTransform{ FMatrix{ TransformMatrix } };
			FOpenCVHelperLocal::ConvertOpenCVToUnreal(HeadTransform);

			HeadTransform.SetScale3D(FVector(InStackedScales[TransformIndex]));
			HeadTransform.SetTranslation(HeadTransform.GetTranslation() * HeadTransform.GetScale3D());

			if (InPose->GetCaptureData()->IsA<UFootageCaptureData>())
			{
				// For footage to metahuman there will be one transform for each promoted frame
				InPose->SetHeadAlignment(HeadTransform, TransformIndex);
			}
			else
			{
				// For mesh to metahuman there is only one transform so set the same one for all promoted frames
				InPose->SetHeadAlignment(HeadTransform);
			}
		}
	}
}

void UMetaHumanIdentityFace::UpdateCaptureDataConfigName()
{
	for (TObjectPtr<UMetaHumanIdentityPose> Pose : Poses)
	{
		Pose->UpdateCaptureDataConfigName();
	}
}

#if WITH_EDITOR

void UMetaHumanIdentityFace::ApplyDNAToRigComponent(TSharedPtr<class IDNAReader>  InDNAReader, bool bInUpdateBlendShapes, bool bInUpdateSkinWeights)
{
	if (RigComponent != nullptr && RigComponent->GetSkeletalMeshAsset() != nullptr && RigComponent->GetSkeletalMeshAsset()->GetSkeleton() != nullptr)
	{
		double ApplyDNADuration = 0.0;
		FDurationTimer Timer{ ApplyDNADuration };

		RigComponent->Modify();

		// Map the structures in SkeletalMesh so we can update them; this needs to be done just once at the beginning (not at every update)

		// TODO
		FDNAToSkelMeshMap* DNAToSkelMeshMap = USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(InDNAReader.Get(), RigComponent->GetSkeletalMeshAsset());

		// TO DO:
		// if there is a window with a skeletal mesh open, then the Behavior in the instance needs to be updated
		// in this test, we don't need this as we are dealing just with assets, not instances
		// for the final version, pass the instance of the skeletal mesh component into this pointer:
		// TObjectPtr<USkeletalMeshComponent> MeshComponent;
		DNAToSkelMeshMap->MapJoints(InDNAReader.Get());
		DNAToSkelMeshMap->MapMorphTargets(InDNAReader.Get());

		// Set the Behavior part of DNA in skeletal mesh AssetUserData
		UDNAAsset* DNAAsset = NewObject<UDNAAsset>(RigComponent->GetSkeletalMeshAsset());
		DNAAsset->SetBehaviorReader(InDNAReader);
		DNAAsset->SetGeometryReader(InDNAReader);
		RigComponent->GetSkeletalMeshAsset()->AddAssetUserData(DNAAsset);

		UpdateJoints(RigComponent->GetSkeletalMeshAsset(), InDNAReader.Get(), DNAToSkelMeshMap);
		USkelMeshDNAUtils::UpdateBaseMesh(RigComponent->GetSkeletalMeshAsset(), InDNAReader.Get(), DNAToSkelMeshMap, ELodUpdateOption::All);

		if (!bInUpdateBlendShapes)
		{
			USkelMeshDNAUtils::RebuildRenderData_VertexPosition(RigComponent->GetSkeletalMeshAsset());
		}

		if (bInUpdateSkinWeights)
		{
			USkelMeshDNAUtils::UpdateSkinWeights(RigComponent->GetSkeletalMeshAsset(), InDNAReader.Get(), DNAToSkelMeshMap, ELodUpdateOption::All);
		}

		if (bInUpdateBlendShapes)
		{
			// we know that blend shapes exist only for LOD 0, so here we ignore the Options.LODsToInclude
			USkelMeshDNAUtils::UpdateMorphTargets(RigComponent->GetSkeletalMeshAsset(), InDNAReader.Get(), DNAToSkelMeshMap, ELodUpdateOption::All);
			USkelMeshDNAUtils::RebuildRenderData(RigComponent->GetSkeletalMeshAsset());
		}

		UMetaHumanIdentityFace::UpdateSourceData(RigComponent->GetSkeletalMeshAsset(), InDNAReader.Get(), DNAToSkelMeshMap);

		// Skeletal mesh has changed, so mark it as dirty
		RigComponent->GetSkeletalMeshAsset()->Modify();

		// TODO: Ideally this would be done by USkelMeshDNAUtils::UpdateBaseMesh
		FSkeletalMeshModel* SkelMeshModel = RigComponent->GetSkeletalMeshAsset()->GetImportedModel();
		TArray<FSoftSkinVertex> SkelMeshVertices;
		SkelMeshModel->LODModels[0].GetVertices(SkelMeshVertices);
		TArray<FVector> Points;
		Algo::Transform(SkelMeshVertices, Points, [](const FSoftSkinVertex& SkelMeshVertex)
		{
			return FVector(SkelMeshVertex.Position.X, SkelMeshVertex.Position.Y, SkelMeshVertex.Position.Z);
		});
		RigComponent->GetSkeletalMeshAsset()->SetImportedBounds(FBoxSphereBounds{ FBox{ Points } });
		RigComponent->GetSkeletalMeshAsset()->ValidateBoundsExtension();
		RigComponent->UpdateBounds();

		UpdateRigTransform();

		RigComponent->GetSkeletalMeshAsset()->MarkPackageDirty();
		RigComponent->GetSkeletalMeshAsset()->PostEditChange();

		Timer.Stop();
		UE_LOG(LogMetaHumanIdentity, Display, TEXT("Apply DNA To Rig took %lf seconds"), ApplyDNADuration);
	}
}

EIdentityErrorCode UMetaHumanIdentityFace::ApplyDNAToRig(TSharedPtr<class IDNAReader> InDNAReader, bool bInUpdateBlendShapes, bool bInUpdateSkinWeights)
{
	if (InDNAReader == nullptr)
	{
		return EIdentityErrorCode::InvalidDNA;
	}

	// Save the DNA and Delta DNA obtained the from service
	ClearDNABuffer();
	ClearRawDeltaDNABuffer();
	ClearCombinedDNABuffer();

	// It is presumed that DNA user manually applies is final (Autorigged DNA with deltas included) and ready for solver training.
	bIsAutoRigged = true;
	SetDNABuffer(ReadStreamFromDNA(InDNAReader.Get(), EDNADataLayer::All));

	return Finalize();
}

bool UMetaHumanIdentityFace::GetPredictiveSolversTaskConfig(FPredictiveSolversTaskConfig& OutConfig) const
{
	UCaptureData* CaptureData = nullptr;

	UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral);
	if (NeutralPose)
	{
		CaptureData = NeutralPose->GetCaptureData();
	}

	FString ConfigurationJson = DefaultSolver->FaceAnimationSolver->GetSolverConfigData(CaptureData);
	FString TemplateDescriptionJson = DefaultSolver->FaceAnimationSolver->GetSolverTemplateData(CaptureData);

	TWeakObjectPtr<UDNAAsset> DNAAsset = USkelMeshDNAUtils::GetMeshDNA(RigComponent->GetSkeletalMeshAsset());

	if (DNAAsset.IsExplicitlyNull() || !DNAAsset.IsValid())
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Face Skeletal Mesh doesn't have DNA asset attached."));
		return false;
	}

	OutConfig = {};
	OutConfig.TemplateDescriptionJson = TemplateDescriptionJson;
	OutConfig.ConfigurationJson = ConfigurationJson;
	OutConfig.DNAAsset = DNAAsset;
	OutConfig.PredictiveSolverTrainingData = DefaultSolver->PredictiveSolver->GetPredictiveTrainingData();
	OutConfig.PredictiveSolverGlobalTeethTrainingData = DefaultSolver->PredictiveSolver->GetPredictiveGlobalTeethTrainingData();
	OutConfig.bTrainPreviewSolvers = CVarTrainPreviewSolvers.GetValueOnAnyThread();

	return true;
}

bool UMetaHumanIdentityFace::RunPredictiveSolverTraining()
{
	if (!IModularFeatures::Get().IsModularFeatureAvailable(IPredictiveSolverInterface::GetModularFeatureName()))
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Unable to prepare Identity for Performance. Please make sure Depth Processing plugin is enabled. (Available on Fab)"));
		return false;
	}

	FPredictiveSolversTaskConfig Config;

	if (GetPredictiveSolversTaskConfig(Config))
	{
		TUniquePtr<FPredictiveSolversTask> Task = MakeUnique<FPredictiveSolversTask>(Config);
		FPredictiveSolversResult Result = Task->StartSync();

		if (Result.bSuccess)
		{
			SetPredictiveSolvers(Result.PredictiveSolvers);
			SetPredictiveWithoutTeethSolver(Result.PredictiveWithoutTeethSolver);

			return true;
		}
	}

	return false;
}

bool UMetaHumanIdentityFace::RunAsyncPredictiveSolverTraining(FOnPredictiveSolversProgress InOnProgressCallback, FOnPredictiveSolversCompleted InOnCompletedCallback)
{
	FPredictiveSolversTaskConfig Config;

	if (CurrentPredictiveSolversTask == nullptr && GetPredictiveSolversTaskConfig(Config))
	{
		CurrentPredictiveSolversTask = FPredictiveSolversTaskManager::Get().New(Config);

		if (CurrentPredictiveSolversTask)
		{
			FOnPredictiveSolversCompleted OnCompletedWrapper;
			OnCompletedWrapper.BindLambda([this, InOnCompletedCallback](FPredictiveSolversResult Result)
			{
				InOnCompletedCallback.ExecuteIfBound(MoveTemp(Result));

				// Be sure to dequeue and nullify solver task
				FPredictiveSolversTaskManager::Get().Remove(CurrentPredictiveSolversTask);
			});

			CurrentPredictiveSolversTask->OnCompletedCallback() = OnCompletedWrapper;
			CurrentPredictiveSolversTask->OnProgressCallback() = InOnProgressCallback;

			CurrentPredictiveSolversTask->StartAsync();

			return true;
		}
	}

	return false;
}

bool UMetaHumanIdentityFace::IsAsyncPredictiveSolverTrainingActive() const
{
	return CurrentPredictiveSolversTask && !CurrentPredictiveSolversTask->IsDone() && !CurrentPredictiveSolversTask->WasCancelled();
}

bool UMetaHumanIdentityFace::IsAsyncPredictiveSolverTrainingCancelling() const
{
	return CurrentPredictiveSolversTask && CurrentPredictiveSolversTask->WasCancelled();
}

void UMetaHumanIdentityFace::CancelAsyncPredictiveSolverTraining()
{
	if (IsAsyncPredictiveSolverTrainingActive())
	{
		CurrentPredictiveSolversTask->Cancel();
	}
}

bool UMetaHumanIdentityFace::PollAsyncPredictiveSolverTrainingProgress(float& OutProgress)
{
	if (IsAsyncPredictiveSolverTrainingActive())
	{
		return CurrentPredictiveSolversTask->PollProgress(OutProgress);
	}

	return false;
}

#endif

void UMetaHumanIdentityFace::LoadDefaultFaceFittingSolvers()
{
	if (DefaultSolver == nullptr)
	{
		static constexpr const TCHAR* GenericSolverPath = TEXT("/" UE_PLUGIN_NAME "/MeshFitting/GenericFaceFittingSolver.GenericFaceFittingSolver");
		if (UMetaHumanFaceFittingSolver* Solver = LoadObject<UMetaHumanFaceFittingSolver>(GetTransientPackage(), GenericSolverPath))
		{
			DefaultSolver = Solver;
		}
	}

	if (DefaultSolver != nullptr && !DefaultSolver->FaceAnimationSolver)
	{
		DefaultSolver->LoadFaceFittingSolvers();
	}

	if (IModularFeatures::Get().IsModularFeatureAvailable(IPredictiveSolverInterface::GetModularFeatureName()))
	{
		if (DefaultSolver != nullptr && !DefaultSolver->PredictiveSolver)
		{
			DefaultSolver->LoadPredictiveSolver();
		}
		
		check(DefaultSolver->PredictiveSolver)
	}

	check(DefaultSolver->FaceAnimationSolver);
}

void UMetaHumanIdentityFace::Initialize()
{
#if WITH_EDITOR

	if (TemplateMeshComponent != nullptr && TemplateMeshComponent->HeadMeshComponent->GetDynamicMesh()->IsEmpty())
	{
		TemplateMeshComponent->LoadMeshAssets();
	}

	const EIdentityErrorCode Initialized = InitializeRig();
	if (Initialized != EIdentityErrorCode::None)
	{
		UMetaHumanIdentity::HandleError(Initialized);
		return;
	}
#endif
}

FTransform UMetaHumanIdentityFace::GetTemplateMeshInitialTransform() const
{
	// Rotate mesh 90 degrees yaw and place on the same position as the skel mesh.
	// This is used to place template mesh on a more user-friendly location before we
	// conform the mesh. Once conform is done, head location will be readjusted.
	// For Z-axis value we're using template mesh bounding box location so template mesh
	// height aligns properly with the skel mesh. X-axis location is the same as in
	// Performance - we want to push template mesh backwards so it won't overlap with
	// the initial position of the camera.
	// NOTE: If we change the archetype and the head position changes, we will probably
	// need to update Z-axis value here.
	return FTransform
	(
		FRotator(0.0, 90.0, 0.0),
		FVector(85.0, 0.0, -141.922768),
		FVector::OneVector
	);
}

#if WITH_EDITOR
// Hard coded index values for picking the mesh section used to match an input asset against the template/archetype (Template to MH)
static const int32 Template2MHLODIndex = 0;
static const int32 Template2MHHeadMeshIndex = 0;

void UMetaHumanIdentityFace::CopyMeshVerticesFromExistingMesh(UCaptureData* CaptureData)
{
	if (UMeshCaptureData* MeshCaptureData = Cast<UMeshCaptureData>(CaptureData))
	{
		FMeshDescription* MeshDescriptionX = nullptr;
		UDynamicMesh* ConformalMesh = TemplateMeshComponent->GetPoseHeadMesh(EIdentityPoseType::Neutral);
		UE::Geometry::FDynamicMesh3& ConformalMeshRef = ConformalMesh->GetMeshRef();
		TArray<FVector3f> TemplateVertices;

		if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(MeshCaptureData->TargetMesh))
		{
			if (TSharedPtr<IDNAReader> ArchetypeDnaReader = UMetaHumanIdentityFace::GetPluginArchetypeDNAReader())
			{
				FDNAToSkelMeshMap* DNAToSkelMeshMap = USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(ArchetypeDnaReader.Get(), SkelMesh);
				TemplateVertices.AddUninitialized(DNAToSkelMeshMap->ImportDNAVtxToUEVtxIndex[Template2MHLODIndex][Template2MHHeadMeshIndex].Num());
				const FSkeletalMeshLODModel& LODModel = SkelMesh->GetImportedModel()->LODModels[Template2MHLODIndex];

				for (const FSkelMeshSection& Section : LODModel.Sections)
				{
					const int32& DNAMeshIndex = DNAToSkelMeshMap->ImportVtxToDNAMeshIndex[Template2MHLODIndex][Section.GetVertexBufferIndex()];
					if (DNAMeshIndex == Template2MHHeadMeshIndex)
					{
						const int32 NumSoftVertices = Section.GetNumVertices();
						int32 VertexBufferIndex = Section.GetVertexBufferIndex();
						for (int32 VertexIndex = 0; VertexIndex < NumSoftVertices; VertexIndex++)
						{
							const int32& DNAVertexIndex = DNAToSkelMeshMap->ImportVtxToDNAVtxIndex[Template2MHLODIndex][VertexBufferIndex++];

							if (DNAVertexIndex >= 0)
							{
								const FSoftSkinVertex& Vertex = Section.SoftVertices[VertexIndex];
								TemplateVertices[DNAVertexIndex] = Vertex.Position;
							}
						}
					}
				}

				TemplateMeshComponent->SetPoseHeadMeshVertices(EIdentityPoseType::Neutral, TemplateVertices, ETemplateVertexConversion::None);
				TemplateMeshComponent->SetTeethMeshVisibility(false);
				TemplateMeshComponent->SetEyeMeshesVisibility(false);
				SetTemplateMeshTransform(GetTemplateMeshInitialTransform());
				bIsConformed = true;
			}
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshCaptureData->TargetMesh))
		{
			FDynamicMesh3 NewMesh;
			FMeshDescriptionToDynamicMesh DynamicMeshConverter;
			DynamicMeshConverter.Convert(StaticMesh->GetMeshDescription(Template2MHHeadMeshIndex), NewMesh);
			if (ConformalMeshRef.VertexCount() == NewMesh.VertexCount())
			{
				for (FVector3d Vert : NewMesh.VerticesItr())
				{
					TemplateVertices.Add((FVector3f)Vert);
				}

				TemplateMeshComponent->SetPoseHeadMeshVertices(EIdentityPoseType::Neutral, TemplateVertices, ETemplateVertexConversion::None);
				ResetTemplateMeshTransform();
				bIsConformed = true;
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Mismatch in number of vertices when setting mesh for neutral pose. %d vertices provided but %d are expected"), NewMesh.VertexCount(), ConformalMeshRef.VertexCount());
			}
		}
	}
}

FString UMetaHumanIdentityFace::TargetTemplateCompatibilityAsString(ETargetTemplateCompatibility InCompatibility)
{
	return StaticEnum<ETargetTemplateCompatibility>()->GetDisplayNameTextByValue(static_cast<int64>(InCompatibility)).ToString();
}

ETargetTemplateCompatibility UMetaHumanIdentityFace::CheckTargetTemplateMesh(const UObject* InAsset)
{
	if (!InAsset->IsA<USkeletalMesh>() && !InAsset->IsA<UStaticMesh>())
	{
		return ETargetTemplateCompatibility::InvalidInputMesh;
	}

	TSharedPtr<IDNAReader> ArchetypeDnaReader = UMetaHumanIdentityFace::GetPluginArchetypeDNAReader();
	if (!ArchetypeDnaReader.IsValid())
	{
		return ETargetTemplateCompatibility::InvalidArchetype;
	}

	int32 ExpectedVertexCount = ArchetypeDnaReader->GetVertexPositionCount(Template2MHLODIndex);

	// Load the input mesh to check against the archetype
	if (const USkeletalMesh* SkelMesh = Cast<const USkeletalMesh>(InAsset))
	{
		if (!SkelMesh->HasMeshDescription(Template2MHLODIndex))
		{
			return ETargetTemplateCompatibility::MissingLOD;
		}

		const FMeshDescription* MeshDescription = SkelMesh->GetMeshDescription(Template2MHLODIndex);
		const FSkeletalMeshConstAttributes MeshAttributes(*MeshDescription);

		if (MeshAttributes.HasSourceGeometryParts())
		{
			if (Template2MHHeadMeshIndex >= MeshAttributes.GetNumSourceGeometryParts())
			{
				return ETargetTemplateCompatibility::MissingMeshInfo;
			}

			const TArrayView<const int32> GeometryPartInfo = MeshAttributes.GetSourceGeometryPartVertexOffsetAndCounts().Get(Template2MHHeadMeshIndex);
			const int32 VertexOffset = GeometryPartInfo[0];
			const int32 VertexCount = GeometryPartInfo[1];

			if (VertexCount != ExpectedVertexCount)
			{
				return ETargetTemplateCompatibility::MismatchNumVertices;
			}

			if (VertexOffset != 0)
			{
				return ETargetTemplateCompatibility::MismatchStartImportedVertex;
			}
		}
		else
		{
			// Check mesh compatibility for SkelMesh created from DNA file
			TArrayView<const unsigned short> MeshIndices = ArchetypeDnaReader->GetMeshIndicesForLOD(Template2MHLODIndex);
			int32 LODVertCount = 0;
			ExpectedVertexCount = MeshDescription->Vertices().Num();

			for (const unsigned short& Index : MeshIndices)
			{
				LODVertCount += ArchetypeDnaReader->GetVertexPositionCount(Index);
			}

			if (ExpectedVertexCount != LODVertCount)
			{
				return ETargetTemplateCompatibility::MismatchNumVertices;
			}
		}		
	}
	else if (const UStaticMesh* StaticMesh = Cast<const UStaticMesh>(InAsset))
	{
		FDynamicMesh3 ImportedMesh;
		FMeshDescriptionToDynamicMesh DynamicMeshConverter;
		DynamicMeshConverter.Convert(StaticMesh->GetMeshDescription(Template2MHHeadMeshIndex), ImportedMesh);
		// Check expected number of vertices against the dynamic mesh since dynamic mesh is used as a template.
		if (ExpectedVertexCount != ImportedMesh.VertexCount())
		{
			return ETargetTemplateCompatibility::MismatchNumVertices;
		}
	}

	return ETargetTemplateCompatibility::Valid;
}

TSharedPtr<IDNAReader> UMetaHumanIdentityFace::GetPluginArchetypeDNAReader()
{
	TSharedPtr<IDNAReader> ArchetypeDnaReader = nullptr;
	const FString PluginContentDir = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir();
	const FString PathToDNA = PluginContentDir / TEXT("IdentityTemplate/Face_Archetype.ardna");
	TArray<uint8> DNADataAsBuffer;
	if (FFileHelper::LoadFileToArray(DNADataAsBuffer, *PathToDNA))
	{
		ArchetypeDnaReader = ReadDNAFromBuffer(&DNADataAsBuffer, EDNADataLayer::All);
	}

	return ArchetypeDnaReader;
}

USkeletalMesh* UMetaHumanIdentityFace::CreateFaceArchetypeSkelmesh(const FString& InNewRigAssetName, const FString& InNewRigPath)
{
	FDNAInterchangeModule& DNAImportModule = FDNAInterchangeModule::GetModule();
	if (USkeletalMesh* SkelMeshAsset = DNAImportModule.ImportSync(InNewRigAssetName, InNewRigPath))
	{
		return SkelMeshAsset;
	}

	return nullptr;
}

EIdentityErrorCode UMetaHumanIdentityFace::InitializeRig()
{
	// If Rig component doesn't have skeletal mesh assigned, create a duplicate of Face_Archetype and assign it to it.
	if (!IsConformalRigValid())
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		FString NewRigAssetName, NewRigPath;

		const FString SanitizedBasePackageName = UPackageTools::SanitizePackageName(GetOutermost()->GetName());
		const FString PackagePath = FPackageName::GetLongPackagePath(SanitizedBasePackageName);
		const FString BaseAssetNameWithPrefix = TEXT("SK_") + FPackageName::GetLongPackageAssetName(SanitizedBasePackageName);
		const FString SanitizedBaseAssetNameWithPrefix = ObjectTools::SanitizeObjectName(BaseAssetNameWithPrefix);

		AssetTools.CreateUniqueAssetName(PackagePath / SanitizedBaseAssetNameWithPrefix, TEXT(""), NewRigPath, NewRigAssetName);
		NewRigPath = FPackageName::GetLongPackagePath(NewRigPath);

		USkeletalMesh* RigSkeletalMesh = CreateFaceArchetypeSkelmesh(NewRigAssetName, NewRigPath);
		if (RigSkeletalMesh == nullptr || RigSkeletalMesh->GetSkeleton() == nullptr)
		{
			return EIdentityErrorCode::CreateRigFromDNA;
		}

		// Set the FaceBoard ControlRig as the default 
		{
			const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
			TArray<FAssetData> FaceboardCRAssetData;
			AssetRegistry.GetAssetsByPackageName(TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/Face_ControlBoard_CtrlRig"), FaceboardCRAssetData);
			if (!FaceboardCRAssetData.IsEmpty())
			{
				FAssetData& FaceboardControlRigAsset = FaceboardCRAssetData[0];
				if (FaceboardControlRigAsset.IsValid())
				{
					RigSkeletalMesh->SetDefaultAnimatingRig(FaceboardControlRigAsset.GetAsset());
				}
				else
				{
					UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Invalid Face_ControlBoard_CtrlRig asset"));
				}
			}
		}

		// Get rid of the physics asset as it interferes with how bounding boxes are calculated
		RigSkeletalMesh->SetPhysicsAsset(nullptr);

		RigComponent->SetSkeletalMesh(RigSkeletalMesh);
		RigComponent->UpdateBounds();

		// Place skeletal mesh so it's visible well enough in the viewport
		RigComponent->SetWorldTransform(GetTemplateMeshInitialTransform());

		const FString TemplateBrowsPath = GetPluginContentDir() / TEXT("/IdentityTemplate/Face_Archetype_Brows.json");
		TArray<uint8> BrowsBuffer;
		if (!FFileHelper::LoadFileToArray(BrowsBuffer, *TemplateBrowsPath))
		{
			return EIdentityErrorCode::LoadBrows;
		}

		SetBrowsBuffer(BrowsBuffer);

		UDNAAsset* DNAAsset = USkelMeshDNAUtils::GetMeshDNA(RigComponent->GetSkeletalMeshAsset());
		if (DNAAsset)
		{
			SetDNABuffer(UE::Wrappers::FMetaHumanConformer::DNAToBuffer(DNAAsset));

			RigSkeletalMesh->MarkPackageDirty();
			RigSkeletalMesh->PostEditChange();

			return Finalize();
		}
		else
		{
			return EIdentityErrorCode::NoDNA;
		}
	}

	return EIdentityErrorCode::None;
}

#endif

void UMetaHumanIdentityFace::UpdateRigTransform()
{
	if (RigComponent != nullptr && TemplateMeshComponent != nullptr && bShouldUpdateRigComponent)
	{
		const FTransform TemplateMeshTransform = TemplateMeshComponent->GetComponentTransform();
		FTransform Transform = FTransform::Identity;
		FOpenCVHelperLocal::ConvertOpenCVToUnreal(Transform);
		Transform = FTransform(FRotator(0.0, 90.0, 180.0)) * Transform;

		const FTransform RigTransform = Transform * TemplateMeshTransform;

		RigComponent->SetWorldTransform(RigTransform);
		RigComponent->UpdateComponentToWorld();
		RigComponent->UpdateBounds();
		RigComponent->TransformUpdated.Broadcast(RigComponent, EUpdateTransformFlags::None, ETeleportType::None);
	}
}

UMetaHumanIdentityPose* UMetaHumanIdentityFace::FindPoseByType(EIdentityPoseType InPoseType) const
{
	const TObjectPtr<UMetaHumanIdentityPose>* PoseFound = Poses.FindByPredicate([InPoseType](UMetaHumanIdentityPose* Pose)
	{
		return Pose && Pose->PoseType == InPoseType;
	});

	if (PoseFound != nullptr)
	{
		return *PoseFound;
	}

	return nullptr;
}

void UMetaHumanIdentityFace::AddPoseOfType(EIdentityPoseType InPoseType, UMetaHumanIdentityPose* InPose)
{
	if (InPose != nullptr && FindPoseByType(InPoseType) == nullptr)
	{
		InPose->PoseName = FText::FromString(FString::Format(TEXT("{0} Pose"), { UMetaHumanIdentityPose::PoseTypeAsString(InPoseType) }));
		InPose->PoseType = InPoseType;

		Poses.Add(InPose);
	}
}

bool UMetaHumanIdentityFace::RemovePose(UMetaHumanIdentityPose* InPose)
{
	return Poses.Remove(InPose) == 1;
}

const TArray<UMetaHumanIdentityPose*>& UMetaHumanIdentityFace::GetPoses() const
{
	return Poses;
}

void UMetaHumanIdentityFace::ShowHeadMeshForPose(EIdentityPoseType InPoseType)
{
	if (TemplateMeshComponent != nullptr)
	{
		TemplateMeshComponent->ShowHeadMeshForPose(InPoseType);
	}
}

FTransform UMetaHumanIdentityFace::GetFrontalViewFrameTransform() const
{
	FTransform Transform;

	if (UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral))
	{
		if (UMetaHumanIdentityPromotedFrame* FrontalViewFrame = NeutralPose->GetFrontalViewPromotedFrame())
		{
			Transform = FrontalViewFrame->HeadAlignment;
		}
	}

	return Transform;
}

void UMetaHumanIdentityFace::SetTemplateMeshTransform(const FTransform& InTransform, bool bInUpdateRigTransform)
{
	if (TemplateMeshComponent != nullptr)
	{
		TemplateMeshComponent->SetWorldTransform(InTransform);
		TemplateMeshComponent->UpdateComponentToWorld();
		TemplateMeshComponent->TransformUpdated.Broadcast(TemplateMeshComponent, EUpdateTransformFlags::None, ETeleportType::None);
		TemplateMeshComponent->UpdateBounds();
	}

	if (bInUpdateRigTransform)
	{
		UpdateRigTransform();
	}
}

void UMetaHumanIdentityFace::ResetTemplateMeshTransform()
{
	constexpr bool bUpdateRigTransform = true;

	FTransform NewTransform = GetTemplateMeshInitialTransform();
	if (bIsConformed)
	{
		// If the mesh is already conformed, apply the UE To RigSpace transform in the initial transform so the mesh is aligned properly with the view and not upside down
		NewTransform = UMetaHumanTemplateMeshComponent::UEToRigSpaceTransform * NewTransform;
	}

	SetTemplateMeshTransform(NewTransform, bUpdateRigTransform);
}

void UMetaHumanIdentityFace::GetConformalVerticesForAutoRigging(TArray<FVector>& OutConformedFaceVertices, 
																TArray<FVector>& OutConformedLeftEyeVertices, 
																TArray<FVector>& OutConformedRightEyeVertices) const
{
	if (TemplateMeshComponent != nullptr)
	{
		// Transform applied to vertices before submitting to autorigging
		const FTransform Transform = FTransform::Identity;
		TemplateMeshComponent->GetPoseHeadMeshVertices(EIdentityPoseType::Neutral, Transform, ETemplateVertexConversion::UEToConformer, OutConformedFaceVertices);

		if (bHasFittedEyes)
		{
			TemplateMeshComponent->GetEyeMeshesVertices(Transform, ETemplateVertexConversion::UEToConformer, OutConformedLeftEyeVertices, OutConformedRightEyeVertices);
		}
	}
}

TMap<EIdentityPartMeshes, TArray<FVector>> UMetaHumanIdentityFace::GetConformalVerticesWorldPos(EIdentityPoseType InPoseType) const
{
	return GetConformalVerticesForTransform(TemplateMeshComponent->GetComponentTransform(), InPoseType);
}

TMap<EIdentityPartMeshes, TArray<FVector>> UMetaHumanIdentityFace::GetConformalVerticesForTransform(const FTransform& InMeshTransform, EIdentityPoseType InPoseType) const
{
	TMap<EIdentityPartMeshes, TArray<FVector>> Vertices;
	TArray<FVector> HeadVerts, LeftEyeVerts, RightEyeVerts, TeethVerts;
	TemplateMeshComponent->GetPoseHeadMeshVertices(InPoseType, InMeshTransform, ETemplateVertexConversion::None, HeadVerts);
	TemplateMeshComponent->GetEyeMeshesVertices(InMeshTransform, ETemplateVertexConversion::None, LeftEyeVerts, RightEyeVerts);
	TemplateMeshComponent->GetTeethMeshVertices(InMeshTransform, ETemplateVertexConversion::None, TeethVerts);

	Vertices.Add(EIdentityPartMeshes::Head, HeadVerts);
	Vertices.Add(EIdentityPartMeshes::LeftEye, LeftEyeVerts);
	Vertices.Add(EIdentityPartMeshes::RightEye, RightEyeVerts);
	Vertices.Add(EIdentityPartMeshes::Teeth, TeethVerts);

	return Vertices;
}

TArray<FCameraCalibration> UMetaHumanIdentityFace::GetCalibrationsForPoseAndFrame(UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const
{
	TArray<FCameraCalibration> CalibrationList;

	for (int32 FrameIndex = 0; FrameIndex < InPose->PromotedFrames.Num(); ++FrameIndex)
	{
		UMetaHumanIdentityPromotedFrame* PromotedFrame = InPose->PromotedFrames[FrameIndex];

		if (PromotedFrame == InPromotedFrame)
		{
			TArray<FCameraCalibration> FrameCalibrations = GetCalibrations(InPose, PromotedFrame, FrameIndex);
			CalibrationList.Append(FrameCalibrations);
		}
	}

	return CalibrationList;
}

TArray<FCameraCalibration> UMetaHumanIdentityFace::GetCalibrations(UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex) const
{
	TArray<FCameraCalibration> CalibrationList;

	const FString FrameName = GetFrameNameForConforming(InPromotedFrame, InFrameIndex);

	if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(InPromotedFrame))
	{
		const int32 SyntheticWidth = UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.X;
		const int32 SyntheticHeight = UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.Y;

		FCameraCalibration Calibration;
		Calibration.CameraId = CombineFrameNameAndCameraViewName(FrameName, FString()); // just use a dummy empty camera view here for M2MH case
		Calibration.ImageSize.X = SyntheticWidth;
		Calibration.ImageSize.Y = SyntheticHeight;
		Calibration.PrincipalPoint.X = SyntheticWidth * 0.5;
		Calibration.PrincipalPoint.Y = SyntheticHeight * 0.5;

		// convert FOV angle to focal length in pixels using:
		//  FOV angle = 2 x arctan (sensor size / 2 f )

		const double ViewFOV = CameraFrame->CameraViewFOV;
		Calibration.FocalLength.X = SyntheticWidth * 0.5 / FMath::Tan(ViewFOV * TMathUtilConstants<float>::Pi / 360.0);
		Calibration.FocalLength.Y = SyntheticHeight * 0.5 / FMath::Tan(ViewFOV * TMathUtilConstants<float>::Pi / 360.0);

		FTransform CameraTransform = CameraFrame->GetCameraTransform();
		FOpenCVHelperLocal::ConvertUnrealToOpenCV(CameraTransform);

		// camera model matrix is the inverse of the camera position and orientation
		Calibration.Transform = CameraTransform.Inverse().ToMatrixWithScale();

		CalibrationList.Add(Calibration);
	}
	else if (UFootageCaptureData* CaptureData = Cast<UFootageCaptureData>(InPose->GetCaptureData()))
	{
		if (!CaptureData->CameraCalibrations.IsEmpty())
		{
			TArray<FCameraCalibration> FrameCalibrationList;
			TArray<TPair<FString, FString>> StereoReconstructionPairs;
			CaptureData->CameraCalibrations[0]->ConvertToTrackerNodeCameraModels(FrameCalibrationList, StereoReconstructionPairs);

			for (int32 CalibIndex = 0; CalibIndex < CaptureData->CameraCalibrations[0]->CameraCalibrations.Num(); ++CalibIndex)
			{
				// ensure that calibration name matches that which will be used for the annotation and depth map data
				FrameCalibrationList[CalibIndex].CameraId = CombineFrameNameAndCameraViewName(FrameName, CaptureData->CameraCalibrations[0]->CameraCalibrations[CalibIndex].Name);
			}

			CalibrationList.Append(FrameCalibrationList);
		}
	}

	return CalibrationList;
}


FString UMetaHumanIdentityFace::GetFullCameraName(UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityPromotedFrame* InPromotedFrame, const FString& InBaseCameraName) const
{
	int32 PromotedFrameIndex = -1;
	for (int32 Frame = 0; Frame < InPose->PromotedFrames.Num(); ++Frame)
	{
		if (InPose->PromotedFrames[Frame] == InPromotedFrame)
		{
			PromotedFrameIndex = Frame;
			break;
		}
	}

	verify(PromotedFrameIndex >= 0);

	const FString FrameName = GetFrameNameForConforming(InPromotedFrame, PromotedFrameIndex);
	return CombineFrameNameAndCameraViewName(FrameName, InBaseCameraName);
}


TArray<FCameraCalibration> UMetaHumanIdentityFace::GetCalibrations(UMetaHumanIdentityPose* InPose) const
{
	TArray<UMetaHumanIdentityPromotedFrame*> PromotedFrames = InPose->GetValidContourDataFramesFrontFirst();
	TArray<FCameraCalibration> CalibrationList;

	for (int32 FrameIndex = 0; FrameIndex < PromotedFrames.Num(); ++FrameIndex)
	{
		UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFrames[FrameIndex];

		TArray<FCameraCalibration> FrameCalibrationList = GetCalibrations(InPose, PromotedFrame, FrameIndex);
		CalibrationList.Append(FrameCalibrationList);
	}

	return CalibrationList;
}

bool UMetaHumanIdentityFace::SetConformerCameraParameters(UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer) const
{
	return OutConformer.SetCameras(GetCalibrations(InPose));
}

bool UMetaHumanIdentityFace::SetConformerScanInputData(const UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer, bool & bOutInvalidMeshTopology) const
{
	if (UMeshCaptureData* CaptureData = Cast<UMeshCaptureData>(InPose->GetCaptureData()))
	{
		TSortedMap<FString, const FFrameTrackingContourData*> ActiveFrameWithData;

		const TArray<UMetaHumanIdentityPromotedFrame*> PromotedFrames = InPose->GetValidContourDataFramesFrontFirst();

		for (int32 FrameIndex = 0; FrameIndex < PromotedFrames.Num(); ++FrameIndex)
		{
			UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFrames[FrameIndex];
			ActiveFrameWithData.Add(CombineFrameNameAndCameraViewName(GetFrameNameForConforming(PromotedFrame, FrameIndex), InPose->Camera), &PromotedFrame->ContourData->FrameTrackingContourData);
		}

		if (CVarEnableExportMeshes.GetValueOnAnyThread())
		{
			if (UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(CaptureData->TargetMesh))
			{
				WriteTargetMeshToFile(TargetStaticMesh);
			}
		}

		TArray<float> Vertices;
		TArray<int32> Triangles;
		CaptureData->GetDataForConforming(InPose->PoseTransform, Vertices, Triangles);
		
		return OutConformer.SetScanInputData(ActiveFrameWithData, {}, Triangles, Vertices, bOutInvalidMeshTopology);
	}

	return false;
}

bool UMetaHumanIdentityFace::SetConformerDepthInputData(const UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer) const
{
	bool bSetDepth = false;

	if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(InPose->GetCaptureData()))
	{
		//for (const FFootageCaptureView& View : FootageCaptureData->Views)
		// TODO just use the first view for now; the structure of UFootageCaptureData needs reviewing in a later version
		verify(!FootageCaptureData->DepthSequences.IsEmpty());

		int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(InPose->Camera);

		verify(ViewIndex >= 0 && ViewIndex < FootageCaptureData->DepthSequences.Num());


		verify(ViewIndex >= 0 && ViewIndex < FootageCaptureData->ImageSequences.Num());
		FFrameRate TargetFrameRate;

		TObjectPtr<class UImgMediaSource> ColorSequence = FootageCaptureData->ImageSequences[ViewIndex];
		TargetFrameRate = ColorSequence->FrameRateOverride;

		TObjectPtr<class UImgMediaSource> DepthSequence = FootageCaptureData->DepthSequences[ViewIndex];
		{
			verify(DepthSequence != nullptr);

			TArray<FString> DepthImageNames;
			const FString DepthImagesPath = DepthSequence->GetFullPath();
			IFileManager::Get().FindFiles(DepthImageNames, *DepthImagesPath);
			TArray<UMetaHumanIdentityPromotedFrame*> ContourDataFramesFrontFirst = InPose->GetValidContourDataFramesFrontFirst();

			FString FirstPromotedFrameName;
			// this is a work-around, so that we represent cameras in the same way as the ActorCreationAPI is expecting them for
			// footage to MetaHuman ie we have a single camera model with the head viewed from multiple transforms (single video camera and
			// single depth camera). Therefore, we just give all frames the same camera name
			if (ContourDataFramesFrontFirst.Num() > 0)
			{
				FirstPromotedFrameName = CombineFrameNameAndCameraViewName(GetFrameNameForConforming(ContourDataFramesFrontFirst[0], 0), InPose->Camera);
			}

			for (UMetaHumanIdentityPromotedFrame* PromotedFrame : ContourDataFramesFrontFirst)
			{
				if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(PromotedFrame))
				{
					TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
					TRange<FFrameNumber> ProcessingLimitFrameRange, MaxFrameRange;

					FootageCaptureData->GetFrameRanges(DepthSequence->FrameRateOverride, InPose->TimecodeAlignment, false, MediaFrameRanges, ProcessingLimitFrameRange, MaxFrameRange);

					FString DepthFilePath;
					int32 DepthFrameOffset = 0;
					int32 DepthNumFrames = 0;

					TUniquePtr<UE::MetaHuman::FFramePathResolver> FramePathResolver;
					if (FTrackingPathUtils::GetTrackingFilePathAndInfo(DepthSequence, DepthFilePath, DepthFrameOffset, DepthNumFrames))
					{
						const int32 FrameNumberOffset = DepthFrameOffset - MediaFrameRanges[DepthSequence].GetLowerBoundValue().Value;

						UE::MetaHuman::FFrameNumberTransformer FrameNumberTransformer(DepthSequence->FrameRateOverride, TargetFrameRate, FrameNumberOffset);
						FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolver>(DepthFilePath, MoveTemp(FrameNumberTransformer));
					}

					TMap<FString, const FFrameTrackingContourData*> ActiveFrameWithData = {
						{ FirstPromotedFrameName, &PromotedFrame->ContourData->FrameTrackingContourData }
					};

					if (FramePathResolver)
					{
						const FString DepthImagePath = FramePathResolver->ResolvePath(FootageFrame->FrameNumber);

						UE_LOG(LogMetaHumanIdentity, Log, TEXT("Resolved promoted frame (%d) to depth path %s"), FootageFrame->FrameNumber, *DepthImagePath);

						TArray<uint8> ImageData;

						if (FFileHelper::LoadFileToArray(ImageData, *DepthImagePath))
						{
							IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
							TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);

							if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
							{
								if (ImageWrapper->GetBitDepth() == 32 && ImageWrapper->GetFormat() == ERGBFormat::GrayF)
								{
									int32 Width = ImageWrapper->GetWidth();
									int32 Height = ImageWrapper->GetHeight();
									TArray<uint8> IntData;

									if (ImageWrapper->GetRaw(ERGBFormat::GrayF, 32, IntData))
									{
										const float* FloatData = static_cast<const float*>((void*)IntData.GetData());

										TMap<FString, const float*> DepthMaps =
										{
											{ CombineFrameNameAndCameraViewName(GetFrameNameForConforming(ContourDataFramesFrontFirst[0], 0), DepthSuffix), FloatData }
										};

										if (!OutConformer.SetDepthInputData(ActiveFrameWithData, DepthMaps))
										{
											return false;
										}

										bSetDepth = true;
									}
								}
							}
						}
						else
						{				
							UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Could not resolve depth frame path for promoted frame (%s) during MetaHuman Identity Solve"), *DepthImagePath);
						}
					}
					else
					{
						UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Could not resolve depth frame path for promoted frame number (%d) during MetaHuman Identity Solve"), FootageFrame->FrameNumber);
					}
				}
			}
		}
	}

	return bSetDepth;
}

bool UMetaHumanIdentityFace::SaveDebuggingData(UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer, const FString& InAssetSavedFolder) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const FString DebuggingFolderBase = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName());
	const FString DebuggingFolder = DebuggingFolderBase / InPose->GetName();
	if (!PlatformFile.DirectoryExists(*DebuggingFolderBase))
	{
		bool bCreatedFolder = PlatformFile.CreateDirectory(*DebuggingFolderBase);
		if (!bCreatedFolder)
		{
			UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to create folder to save debugging data during mesh fitting"));
			return false;
		}
	}

	if (!PlatformFile.DirectoryExists(*DebuggingFolder))
	{
		bool bCreatedFolder = PlatformFile.CreateDirectory(*DebuggingFolder);
		if (!bCreatedFolder)
		{
			UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to create folder to save debugging data during mesh fitting"));
			return false;
		}
	}

	// save the image data and depth map data for each frame for footage to MetaHuman
	if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(InPose->GetCaptureData()))
	{
		const FString ImageFolder = DebuggingFolder / FString("color");
		if (!PlatformFile.DirectoryExists(*ImageFolder))
		{
			bool bCreatedFolder = PlatformFile.CreateDirectory(*ImageFolder);
			if (!bCreatedFolder)
			{
				UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to save debugging data (images) during mesh fitting"));
				return false;
			}
		}

		//for (const FFootageCaptureView& View : FootageCaptureData->Views)
		// TODO just use the first view for now; the structure of UFootageCaptureData needs reviewing in a later version
		verify(!FootageCaptureData->ImageSequences.IsEmpty());
		verify(!FootageCaptureData->DepthSequences.IsEmpty());

		int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(InPose->Camera);

		verify(ViewIndex >= 0 && ViewIndex < FootageCaptureData->ImageSequences.Num() && ViewIndex < FootageCaptureData->DepthSequences.Num());

		TObjectPtr<class UImgMediaSource> ImageSequence = FootageCaptureData->ImageSequences[ViewIndex];
		{
			if (ImageSequence)
			{
				TArray<FString> ImageNames;
				FString ImagesPath;
				bool bGetImageNames = FImageSequenceUtils::GetImageSequencePathAndFilesFromAsset(ImageSequence, ImagesPath, ImageNames);
				if (!bGetImageNames)
				{
					UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to save debugging data (images) during mesh fitting"));
					return false;
				}
				int32 FrameNumber = 0;

				for (UMetaHumanIdentityPromotedFrame* PromotedFrame : InPose->GetValidContourDataFramesFrontFirst())
				{
					if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(PromotedFrame))
					{
						int32 FrameIndex = FootageFrame->FrameNumber;

						// copy the file into the target folder and give it a new frame number
						const FString Filename = FString::Printf(TEXT("%06d"), FrameNumber) + FPaths::GetExtension(ImageNames[FrameIndex], true);
						FrameNumber++;
						FString ImageFileDest = ImageFolder / Filename;
						FString ImageFileSrc = ImagesPath / ImageNames[FrameIndex];
						bool bCopied = PlatformFile.CopyFile(*ImageFileDest, *ImageFileSrc);
						if (!bCopied)
						{
							UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to save debugging data (images) during mesh fitting"));
							return false;
						}
					}
				}
			}
		}

		const FString DepthMapFolder = DebuggingFolder / FString("depth");
		if (!PlatformFile.DirectoryExists(*DepthMapFolder))
		{
			bool bCreatedFolder = PlatformFile.CreateDirectory(*DepthMapFolder);
			if (!bCreatedFolder)
			{
				UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to save debugging data (depth-maps) during mesh fitting"));
				return false;
			}
		}

		//for (const FFootageCaptureView& View : FootageCaptureData->Views)
		// TODO just use the first view for now; the structure of UFootageCaptureData needs reviewing in a later version
		TObjectPtr<class UImgMediaSource> DepthSequence = FootageCaptureData->DepthSequences[ViewIndex];
		{
			if (DepthSequence)
			{
				TArray<FString> DepthMapNames;
				FString DepthMapPath;
				bool bGetDepthNames = FImageSequenceUtils::GetImageSequencePathAndFilesFromAsset(DepthSequence, DepthMapPath, DepthMapNames);
				if (!bGetDepthNames)
				{
					UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to save debugging data (depth maps) during mesh fitting"));
					return false;
				}


				int32 FrameNumber = 0;

				for (UMetaHumanIdentityPromotedFrame* PromotedFrame : InPose->GetValidContourDataFramesFrontFirst())
				{
					if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(PromotedFrame))
					{
						int32 FrameIndex = FootageFrame->FrameNumber;

						// copy the file into the target folder and give it a new frame number
						const FString Filename = FString::Printf(TEXT("%06d"), FrameNumber) + FPaths::GetExtension(DepthMapNames[FrameIndex], true);
						FrameNumber++;
						FString DepthMapFileDest = DepthMapFolder / Filename;
						FString DepthMapFileSrc = DepthMapPath / DepthMapNames[FrameIndex];
						bool bCopied = PlatformFile.CopyFile(*DepthMapFileDest, *DepthMapFileSrc);
						if (!bCopied)
						{
							UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to save debugging data (depth-maps) during mesh fitting"));
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

EIdentityErrorCode UMetaHumanIdentityFace::RunMeshConformer(UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer)
{
	TArray<float> ConformalVertsFace, ConformalVertsLeftEye, ConformalVertsRightEye;
	TArray<float> StackedToScanTransforms;
	TArray<float> StackedScales;
	EIdentityErrorCode ConformerReturnCode = EIdentityErrorCode::None;

	FString DebuggingFolder = "";
	if (CVarEnableExportMeshes.GetValueOnAnyThread())
	{
		FString DebuggingFolderBase = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName());
		DebuggingFolder = DebuggingFolderBase / InPose->GetName();

		// note that we don't check whether saving debugging data has been successful
		SaveDebuggingData(InPose, OutConformer, DebuggingFolderBase);
	}

	if (UMetaHumanIdentityPose* NeutralPose = FindPoseByType(EIdentityPoseType::Neutral))
	{
		bHasFittedEyes = NeutralPose->bFitEyes;
	}

	ConformerReturnCode = OutConformer.FitIdentity(ConformalVertsFace, ConformalVertsLeftEye, ConformalVertsRightEye, StackedToScanTransforms, StackedScales, bHasFittedEyes, DebuggingFolder);
	if (ConformerReturnCode == EIdentityErrorCode::None)
	{
		TemplateMeshComponent->ResetMeshes();

		const TConstArrayView<FVector3f> ConformalVertsFaceView((FVector3f*)ConformalVertsFace.GetData(), ConformalVertsFace.Num() / 3);

		// When fitting the Neutral pose, set booth neutral and teeth poses to use the same set of vertices so they align in the viewport
		TemplateMeshComponent->SetPoseHeadMeshVertices(EIdentityPoseType::Neutral, ConformalVertsFaceView, ETemplateVertexConversion::ConformerToUE);
		TemplateMeshComponent->SetPoseHeadMeshVertices(EIdentityPoseType::Teeth, ConformalVertsFaceView, ETemplateVertexConversion::ConformerToUE);

		TemplateMeshComponent->ShowHeadMeshForPose(EIdentityPoseType::Neutral);

		// For an identity that has already been conformed, bake UEToRigSpaceTransform in the teeth
		// meshes so we can position the original teeth mesh correctly. If we don't do this
		// the original teeth mesh will be placed upside down.
		TemplateMeshComponent->BakeTeethMeshTransform(UMetaHumanTemplateMeshComponent::UEToRigSpaceTransform);

		const TConstArrayView<FMatrix44f> StackedTransformsView((FMatrix44f*)StackedToScanTransforms.GetData(), StackedToScanTransforms.Num() / 16);
		SetHeadAlignmentForPose(InPose, StackedTransformsView, StackedScales);

		if (bHasFittedEyes && !ConformalVertsLeftEye.IsEmpty() && !ConformalVertsRightEye.IsEmpty())
		{
			// transform the eye vertices into UE coord system but keep them in  rig space
			const TConstArrayView<FVector3f> ConformalVertsLeftEyeView((FVector3f*)ConformalVertsLeftEye.GetData(), ConformalVertsLeftEye.Num() / 3);
			const TConstArrayView<FVector3f> ConformalVertsRightEyeView((FVector3f*)ConformalVertsRightEye.GetData(), ConformalVertsRightEye.Num() / 3);
			TemplateMeshComponent->SetEyeMeshesVertices(ConformalVertsLeftEyeView, ConformalVertsRightEyeView, ETemplateVertexConversion::ConformerToUE);
		}
		else
		{
			// if not fitting eyes, update the eye meshes transform so the meshes are aligned correctly with the mesh from the conformer.
			// Even though the eyes are going to be hidden by default, in case the user turns the visibility on the eyes will be in a sensible location
			// relative to the face
			TemplateMeshComponent->BakeEyeMeshesTransform(UMetaHumanTemplateMeshComponent::UEToRigSpaceTransform);
		}

		// Only show the eye meshes if fitting eyes
		TemplateMeshComponent->SetEyeMeshesVisibility(bHasFittedEyes);

		if (CVarEnableExportMeshes.GetValueOnAnyThread())
		{
			WriteConformalVerticesToFile(InPose->GetName());
		}

		bIsConformed = true;
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Unable to fit the mesh"));
		bIsConformed = false;
	}

	return ConformerReturnCode;
}

void UMetaHumanIdentityFace::WriteConformalVerticesToFile(const FString& InNameSuffix) const
{
	TArray<FString> Data;

	Data.Add("# This file uses centimeters as units for non-parametric coordinates.");
	Data.Add("");
	Data.Add("mtllib mean.mtl");
	Data.Add("g default");

	TArray<FVector> ConformedFaceVertices, ConformedLeftEyeVertices, ConformedRightEyeVertices;
	GetConformalVerticesForAutoRigging(ConformedFaceVertices, ConformedLeftEyeVertices, ConformedRightEyeVertices);

	for (FVector Vertex : ConformedFaceVertices)
	{
		// Transform the vertex back to UE space
		Vertex = FVector(Vertex.Z, Vertex.X, -Vertex.Y);

		// Finally, transform it to Obj space (flip Z and Y) so it is exported in the correct orientation
		Data.Add(FString::Format(TEXT("v {0} {1} {2}"), { Vertex.X, Vertex.Z, Vertex.Y }));
	}

	const FString PathToMeanObj = GetPluginContentDir() / TEXT("MeshFitting/Template/mean.obj");

	TArray<FString> Faces;
	FFileHelper::LoadFileToStringArrayWithPredicate(Faces, *PathToMeanObj, [](const FString& Line)
	{
		return Line.StartsWith("f ") || Line.StartsWith("vt ") || Line.StartsWith("vn ");
	});

	for (const FString& FaceString : Faces)
	{
		Data.Add(FaceString);
	}

	const FString PathToConformalObject = FPaths::ProjectSavedDir() / FPaths::GetCleanFilename(GetOuter()->GetName()) / FString::Format(TEXT("ConformalFaceMesh_{0}.obj"), { InNameSuffix });
	FFileHelper::SaveStringArrayToFile(Data, *PathToConformalObject);
}

void UMetaHumanIdentityFace::WriteTargetMeshToFile(UStaticMesh* InTargetMesh, const FString& InNameSuffix) const
{
#if WITH_EDITOR
	if (InTargetMesh != nullptr)
	{
		TArray<UExporter*> Exporters;
		ObjectTools::AssembleListOfExporters(Exporters);

		UExporter* ObjExporter = nullptr;
		for (int32 ExporterIndex = Exporters.Num() - 1; ExporterIndex >= 0; --ExporterIndex)
		{
			UExporter* Exporter = Exporters[ExporterIndex];
			if (Exporter->SupportedClass == UStaticMesh::StaticClass() &&
				Exporter->FormatExtension.Contains(TEXT("OBJ")))
			{
				ObjExporter = Exporter;
				break;
			}
		}

		if (ObjExporter != nullptr)
		{
			UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
			FGCObjectScopeGuard ExportTaskGuard(ExportTask);
			ExportTask->Object = InTargetMesh;
			ExportTask->Exporter = ObjExporter;
			ExportTask->Filename = FPaths::ProjectSavedDir() / FString::Format(TEXT("{0}_ScannedMesh{1}.obj"), { GetOuter()->GetName(), InNameSuffix });
			ExportTask->bSelected = false;
			ExportTask->bReplaceIdentical = true;
			ExportTask->bPrompt = false;
			ExportTask->bUseFileArchive = true;
			ExportTask->bWriteEmptyFiles = false;
			UExporter::RunAssetExportTask(ExportTask);
		}
	}
#endif
}

FString UMetaHumanIdentityFace::GetPluginContentDir() const
{
	return IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir();
}

FString UMetaHumanIdentityFace::GetFrameNameForConforming(UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex) const
{
	FString FrameName = FString::Format(TEXT("Frame_{0}_{1}"), { InFrameIndex, FText::TrimPrecedingAndTrailing(InPromotedFrame->FrameName).ToString() });

	if (InPromotedFrame->bIsFrontView)
	{
		// The conformer takes a map from frame names to contour data.
		// As the name of the frame can be changed freely by users we cannot guarantee order and the API currently expects the front frame to be the first one
		// Adding the prefix Frontal_ forces it to be first in a TSortedMap and std::map, which is used internally by the mesh conformer
		FrameName = FString::Format(TEXT("Frontal_{0}"), { FrameName });
	}
	else
	{
		// non frontal frames are named with the prefix NonFrontal which will mean they are sorted AFTER the frontal frame
		FrameName = FString::Format(TEXT("NonFrontal_{0}"), { FrameName });
	}

	return FrameName;
}

//////////////////////////////////////////////////////////////////////////
// UMetaHumanIdentityBody

UMetaHumanIdentityBody::UMetaHumanIdentityBody()
	: Super{}
	, Height(1)
	, BodyTypeIndex(INDEX_NONE)
{
}

FText UMetaHumanIdentityBody::GetPartName() const
{
	return LOCTEXT("IdentityBodyComponentName", "Body");
}

FText UMetaHumanIdentityBody::GetPartDescription() const
{
	return LOCTEXT("IdentityBodyComponentDescription", "The Body of the MetaHuman Identity");
}

FSlateIcon UMetaHumanIdentityBody::GetPartIcon(const FName& InPropertyName) const
{
	return FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("Identity.Body.Part") };
}

FText UMetaHumanIdentityBody::GetPartTooltip(const FName& InPropertyName) const
{
	return LOCTEXT("IdentityPartBody", "Body Part of MetaHuman Identity\nUse Details panel to set the body type before using Mesh to MetaHuman button");
}

bool UMetaHumanIdentityBody::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	OutDiagnosticsWarningMessage = {};
	return false;
}


//////////////////////////////////////////////////////////////////////////
// UMetaHumanIdentityHands

UMetaHumanIdentityHands::UMetaHumanIdentityHands()
	: Super{}
{
}

FText UMetaHumanIdentityHands::GetPartName() const
{
	return LOCTEXT("IdentityHandComponentName", "Hands");
}

FText UMetaHumanIdentityHands::GetPartDescription() const
{
	return LOCTEXT("IdentityHandComponentDescription", "The Hands of the MetaHuman Identity");
}

FSlateIcon UMetaHumanIdentityHands::GetPartIcon(const FName& InPropertyName) const
{
	return FSlateIcon{};
}

FText UMetaHumanIdentityHands::GetPartTooltip(const FName& InPropertyName) const
{
	return LOCTEXT("IdentityPartHands", "Identity Hands Part");
}

bool UMetaHumanIdentityHands::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	OutDiagnosticsWarningMessage = {};
	return false;
}

//////////////////////////////////////////////////////////////////////////
// UMetaHumanIdentityOutfit

UMetaHumanIdentityOutfit::UMetaHumanIdentityOutfit()
	: Super{}
{
}

FText UMetaHumanIdentityOutfit::GetPartName() const
{
	return LOCTEXT("IdentityOutfitComponentName", "Outfit");
}

FText UMetaHumanIdentityOutfit::GetPartDescription() const
{
	return LOCTEXT("IdentityOutfitComponentDescription", "The Outfit of the MetaHuman Identity");
}

FSlateIcon UMetaHumanIdentityOutfit::GetPartIcon(const FName& InPropertyName) const
{
	return FSlateIcon{};
}

FText UMetaHumanIdentityOutfit::GetPartTooltip(const FName& InPropertyName) const
{
	return LOCTEXT("IdentityPartOutfit", "Identity Prop Part");
}

bool UMetaHumanIdentityOutfit::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	OutDiagnosticsWarningMessage = {};
	return false;
}

//////////////////////////////////////////////////////////////////////////
// UMetaHumanIdentityProp

UMetaHumanIdentityProp::UMetaHumanIdentityProp()
	: Super{}
{
}

FText UMetaHumanIdentityProp::GetPartName() const
{
	return LOCTEXT("IdentityPropComponentName", "Prop");
}

FText UMetaHumanIdentityProp::GetPartDescription() const
{
	return LOCTEXT("IdentityPropComponentDescription", "A Prop for the MetaHuman Identity");
}

FSlateIcon UMetaHumanIdentityProp::GetPartIcon(const FName& InPropertyName) const
{
	return FSlateIcon{};
}

FText UMetaHumanIdentityProp::GetPartTooltip(const FName& InPropertyName) const
{
	return LOCTEXT("IdentityPartProp", "Identity Prop Part");
}

bool UMetaHumanIdentityProp::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	OutDiagnosticsWarningMessage = {};
	return false;
}

#undef LOCTEXT_NAMESPACE
