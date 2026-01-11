// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "UDynamicMesh.h"
#include "Engine/StaticMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshTransforms.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "MetaHumanIdentityAssetEditorToolkit.h"
#include "UI/SMetaHumanIdentityPartsEditor.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanTemplateMeshComponent.h"
#include "MetaHumanIdentityViewportSettings.h"

#include "SkelMeshDNAReader.h"
#include "ControlRigBlueprint.h"
#include "DNAAsset.h"


#if WITH_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMetaHumanIdentityEditorTest, "MetaHuman.Identity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FRunIdentityCreationCheck, FAutomationTestBase*, Test, FMetaHumanIdentityAssetEditorToolkit*, Toolkit,
	UMetaHumanIdentity*, Identity, UE::Geometry::FDynamicMesh3, GoldDataMesh);


/**
 *  Waits until textures are streamed or number of attempts has exceeded
 */
class FWaitForTexturesToStream : public IAutomationLatentCommand
{
public:
	FWaitForTexturesToStream(UStaticMeshComponent* InMeshComponent) :
		MeshComponent(InMeshComponent)
	{
	}

	virtual bool Update() override
	{
		TArray<UTexture*> Textures;
		MeshComponent->GetUsedTextures(Textures, EMaterialQualityLevel::Num);

		for (const auto& Tex : Textures)
		{
			if (!Tex->IsFullyStreamedIn())
			{
				return ++CurrentAttempt >= NumOfAttempts;
			}
		}

		return true;
	}

private:
	UStaticMeshComponent* MeshComponent;
	int32 NumOfAttempts = 200;
	int32 CurrentAttempt = 0;
};

bool FRunIdentityCreationCheck::Update()
{
	UMetaHumanIdentityFace* Face = Cast<UMetaHumanIdentityFace>(Identity->GetOrCreatePartOfClass(UMetaHumanIdentityFace::StaticClass()));

	UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral);
	TObjectPtr<UMetaHumanIdentityPromotedFrame> PromotedFrame = NeutralPose->PromotedFrames[0];
	Toolkit->HandleTrackCurrent();
	Face->Conform();

	// Test against the existing conformed mesh
	UDynamicMesh* Head = Face->TemplateMeshComponent->GetPoseHeadMesh(EIdentityPoseType::Neutral);
	UE::Geometry::FDynamicMesh3 DynMesh = Head->GetMeshRef();

	Face->SetTemplateMeshTransform(PromotedFrame->HeadAlignment, true /*bUpdateRigTransform*/);
	UE::Geometry::FTransformSRT3d TemplateMeshTransform = Face->TemplateMeshComponent->GetRelativeTransform();
	MeshTransforms::ApplyTransform(DynMesh, TemplateMeshTransform, true /*bReverseOrientationIfNeeded*/);

	// Due to floating point precision, check if the greatest difference between vertices is small number
	UE::Geometry::TDynamicVector<FVector3d> GoldVertData = GoldDataMesh.GetVerticesBuffer();
	UE::Geometry::TDynamicVector<FVector3d> LocalVertData = DynMesh.GetVerticesBuffer();
	float MaxLength = TNumericLimits<float>::Lowest();
	
	for (int32 Ctr = 0; Ctr < GoldDataMesh.GetVerticesBuffer().Num(); ++Ctr)
	{
		float Length = FVector::Distance(GoldVertData[Ctr], LocalVertData[Ctr]);
		if (Length > MaxLength)
		{
			MaxLength = Length;
		}
	}

	Test->TestLessThan(TEXT("Check conforming from mesh"), MaxLength, 0.005f, UE_SMALL_NUMBER);

	return true;
}

void FMetaHumanIdentityEditorTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("Blueprint");

	/**
	 * IdFromMesh test checks if identity can be successfully created using mesh as input data
	 * It uses the "gold data" generated using the very same test it's running. It was not straightforward to
	 * get the selection contour (golden halo around viewport component) to appear and it's presence affects the
	 * results. In order to update the "gold data" this test would have to be run manually in UE. As part of the
	 * test, asset editor is opened. To update, please export the template mesh from the editor to required location
	 */

	//TODO: Fix the test outside horde environment
	//Tests.Add("IdFromMesh");

	for (const FString& Test : Tests)
	{
		OutBeautifiedNames.Add(Test);
		OutTestCommands.Add(Test);
	}
}

