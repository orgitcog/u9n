// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ConstructorHelpers.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/SkeletalMesh.h"
#include "MetaHumanCharacterIdentity.h"
#include "SkelMeshDNAUtils.h"
#include "MetaHumanRigEvaluatedState.h"
#include "MetaHumanCharacterEditorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanCoreTechLibTest, Verbose, All)

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTestMetaHumanCharacterIdentityTest, "MetaHuman.Creator.MetaHumanCharacterIdentityTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTestMetaHumanCharacterIdentityTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add("Basic test of MetaHumanCharacterIdentity API for full MH");
	OutTestCommands.Add("MetaHumanCharacterIdentity_MH");

	// turn off FN test for now
	//OutBeautifiedNames.Add("Basic test of MetaHumanCharacterIdentity API for FN");
	//OutTestCommands.Add("MetaHumanCharacterIdentity_FN");
}

bool CheckNumVertices(TSharedPtr<FMetaHumanCharacterIdentity::FState> InState, int32 InExpectedNumVertices)
{
	bool bResult = true;
	TArray<FVector3f> Vertices = InState->Evaluate().Vertices;
	if (Vertices.Num() != InExpectedNumVertices)
	{
		UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of vertices in FState::Evaluate. Expected %d, got %d."), InExpectedNumVertices, Vertices.Num());
		bResult = false;
	}

	return bResult;
}

bool CheckValues(const TArray<FVector3f> & InNewPositions, const TArray<FVector3f>& InOldPositions, const FString & InLabel, float InMaxTolerance = 0.000001f, float InMedianTolerance = 0.000001f)
{
	bool bResult = true;
	TArray<float> Diffs;
	Diffs.SetNumUninitialized(InOldPositions.Num());

	if (InNewPositions.Num() != InOldPositions.Num())
	{
		UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of %s. Expected %d, got %d."), *InLabel, InOldPositions.Num(), InNewPositions.Num());
		bResult = false;
	}
	else
	{
		float MaxDiff = 0;
		int32 MaxDiffInd = -1;

		for (int32 I = 0; I < InOldPositions.Num(); ++I)
		{
			const float Diff = (InOldPositions[I] - InNewPositions[I]).Length();
			if (Diff > MaxDiff)
			{
				MaxDiff = Diff;
				MaxDiffInd = I;
			}
			Diffs[I] = Diff;
		}

		if (MaxDiff > InMaxTolerance)
		{
			UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Max difference of %f between point in %s exceeds tolerance of %f for vertex %d"), MaxDiff, *InLabel, InMaxTolerance, MaxDiffInd);
			bResult = false;
		}

		const float MedianDiff = Diffs[Diffs.Num() / 2];
		if (MedianDiff > InMedianTolerance)
		{
			UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Median difference of %f between point in %s exceeds tolerance of %f"), MedianDiff, *InLabel, InMedianTolerance);
			bResult = false;
		}
	}

	return bResult;
}

TArray<FVector3f> GetHeadMeshVertices(UDNAAsset* InDNAAsset)
{
#if WITH_EDITORONLY_DATA // can only use InDNAAsset->GetGeometryReader() in editor
	TArray<FVector3f> HeadMeshVertices;
	TSharedPtr<IDNAReader> GeometryReader = InDNAAsset->GetGeometryReader();
	HeadMeshVertices.SetNum(GeometryReader->GetVertexPositionCount(0));
	for (int32 I = 0; I < static_cast<int32>(GeometryReader->GetVertexPositionCount(0)); ++I)
	{
		HeadMeshVertices[I] = FVector3f{ static_cast<float>(GeometryReader->GetVertexPosition(0, I).X),
			static_cast<float>(GeometryReader->GetVertexPosition(0, I).Y), 
			static_cast<float>(GeometryReader->GetVertexPosition(0, I).Z) };
	}
	return HeadMeshVertices;
#else
	UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("MHC API only works with EditorOnly Data "));
	{
		return {};
	}
#endif
}


