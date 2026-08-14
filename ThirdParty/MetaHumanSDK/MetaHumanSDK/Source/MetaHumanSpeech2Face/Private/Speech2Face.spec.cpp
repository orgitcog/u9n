// Copyright Epic Games, Inc. All Rights Reserved.

#include "Speech2Face.h"

#include "Misc/AutomationTest.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/Package.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

BEGIN_DEFINE_SPEC(TSpeech2FaceTest, "Speech2Face", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::MediumPriority)

static constexpr float DefaultTolerance = 0.01f;
FString TestDataDir;
FString TestAssetDir;

bool ProcessAudio(const FString& InAssetName,
				  uint32 InExpectedFrameCount,
				  bool bGenerateBlinks = false,
				  bool bInMixChannels = false,
				  uint32 InAudioChannelIndex = 0,
				  float InOutputAnimationFps = 50.0f);
bool ProcessAudioAndCompareOutput(const FString& InAssetName,
								  const FString& InValidationDataJson,
								  bool bGenerateBlinks = false,
								  bool bInMixChannels = false,
								  uint32 InAudioChannelIndex = 0,
								  float InOutputAnimationFps = 50.0f,
								  float InTolerance = DefaultTolerance);
bool ProcessAudioInternal(const FString& InAssetName,
						  bool bGenerateBlinks,
						  bool bInMixChannels,
						  uint32 InAudioChannelIndex,
						  float InOutputAnimationFps,
						  TArray<FSpeech2Face::FAnimationFrame>& OutAnimation,
						  TArray<FSpeech2Face::FAnimationFrame>& OutHeadAnimation);
void GenerateValidationData(const FString& InPathToJson, const TArray<FSpeech2Face::FAnimationFrame>& InValidationData);
bool LoadValidationDataFromJsonFile(const FString& InPathToJson, TArray<FSpeech2Face::FAnimationFrame>& OutValidationData);
bool IsAnimationDataEqualIsh(const TArray<FSpeech2Face::FAnimationFrame>& InLeft, const TArray<FSpeech2Face::FAnimationFrame>& InRight, float InTolerance);
bool IsAnimationFrameEqualIsh(int32 InFrame, const FSpeech2Face::FAnimationFrame& InLeft, const FSpeech2Face::FAnimationFrame& InRight, float InTolerance);

END_DEFINE_SPEC(TSpeech2FaceTest)

bool TSpeech2FaceTest::ProcessAudio(const FString& InAssetName, uint32 InExpectedFrameCount,  bool bGenerateBlinks, bool bInMixChannels, uint32 InAudioChannelIndex, float InOutputAnimationFps)
{
	TArray<FSpeech2Face::FAnimationFrame> Animation;
	TArray<FSpeech2Face::FAnimationFrame> HeadAnimation;
	bool bIsSuccess = ProcessAudioInternal(InAssetName, bGenerateBlinks, bInMixChannels, InAudioChannelIndex, InOutputAnimationFps, Animation, HeadAnimation);
	if(!bIsSuccess)
	{
		return false;
	}

	UTEST_EQUAL(TEXT("Correct number of frames was generated for the face"), Animation.Num(), InExpectedFrameCount);
	UTEST_EQUAL(TEXT("Correct number of frames was generated for the head"), HeadAnimation.Num(), InExpectedFrameCount);
	return true;
}

bool TSpeech2FaceTest::ProcessAudioAndCompareOutput(const FString& InAssetName, const FString& InValidationDataJson, bool bGenerateBlinks, bool bInMixChannels, uint32 InAudioChannelIndex, float InOutputAnimationFps, float InTolerance)
{
	TArray<FSpeech2Face::FAnimationFrame> Animation;
	TArray<FSpeech2Face::FAnimationFrame> UnusedHeadAnimation;
	bool bIsSuccess = ProcessAudioInternal(InAssetName, bGenerateBlinks, bInMixChannels, InAudioChannelIndex, InOutputAnimationFps, Animation, UnusedHeadAnimation);
	if (!bIsSuccess)
	{
		return false;
	}

	FString ValidationJsonPath = TestDataDir + InValidationDataJson;

	TArray<FSpeech2Face::FAnimationFrame> ValidationData;
	bIsSuccess = LoadValidationDataFromJsonFile(ValidationJsonPath, ValidationData);
	UTEST_TRUE(TEXT("Validation data loaded successfully"), bIsSuccess);
	UTEST_TRUE(TEXT("Produced animation matches validation data with tolerance of ") + FString::Format(TEXT("{0}"), { InTolerance }), IsAnimationDataEqualIsh(ValidationData, Animation, InTolerance));

	return true;
}

