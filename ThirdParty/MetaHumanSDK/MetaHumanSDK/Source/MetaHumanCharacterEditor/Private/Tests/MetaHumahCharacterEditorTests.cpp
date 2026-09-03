// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Find.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Editor/EditorEngine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "UObject/NameTypes.h"
#include "UObject/ScriptInterface.h"
#include "Stats/StatsMisc.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "ImageUtils.h"
#include "SkelMeshDNAUtils.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterTextureSynthesis.h"
#include "MetaHumanFaceTextureSynthesizer.h"
#include "MetaHumanRigEvaluatedState.h"
#include "Cloud/MetaHumanTextureSynthesisServiceRequest.h"
#include "Cloud/MetaHumanARServiceRequest.h"

extern UNREALED_API UEditorEngine* GEditor;

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanCharacterEditorTests, Log, All)


struct FScopedTestWorld
{
	FScopedTestWorld()
	{
		const FName UniqueWorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("MetaHumanCharacterTestWorld"));
		World = NewObject<UWorld>(GetTransientPackage(), UniqueWorldName);
		World->WorldType = EWorldType::EditorPreview;

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(World->WorldType);
		WorldContext.SetCurrentWorld(World);

		World->CreatePhysicsScene(nullptr);
		World->InitializeNewWorld();

		FURL URL;
		World->InitializeActorsForPlay(URL);
	}

	~FScopedTestWorld()
	{
		GEngine->DestroyWorldContext(World);
		const bool bInformEngineOfWorld = false;
		World->DestroyWorld(bInformEngineOfWorld);
	}

	UWorld* World = nullptr;
};

