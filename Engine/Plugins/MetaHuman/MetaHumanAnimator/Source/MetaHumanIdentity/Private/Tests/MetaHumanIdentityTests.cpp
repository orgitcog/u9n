// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Rendering/SkeletalMeshModel.h"

#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanFaceContourTrackerAsset.h"

#include "CaptureData.h"

#include "Engine/SkeletalMesh.h"
#include "DNAInterchangeModule.h"
#include "SkeletalMeshAttributes.h"


#if WITH_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMetaHumanIdentityTest, "MetaHuman.Identity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanIdentityTemplate2MHTest, "MetaHuman.Identity.Template to MH", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FMetaHumanIdentityTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("Pose");
	Tests.Add("Face");

	for (const FString& Test : Tests)
	{
		OutBeautifiedNames.Add(Test);
		OutTestCommands.Add(Test);
	}
}

bool FMetaHumanIdentityTest::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;
	if (InTestCommand == "Pose")
	{
		UMetaHumanIdentityPose* Pose = NewObject<UMetaHumanIdentityPose>();
		UTEST_NULL("Newly created pose doesn't have capture data set", Pose->GetCaptureData());
		UTEST_FALSE("Newly created pose doesn't have capture data set", Pose->IsCaptureDataValid());
		UTEST_TRUE("Newly created pose doesn't have valid frames", Pose->PromotedFrames.IsEmpty());
		UTEST_NULL("Newly created pose doesn't have valid promoted frame class", Pose->PromotedFrameClass);
		UTEST_TRUE("Newly created pose doesn't have valid frames", Pose->GetAllPromotedFramesWithValidContourData().IsEmpty());
		UTEST_TRUE("Newly created pose doesn't have valid frames", Pose->GetValidContourDataFramesFrontFirst().IsEmpty());
		UTEST_NULL("Newly created pose doesn't have frontal view frame", Pose->GetFrontalViewPromotedFrame());
		UTEST_EQUAL("Newly created pose doesn't have its type set", Pose->PoseType, EIdentityPoseType::Invalid);
		UTEST_TRUE("Newly created pose doesn't have name set", Pose->PoseName.IsEmptyOrWhitespace());
		UTEST_NULL("Newly created pose doesn't have a capture data scene component", Pose->CaptureDataSceneComponent);
		UTEST_FALSE("Newly created pose has eye fitting disabled", Pose->bFitEyes);
		UTEST_EQUAL("Newly create pose has identity pose transform", Pose->PoseTransform, FTransform::Identity);

		static constexpr const TCHAR* GenericTrackerPath = TEXT("/" UE_PLUGIN_NAME "/GenericTracker/GenericFaceContourTracker.GenericFaceContourTracker");
		UMetaHumanFaceContourTrackerAsset* DefaultTracker = LoadObject<UMetaHumanFaceContourTrackerAsset>(nullptr, GenericTrackerPath);
		UTEST_NOT_NULL("Default tracker is valid", DefaultTracker);

		UTEST_TRUE("Newly created pose should load the default tracker", Pose->DefaultTracker == DefaultTracker);
	}
	else if (InTestCommand == "Face")
	{
		UMetaHumanIdentityFace* Face = NewObject<UMetaHumanIdentityFace>();
		UTEST_FALSE("Cannot conform newly created faces", Face->CanConform());
		UTEST_TRUE("Face should not have poses when newly created", Face->GetPoses().IsEmpty());
		UTEST_EQUAL("Face should be named 'Face'", Face->GetPartName().ToString(), NSLOCTEXT("FaceTest", "FacePartName", "Face").ToString());
		UTEST_FALSE("Newly created face can't conform", Face->CanConform());

		UMetaHumanIdentityPose* NeutralPose = NewObject<UMetaHumanIdentityPose>(Face);
		UTEST_NOT_NULL("Neutral pose should be valid", NeutralPose);

		Face->AddPoseOfType(EIdentityPoseType::Neutral, NeutralPose);
		UTEST_TRUE("Face has neutral pose", Face->FindPoseByType(EIdentityPoseType::Neutral) == NeutralPose);

		UTEST_FALSE("Face cannot conform without an initialized captured data", Face->CanConform());

		UMeshCaptureData* CaptureData = NewObject<UMeshCaptureData>();
		UTEST_NOT_NULL("CaptureData should be valid", CaptureData);
		UTEST_FALSE("CaptureData is not initialized by default", CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

		// Load some static mesh to validate the capture data
		CaptureData->TargetMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'"));

		UTEST_TRUE("CaptureData should be valid with a TargetMesh set", CaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

		NeutralPose->SetCaptureData(CaptureData);

		int32 FrameIndex = 0;
		UMetaHumanIdentityPromotedFrame* PromotedFrame = NeutralPose->AddNewPromotedFrame(FrameIndex);
		UTEST_NOT_NULL("PromotedFrame should be valid", PromotedFrame);
		UTEST_TRUE("PromotedFrame should be a camera frame", PromotedFrame->IsA<UMetaHumanIdentityCameraFrame>());
		UTEST_FALSE("Frame shouldn't be set a front frame", PromotedFrame->bIsFrontView);
		UTEST_TRUE("Frame should be created as used to solve", PromotedFrame->bUseToSolve);

		UTEST_FALSE("Cannot conform without valid promoted frames", Face->CanConform());

		// Create some dummy tracking contour data
		FTrackingContour TrackingContour;
		TrackingContour.DensePoints.Add(FVector2D::ZeroVector);
		TrackingContour.State.bActive = true;
		PromotedFrame->ContourData->FrameTrackingContourData.TrackingContours.Add(TEXT("Some Curve"), TrackingContour);

		UTEST_FALSE("Cannot conform without the front frame set", Face->CanConform());

		PromotedFrame->bIsFrontView = true;

		UTEST_TRUE("Should be able to conform with a frame set as the front view", Face->CanConform());
	}
	else
	{
		bIsOK &= TestTrue(TEXT("Known test"), false);
	}

	return bIsOK;
}

/**
 *  Template to MetaHuman related test
 */

bool FMetaHumanIdentityTemplate2MHTest::RunTest(const FString& Parameters)
{
	FDNAInterchangeModule& DNAImportModule = FDNAInterchangeModule::GetModule();
	const FString OutputDir = FPaths::ProjectIntermediateDir() / "IdentityTest";
	USkeletalMesh* PluginSkelMesh = DNAImportModule.ImportSync("Template_2MHMesh", "/Game/TestData/");
	bool bIsOK = TestNotNull(TEXT("Valid Archetype"), PluginSkelMesh);

	// Create a incompatible static mesh using a default editor object
	FString LoadedObjectType = TEXT("EditorCylinder");
	UObject* EditorCylinder = (UStaticMesh*)StaticLoadObject(UStaticMesh::StaticClass(),
		NULL,
		*FString::Printf(TEXT("/Engine/EditorMeshes/%s.%s"), *LoadedObjectType, *LoadedObjectType),
		NULL,
		LOAD_None,
		NULL);

#if WITH_EDITOR
	// Test an incompatible static mesh fails
	bIsOK = TestEqual(TEXT("Incompatible Static Mesh"), UMetaHumanIdentityFace::CheckTargetTemplateMesh(EditorCylinder), ETargetTemplateCompatibility::MismatchNumVertices);
	// Test a compatible skeletal mesh succeeds
	bIsOK = TestEqual(TEXT("Compatible Skeletal Mesh"), UMetaHumanIdentityFace::CheckTargetTemplateMesh(PluginSkelMesh), ETargetTemplateCompatibility::Valid);
#endif

	return bIsOK;
}

#endif