bool TSpeech2FaceTest::ProcessAudioInternal(const FString& InAssetName,
	bool bGenerateBlinks,
	bool bInMixChannels,
	uint32 InAudioChannelIndex,
	float InOutputAnimationFps,
	TArray<FSpeech2Face::FAnimationFrame>& OutAnimation,
	TArray<FSpeech2Face::FAnimationFrame>& OutHeadAnimation)
{
	TUniquePtr<FSpeech2Face> Speech2Face = FSpeech2Face::Create();
	UTEST_TRUE(TEXT("FSpeech2Face::Create succeeded"), Speech2Face != nullptr);

	FString AssetPath = TestAssetDir + InAssetName;
	USoundWave* SoundSample = LoadObject<USoundWave>(GetTransientPackage(), AssetPath.GetCharArray().GetData());
	UTEST_TRUE(TEXT("Test audio asset loaded successfully"), SoundSample != nullptr);

	FSpeech2Face::FAudioParams AudioParams(SoundSample, FSpeech2Face::AudioEncoderWarmUpSec, bInMixChannels, InAudioChannelIndex);

	bool bIsSuccess = Speech2Face->GenerateFaceAnimation(AudioParams, InOutputAnimationFps, bGenerateBlinks, [](){return false;}, OutAnimation, OutHeadAnimation);
	UTEST_TRUE(TEXT("FSpeech2Face::GenerateFaceAnimation succeeded"), bIsSuccess);
	return true;
}

void TSpeech2FaceTest::GenerateValidationData(const FString& InPathToJson, const TArray<FSpeech2Face::FAnimationFrame>& InValidationData)
{
	TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());
	TSharedRef<FJsonObject> SequenceJsonObject = MakeShareable(new FJsonObject());
	RootJsonObject->SetObjectField("sequence", SequenceJsonObject);

	for (int32 FrameIndex = 0; FrameIndex < InValidationData.Num(); ++FrameIndex)
	{
		const FSpeech2Face::FAnimationFrame& FrameData = InValidationData[FrameIndex];
		TSharedRef<FJsonObject> FrameJsonObject = MakeShareable(new FJsonObject());
		SequenceJsonObject->SetObjectField(FString::FromInt(FrameIndex), FrameJsonObject);

		for (const TPair<FString, float>& ControlValue : FrameData)
		{
			FrameJsonObject->SetNumberField(ControlValue.Key, ControlValue.Value);
		}
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString, 0);
	FJsonSerializer::Serialize(RootJsonObject, JsonWriter, true);

	FFileHelper::SaveStringToFile(JsonString, *InPathToJson);
}

bool TSpeech2FaceTest::LoadValidationDataFromJsonFile(const FString& InPathToJson, TArray<FSpeech2Face::FAnimationFrame>& OutValidationData)
{
	FString TestDataString;
	bool bIsSuccess = FFileHelper::LoadFileToString(TestDataString, InPathToJson.GetCharArray().GetData());
	UTEST_TRUE(TEXT("Loading JSON validation file succeeded"), bIsSuccess);

	TSharedPtr<FJsonObject> TestDataJson;
	bIsSuccess = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TestDataString), TestDataJson);
	UTEST_TRUE(TEXT("Parsing JSON validation file succeeded"), bIsSuccess);

	TSharedPtr<FJsonObject> SequenceObject = TestDataJson->GetObjectField(TEXT("sequence"));
	UTEST_TRUE(TEXT("Find 'sequence' object in JSON validation file"), SequenceObject != nullptr);
	int32 FrameCountJson = SequenceObject->Values.Num();
	OutValidationData.Reset(FrameCountJson);
	for (int32 I = 0; I < FrameCountJson; ++I)
	{
		TSharedPtr<FJsonObject> FrameObject = SequenceObject->GetObjectField(FString::FromInt(I));
		TArray<FString> FrameObjectKeys;
		FrameObject->Values.GetKeys(FrameObjectKeys);

		FSpeech2Face::FAnimationFrame AnimFrame;
		for (const FString& Key : FrameObjectKeys)
		{
			AnimFrame.Add(Key, static_cast<float>(FrameObject->GetNumberField(Key)));
		}
		OutValidationData.Emplace(MoveTemp(AnimFrame));
	}

	return true;
}

bool TSpeech2FaceTest::IsAnimationDataEqualIsh(const TArray<FSpeech2Face::FAnimationFrame>& InExpected, const TArray<FSpeech2Face::FAnimationFrame>& InGenerated, float InTolerance)
{
	UTEST_TRUE(TEXT("Generated animation length is the same as in validation data"), InExpected.Num() == InGenerated.Num());

	for (int32 I = 0; I < InExpected.Num(); ++I)
	{
		UTEST_TRUE(TEXT("Compare animation frame number ") + FString::FromInt(I),
			IsAnimationFrameEqualIsh(I, InExpected[I], InGenerated[I], InTolerance));
	}

	return true;
}