static bool CheckSynthesizedTexturesInFaceMaterial(const UMaterialInterface* FaceMaterial, const UMetaHumanCharacter* Character, EFaceTextureType TextureType)
{
	check(FaceMaterial);
	check(Character);

	if (!Character->SynthesizedFaceTextures.Contains(TextureType))
	{
		return false;
	}

	// Get the Texture for the slot with the same name as the TextureType
	const FName TextureSlotName = FName(StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(TextureType)));
	UTexture* OutTexture = nullptr;
	if (!FaceMaterial->GetTextureParameterValue(FHashedMaterialParameterInfo(TextureSlotName), OutTexture))
	{
		return false;
	}

	if (OutTexture == nullptr)
	{
		return false;
	}

	return Character->SynthesizedFaceTextures[TextureType].Get() == Cast<UTexture2D>(OutTexture);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCharacterEditorTest, "MetaHuman.Creator.Character", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterEditorTest::RunTest(const FString& InParams)
{
	UMetaHumanCharacter* Character = NewObject<UMetaHumanCharacter>(GetTransientPackage());
	UTEST_FALSE_EXPR(Character->IsCharacterValid());
	UTEST_TRUE("Initial FaceStateData is empty", Character->GetFaceStateData().GetSize() == 0);
	UTEST_TRUE("Initial BodyStateData is empty", Character->GetBodyStateData().GetSize() == 0);
	UTEST_TRUE("MetaHuman Character Synthesized Face Textures are empty for new Character", Character->SynthesizedFaceTextures.IsEmpty());

	UTEST_NOT_NULL_EXPR(GEditor);
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	UTEST_NOT_NULL_EXPR(MetaHumanCharacterSubsystem);

	MetaHumanCharacterSubsystem->InitializeMetaHumanCharacter(Character);
	UTEST_TRUE_EXPR(Character->IsCharacterValid());
	UTEST_FALSE("MetaHuman Character Face State is valid", Character->GetFaceStateData().GetSize() == 0);
	UTEST_FALSE("MetaHuman Character Body State is valid", Character->GetBodyStateData().GetSize() == 0);

	UTEST_TRUE("Character is added for editing", MetaHumanCharacterSubsystem->TryAddObjectToEdit(Character));
	ON_SCOPE_EXIT
	{
		// Ensure the subsystem cleans up any resources in the case
		// In particular for handling lifetime issues with the titan texture synthesis static array of allocations
		MetaHumanCharacterSubsystem->RemoveObjectToEdit(Character);
	};

	// Create a transient world where we can spawn an actor
	FScopedTestWorld TestWorld;
	UTEST_NOT_NULL_EXPR(TestWorld.World);
	
	USkeletalMesh* FaceArchetypeMesh = UMetaHumanCharacterEditorSubsystem::GetFaceArchetypeMesh(EMetaHumanCharacterTemplateType::MetaHuman);
	UTEST_NOT_NULL_EXPR(FaceArchetypeMesh);
	
	USkeletalMesh* BodyArchetypeMesh = UMetaHumanCharacterEditorSubsystem::GetBodyArchetypeMesh(EMetaHumanCharacterTemplateType::MetaHuman);
	UTEST_NOT_NULL_EXPR(BodyArchetypeMesh);

	// Check the CreateMetaHumanCharacterEditorActor expectations
	FText FailureReason;
	TSubclassOf<AActor> EditorActorClass;
	UTEST_TRUE_EXPR(MetaHumanCharacterSubsystem->TryGetMetaHumanCharacterEditorActorClass(Character, EditorActorClass, FailureReason));
	UTEST_NOT_NULL_EXPR(EditorActorClass.Get());

	TScriptInterface<IMetaHumanCharacterEditorActorInterface> CharacterActor = MetaHumanCharacterSubsystem->CreateMetaHumanCharacterEditorActor(Character, TestWorld.World);
	UTEST_NOT_NULL_EXPR(CharacterActor.GetObject());
	UTEST_NOT_SAME_PTR("MetaHuman Character Actor Face Skeletal Mesh", CharacterActor->GetFaceComponent()->GetSkeletalMeshAsset(), FaceArchetypeMesh);
	UTEST_NOT_SAME_PTR("MetaHuman Character Actor Body Skeletal Mesh", CharacterActor->GetBodyComponent()->GetSkeletalMeshAsset(), BodyArchetypeMesh);
	UTEST_FALSE("MetaHuman Character FaceStateData has data", Character->GetFaceStateData().GetSize() == 0);
	UTEST_FALSE("MetaHuman Character BodyStateData has data", Character->GetBodyStateData().GetSize() == 0);

	UTEST_EQUAL("MetaHuman Character synthesized face textures expected count", Character->SynthesizedFaceTextures.Num(), static_cast<int32>(EFaceTextureType::Count));

	// Check that synthesized textures are referenced by the Face preview material
	const TArray<FSkeletalMaterial>& FaceMaterials = CharacterActor->GetFaceComponent()->GetSkeletalMeshAsset()->GetMaterials();
	const FSkeletalMaterial* MaterialSlot = Algo::FindBy(FaceMaterials, TEXT("head_shader_shader"), &FSkeletalMaterial::MaterialSlotName);
	UTEST_NOT_NULL("MetaHuman Character Face Material slot", MaterialSlot);
	const UMaterialInterface* FaceMaterial = MaterialSlot->MaterialInterface;
	UTEST_NOT_NULL("MetaHuman Character Face Material", FaceMaterial);
	for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
	{
		UTEST_TRUE("MetaHuman Character face material texture slot", CheckSynthesizedTexturesInFaceMaterial(FaceMaterial, Character, TextureType));
	}

	return true;
}