bool FMetaHumanIdentityEditorTest::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	if (InTestCommand == "Blueprint")
	{
		const FString PluginFile = "/" UE_PLUGIN_NAME "/IdentityTemplate/Face_PostProcess_AnimBP.Face_PostProcess_AnimBP";
		const FName AnimGraphName = FName(TEXT("AnimGraph"));

		// Load blueprints to check
		UAnimBlueprint* PluginBlueprint = LoadObject<UAnimBlueprint>(GetTransientPackage(), *PluginFile);
		bIsOK &= TestNotNull(TEXT("Plugin Blueprint"), PluginBlueprint);

		if (bIsOK)
		{
			// Check name			
			bIsOK &= TestEqual(TEXT("Blueprint name"), PluginBlueprint->GetName(), TEXT("Face_PostProcess_AnimBP"));

			// Check dependencies
			const TSet<TWeakObjectPtr<UBlueprint>>& PluginDependencies = PluginBlueprint->CachedDependencies;
			bIsOK &= TestEqual(TEXT("Blueprint dependencies count"), PluginDependencies.Num(), 2);

			// Check graphs
			TArray<UEdGraph*> PluginGraphs;
			PluginBlueprint->GetAllGraphs(PluginGraphs);
			bIsOK &= TestEqual(TEXT("Graph count"), PluginGraphs.Num(), 2);

			if (bIsOK)
			{
				bool bAnimGraphExists = false;
				for (int32 GraphIndex = 0; GraphIndex < PluginGraphs.Num(); ++GraphIndex)
				{
					UEdGraph* PluginGraph = PluginGraphs[GraphIndex];
					FName PluginGraphName = PluginGraph->GetFName();

					if (PluginGraphName == AnimGraphName)
					{
						bAnimGraphExists = true;

						// We expect the node counts on the AnimGraph to be different. We want the test to pickup any changes here so we can manually review.
						// This is because we removed the RBF neck correctives from the plugin to reduce the plugin size.
						bIsOK &= TestEqual(TEXT("Plugin Postprocess AnimGraph Node count"), PluginGraph->Nodes.Num(), 7);
					}
				}

				bIsOK &= TestTrue("AnimGraph exists", bAnimGraphExists);
			}
		}
	}
	else if (InTestCommand == "IdFromMesh")
	{
		// IMPORTANT NOTE: Meshes generated for test data were produced with UseSelectionOutline option swithced OFF
		// This affects the tracking and subsequently conformed mesh vertex positions.
		bool bLatentTestRunning = false;

		if (UMetaHumanIdentity* MockIdentity = NewObject<UMetaHumanIdentity>(GetTransientPackage()))
		{
			UMetaHumanIdentityFace* Face = Cast<UMetaHumanIdentityFace>(MockIdentity->GetOrCreatePartOfClass(UMetaHumanIdentityFace::StaticClass()));
			UMetaHumanIdentityPose* NeutralPose = NewObject<UMetaHumanIdentityPose>(Face, UMetaHumanIdentityPose::StaticClass(), NAME_None, RF_Transactional);
			Face->AddPoseOfType(EIdentityPoseType::Neutral, NeutralPose);

			UMeshCaptureData* CaptureData = NewObject<UMeshCaptureData>();
			bool EditorOpened = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(MockIdentity);

			UTEST_TRUE("Asset editor should be successfully opened", EditorOpened);

			// Load a conformed skelmesh used for testing
			UStaticMesh* TheMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/MetaHuman/TestData/Meshes/Ada_StaticMesh.Ada_StaticMesh"));
			CaptureData->TargetMesh = TheMesh;
			NeutralPose->SetCaptureData(CaptureData);

			int32 PromotedFrameIndex = 0;
			UMetaHumanIdentityPromotedFrame* PromotedFrame = NeutralPose->AddNewPromotedFrame(PromotedFrameIndex);
			PromotedFrame->bIsFrontView = true;

			if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(PromotedFrame))
			{
				CameraFrame->ViewMode = EViewModeIndex::VMI_Unlit;
				CameraFrame->CameraViewFOV = 45.0f;
				CameraFrame->ViewLocation = FVector(0.0, 60.0, 140.0);
				CameraFrame->ViewRotation = FRotator(0.0, -90.0, 0.0);
				CameraFrame->LookAtLocation = FVector(0.0, 0.0, 0.0);
				CameraFrame->bIsNavigationLocked = true;

				IAssetEditorInstance* MockIDEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(MockIdentity, false /*bFocusIfOpen*/);

				MockIdentity->SetBlockingProcessing(true);
				if (FMetaHumanIdentityAssetEditorToolkit* IdEditor = static_cast<FMetaHumanIdentityAssetEditorToolkit*>(MockIDEditor))
				{
					UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(NeutralPose->CaptureDataSceneComponent);
					auto CompInstance = Cast<UStaticMeshComponent>(IdEditor->GetIdentityPartsEditor()->GetPrimitiveComponent(Comp, true));
					CompInstance->bForceMipStreaming = true;

					FFrameTrackingContourData Contours = IdEditor->GetPoseSpecificContourDataForPromotedFrame(CameraFrame, NeutralPose);
					const FString ConfigVersion = "0.0.0";
					CameraFrame->InitializeMarkersFromParsedConfig(Contours, ConfigVersion);

					MockIdentity->ViewportSettings->SetSelectedPromotedFrame(EIdentityPoseType::Neutral, 0);
					IdEditor->HandleIdentityTreeSelectionChanged(NeutralPose, EIdentityTreeNodeIdentifier::FaceNeutralPose);

					if (UStaticMesh* GoldenConformedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/MetaHuman/TestData/Meshes/Remeshed_Ada_StaticMesh.Remeshed_Ada_StaticMesh")))
					{
						FMeshDescriptionToDynamicMesh GetLOD0Mesh;
						UE::Geometry::FDynamicMesh3 GoldDataMesh;
						const FMeshDescription* MeshDescription = GoldenConformedMesh->GetMeshDescription(0);
						GetLOD0Mesh.Convert(MeshDescription, GoldDataMesh);

						// Latent loading of the textures has to happen before tracking
						ADD_LATENT_AUTOMATION_COMMAND(FWaitForTexturesToStream(CompInstance));
						// This runs a latent test to check if the template mesh of created identity matches the stored one
						ADD_LATENT_AUTOMATION_COMMAND(FRunIdentityCreationCheck(this, IdEditor, MockIdentity, GoldDataMesh));

						bLatentTestRunning = true;
					}
				}
			}
		}

		bIsOK &= bLatentTestRunning;
	}

	return bIsOK;
}

#endif