bool TSpeech2FaceTest::IsAnimationFrameEqualIsh(int32 InFrame, const FSpeech2Face::FAnimationFrame& InExpected, const FSpeech2Face::FAnimationFrame& InGenerated, float InTolerance)
{
	UTEST_TRUE(TEXT("Check that number of controls in an animation frame is the same: ") + FString::FromInt(InExpected.Num()) + TEXT(", ") +  FString::FromInt(InGenerated.Num()), InExpected.Num() == InGenerated.Num());

	TArray<FString> Controls;
	InExpected.GetKeys(Controls);
	for (const TPair<FString, float>& ExpectedControlValue : InExpected)
	{
		const float* GeneratedControlValue = InGenerated.Find(ExpectedControlValue.Key);
		UTEST_TRUE(TEXT("Rig control ") + ExpectedControlValue.Key + TEXT(" is present in the generated frame("),
			GeneratedControlValue != nullptr);
		if (GeneratedControlValue)
		{
			UTEST_EQUAL_TOLERANCE(TEXT("Rig control (") + ExpectedControlValue.Key + TEXT(") value matches expected"),
				ExpectedControlValue.Value, *GeneratedControlValue, InTolerance);
		}

	}

	return true;
}

void TSpeech2FaceTest::Define()
{
	FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
	TestDataDir = PluginDir / "TestData/Audio/";
	TestAssetDir = TEXT("/MetaHuman/TestData/Audio/");

	Describe("GenerateAnimation()", [this]()
	{
		It("should process mono audio, 44100 Hz and pass checks", [this]()
		{
			const uint32 ExpectedFrameCount = 308;
			return ProcessAudio(TEXT("44_1kHz_1channel.44_1kHz_1channel"), ExpectedFrameCount);
		});

		It("should process mono audio, 16 kHz and match validation data", [this]()
		{
			return ProcessAudioAndCompareOutput(TEXT("16kHz_1channel.16kHz_1channel"), TEXT("16kHz_1channel.json"));
		});

		It("should process stereo audio, second channel, 16 kHz and match validation data", [this]()
		{
			const bool bGenerateBlinks = false;
			const bool bMixChannels = false;
			const int32 AudioChannel = 1;
			return ProcessAudioAndCompareOutput(TEXT("16kHz_2channels.16kHz_2channels"),
				TEXT("16kHz_2channels.json"),
				bGenerateBlinks,
				bMixChannels,
				AudioChannel);
		});

		It("should process stereo audio, mix channels, 16 kHz and match validation data", [this]()
		{
			const bool bGenerateBlinks = false;
			const bool bMixChannels = true;
			return ProcessAudioAndCompareOutput(TEXT("16kHz_2channels.16kHz_2channels"),
				TEXT("16kHz_2channels_mixed.json"),
				bGenerateBlinks,
				bMixChannels);
		});
		
		It("should process mono audio, 16 kHz, resample to 60 fps and match validation data", [this]()
		{
			const bool bGenerateBlinks = false;
			const bool bMixChannels = false;
			const int32 AudioChannel = 0;
			const float OutputFPS = 60;
			return ProcessAudioAndCompareOutput(TEXT("16kHz_1channel.16kHz_1channel"),
				TEXT("16kHz_1channel_resample_to_60fps.json"),
				bGenerateBlinks,
				bMixChannels,
				AudioChannel,
				OutputFPS);
		});

		It("should process mono 16kHz audio, generate blinks and match validation data", [this]()
		{
			const bool bGenerateBlinks = true;
			const bool bMixChannels = false;
			return ProcessAudioAndCompareOutput(TEXT("16kHz_1channel.16kHz_1channel"),
				TEXT("16kHz_1channel_blinks.json"),
				bGenerateBlinks,
				bMixChannels);
		});

		It("should process mono 16kHz audio, generate blinks, resample to 60pfs and match validation data", [this]()
		{
			const bool bGenerateBlinks = true;
			const bool bMixChannels = false;
			const int32 AudioChannel = 0;
			const float OutputFPS = 60;
			return ProcessAudioAndCompareOutput(TEXT("16kHz_1channel.16kHz_1channel"),
				TEXT("16kHz_1channel_blinks_resample_to_60fps.json"),
				bGenerateBlinks,
				bMixChannels,
				AudioChannel,
				OutputFPS);
		});
	});
}
#endif