// This test reproduces the crash from UE-289477, where RemoveObjectToEdit can be called from within TryAddObjectToEdit
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetHumanCharacterOpenAndClose, "MetaHuman.Creator.OpenAndClose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetHumanCharacterOpenAndClose::RunTest(const FString& InParams)
{
	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();

	UTEST_NOT_NULL_EXPR(Subsystem);

	UMetaHumanCharacter* Char1 = NewObject<UMetaHumanCharacter>();
	UMetaHumanCharacter* Char2 = NewObject<UMetaHumanCharacter>();

	UTEST_NOT_NULL_EXPR(Char1);
	UTEST_NOT_NULL_EXPR(Char2);
	
	Subsystem->InitializeMetaHumanCharacter(Char1);
	Subsystem->InitializeMetaHumanCharacter(Char2);

	UTEST_TRUE_EXPR(Subsystem->TryAddObjectToEdit(Char1));

	Subsystem->RequestHighResolutionTextures(Char1, ERequestTextureResolution::Res2k);

	// Register the removal of Char1 into TaskGraph, simulating the removal of an object while
	// another one is being added to the subsystem. It is important for this to be registered
	// in the game thread since this is a requirement for RemoveObjectToEdit
	bool bChar1Removed = false;
	AsyncTask(ENamedThreads::GameThread, [Char1, Subsystem, &bChar1Removed]
			  {
				Subsystem->RemoveObjectToEdit(Char1);
				bChar1Removed = true;
			  });

	// This will flush the tasks, which will call the registered lambda above
	UTEST_TRUE_EXPR(Subsystem->TryAddObjectToEdit(Char2));
	UTEST_TRUE_EXPR(bChar1Removed);

	Subsystem->RequestHighResolutionTextures(Char1, ERequestTextureResolution::Res2k);

	Subsystem->RemoveObjectToEdit(Char2);

	// If we get to this point without any errors in the log the requests were successful

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCharacterEditorPropertiesTest, "MetaHuman.Creator.SkinProperties", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterEditorPropertiesTest::RunTest(const FString& InParams)
{
	// Makes sure FMetaHumanCharacterAccentRegions has the correct property names
	// Properties names should match the values in the EMetaHumanCharacterAccentRegion
	for (EMetaHumanCharacterAccentRegion AccentRegion : TEnumRange<EMetaHumanCharacterAccentRegion>())
	{
		const FString AccentRegionName = StaticEnum<EMetaHumanCharacterAccentRegion>()->GetAuthoredNameStringByValue(static_cast<int64>(AccentRegion));
		const FStructProperty* AccentRegionProperty = FindFProperty<FStructProperty>(FMetaHumanCharacterAccentRegions::StaticStruct(), *AccentRegionName);

		const FString PropertyTestName = FString::Format(TEXT("FMetaHumanCharacterAccentRegions has '{0}' property"), { AccentRegionName });
		UTEST_NOT_NULL(PropertyTestName, AccentRegionProperty);

		UTEST_SAME_PTR("Accent Region Param is of type FMetaHumanCharacterAccentRegionProperties", AccentRegionProperty->Struct.Get(), FMetaHumanCharacterAccentRegionProperties::StaticStruct());
	}

	// Makes sure the FMetaHumanCharacterAccentRegionProperties has the correct property names
	// Property names should match the values in EMetaHumanCharacterAccentRegionParameter
	for (EMetaHumanCharacterAccentRegionParameter AccentRegionParam : TEnumRange<EMetaHumanCharacterAccentRegionParameter>())
	{
		const FString AccentRegionParamName = StaticEnum<EMetaHumanCharacterAccentRegionParameter>()->GetAuthoredNameStringByValue(static_cast<int64>(AccentRegionParam));
		const FFloatProperty* AccentRegionParamProperty = FindFProperty<FFloatProperty>(FMetaHumanCharacterAccentRegionProperties::StaticStruct(), *AccentRegionParamName);

		const FString PropertyTestName = FString::Format(TEXT("FMetaHumanCharacterAccentRegionProperties has '{0}' property"), { AccentRegionParamName });
		UTEST_NOT_NULL(PropertyTestName, AccentRegionParamProperty);
	}

	// Makes sure the FMetaHumanCharacterFrecklesProperties has the correct property names
	// Property names should match the values in EMetaHumanCharacterFrecklesParameter
	for (EMetaHumanCharacterFrecklesParameter FrecklesParam : TEnumRange<EMetaHumanCharacterFrecklesParameter>())
	{
		const FString FrecklesParamName = StaticEnum<EMetaHumanCharacterFrecklesParameter>()->GetAuthoredNameStringByValue(static_cast<int64>(FrecklesParam));
		const FProperty* FrecklesParamProperty = FindFProperty<FProperty>(FMetaHumanCharacterFrecklesProperties::StaticStruct(), *FrecklesParamName);

		const FString PropertyTestName = FString::Format(TEXT("FMetaHumanCharacterFrecklesProperties has '{0}' property"), { FrecklesParamName });
		UTEST_NOT_NULL(PropertyTestName, FrecklesParamProperty);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCharacterSynthesizeAndUpdateTexturesTest, "MetaHuman.Creator.TextureSynthesis.SynthesizeAndUpdate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterSynthesizeAndUpdateTexturesTest::RunTest(const FString& InParams)
{
	// Test Texture Synthesis helper

	// Initialize the synthesizer
	FMetaHumanFaceTextureSynthesizer FaceTextureSynthesizer;
	FMetaHumanCharacterTextureSynthesis::InitFaceTextureSynthesizer(FaceTextureSynthesizer);
	UTEST_VALID("Face Texture Synthesizer is valid", FaceTextureSynthesizer);

	// Create the Texture objects and Images
	TMap<EFaceTextureType, FMetaHumanCharacterTextureInfo> SynthesizedTexturesInfo;
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>> SynthesizedFaceTextures;
	TMap<EFaceTextureType, FImage> CachedSynthesizedImages;
	FMetaHumanCharacterTextureSynthesis::InitSynthesizedFaceData(FaceTextureSynthesizer,
																 SynthesizedTexturesInfo,
																 SynthesizedFaceTextures,
																 CachedSynthesizedImages);

	// Do some sanity checks to the created data
	UTEST_EQUAL("Number of synthesized Textures", SynthesizedFaceTextures.Num(), static_cast<int32>(EFaceTextureType::Count));

	for (const TPair<EFaceTextureType, TObjectPtr<UTexture2D>>& FaceTexturePair : SynthesizedFaceTextures)
	{
		UTEST_NOT_NULL("Synthesized face texture not null", FaceTexturePair.Value.Get());
	}
	for (TPair<EFaceTextureType, FImage> Kvp : CachedSynthesizedImages)
	{
		const FImage& CachedImage = Kvp.Value;
		UTEST_EQUAL("Cached image size X", CachedImage.SizeX, FaceTextureSynthesizer.GetTextureSizeX());
		UTEST_EQUAL("Cached image size Y", CachedImage.SizeY, FaceTextureSynthesizer.GetTextureSizeY());
		UTEST_EQUAL("Cached image format", CachedImage.Format, FaceTextureSynthesizer.GetTextureFormat());
		UTEST_EQUAL("Cached image color space", CachedImage.GammaSpace, FaceTextureSynthesizer.GetTextureColorSpace());
	}

	FMetaHumanCharacterSkinProperties SkinProperties = {};

	// Test and time synthesize on single thread
	bool bResult = false;
	{
		FScopeLogTime LogTimePtr(TEXT("FMetaHumanCharacterTextureSynthesis::SynthesizeFaceTextures"), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
		bResult = FMetaHumanCharacterTextureSynthesis::SynthesizeFaceTextures(SkinProperties, FaceTextureSynthesizer, CachedSynthesizedImages);
	}
	UTEST_TRUE("Synthesize and update textures result", bResult);

	// Test and time update only
	{
		FScopeLogTime LogTimePtr(TEXT("FMetaHumanCharacterTextureSynthesis::UpdateFaceTextures"), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
		bResult = FMetaHumanCharacterTextureSynthesis::UpdateFaceTextures(CachedSynthesizedImages, SynthesizedFaceTextures);
	}
	UTEST_TRUE("Synthesize textures async", bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanTextureSynthesisServiceTests, "MetaHuman.Creator.TextureSynthesis.Service", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanTextureSynthesisServiceTests::RunTest(const FString& InParams)
{
	using namespace UE::MetaHuman;
	constexpr int32 TestResolution = 2048;
	FFaceTextureRequestCreateParams FaceTextureRequestCreateParams;
	FaceTextureRequestCreateParams.HighFrequency = 1;
	
	struct FLocalTestState
	{
		FLocalTestState()
			: bDone(false)
			, ResultCode(EMetaHumanServiceRequestResult::Ok)
		{}

		bool bDone;
		EMetaHumanServiceRequestResult ResultCode;
	};

	TSharedRef<FLocalTestState> LocalState = MakeShared<FLocalTestState>();
	TSharedRef<FFaceTextureSynthesisServiceRequest> TextureSynthesisServiceRequest = FFaceTextureSynthesisServiceRequest::CreateRequest(FaceTextureRequestCreateParams);
	TextureSynthesisServiceRequest->FaceTextureSynthesisRequestCompleteDelegate.BindLambda([LocalState](TSharedPtr<FFaceHighFrequencyData> HighFrequencyData)
		{
			if (LocalState->bDone)
			{
				// we might still be invoked even if an error has occurred so for this test we just do an early out
				return;
			}

			int32 TotalTypeSum = ((static_cast<int32>(EFaceTextureType::Count) + 1)*static_cast<int32>(EFaceTextureType::Count)/2);
			for(EFaceTextureType Type : TEnumRange<EFaceTextureType>())
			{
				TotalTypeSum -= (static_cast<int32>(Type) + 1);
				FImage TextureImage;
				TConstArrayView<uint8> PngData = (*HighFrequencyData)[Type];
				if (PngData.Num() == 0)
				{
					continue;
				}
				verify(FImageUtils::DecompressImage(PngData.GetData(), PngData.Num(), TextureImage));
				UTexture2D* Texture = FImageUtils::CreateTexture2DFromImage(TextureImage);
				if (Texture->GetSizeX() != TestResolution || Texture->GetSizeY() != TestResolution)
				{
					// in this test we just report a server error
					// in real code this would need to do something more intelligent
					TotalTypeSum = 1;
					break;
				}
			}
			LocalState->bDone = true;
			LocalState->ResultCode = TotalTypeSum == 0 ? EMetaHumanServiceRequestResult::Ok : EMetaHumanServiceRequestResult::ServerError;
		}
	);

	TextureSynthesisServiceRequest->OnMetaHumanServiceRequestFailedDelegate.BindLambda([LocalState](EMetaHumanServiceRequestResult Result)
		{
			LocalState->bDone = true;
			LocalState->ResultCode = Result;
		}
	);

	// start the request
	const TArray<FFaceTextureRequestParams, TInlineAllocator<static_cast<int32>(EFaceTextureType::Count)>> TextureTypesToRequest =
	{
		{ EFaceTextureType::Basecolor, TestResolution },
		{ EFaceTextureType::Basecolor_Animated_CM1, TestResolution },
		{ EFaceTextureType::Basecolor_Animated_CM2, TestResolution },
		{ EFaceTextureType::Basecolor_Animated_CM3, TestResolution },
		{ EFaceTextureType::Normal, TestResolution },
		{ EFaceTextureType::Normal_Animated_WM1, TestResolution },
		{ EFaceTextureType::Normal_Animated_WM2, TestResolution },
		{ EFaceTextureType::Normal_Animated_WM3, TestResolution },
		{ EFaceTextureType::Cavity, TestResolution },
	};
	TextureSynthesisServiceRequest->RequestTexturesAsync(MakeConstArrayView<FFaceTextureRequestParams>(TextureTypesToRequest.GetData(), TextureTypesToRequest.Num()));

	while (!LocalState->bDone)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.1f);
		FPlatformProcess::Sleep(0.05f);
	}
	UTEST_TRUE("Didn't get all the textures, missing, invalid, or dupes", LocalState->ResultCode==EMetaHumanServiceRequestResult::Ok);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanTextureSynthesisSkinToneEstimationTest, "MetaHuman.Creator.TextureSynthesis.SkinToneEstimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanTextureSynthesisSkinToneEstimationTest::RunTest(const FString& InParams)
{
	// Initialize the synthesizer
	FMetaHumanFaceTextureSynthesizer FaceTextureSynthesizer;
	FMetaHumanCharacterTextureSynthesis::InitFaceTextureSynthesizer(FaceTextureSynthesizer);
	UTEST_VALID("Face Texture Synthesizer is valid", FaceTextureSynthesizer);

	// This test works by sampling the Skin Tone UI space then obtaining the skin tone from the texture synthesis model
	// The returned skin tone is then projected back to the texture model space and the result projection is
	// compared against the input one and will fail if they don't match

	const int32 NumSteps = 5;
	for (int32 StepU = 0; StepU < NumSteps + 1; ++StepU)
	{
		for (int32 StepV = 0; StepV < NumSteps + 1; ++StepV)
		{
			const float U = FMath::Lerp(0.0f, 1.0f, (float) StepU / NumSteps);
			const float V = FMath::Lerp(0.0f, 1.0f, (float) StepV / NumSteps);

			const FVector2f InputSkinToneUI{ U, V };
			const FLinearColor SkinTone = FaceTextureSynthesizer.GetSkinTone(InputSkinToneUI);

			const FVector2f EstimatedSkinToneUI = FaceTextureSynthesizer.ProjectSkinTone(SkinTone);
			const FLinearColor EstimatedSkinTone = FaceTextureSynthesizer.GetSkinTone(EstimatedSkinToneUI);

			const float Dist = FVector2f::Distance(InputSkinToneUI, EstimatedSkinToneUI);
			UTEST_LESS_EQUAL_EXPR(Dist, UE_SMALL_NUMBER);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanAutorigServiceTests, "MetaHuman.Creator.MetaHumanAutorigServiceTests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanAutorigServiceTests::RunTest(const FString& InParams)
{
	//TODO: test mesh upload and response of valid DNA
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCharacterAutoRigServiceTest, "MetaHuman.Creator.AutoRigServiceTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterAutoRigServiceTest::RunTest(const FString& InParams)
{
	UMetaHumanCharacter* Character = NewObject<UMetaHumanCharacter>(GetTransientPackage());
	UTEST_FALSE_EXPR(Character->IsCharacterValid());
	UTEST_TRUE("Initial FaceStateData is empty", Character->GetFaceStateData().GetSize() == 0);

	UTEST_NOT_NULL_EXPR(GEditor);
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	UTEST_NOT_NULL_EXPR(MetaHumanCharacterSubsystem);

	MetaHumanCharacterSubsystem->InitializeMetaHumanCharacter(Character);
	UTEST_TRUE_EXPR(MetaHumanCharacterSubsystem->TryAddObjectToEdit(Character));
	UTEST_TRUE_EXPR(Character->IsCharacterValid());

	ON_SCOPE_EXIT
	{
		// Ensure the subsystem cleans up any resources in the case
		// In particular for handling lifetime issues with the titan texture synthesis static array of allocations
		MetaHumanCharacterSubsystem->RemoveObjectToEdit(Character);
	};
	
	USkeletalMesh* FaceArchetypeMesh = UMetaHumanCharacterEditorSubsystem::GetFaceArchetypeMesh(EMetaHumanCharacterTemplateType::MetaHuman);
	UTEST_NOT_NULL_EXPR(FaceArchetypeMesh);

	UAssetUserData* UserData = FaceArchetypeMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass());
	UTEST_NOT_NULL_EXPR(UserData);
	UDNAAsset* DNAAsset = Cast<UDNAAsset>(UserData);
	UTEST_NOT_NULL_EXPR(DNAAsset);
	TSharedPtr<IDNAReader> DNAReader = DNAAsset->GetBehaviorReader();
	UTEST_VALID("Archetype DNA is valid", DNAReader);
	// Map vertices by creating map from skeletal mesh user asset.
	TSharedPtr<FDNAToSkelMeshMap> FaceArchetypeDnaToSkelMeshMap = TSharedPtr<FDNAToSkelMeshMap>(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(FaceArchetypeMesh));
	UTEST_VALID("DNA to SkeletalMesh map is valid", FaceArchetypeDnaToSkelMeshMap);

	// Create a transient world where we can spawn an actor
	FScopedTestWorld TestWorld;
	UTEST_NOT_NULL_EXPR(TestWorld.World);

	// Spawn the editor actor
	FText FailureReason;
	TSubclassOf<AActor> EditorActorClass;
	UTEST_TRUE_EXPR(MetaHumanCharacterSubsystem->TryGetMetaHumanCharacterEditorActorClass(Character, EditorActorClass, FailureReason));
	UTEST_NOT_NULL_EXPR(EditorActorClass.Get());

	TScriptInterface<IMetaHumanCharacterEditorActorInterface> CharacterActor = MetaHumanCharacterSubsystem->CreateMetaHumanCharacterEditorActor(Character, TestWorld.World);
	UTEST_NOT_NULL_EXPR(CharacterActor.GetObject());
	UTEST_NOT_SAME_PTR("MetaHuman Character Actor Face Skeletal Mesh", CharacterActor->GetFaceComponent()->GetSkeletalMeshAsset(), FaceArchetypeMesh);
	// Map joints explicitly.
	TSharedRef<FDNAToSkelMeshMap> FaceDnaToSkelMeshMap = MakeShared<FDNAToSkelMeshMap>(*MetaHumanCharacterSubsystem->GetFaceDnaToSkelMeshMap(Character));
	FaceDnaToSkelMeshMap->MapJoints(DNAReader.Get());
	UTEST_FALSE("MetaHuman Character FaceStateData has data", Character->GetFaceStateData().GetSize() == 0);
	
	// TODO: Calling Reset then calling AutorigService should ideally attach updated DNA to updated SkeletalMesh.
	// TODO: Until AutoRigService calls are implemented the test will work with a copy of Archetype DNA from the MH Character.
	MetaHumanCharacterSubsystem->ResetCharacterFace(Character);
	MetaHumanCharacterSubsystem->AutoRigFace(Character, UE::MetaHuman::ERigType::JointsOnly);

	// Fetch updated DNA.
	UserData = CharacterActor->GetFaceComponent()->GetSkeletalMeshAsset()->GetAssetUserDataOfClass(UDNAAsset::StaticClass());
	UTEST_NOT_NULL_EXPR(UserData);
	DNAAsset = Cast<UDNAAsset>(UserData);
	UTEST_NOT_NULL_EXPR(DNAAsset);
	TSharedPtr<IDNAReader> UpdatedDNAReader = DNAAsset->GetBehaviorReader();
	UTEST_VALID("Updated DNA is valid", UpdatedDNAReader);

	UTEST_EQUAL("DNA number of LODs", DNAReader->GetLODCount(), UpdatedDNAReader->GetLODCount());
	// Make sure DNAs have the same mesh count and same vertex count per mesh.
	const int32 MeshCount = DNAReader->GetMeshCount();
	UTEST_EQUAL("DNA mesh count", MeshCount, UpdatedDNAReader->GetMeshCount());
	for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
	{
		UTEST_EQUAL(FString::Printf(TEXT("DNA vertex position count for mesh %d"), MeshIndex), DNAReader->GetVertexPositionCount(MeshIndex), UpdatedDNAReader->GetVertexPositionCount(MeshIndex));
	}
	// Make sure DNAs have the same joint count and same joints name in the same order.
	const int32 JointCount = DNAReader->GetJointCount();
	UTEST_EQUAL("DNA joint count", JointCount, UpdatedDNAReader->GetJointCount());
	for (int32 JointIndex = 0; JointIndex < JointCount; JointIndex++)
	{
		UTEST_EQUAL("DNA joint name", DNAReader->GetJointName(JointIndex), UpdatedDNAReader->GetJointName(JointIndex));
	}

	// Test bind pose hierarchy.
	const USkeletalMesh* UpdateSkeletalMesh = CharacterActor->GetFaceComponent()->GetSkeletalMeshAsset();
	const FReferenceSkeleton* RefSkeleton = &UpdateSkeletalMesh->GetRefSkeleton();
	for (int32 JointIndex = 0; JointIndex < UpdatedDNAReader->GetJointCount(); JointIndex++)
	{
		const FString BoneNameStr = UpdatedDNAReader->GetJointName(JointIndex);
		const FName BoneName = FName{ BoneNameStr };
		const int32 BoneIndex = RefSkeleton->FindBoneIndex(BoneName);
		const int32 ExpectedJointIndex = FaceDnaToSkelMeshMap->GetUEBoneIndex(JointIndex);

		UTEST_TRUE("DNA joint not found in Skeleton hierarchy", BoneIndex != INDEX_NONE);
		UTEST_EQUAL(FString::Printf(TEXT("DNA joint index %d mismatch"), ExpectedJointIndex), ExpectedJointIndex, BoneIndex);
	}

	// Test if vertex positions match.
	FSkeletalMeshModel* ImportedModel = UpdateSkeletalMesh->GetImportedModel();
	UTEST_EQUAL("Skeletal mesh number of LODs", ImportedModel->LODModels.Num(), UpdatedDNAReader->GetLODCount());
	for (int32 LODIndex = 0; LODIndex < UpdatedDNAReader->GetLODCount(); LODIndex++)
	{
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
		for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
		{
			const int32 VertexCount = UpdatedDNAReader->GetVertexPositionCount(MeshIndex);
			for (int32 DNAVertexIndex = 0; DNAVertexIndex < VertexCount; DNAVertexIndex++)
			{
				int32 VertexIndex = FaceDnaToSkelMeshMap->ImportDNAVtxToUEVtxIndex[LODIndex][MeshIndex][DNAVertexIndex];
				TArray<FSoftSkinVertex> Vertices;
				LODModel.GetVertices(Vertices);
				UTEST_TRUE("Skeletal mesh vertex index valid", Vertices.IsValidIndex(VertexIndex));
				const FVector UpdatedPosition = UpdatedDNAReader->GetVertexPosition(MeshIndex, DNAVertexIndex);
				const bool bPositionsEqual = Vertices[VertexIndex].Position.Equals(FVector3f{ UpdatedPosition }, UE_KINDA_SMALL_NUMBER);
				UTEST_TRUE("Skeletal mesh vertex correct position", bPositionsEqual);
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanCharacterDuplicationTest, "MetaHuman.Creator.CharacterDuplication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterDuplicationTest::RunTest(const FString& InParams)
{
	UMetaHumanCharacter* Character = NewObject<UMetaHumanCharacter>(GetTransientPackage());
	UTEST_FALSE_EXPR(Character->IsCharacterValid());
	UTEST_TRUE("Initial FaceStateData is empty", Character->GetFaceStateData().GetSize() == 0);
	UTEST_TRUE("Initial BodyStateData is empty", Character->GetBodyStateData().GetSize() == 0);

	UTEST_NOT_NULL_EXPR(GEditor);
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	UTEST_NOT_NULL_EXPR(MetaHumanCharacterSubsystem);

	MetaHumanCharacterSubsystem->InitializeMetaHumanCharacter(Character);
	UTEST_TRUE_EXPR(Character->IsCharacterValid());
	UTEST_FALSE("MetaHuman Character Face State is valid", Character->GetFaceStateData().GetSize() == 0);
	UTEST_FALSE("MetaHuman Character Body State is valid", Character->GetBodyStateData().GetSize() == 0);

	UTEST_TRUE("Character is added for editing", MetaHumanCharacterSubsystem->TryAddObjectToEdit(Character));

	MetaHumanCharacterSubsystem->CommitFaceState(Character, MetaHumanCharacterSubsystem->GetFaceState(Character));

	FMetaHumanRigEvaluatedState FaceState = MetaHumanCharacterSubsystem->GetFaceState(Character)->Evaluate();

	UMetaHumanCharacter* DuplicateCharacter = CastChecked<UMetaHumanCharacter>(StaticDuplicateObject(Character, Character->GetOuter(), FName(*FString::Printf(TEXT("%s_Duplicate"), *Character->GetName()))));
	UTEST_NOT_NULL_EXPR(DuplicateCharacter);
	UTEST_TRUE_EXPR(DuplicateCharacter->IsCharacterValid());
	UTEST_TRUE("Added DuplicateCharacter for editing", MetaHumanCharacterSubsystem->TryAddObjectToEdit(DuplicateCharacter));

	ON_SCOPE_EXIT
	{
		// Ensure the subsystem cleans up any resources in the case
		// In particular for handling lifetime issues with the titan texture synthesis static array of allocations
		MetaHumanCharacterSubsystem->RemoveObjectToEdit(Character);
		MetaHumanCharacterSubsystem->RemoveObjectToEdit(DuplicateCharacter);
	};

	FMetaHumanRigEvaluatedState DuplicateFaceState = MetaHumanCharacterSubsystem->GetFaceState(DuplicateCharacter)->Evaluate();

	int32 NumSame = 0;
	int32 NumDifferent = 0;
	bool bSuccess = true;
	for (int32 V = 0; V < FaceState.Vertices.Num(); ++V)
	{
		FVector3f Diff = FaceState.Vertices[V] - DuplicateFaceState.Vertices[V];
		if (Diff.Length() > 0.00001)
		{
			NumDifferent++;
			bSuccess = false;
		}
		else
		{
			NumSame++;
		}
	}
	UE_LOG(LogMetaHumanCharacterEditorTests, Display, TEXT("Number of vertices which are the same = %d; number which are different = %d"), NumSame, NumDifferent);

	NumSame = 0;
	NumDifferent = 0;
	for (int32 V = 0; V < FaceState.VertexNormals.Num(); ++V)
	{
		FVector3f Diff = FaceState.VertexNormals[V] - DuplicateFaceState.VertexNormals[V];
		if (Diff.Length() > 0.00001)
		{
			NumDifferent++;
			bSuccess = false;
		}
		else
		{
			NumSame++;
		}
	}
	UE_LOG(LogMetaHumanCharacterEditorTests, Display, TEXT("Number of vertex normals which are the same = %d; number which are different = %d"), NumSame, NumDifferent);

	return bSuccess;
}


#endif // WITH_DEV_AUTOMATION_TESTS