bool FTestMetaHumanCharacterIdentityTest::RunTest(const FString& InParameters)
{
	bool bResult = true;

	if (InParameters == "MetaHumanCharacterIdentity_MH" || InParameters == "MetaHumanCharacterIdentity_FN")
	{
		// load a Skel Mesh with an embedded DNA
		FString ExampleSkelMeshPath{};
		FString MHCDataRelativePath{};
		FString MHCBodyDataRelativePath{};
		int32 ExpectedNumberOfPresets{};
		int32 ExpectedNumVertices{};
		int32 ExpectedNumGizmos{};
		int32 ExpectedNumLandmarks{};

		if (InParameters == "MetaHumanCharacterIdentity_MH")
		{
			ExampleSkelMeshPath = TEXT("/MetaHumanCharacter/Face/IdentityTemplate/SKM_Face.dna");
			MHCDataRelativePath = TEXT("/Content/Face/IdentityTemplate/");
			MHCBodyDataRelativePath = TEXT("/Content/Body/IdentityTemplate/");
			ExpectedNumberOfPresets = 0;
			ExpectedNumVertices = 69614;
			ExpectedNumGizmos = 22;
			ExpectedNumLandmarks = 79;
		}
		else if (InParameters == "MetaHumanCharacterIdentity_FN")
		{
			ExampleSkelMeshPath = TEXT("/MetaHumanCharacter/Face/SKM_FN_Face.SKM_FN_Face");
			MHCDataRelativePath = TEXT("/Content/Face/IdentityTemplate/FN/");
			MHCBodyDataRelativePath = TEXT("/Content/Body/IdentityTemplate/FN/");
			ExpectedNumberOfPresets = 122;
			ExpectedNumVertices = 3153;
			ExpectedNumGizmos = 19;
			ExpectedNumLandmarks = 42;
		}

		USkeletalMesh* ExampleSkelMesh = UMetaHumanCharacterEditorSubsystem::GetFaceArchetypeMesh(EMetaHumanCharacterTemplateType::MetaHuman);
		if (!ExampleSkelMesh)
		{
			UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Failed to load SkeletalMesh asset from path: %s"), *ExampleSkelMeshPath);
			bResult = false;
		}
		else
		{
			// extract the DNA
			UAssetUserData* UserData = ExampleSkelMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass());
			if (UserData)
			{
				UDNAAsset* DNAAsset = Cast<UDNAAsset>(UserData);

				if (!DNAAsset)
				{
					UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Failed to extract UDNAAsset data from SkeletalMesh asset from path: %s"), *ExampleSkelMeshPath);
					bResult = false;
				}
				else
				{
					const auto HeadOrientation = EMetaHumanCharacterOrientation::Y_UP; 
					FMetaHumanCharacterIdentity MetaHumanCharacterIdentity;
					FString MHCDataPath;
					FString MHCBodyDataPath;
					FString PluginDir = FPaths::ConvertRelativePathToFull(*IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir());				

					if (!PluginDir.IsEmpty())
					{
						MHCDataPath = PluginDir + MHCDataRelativePath;
						MHCBodyDataPath = PluginDir + MHCBodyDataRelativePath;
						TArray<FVector3f> OrigHeadMeshVertices = GetHeadMeshVertices(DNAAsset);

						bool bInitializedAPI = MetaHumanCharacterIdentity.Init(MHCDataPath, MHCBodyDataPath, DNAAsset, HeadOrientation);

						if (bInitializedAPI)
						{
							TArray<FString> PresetNames = MetaHumanCharacterIdentity.GetPresetNames();
							if (PresetNames.Num() != ExpectedNumberOfPresets)
							{
								UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Incorrect number of presets. Found %d, expected %d"), PresetNames.Num(), ExpectedNumberOfPresets);
								bResult = false;
							}

							TSharedPtr<FMetaHumanCharacterIdentity::FState> State = MetaHumanCharacterIdentity.CreateState();

							if (!State)
							{
								UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Failed to create FMetaHumaneCharacterIdentity state"));
								bResult = false;
							}
							else
							{

								// test / exercise the API
								TArray<FVector3f> OrigVertices = State->Evaluate().Vertices;

								TArray<FVector3f> NewLandmarks, NewVertices;
								if (OrigVertices.Num() != ExpectedNumVertices)
								{
									UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of vertices in FState::Evaluate. Expected %d, got %d."), ExpectedNumVertices, OrigVertices.Num());
									bResult = false;
								}

								TArray<FVector3f> OrigGizmoPositions = State->EvaluateGizmos(OrigVertices);
								if (OrigGizmoPositions.Num() != ExpectedNumGizmos)
								{
									UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of gizmo positions in FState::EvaluateGizmos. Expected %d, got %d."), ExpectedNumGizmos, OrigGizmoPositions.Num());
									bResult = false;
								}

								TArray<FVector3f> OrigLandmarks = State->EvaluateLandmarks(OrigVertices);
								if (OrigLandmarks.Num() != ExpectedNumLandmarks)
								{
									UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of landmarks in FState::EvaluateLandmarks. Expected %d, got %d."), ExpectedNumLandmarks, OrigLandmarks.Num());
									bResult = false;
								}

								check(!State->HasLandmark(2)); // check there is not a landmark there already
								State->AddLandmark(2);
								NewLandmarks = State->EvaluateLandmarks(OrigVertices);
								if (NewLandmarks.Num() != ExpectedNumLandmarks + 2) // note that 2 landmarks are added due to use of symmetry
								{
									UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of landmarks in FState::EvaluateLandmarks. Expected %d, got %d."), ExpectedNumLandmarks + 2, NewLandmarks.Num());
									bResult = false;
								}

								// add another landmark and then take it away
								check(!State->HasLandmark(1));
								State->AddLandmark(1);
								State->RemoveLandmark(State->NumLandmarks() - 1); // remove the last one we added; note that RemoveLandmark uses landmark indices and not vertex indices
								check(!State->HasLandmark(1));
								NewLandmarks = State->EvaluateLandmarks(OrigVertices);
								if (NewLandmarks.Num() != ExpectedNumLandmarks + 2)
								{
									UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of landmarks in FState::EvaluateLandmarks. Expected %d, got %d."), ExpectedNumLandmarks + 2, NewLandmarks.Num());
									bResult = false;
								}

								State->Reset(); 
								NewVertices = State->Evaluate().Vertices; 
								bResult = CheckValues(NewVertices, OrigVertices, FString(TEXT("vertices")), 0.000001f, 0.000001f);

								TArray<FVector3f> NewGizmoPositions = State->EvaluateGizmos(NewVertices);
								bResult = CheckValues(NewGizmoPositions, OrigGizmoPositions, FString(TEXT("gizmos")), 0.000001f, 0.000001f);

								// at the moment we don't expect landmarks to be reset in terms of number, so should still be the same number as before the reset
								NewLandmarks = State->EvaluateLandmarks(OrigVertices);
								if (NewLandmarks.Num() != ExpectedNumLandmarks + 2)
								{
									UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected number of landmarks in FState::EvaluateLandmarks. Expected %d, got %d."), ExpectedNumLandmarks + 2, NewLandmarks.Num());
									bResult = false;
								}

								State->Randomize(/*magnitude*/ 0.5f);
								bResult &= CheckNumVertices(State, ExpectedNumVertices);

								if (PresetNames.Num() > 0)
								{
									State->GetPreset(PresetNames[0], /*preset type*/0, /*preset region*/0);
								}
								else
								{
									//bResult = false;
								}

								bResult &= CheckNumVertices(State, ExpectedNumVertices);
								
								FVector3f GizmoPosition;
								State->GetGizmoPosition(1, GizmoPosition);
								GizmoPosition += FVector3f{ 1.0f, 1.0f, 11.0f };
								State->SetGizmoPosition(1 /*gizmo index*/, GizmoPosition, /*bInSymmetric*/ true, /*bInEnforceBounds*/ true); // delta translation
								bResult &= CheckNumVertices(State, ExpectedNumVertices);
								
								FVector3f GizmoRotation;
								State->GetGizmoRotation(2, GizmoRotation);
								GizmoRotation += FVector3f{ 5.0f, -10.0f, 0.0f };
								State->SetGizmoRotation(2 /*gizmo index*/, GizmoRotation, /*bInSymmetric*/ true, /*bInEnforceBounds*/ true); // delta eulers
								bResult &= CheckNumVertices(State, ExpectedNumVertices);
								
								State->TranslateLandmark(10 /*landmark index*/, { 0.5f, -0.1f, 1.0f }, /*bInSymmetric*/ true); // delta translation
								bResult &= CheckNumVertices(State, ExpectedNumVertices);
								
								// reset the neck exclusion mask so fitting to target should give the same result for the head mesh vertices
								State->ResetNeckExclusionMask();
								
								// test FitToTarget to the original vertices to check that this is still giving correct results
								// TODO this causes a crash so commenting it out for now; needs investigation
								FFitToTargetOptions FitToTargetOptions;
								FitToTargetOptions.AlignmentOptions = EAlignmentOptions::None;
								FitToTargetOptions.bAdaptNeck = false;
								FitToTargetOptions.bDisableHighFrequencyDelta = true;
								State->FitToTarget(TMap<int, TArray<FVector3f>>{ { 0, OrigHeadMeshVertices } }, FitToTargetOptions);
								NewVertices = State->Evaluate().Vertices; 

								// use GetVertex to extract the head mesh vertices from the state and check that they are the same as originally
								TArray<FVector3f> NewHeadMeshVertices = OrigHeadMeshVertices; // just take a copy of the correct size
								for (int32 I = 0; I < NewHeadMeshVertices.Num(); ++I)
								{
									NewHeadMeshVertices[I] = State->GetVertex(NewVertices, /*DNAMeshIndex*/ 0, /*DNAVertexIndex*/ I);
								}

								// TODO still not sure why the differences here are as large as they are. When testing on the titan side 
								// with a dna which is the face archetype the differences are much smaller.
								bResult &= CheckValues(NewHeadMeshVertices, OrigHeadMeshVertices, FString(TEXT("head mesh vertices extracted from state")), 0.001f, 0.001f);

							}
						}
						else
						{
							UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Failed to initialize MetaHumanCharacterIdentity"));
							bResult = false;
						}
					}
					else
					{
						UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Failed to find plugin directory"));
						bResult = false;
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogMetaHumanCoreTechLibTest, Error, TEXT("Unexpected test: %s"), *InParameters);
		bResult = false;
	}

	return bResult;
}

#endif