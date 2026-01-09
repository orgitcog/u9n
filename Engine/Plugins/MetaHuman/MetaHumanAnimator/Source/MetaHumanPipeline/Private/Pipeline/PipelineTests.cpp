// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pipeline/Pipeline.h"
#include "Pipeline/Log.h"
#include "Pipeline/PipelineData.h"
#include "Nodes/TestNodes.h"
#include "Nodes/ImageUtilNodes.h"
#include "Nodes/AudioUtilNodes.h"
#include "Nodes/HyprsenseNode.h"
#include "Nodes/HyprsenseSparseNode.h"
#include "Nodes/DepthMapDiagnosticsNode.h"
#include "Nodes/HyprsenseTestNode.h"
#include "Nodes/HyprsenseRealtimeNode.h"
#include "Nodes/HyprsenseRealtimeSmoothingNode.h"
#include "Nodes/NeutralFrameNode.h"
#include "Nodes/RealtimeSpeechToAnimNode.h"
#include "Nodes/FaceTrackerNode.h"
#include "Nodes/FaceTrackerPostProcessingNode.h"
#include "Nodes/FaceTrackerPostProcessingFilterNode.h"
#include "Nodes/TrackerUtilNodes.h"
#include "Nodes/AsyncNode.h"
#include "Nodes/AnimationUtilNodes.h"
#include "Nodes/ControlUtilNodes.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "MetaHumanConfig.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "NNEModelData.h"
#include "DoesNNEAssetExist.h"
#include "FramePathResolver.h"

DEFINE_LOG_CATEGORY_STATIC(LogMHABenchmark, Log, All);

namespace UE::MetaHuman::Pipeline
{

#if WITH_DEV_AUTOMATION_TESTS

class FPipelineTestHelper
{
public:

	FPipelineTestHelper();

	void Run(EPipelineMode InPipelineMode)
	{
		OnFrameComplete.AddRaw(this, &FPipelineTestHelper::FrameComplete);
		OnProcessComplete.AddRaw(this, &FPipelineTestHelper::ProcessComplete);

		FPipelineRunParameters PipelineRunParameters;
		PipelineRunParameters.SetMode(InPipelineMode);
		PipelineRunParameters.SetCheckThreadLimit(false);
		PipelineRunParameters.SetStartFrame(StartFrame);
		PipelineRunParameters.SetEndFrame(EndFrame);
		PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
		PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
		PipelineRunParameters.SetProcessNodesInRandomOrder(bProcessNodesInRandomOrder);

		StartTime = FPlatformTime::Seconds();

		Pipeline.Run(PipelineRunParameters);
	}

	FPipeline Pipeline;
	int32 FrameCompleteCount = 0;
	int32 ProcessCompleteCount = 0;
	EPipelineExitStatus ExitStatus = EPipelineExitStatus::Unknown;
	int32 ErrorNodeCode = -1;
	int32 StartFrame = 0;
	int32 EndFrame = -1;
	bool bProcessNodesInRandomOrder = true;
	bool bDropFramesPresent = false;
	TArray<TSharedPtr<FProcessCountNode>> ProcessCountNodes;
	double StartTime = 0;
	double EndTime = 0;
	TSharedPtr<FFaceTrackerIPhoneManagedNode> NLS;
	int32 CancelOnFrame = -1;

	TArray<TSharedPtr<const FPipelineData>> PipelineData;

	FCameraCalibration BotCamera;
	FCameraCalibration TopCamera;

private:

	FFrameComplete OnFrameComplete;
	FProcessComplete OnProcessComplete;

	void FrameComplete(TSharedPtr<FPipelineData> InPipelineData)
	{
		int32 FrameNumber = InPipelineData->GetFrameNumber();

		checkf(bDropFramesPresent || (StartFrame + FrameCompleteCount == FrameNumber), TEXT("Out of order"));

		++FrameCompleteCount;
		PipelineData.Add(InPipelineData);

		if (!ProcessCountNodes.IsEmpty() && FrameNumber < EndFrame / 2) // Process count naturally syncs towards end of sequence, so only check upto half way point
		{
			bool bInOrder = true;

			for (TSharedPtr<FProcessCountNode> Node : ProcessCountNodes)
			{
				bInOrder &= ((Node->ProcessCount - 1) == FrameNumber);
			}

			if (bProcessNodesInRandomOrder)
			{
				checkf(!bInOrder, TEXT("Node process in order"));
			}
			else
			{
				checkf(bInOrder, TEXT("Node process out of order"));
			}
		}

		if (FrameNumber == CancelOnFrame)
		{
			Pipeline.Cancel();
		}
	}

	void ProcessComplete(TSharedPtr<FPipelineData> InPipelineData)
	{
		++ProcessCompleteCount;

		ExitStatus = InPipelineData->GetExitStatus();
		if (ExitStatus != EPipelineExitStatus::Ok)
		{
			ErrorNodeCode = InPipelineData->GetErrorNodeCode();
		}

		EndTime = FPlatformTime::Seconds();
	}
};

FPipelineTestHelper::FPipelineTestHelper()
{
	BotCamera.CameraId = "bot";
	BotCamera.CameraType = FCameraCalibration::Video;
	BotCamera.ImageSize = FVector2D(480, 640);
	BotCamera.FocalLength = FVector2D(1494.448551325808, 1494.448551325808);
	BotCamera.PrincipalPoint = FVector2D(240, 320);

	BotCamera.Transform.M[0][0] = 0.94567626642431013;
	BotCamera.Transform.M[1][0] = 0.31667052248821492;
	BotCamera.Transform.M[2][0] = -0.07359469620032788;
	BotCamera.Transform.M[3][0] = -3.907948145288703;
	BotCamera.Transform.M[0][1] = -0.3222698742519966;
	BotCamera.Transform.M[1][1] = 0.94293701001580899;
	BotCamera.Transform.M[2][1] = -0.083737227635314729;
	BotCamera.Transform.M[3][1] = -4.9820764679624876;
	BotCamera.Transform.M[0][2] = 0.042878051161169294;
	BotCamera.Transform.M[1][2] = 0.10290566228098054;
	BotCamera.Transform.M[2][2] = 0.9937665205666435;
	BotCamera.Transform.M[3][2] = 15.4025488551616;
	BotCamera.Transform.M[0][3] = 0.0;
	BotCamera.Transform.M[1][3] = -0.0;
	BotCamera.Transform.M[2][3] = -0.0;
	BotCamera.Transform.M[3][3] = 1.0;

	TopCamera = BotCamera;
	TopCamera.CameraId = "top";
	TopCamera.FocalLength = FVector2D(1495.6382196765228, 1495.6382196765228);

	TopCamera.Transform.M[0][0] = 0.94308271212090788;
	TopCamera.Transform.M[1][0] = 0.32360438403460923;
	TopCamera.Transform.M[2][0] = -0.076649858005443924;
	TopCamera.Transform.M[3][0] = -3.974577095623212;
	TopCamera.Transform.M[0][1] = -0.33118177108338021;
	TopCamera.Transform.M[1][1] = 0.89294359132427703;
	TopCamera.Transform.M[2][1] = -0.30491044130199768;
	TopCamera.Transform.M[3][1] = -5.2968628682006909;
	TopCamera.Transform.M[0][2] = -0.03022635606137684;
	TopCamera.Transform.M[1][2] = 0.31294080166460331;
	TopCamera.Transform.M[2][2] = 0.94929153691201063;
	TopCamera.Transform.M[3][2] = 14.300102330206053;
}

#define TEST_PIPELINE_DATA(A, B, C, D) \
 bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[(A)]->HasData<B>((C))); \
 bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[(A)]->GetData<B>((C)), (D)));

static TSharedPtr<FPipelineTestHelper> TestHelper = nullptr; // Assumes tests runs one at a time and in order!
static TArray<uint8> PredictiveWithoutTeethSolver; // used for the global teeth solve
static TArray<FFrameTrackingContourData> TrackingData;
static TArray<FFrameAnimationData> FrameData;

static bool UEImagesAreEqual(const FUEImageDataType& InImage1, const FUEImageDataType& InImage2)
{
	return InImage1.Width == InImage2.Width && InImage1.Height == InImage2.Height && InImage1.Data == InImage2.Data;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPipelineTestNodesComplete, FString, Name);

bool FPipelineTestNodesComplete::Update()
{
	return (TestHelper && TestHelper->ProcessCompleteCount != 0);
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPipelineTestBasicNodes, "MetaHuman.Pipeline.BasicNodes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPipelineTestAdvancedNodes, "MetaHuman.Pipeline.AdvancedNodes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPipelineTestBenchmarks, "MetaHuman.Benchmarks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPipelineTestBasicNodes::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("Int");
	Tests.Add("Float");
	Tests.Add("Mix");
	Tests.Add("MultiSrc");
	Tests.Add("MultiInput");
	Tests.Add("MultiOutput");
	Tests.Add("MultiCommonInput");
	Tests.Add("MultiPath");
	Tests.Add("NonDirectInput");
	Tests.Add("NodeError-0-3-6");
	Tests.Add("NodeError-1-3-7");
	Tests.Add("NodeError-2-3-8");
	Tests.Add("NodeError-3-3-9");
	Tests.Add("PipelineError-1");
	Tests.Add("PipelineError-2");
	Tests.Add("PipelineError-3");
	Tests.Add("PipelineError-4");
	Tests.Add("PipelineError-5");
	Tests.Add("PipelineError-6");
	Tests.Add("PipelineError-7");
	Tests.Add("PipelineError-8");
	Tests.Add("PipelineError-9");
	Tests.Add("PipelineError-10");
	Tests.Add("PipelineError-11");
	Tests.Add("Queue-5-20-1");
	Tests.Add("Queue-20-5-1");
	Tests.Add("Queue-20-20-1");
	Tests.Add("Queue-5-20-10");
	Tests.Add("Queue-20-5-10");
	Tests.Add("Queue-20-20-10");
	Tests.Add("Queue-5-20-1000");
	Tests.Add("Queue-20-5-1000");
	Tests.Add("Queue-20-20-1000");
	Tests.Add("StartEndFrame");
	Tests.Add("DropFrame");
	Tests.Add("Buffer");
	Tests.Add("NonAsync");
	Tests.Add("Async");
	Tests.Add("AnimMergeOK");
	Tests.Add("AnimMergeError");
	Tests.Add("DepthQuantize");
	Tests.Add("DepthResize");
	Tests.Add("Cancel");

	for (const FString& Test : Tests)
	{
		for (const FString& Method : { "PushSync", "PushAsync", "PushSyncNodes", "PushAsyncNodes" })
		{
			for (const FString& Stage : { "Stage1", "Stage2", "Stage3" })
			{
				OutBeautifiedNames.Add(Test + " " + Method + " " + Stage);
				OutTestCommands.Add(Test + " " + Method + " " + Stage);
			}
		}
	}

	Tests.Reset();
	Tests.Add("RandomNodeOrder");
	Tests.Add("LinearNodeOrder");

	for (const FString& Test : Tests)
	{
		for (const FString& Method : { "PushSync" })
		{
			for (const FString& Stage : { "Stage1", "Stage2", "Stage3" })
			{
				OutBeautifiedNames.Add(Test + " " + Method + " " + Stage);
				OutTestCommands.Add(Test + " " + Method + " " + Stage);
			}
		}
	}
}

void FPipelineTestAdvancedNodes::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("Hyprsense");
	Tests.Add("HyprsenseSparse");
	Tests.Add("HyprsenseCompareColor");
	Tests.Add("HyprsenseCompareHMC");
	Tests.Add("Depth");
	Tests.Add("DepthGenerate");
	Tests.Add("FaceTrackerMonoPass1");
	Tests.Add("FaceTrackerMonoPass2");
	Tests.Add("FaceTrackerMonoPass3");
	Tests.Add("Grayscale");
	Tests.Add("Crop");
	Tests.Add("Composite");
	Tests.Add("Rotate");
	// TODO commenting out refinement tracker tests until upgraded to match the new HyprFace tracker
	Tests.Add("JsonTracker");
	Tests.Add("DepthMapDiagnostics");
	Tests.Add("RealtimeMono-None");
	Tests.Add("RealtimeMono-Input");
	Tests.Add("RealtimeMono-FaceDetect");
	Tests.Add("RealtimeMono-Headpose");
	Tests.Add("RealtimeMono-Trackers");
	Tests.Add("RealtimeMono-Solver");
	Tests.Add("RealtimeMonoSmoothing");
	Tests.Add("Audio");
	Tests.Add("RealtimeAudio");

	for (const FString& Test : Tests)
	{
		for (const FString& Method : { "PushSync", "PushAsync", "PushSyncNodes", "PushAsyncNodes" })
		{
			for (const FString& Stage : { "Stage1", "Stage2", "Stage3" })
			{
				OutBeautifiedNames.Add(Test + " " + Method + " " + Stage);
				OutTestCommands.Add(Test + " " + Method + " " + Stage);
			}
		}
	}
}

void FPipelineTestBenchmarks::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("RealtimeMono");

	for (const FString& Test : Tests)
	{
		for (const FString& Method : { "PushAsyncNodes" })
		{
			for (const FString& Stage : { "Stage1", "Stage2", "Stage3" })
			{
				OutBeautifiedNames.Add(Test + " " + Method + " " + Stage);
				OutTestCommands.Add(Test + " " + Method + " " + Stage);
			}
		}
	}
}

bool FPipelineTestBasicNodes::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	TArray<FString> Tokens;
	InTestCommand.ParseIntoArray(Tokens, TEXT(" "), true);
	bIsOK &= TestEqual<int32>(TEXT("Well formed Parameters"), Tokens.Num(), 3);

	if (bIsOK)
	{
		TArray<FString> Params;
		Tokens[0].ParseIntoArray(Params, TEXT("-"), true);
		FString Test = Params[0];
		Params.RemoveAt(0);

		FString Method = Tokens[1];
		FString Stage = Tokens[2];

		if (Stage == "Stage1")
		{
			bIsOK &= TestInvalid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				TestHelper = MakeShared<FPipelineTestHelper>();

				if (Test == "Int")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FIntIncNode> Inc = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc");
					TSharedPtr<FIntLogNode> Log = TestHelper->Pipeline.MakeNode<FIntLogNode>("Log");

					Src->Value = 10;
					Src->NumberOfFrames = 5;

					TestHelper->Pipeline.MakeConnection(Src, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Log);
				}
				else if (Test == "Float")
				{
					TSharedPtr<FFltSrcNode> Src = TestHelper->Pipeline.MakeNode<FFltSrcNode>("Src");
					TSharedPtr<FFltIncNode> Inc = TestHelper->Pipeline.MakeNode<FFltIncNode>("Inc");
					TSharedPtr<FFltLogNode> Log = TestHelper->Pipeline.MakeNode<FFltLogNode>("Log");

					Src->Value = 20.4f;
					Src->NumberOfFrames = 7;

					TestHelper->Pipeline.MakeConnection(Src, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Log);
				}
				else if (Test == "Mix")
				{
					TSharedPtr<FMixSrcNode> Src = TestHelper->Pipeline.MakeNode<FMixSrcNode>("Src");
					TSharedPtr<FMixIncNode> Inc = TestHelper->Pipeline.MakeNode<FMixIncNode>("Inc");
					TSharedPtr<FMixLogNode> Log = TestHelper->Pipeline.MakeNode<FMixLogNode>("Log");

					Src->IntValue = 30;
					Src->FltValue = 40.6f;
					Src->NumberOfFrames = 8;

					TestHelper->Pipeline.MakeConnection(Src, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Log);
				}
				else if (Test == "MultiSrc")
				{
					TSharedPtr<FIntSrcNode> Src1 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src1");
					TSharedPtr<FIntSrcNode> Src2 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src2");

					Src1->Value = 50;
					Src1->NumberOfFrames = 10;

					Src2->Value = 60;
					Src2->NumberOfFrames = 20;
				}
				else if (Test == "MultiInput")
				{
					TSharedPtr<FIntSrcNode> Src1 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src1");
					TSharedPtr<FIntSrcNode> Src2 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src2");
					TSharedPtr<FIntSumNode> Sum = TestHelper->Pipeline.MakeNode<FIntSumNode>("Sum");

					Src1->Value = 5;
					Src1->NumberOfFrames = 2;

					Src2->Value = 3;
					Src2->NumberOfFrames = 2;

					TestHelper->Pipeline.MakeConnection(Src1, Sum, 0, 0);
					TestHelper->Pipeline.MakeConnection(Src2, Sum, 0, 1);
				}
				else if (Test == "MultiOutput")
				{
					TSharedPtr<FIntSrcNode> Src1 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src1");
					TSharedPtr<FIntSrcNode> Src2 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src2");
					TSharedPtr<FIntSumNode> Sum = TestHelper->Pipeline.MakeNode<FIntSumNode>("Sum");
					TSharedPtr<FIntIncNode> Inc1 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc1");
					TSharedPtr<FIntDecNode> Dec1 = TestHelper->Pipeline.MakeNode<FIntDecNode>("Dec1");
					TSharedPtr<FIntIncNode> Inc2 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc2");
					TSharedPtr<FIntDecNode> Dec2 = TestHelper->Pipeline.MakeNode<FIntDecNode>("Dec2");

					Src1->Value = 5;
					Src1->NumberOfFrames = 2;

					Src2->Value = 3;
					Src2->NumberOfFrames = 2;

					TestHelper->Pipeline.MakeConnection(Src1, Sum, 0, 0);
					TestHelper->Pipeline.MakeConnection(Src2, Sum, 0, 1);
					TestHelper->Pipeline.MakeConnection(Sum, Inc1, 0, 0);
					TestHelper->Pipeline.MakeConnection(Sum, Dec1, 0, 0);
					TestHelper->Pipeline.MakeConnection(Sum, Inc2, 1, 0);
					TestHelper->Pipeline.MakeConnection(Sum, Dec2, 1, 0);
				}
				else if (Test == "MultiCommonInput")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FIntSumNode> Sum = TestHelper->Pipeline.MakeNode<FIntSumNode>("Sum");

					Src->Value = 5;
					Src->NumberOfFrames = 2;

					TestHelper->Pipeline.MakeConnection(Src, Sum, 0, 0);
					TestHelper->Pipeline.MakeConnection(Src, Sum, 0, 1);
				}
				else if (Test == "MultiPath")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FIntIncNode> Inc1 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc1");
					TSharedPtr<FIntIncNode> Inc2 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc2");
					TSharedPtr<FIntIncNode> Inc3 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc3");
					TSharedPtr<FIntSumNode> Sum = TestHelper->Pipeline.MakeNode<FIntSumNode>("Sum");
					TSharedPtr<FIntIncNode> Inc4 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc4");

					Src->NumberOfFrames = 33;
					Src->Value = 21;

					TestHelper->Pipeline.MakeConnection(Src, Inc1);
					TestHelper->Pipeline.MakeConnection(Inc1, Inc2);
					TestHelper->Pipeline.MakeConnection(Inc1, Inc3);
					TestHelper->Pipeline.MakeConnection(Inc2, Sum, 0, 0);
					TestHelper->Pipeline.MakeConnection(Inc3, Sum, 0, 1);
					TestHelper->Pipeline.MakeConnection(Sum, Inc4);
				}
				else if (Test == "NonDirectInput")
				{
					TSharedPtr<FMixSrcNode> Src = TestHelper->Pipeline.MakeNode<FMixSrcNode>("Src");
					TSharedPtr<FIntIncNode> Inc1 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc1");
					TSharedPtr<FFltIncNode> Inc2 = TestHelper->Pipeline.MakeNode<FFltIncNode>("Inc2");

					Src->IntValue = 289;
					Src->FltValue = -67.3f;
					Src->NumberOfFrames = 23;

					TestHelper->Pipeline.MakeConnection(Src, Inc1);
					TestHelper->Pipeline.MakeConnection(Inc1, Inc2);
				}
				else if (Test == "NodeError")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FErrorNode> Err = TestHelper->Pipeline.MakeNode<FErrorNode>("Err");

					Src->Value = 10;
					Src->NumberOfFrames = 5;

					Err->ErrorOnStage = FCString::Atoi(*Params[0]);
					Err->ErrorOnFrame = FCString::Atoi(*Params[1]);
					Err->ErrorCode = FCString::Atoi(*Params[2]);

					TestHelper->Pipeline.MakeConnection(Src, Err);
				}
				else if (Test == "PipelineError")
				{
					int32 TestNumber = FCString::Atoi(*Params[0]);

					if (TestNumber == 1)
					{
						TestHelper->Pipeline.MakeNode<FIntSrcNode>("");
					}
					else if (TestNumber == 2)
					{
						TestHelper->Pipeline.MakeNode<FIntSrcNode>("A.B");
					}
					else if (TestNumber == 3)
					{
						TestHelper->Pipeline.MakeNode<FIntSrcNode>("Reserved");
					}
					else if (TestNumber == 4)
					{
						TestHelper->Pipeline.MakeNode<FIntSrcNode>("Same name");
						TestHelper->Pipeline.MakeNode<FIntSrcNode>("Same name");
					}
					else if (TestNumber == 5)
					{
						TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
						Src->Pins.Add(FPin("", EPinDirection::Output, EPinType::Float));
					}
					else if (TestNumber == 6)
					{
						TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
						Src->Pins.Add(FPin("A.B", EPinDirection::Output, EPinType::Float));
					}
					else if (TestNumber == 7)
					{
						TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
						Src->Pins.Add(FPin("Int Out", EPinDirection::Output, EPinType::Float));
					}
					else if (TestNumber == 8)
					{
						TSharedPtr<FIntSrcNode> Src1 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src1");
						TSharedPtr<FIntSrcNode> Src2 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src2");
						TSharedPtr<FIntIncNode> Inc = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc");

						TestHelper->Pipeline.MakeConnection(Src1, Inc);
						TestHelper->Pipeline.MakeConnection(Src2, Inc);
					}
					else if (TestNumber == 9)
					{
						TSharedPtr<FIntSrcNode> Src1 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src1");
						TSharedPtr<FIntSrcNode> Src2 = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src2");
						TSharedPtr<FIntsToFltNode> IntsToFlt = TestHelper->Pipeline.MakeNode<FIntsToFltNode>("IntsToFlt");
						TSharedPtr<FIntIncNode> Inc = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc");

						TestHelper->Pipeline.MakeConnection(Src1, IntsToFlt, 0, 0);
						TestHelper->Pipeline.MakeConnection(Src2, IntsToFlt, 0, 1);
						TestHelper->Pipeline.MakeConnection(IntsToFlt, Inc);
					}
					else if (TestNumber == 10)
					{
						TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
						TSharedPtr<FIntSumNode> Sum = TestHelper->Pipeline.MakeNode<FIntSumNode>("Sum");

						TestHelper->Pipeline.MakeConnection(Src, Sum);
					}
					else if (TestNumber == 11)
					{
						TSharedPtr<FIntIncNode> Inc1 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc1");
						TSharedPtr<FIntIncNode> Inc2 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc2");
						TSharedPtr<FIntIncNode> Inc3 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc3");
						TSharedPtr<FIntIncNode> Inc4 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc4");
						TSharedPtr<FIntIncNode> Inc5 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc5");

						TestHelper->Pipeline.MakeConnection(Inc1, Inc2);
						TestHelper->Pipeline.MakeConnection(Inc2, Inc3);
						TestHelper->Pipeline.MakeConnection(Inc3, Inc4);
						TestHelper->Pipeline.MakeConnection(Inc4, Inc5);
						TestHelper->Pipeline.MakeConnection(Inc3, Inc1);
					}
					else
					{
						bIsOK &= TestTrue(TEXT("Known test number"), false);
					}
				}
				else if (Test == "Queue")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");

					Src->Value = 90;
					Src->NumberOfFrames = FCString::Atoi(*Params[0]);
					Src->QueueSize = FCString::Atoi(*Params[2]);

					TSharedPtr<FIntIncNode> Inc1 = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc1");
					Inc1->QueueSize = FCString::Atoi(*Params[2]);
					TestHelper->Pipeline.MakeConnection(Src, Inc1);

					TSharedPtr<FNode> UpstreamNode = Src;
					for (int32 Index = 0; Index < FCString::Atoi(*Params[1]); ++Index)
					{
						TSharedPtr<FIntIncNode> Inc2N = TestHelper->Pipeline.MakeNode<FIntIncNode>(FString::Printf(TEXT("Inc2-%04i"), Index));
						Inc2N->QueueSize = FCString::Atoi(*Params[2]);

						TestHelper->Pipeline.MakeConnection(UpstreamNode, Inc2N);
						UpstreamNode = Inc2N;
					}
				}
				else if (Test == "StartEndFrame")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FIntIncNode> Inc = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc");
					TSharedPtr<FIntLogNode> Log = TestHelper->Pipeline.MakeNode<FIntLogNode>("Log");

					Src->Value = 10;
					Src->NumberOfFrames = 20;

					TestHelper->Pipeline.MakeConnection(Src, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Log);

					TestHelper->StartFrame = 3;
					TestHelper->EndFrame = 7;
				}
				else if (Test == "RandomNodeOrder" || Test == "LinearNodeOrder")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");

					Src->Value = 10;
					Src->NumberOfFrames = 50;

					TSharedPtr<FNode> UpstreamNode = Src;
					for (int32 Index = 0; Index < 100; ++Index)
					{
						TSharedPtr<FProcessCountNode> ProcessCountNode = TestHelper->Pipeline.MakeNode<FProcessCountNode>(FString::Printf(TEXT("ProcessCount-%04i"), Index));

						TestHelper->Pipeline.MakeConnection(UpstreamNode, ProcessCountNode);
						UpstreamNode = ProcessCountNode;

						TestHelper->ProcessCountNodes.Add(ProcessCountNode);
					}

					TestHelper->StartFrame = 0;
					TestHelper->EndFrame = 50;

					TestHelper->bProcessNodesInRandomOrder = (Test == "RandomNodeOrder");
				}
				else if (Test == "DropFrame")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FDropFrameNode> Drop1 = TestHelper->Pipeline.MakeNode<FDropFrameNode>("Drop1");
					TSharedPtr<FIntIncNode> Inc = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc");
					TSharedPtr<FDropFrameNode> Drop2 = TestHelper->Pipeline.MakeNode<FDropFrameNode>("Drop2");
					TSharedPtr<FIntLogNode> Log = TestHelper->Pipeline.MakeNode<FIntLogNode>("Log");

					Src->Value = 400;
					Src->NumberOfFrames = 20;

					Drop1->DropEvery = 2;
					Drop2->DropEvery = 5;

					TestHelper->Pipeline.MakeConnection(Src, Drop1);
					TestHelper->Pipeline.MakeConnection(Drop1, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Drop2);
					TestHelper->Pipeline.MakeConnection(Drop2, Log);

					TestHelper->bDropFramesPresent = true;
				}
				else if (Test == "Buffer")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FBufferNode> Buf1 = TestHelper->Pipeline.MakeNode<FBufferNode>("Buf1");
					TSharedPtr<FIntIncNode> Inc = TestHelper->Pipeline.MakeNode<FIntIncNode>("Inc");
					TSharedPtr<FBufferNode> Buf2 = TestHelper->Pipeline.MakeNode<FBufferNode>("Buf2");
					TSharedPtr<FIntLogNode> Log = TestHelper->Pipeline.MakeNode<FIntLogNode>("Log");

					Src->Value = 10;
					Src->NumberOfFrames = 5;

					TestHelper->Pipeline.MakeConnection(Src, Buf1);
					TestHelper->Pipeline.MakeConnection(Buf1, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Buf2);
					TestHelper->Pipeline.MakeConnection(Buf2, Log);
				}
				else if (Test == "NonAsync" || Test == "Async")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");

					TSharedPtr<FNode> Inc;
					if (Test == "NonAsync")
					{
						Inc = TestHelper->Pipeline.MakeNode<FSlowIntIncNode>("Inc");
					}
					else
					{
						Inc = TestHelper->Pipeline.MakeNode<FAsyncNode<FSlowIntIncNode>>(3, "Inc");
					}

					TSharedPtr<FIntLogNode> Log = TestHelper->Pipeline.MakeNode<FIntLogNode>("Log");

					Src->Value = 10;
					Src->NumberOfFrames = 5;

					TestHelper->Pipeline.MakeConnection(Src, Inc);
					TestHelper->Pipeline.MakeConnection(Inc, Log);
				}
				else if (Test == "AnimMergeOK" || Test == "AnimMergeError")
				{
					TSharedPtr<FAnimSrcNode> Src1 = TestHelper->Pipeline.MakeNode<FAnimSrcNode>("Src1");
					TSharedPtr<FAnimSrcNode> Src2 = TestHelper->Pipeline.MakeNode<FAnimSrcNode>("Src2");
					TSharedPtr<FAnimationMergeNode> Merge = TestHelper->Pipeline.MakeNode<FAnimationMergeNode>("Merge");

					Src1->AnimationData.Add("Control1", 1.0f);
					Src1->AnimationData.Add("Control2", 2.0f);
					Src1->AnimationData.Add("Control3", 3.0f);

					Src2->AnimationData.Add("Control2", 2.5f);

					if (Test == "AnimMergeError")
					{
						Src2->AnimationData.Add("Control4", 4);

						Src1->NumberOfFrames = 10;
						Src2->NumberOfFrames = 10;
					}

					TestHelper->Pipeline.MakeConnection(Src1, Merge, 0, 0);
					TestHelper->Pipeline.MakeConnection(Src2, Merge, 0, 1);
				}
				else if (Test == "DepthQuantize")
				{
					TSharedPtr<FDepthSrcNode> Src = TestHelper->Pipeline.MakeNode<FDepthSrcNode>("Src");
					TSharedPtr<FDepthQuantizeNode> Quantize = TestHelper->Pipeline.MakeNode<FDepthQuantizeNode>("Quantize");

					Src->DepthData.Width = 4;
					Src->DepthData.Height = 2;
					Src->DepthData.Data.Add(0.0000f);
					Src->DepthData.Data.Add(0.0100f);
					Src->DepthData.Data.Add(0.0125f);
					Src->DepthData.Data.Add(0.0150f);
					Src->DepthData.Data.Add(2.4900f);
					Src->DepthData.Data.Add(2.5000f);
					Src->DepthData.Data.Add(2.5100f);
					Src->DepthData.Data.Add(0.0000f);

					Quantize->Factor = 80;

					TestHelper->Pipeline.MakeConnection(Src, Quantize);
				}
				else if (Test == "DepthResize")
				{
					TSharedPtr<FDepthSrcNode> Src = TestHelper->Pipeline.MakeNode<FDepthSrcNode>("Src");
					TSharedPtr<FDepthResizeNode> Resize = TestHelper->Pipeline.MakeNode<FDepthResizeNode>("Resize");

					Src->DepthData.Width = 5;
					Src->DepthData.Height = 2;

					Src->DepthData.Data.Add(0.0000f); // (0, 0)
					Src->DepthData.Data.Add(0.0100f); // (1, 0)
					Src->DepthData.Data.Add(0.0125f); // (2, 0)
					Src->DepthData.Data.Add(0.0150f); // (3, 0)
					Src->DepthData.Data.Add(9.9999f); // (4, 0)

					Src->DepthData.Data.Add(2.4900f); // (0, 1)
					Src->DepthData.Data.Add(2.5000f); // (1, 1)
					Src->DepthData.Data.Add(2.5100f); // (2, 1)
					Src->DepthData.Data.Add(2.5200f); // (3, 1)
					Src->DepthData.Data.Add(9.9999f); // (4, 1)

					Resize->Factor = 2;

					TestHelper->Pipeline.MakeConnection(Src, Resize);
				}
				else if (Test == "Cancel")
				{
					TSharedPtr<FIntSrcNode> Src = TestHelper->Pipeline.MakeNode<FIntSrcNode>("Src");
					TSharedPtr<FSlowIntIncNode> Slow = TestHelper->Pipeline.MakeNode<FSlowIntIncNode>("Inc");

					Src->Value = 10;
					Src->NumberOfFrames = 20;

					TestHelper->CancelOnFrame = 5;

					TestHelper->Pipeline.MakeConnection(Src, Slow);
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known test"), false);
				}
			}
		}
		else if (Stage == "Stage2")
		{
			bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				if (Test == "NodeError")
				{
					int32 ErrorOnStage = FCString::Atoi(*Params[0]);

					if (ErrorOnStage == 0)
					{
						AddExpectedError("Start error in node \"Err\"", EAutomationExpectedMessageFlags::Contains, 1);
					}
					else if (ErrorOnStage == 1)
					{
						AddExpectedError("Process error in node \"Err\"", EAutomationExpectedMessageFlags::Contains, 1);
					}
					else if (ErrorOnStage == 2)
					{
						AddExpectedError("End error in node \"Err\"", EAutomationExpectedMessageFlags::Contains, 1);
					}
				}
				else if (Test == "AnimMergeError")
				{
					AddExpectedError("Process error in node \"Merge\" on frame 0", EAutomationExpectedMessageFlags::Contains, 1);
				}

				if (Method == "PushSync")
				{
					TestHelper->Run(EPipelineMode::PushSync);
				}
				else if (Method == "PushAsync")
				{
					TestHelper->Run(EPipelineMode::PushAsync);
				}
				else if (Method == "PushSyncNodes")
				{
					TestHelper->Run(EPipelineMode::PushSyncNodes);
				}
				else if (Method == "PushAsyncNodes")
				{
					TestHelper->Run(EPipelineMode::PushAsyncNodes);
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known method"), false);
				}

				if (bIsOK)
				{
					ADD_LATENT_AUTOMATION_COMMAND(FPipelineTestNodesComplete(Test));
				}
			}
		}
		else if (Stage == "Stage3")
		{
			bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				bIsOK &= TestEqual(TEXT("Process complete count"), TestHelper->ProcessCompleteCount, 1);

				if (Test != "NodeError" && Test != "PipelineError" && Test != "AnimMergeError" && Test != "Cancel")
				{
					bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::Ok);
					bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, -1);
				}

				if (Test == "Int" || Test == "Buffer")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 5);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 10 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Inc.Int Out", 10 + Frame + 1);

						bIsOK &= TestTrue(TEXT("Data is correct type"), !TestHelper->PipelineData[Frame]->HasData<float>("Inc.Int Out"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<int32>("Inc.BOGUS"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<int32>("BOGUS.Int Out"));
					}
				}
				else if (Test == "Float")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 7);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, float, "Src.Flt Out", 20.4f + Frame);
						TEST_PIPELINE_DATA(Frame, float, "Inc.Flt Out", 20.4f + Frame + 0.1f);

						bIsOK &= TestTrue(TEXT("Data is correct type"), !TestHelper->PipelineData[Frame]->HasData<int32>("Inc.Flt Out"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<float>("Inc.BOGUS"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<float>("BOGUS.Flt Out"));
					}
				}
				else if (Test == "Mix")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 8);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 30 + Frame);
						TEST_PIPELINE_DATA(Frame, float, "Src.Flt Out", 40.6f + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Inc.Int Out", 30 + Frame + 1);
						TEST_PIPELINE_DATA(Frame, float, "Inc.Flt Out", 40.6f + Frame + 0.1f);

						bIsOK &= TestTrue(TEXT("Data is correct type"), !TestHelper->PipelineData[Frame]->HasData<float>("Inc.Int Out"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<int32>("Inc.BOGUS"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<int32>("BOGUS.Int Out"));

						bIsOK &= TestTrue(TEXT("Data is correct type"), !TestHelper->PipelineData[Frame]->HasData<int32>("Inc.Flt Out"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<float>("Inc.BOGUS"));
						bIsOK &= TestTrue(TEXT("Invalid data present"), !TestHelper->PipelineData[Frame]->HasData<float>("BOGUS.Flt Out"));
					}
				}
				else if (Test == "MultiSrc")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src1.Int Out", 50 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Src2.Int Out", 60 + Frame);
					}
				}
				else if (Test == "MultiInput")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 2);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src1.Int Out", 5 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Src2.Int Out", 3 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int1 Out", (5 + Frame) + (3 + Frame));
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int2 Out", (5 + Frame) - (3 + Frame));
					}
				}
				else if (Test == "MultiOutput")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 2);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src1.Int Out", 5 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Src2.Int Out", 3 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int1 Out", (5 + Frame) + (3 + Frame));
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int2 Out", (5 + Frame) - (3 + Frame));
						TEST_PIPELINE_DATA(Frame, int32, "Inc1.Int Out", (5 + Frame) + (3 + Frame) + 1);
						TEST_PIPELINE_DATA(Frame, int32, "Dec1.Int Out", (5 + Frame) + (3 + Frame) - 1);
						TEST_PIPELINE_DATA(Frame, int32, "Inc2.Int Out", (5 + Frame) - (3 + Frame) + 1);
						TEST_PIPELINE_DATA(Frame, int32, "Dec2.Int Out", (5 + Frame) - (3 + Frame) - 1);
					}
				}
				else if (Test == "MultiCommonInput")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 2);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 5 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int1 Out", (5 + Frame) + (5 + Frame));
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int2 Out", (5 + Frame) - (5 + Frame));
					}
				}
				else if (Test == "NonDirectInput")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 23);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 289 + Frame);
						TEST_PIPELINE_DATA(Frame, float, "Src.Flt Out", -67.3f + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Inc1.Int Out", 289 + Frame + 1);
						TEST_PIPELINE_DATA(Frame, float, "Inc2.Flt Out", -67.3f + Frame + 0.1f);
					}
				}
				else if (Test == "MultiPath")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 33);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 21 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Inc1.Int Out", 21 + Frame + 1);
						TEST_PIPELINE_DATA(Frame, int32, "Inc2.Int Out", 21 + Frame + 1 + 1);
						TEST_PIPELINE_DATA(Frame, int32, "Inc3.Int Out", 21 + Frame + 1 + 1);
						TEST_PIPELINE_DATA(Frame, int32, "Sum.Int1 Out", (21 + Frame + 1 + 1) * 2);
						TEST_PIPELINE_DATA(Frame, int32, "Inc4.Int Out", (21 + Frame + 1 + 1) * 2 + 1);
					}
				}
				else if (Test == "NodeError")
				{
					if (FCString::Atoi(*Params[0]) == 0)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::StartError);
						bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, FCString::Atoi(*Params[2]));
						bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 0);
					}
					else if (FCString::Atoi(*Params[0]) == 2)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::EndError);
						bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, FCString::Atoi(*Params[2]));
						bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 5);
					}
					else
					{
						if (FCString::Atoi(*Params[0]) == 1)
						{
							bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::ProcessError);
							bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, FCString::Atoi(*Params[2]));
							bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, FCString::Atoi(*Params[1]));
						}
						else
						{
							bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::Ok);
							bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, -1);
							bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 5);
						}

						for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
						{
							TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 10 + Frame);
							TEST_PIPELINE_DATA(Frame, int32, "Err.Int Out", 10 + Frame + 1);
						}
					}
				}
				else if (Test == "PipelineError")
				{
					int32 TestNumber = FCString::Atoi(*Params[0]);

					if (TestNumber == 1 || TestNumber == 2 || TestNumber == 3)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::InvalidNodeName);
					}
					else if (TestNumber == 4)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::DuplicateNodeName);
					}
					else if (TestNumber == 5 || TestNumber == 6)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::InvalidPinName);
					}
					else if (TestNumber == 7)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::DuplicatePinName);
					}
					else if (TestNumber == 8 || TestNumber == 9)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::AmbiguousConnection);
					}
					else if (TestNumber == 10)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::Unconnected);
					}
					else if (TestNumber == 11)
					{
						bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::LoopConnection);
					}
					else
					{
						bIsOK &= TestTrue(TEXT("Known test number"), false);
					}

					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 0);
				}
				else if (Test == "Queue")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, FCString::Atoi(*Params[0]));

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 90 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Inc1.Int Out", 90 + Frame + 1);

						for (int32 Index = 0; Index < FCString::Atoi(*Params[1]); ++Index)
						{
							FString Inc2N = FString::Printf(TEXT("Inc2-%04i.Int Out"), Index);
							TEST_PIPELINE_DATA(Frame, int32, Inc2N, 90 + Frame + 1 + Index);
						}
					}
				}
				else if (Test == "StartEndFrame")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 4);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 10 + Frame + 3);
						TEST_PIPELINE_DATA(Frame, int32, "Inc.Int Out", 10 + Frame + 3 + 1);
					}
				}
				else if (Test == "RandomNodeOrder" || Test == "LinearNodeOrder")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 50);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						for (int32 Index = 0; Index < 100; ++Index)
						{
							FString ProcessCountN = FString::Printf(TEXT("ProcessCount-%04i.Int Out"), Index);
							TEST_PIPELINE_DATA(Frame, int32, ProcessCountN, 10 + Frame + 1 + Index);
						}
					}
				}
				else if (Test == "DropFrame")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 8);

					int PresentFrames = 0;
					for (int32 AllFrames = 0; AllFrames < 20; ++AllFrames)
					{
						if (AllFrames % 2 == 0) continue;
						if (AllFrames % 5 == 0) continue;

						TEST_PIPELINE_DATA(PresentFrames, int32, "Src.Int Out", 400 + AllFrames);
						TEST_PIPELINE_DATA(PresentFrames, int32, "Inc.Int Out", 400 + AllFrames + 1);

						PresentFrames++;
					}

					bIsOK &= TestEqual(TEXT("Present frame completed count"), PresentFrames, TestHelper->FrameCompleteCount);
				}
				else if (Test == "NonAsync" || Test == "Async")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 5);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						TEST_PIPELINE_DATA(Frame, int32, "Src.Int Out", 10 + Frame);
						TEST_PIPELINE_DATA(Frame, int32, "Inc.Int Out", 10 + Frame + 1);
					}

					if (Test == "NonAsync")
					{
						bIsOK &= TestTrue(TEXT("Expected speed"), TestHelper->EndTime - TestHelper->StartTime > 4); // should take ~5 secs
					}
					else
					{
						bIsOK &= TestTrue(TEXT("Expected speed"), TestHelper->EndTime - TestHelper->StartTime < 3); // should take ~2 secs
					}
				}
				else if (Test == "AnimMergeOK")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 1);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("Merge.Animation Out"));

						if (bIsOK)
						{
							FFrameAnimationData FrameAnimationData = TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("Merge.Animation Out");

							if (FrameAnimationData.AnimationData.Num() == 3 && 
								FrameAnimationData.AnimationData.Contains("Control1") && 
								FrameAnimationData.AnimationData.Contains("Control2") && 
								FrameAnimationData.AnimationData.Contains("Control3"))
							{
								bIsOK &= TestEqual(TEXT("Control1 value"), FrameAnimationData.AnimationData["Control1"], 1.0f);
								bIsOK &= TestEqual(TEXT("Control2 value"), FrameAnimationData.AnimationData["Control2"], 2.5f);
								bIsOK &= TestEqual(TEXT("Control3 value"), FrameAnimationData.AnimationData["Control3"], 3.0f);
							}
							else
							{
								bIsOK &= TestTrue(TEXT("Controls present"), false);
							}
						}
					}
				}
				else if (Test == "AnimMergeError")
				{
					bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::ProcessError);
					bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, FAnimationMergeNode::UnknownControlValue);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 0);
				}
				else if (Test == "DepthQuantize")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 1);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FDepthDataType>("Quantize.Depth Out"));

						if (bIsOK)
						{
							FDepthDataType DepthData = TestHelper->PipelineData[Frame]->GetData<FDepthDataType>("Quantize.Depth Out");

							bIsOK &= TestEqual(TEXT("Width"), DepthData.Width, 4);
							bIsOK &= TestEqual(TEXT("Height"), DepthData.Height, 2);
							bIsOK &= TestEqual(TEXT("Size"), DepthData.Data.Num(), DepthData.Width * DepthData.Height);

							if (bIsOK)
							{
								bIsOK &= TestEqual(TEXT("Data0"), DepthData.Data[0], 0.0000f);
								bIsOK &= TestEqual(TEXT("Data1"), DepthData.Data[1], 0.0000f);
								bIsOK &= TestEqual(TEXT("Data2"), DepthData.Data[2], 0.0125f);
								bIsOK &= TestEqual(TEXT("Data3"), DepthData.Data[3], 0.0125f);
								bIsOK &= TestEqual(TEXT("Data4"), DepthData.Data[4], 2.4875f);
								bIsOK &= TestEqual(TEXT("Data5"), DepthData.Data[5], 2.5000f);
								bIsOK &= TestEqual(TEXT("Data6"), DepthData.Data[6], 2.5000f);
								bIsOK &= TestEqual(TEXT("Data7"), DepthData.Data[7], 0.0000f);
							}
						}
					}
				}
				else if (Test == "DepthResize")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 1);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FDepthDataType>("Resize.Depth Out"));

						if (bIsOK)
						{
							FDepthDataType DepthData = TestHelper->PipelineData[Frame]->GetData<FDepthDataType>("Resize.Depth Out");

							bIsOK &= TestEqual(TEXT("Width"), DepthData.Width, 2);
							bIsOK &= TestEqual(TEXT("Height"), DepthData.Height, 1);
							bIsOK &= TestEqual(TEXT("Size"), DepthData.Data.Num(), DepthData.Width * DepthData.Height);

							if (bIsOK)
							{
								bIsOK &= TestEqual(TEXT("Data0"), DepthData.Data[0], (0.0000f + 0.0100f + 2.4900f + 2.5000f) / 3);
								bIsOK &= TestEqual(TEXT("Data1"), DepthData.Data[1], (0.0125f + 0.0150f + 2.5100f + 2.5200f) / 4);
							}
						}
					}
				}
				else if (Test == "Cancel")
				{
					bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::Aborted);
					bIsOK &= TestTrue(TEXT("Frame completed count"), TestHelper->FrameCompleteCount < 10);
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known test"), false);
				}
			}

			TestHelper = nullptr;
		}
		else
		{
			bIsOK &= TestTrue(TEXT("Known stage"), false);
		}
	}

	return bIsOK;
}

bool FPipelineTestAdvancedNodes::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	TArray<FString> Tokens;
	InTestCommand.ParseIntoArray(Tokens, TEXT(" "), true);
	bIsOK &= TestEqual<int32>(TEXT("Well formed Parameters"), Tokens.Num(), 3);

	if (bIsOK)
	{
		TArray<FString> Params;
		Tokens[0].ParseIntoArray(Params, TEXT("-"), true);
		FString Test = Params[0];
		Params.RemoveAt(0);

		FString Method = Tokens[1];
		FString Stage = Tokens[2];

		if (Stage == "Stage1")
		{
			bIsOK &= TestInvalid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				TestHelper = MakeShared<FPipelineTestHelper>();

				FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
				FString TestDataDir = PluginDir + "/TestData/";
				FString OutputDir = FPaths::ProjectIntermediateDir() + "/TestOutput";

				if (Test == "Hyprsense")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FHyprsenseManagedNode> Track = TestHelper->Pipeline.MakeNode<FHyprsenseManagedNode>("Track");
					TSharedPtr<FBurnContoursNode> Burn = TestHelper->Pipeline.MakeNode<FBurnContoursNode>("Burn");
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					Save->FilePath = OutputDir + "/Hyprsense/%04d.png";
					Save->FrameNumberOffset = 0;

					TestHelper->Pipeline.MakeConnection(Load, Track);
					TestHelper->Pipeline.MakeConnection(Track, Burn);
					TestHelper->Pipeline.MakeConnection(Burn, Save);
				}
				else if(Test == "HyprsenseSparse")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FHyprsenseSparseManagedNode> Track = TestHelper->Pipeline.MakeNode<FHyprsenseSparseManagedNode>("Track");
					TSharedPtr<FBurnContoursNode> Burn = TestHelper->Pipeline.MakeNode<FBurnContoursNode>("Burn");
					Burn->Size = 2;
					Burn->LineWidth = 1;
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					Save->FilePath = OutputDir + "/HyprsenseSparse/%04d.png";
					Save->FrameNumberOffset = 0;

					TestHelper->Pipeline.MakeConnection(Load, Track);
					TestHelper->Pipeline.MakeConnection(Track, Burn);
					TestHelper->Pipeline.MakeConnection(Burn, Save);
				}
				else if (Test == "DepthMapDiagnostics")
				{
					
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FHyprsenseSparseManagedNode> Track = TestHelper->Pipeline.MakeNode<FHyprsenseSparseManagedNode>("Track");
					TSharedPtr<FDepthLoadNode> Depth = TestHelper->Pipeline.MakeNode<FDepthLoadNode>("Depth");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Bot/%04d.png");
					Depth->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Depth/%04d.exr");

					TSharedPtr<FDepthMapDiagnosticsNode> Diagnostics = TestHelper->Pipeline.MakeNode<FDepthMapDiagnosticsNode>("Diagnostics");

					TestHelper->Pipeline.MakeConnection(Load, Track);
					TestHelper->Pipeline.MakeConnection(Depth, Diagnostics);
					TestHelper->Pipeline.MakeConnection(Track, Diagnostics);

					Diagnostics->Calibrations.SetNum(2);
					Diagnostics->Calibrations[0] = TestHelper->BotCamera;
					Diagnostics->Calibrations[0].CameraId = "color";
					Diagnostics->Calibrations[0].Transform = FMatrix::Identity;

					Diagnostics->Calibrations[1] = Diagnostics->Calibrations[0];
					Diagnostics->Calibrations[1].CameraId = "depth";
					Diagnostics->Calibrations[1].CameraType = FCameraCalibration::Depth;
					Diagnostics->Calibrations[1].ImageSize = FVector2D(832.0, 488.0);
					Diagnostics->Calibrations[1].FocalLength = FVector2D(1495.0434570312500, 1495.0434570312500);
					Diagnostics->Calibrations[1].PrincipalPoint = FVector2D(468.8218994140625, 247.99615478515625);

					Diagnostics->Camera = "color";
				}
				else if (Test == "RealtimeMono")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FNeutralFrameNode> NeutralFrame = TestHelper->Pipeline.MakeNode<FNeutralFrameNode>("Neutral Frame");
					TSharedPtr<FHyprsenseRealtimeNode> Realtime = TestHelper->Pipeline.MakeNode<FHyprsenseRealtimeNode>("Realtime");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					if (Params[0] == "None")
					{
						Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::None);
					}
					else if (Params[0] == "Input")
					{
						Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::Input);
					}
					else if (Params[0] == "FaceDetect")
					{
						Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::FaceDetect);
					}
					else if (Params[0] == "Headpose")
					{
						Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::Headpose);
					}
					else if (Params[0] == "Trackers")
					{
						Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::Trackers);
					}
					else if (Params[0] == "Solver")
					{
						Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::Solver);
					}
					else
					{
						check(false);
					}
					Realtime->LoadModels();

					TestHelper->Pipeline.MakeConnection(Load, NeutralFrame);
					TestHelper->Pipeline.MakeConnection(NeutralFrame, Realtime);

					if (Params[0] != "None")
					{
						TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

						Save->FilePath = OutputDir + "/Realtime/" + Params[0] + "/%04d.png";
						Save->FrameNumberOffset = 0;

						TestHelper->Pipeline.MakeConnection(Realtime, Save);
					}
				}
				else if (Test == "RealtimeMonoSmoothing")
				{
					TSharedPtr<FAnimSrcNode> Src = TestHelper->Pipeline.MakeNode<FAnimSrcNode>("Src");

					TMap<FString, float> ZeroValue;
					ZeroValue.Add("Control1", 0);
					ZeroValue.Add("Control2", 0);
					ZeroValue.Add("Control3", 0);
					ZeroValue.Add("Control4", 0);
					ZeroValue.Add("Control5", 0);

					TMap<FString, float> OneValue;
					OneValue.Add("Control1", 1);
					OneValue.Add("Control2", 1);
					OneValue.Add("Control3", 1);
					OneValue.Add("Control4", 1);
					OneValue.Add("Control5", 1);

					Src->NumberOfFrames = 12;
					for (int32 Frame = 0; Frame < Src->NumberOfFrames; ++Frame)
					{
						if ((Frame / 2) % 2 == 0) 
						{
							Src->AnimationDataPerFrame.Add(OneValue); // Frames 0, 1, 4, 5, 8, 9 
						}
						else
						{
							Src->AnimationDataPerFrame.Add(ZeroValue);// Frames 2, 3, 6, 7, 10, 11
						}
					}

					TSharedPtr<FHyprsenseRealtimeSmoothingNode> Smoothing = TestHelper->Pipeline.MakeNode<FHyprsenseRealtimeSmoothingNode>("Smoothing");
					Smoothing->Parameters.Add(FName("Control1"), { EMetaHumanRealtimeSmoothingParamMethod::RollingAverage, 1 });
					Smoothing->Parameters.Add(FName("Control2"), { EMetaHumanRealtimeSmoothingParamMethod::RollingAverage, 2 });
					Smoothing->Parameters.Add(FName("Control3"), { EMetaHumanRealtimeSmoothingParamMethod::RollingAverage, 3 });
					Smoothing->Parameters.Add(FName("Control4"), { EMetaHumanRealtimeSmoothingParamMethod::RollingAverage, 4 });

					TestHelper->Pipeline.MakeConnection(Src, Smoothing);
				}
				else if (Test == "Audio")
				{
					USoundWave* SoundWave = LoadObject<USoundWave>(nullptr, TEXT("/MetaHuman/TestData/Audio/I_Am_MetaHuman.I_Am_MetaHuman"));
					check(SoundWave);

					TSharedPtr<FAudioLoadNode> Load = TestHelper->Pipeline.MakeNode<FAudioLoadNode>("Load");
					Load->Load(SoundWave);
					Load->FrameRate = 30; // Something not divisible by audio sample rate

					TSharedPtr<FAudioConvertNode> Convert = TestHelper->Pipeline.MakeNode<FAudioConvertNode>("Convert");
					Convert->NumChannels = 1;
					Convert->SampleRate = 22050;

					TSharedPtr<FAudioSaveNode> Save = TestHelper->Pipeline.MakeNode<FAudioSaveNode>("Save");
					Save->FilePath = OutputDir + "/Audio/test.wav";

					TestHelper->Pipeline.MakeConnection(Load, Convert);
					TestHelper->Pipeline.MakeConnection(Convert, Save);
				}
				else if (Test == "RealtimeAudio")
				{
					USoundWave* SoundWave = LoadObject<USoundWave>(nullptr, TEXT("/MetaHuman/TestData/Audio/I_Am_MetaHuman.I_Am_MetaHuman"));
					check(SoundWave);

					TSharedPtr<FAudioLoadNode> Load = TestHelper->Pipeline.MakeNode<FAudioLoadNode>("Load");
					Load->Load(SoundWave);
					Load->FrameRate = 30;

					TSharedPtr<FAudioConvertNode> Convert = TestHelper->Pipeline.MakeNode<FAudioConvertNode>("Convert");
					Convert->NumChannels = 1;
					Convert->SampleRate = 16000;

					TSharedPtr<FRealtimeSpeechToAnimNode> Realtime = TestHelper->Pipeline.MakeNode<FRealtimeSpeechToAnimNode>("Realtime");
					Realtime->LoadModels();

					TestHelper->Pipeline.MakeConnection(Load, Convert);
					TestHelper->Pipeline.MakeConnection(Convert, Realtime);
				}
				else if (Test == "HyprsenseCompareColor")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FHyprsenseManagedNode> Track = TestHelper->Pipeline.MakeNode<FHyprsenseManagedNode>("Track");
					Track->bAddSparseTrackerResultsToOutput = false;
					TSharedPtr<FHyprsenseTestNode> Compare = TestHelper->Pipeline.MakeNode<FHyprsenseTestNode>("Compare");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					Compare->InJsonFilePath = TestDataDir + "Tracking/ColorCurves.json";
					Compare->OutJsonFilePath = OutputDir + "/HyprsenseCompare/UnrealDiff_Color.json";

					TestHelper->Pipeline.MakeConnection(Load, Track);
					TestHelper->Pipeline.MakeConnection(Track, Compare);
				}
				else if (Test == "HyprsenseCompareHMC")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FHyprsenseManagedNode> Track = TestHelper->Pipeline.MakeNode<FHyprsenseManagedNode>("Track");
					Track->bAddSparseTrackerResultsToOutput = false;
					TSharedPtr<FHyprsenseTestNode> Compare = TestHelper->Pipeline.MakeNode<FHyprsenseTestNode>("Compare");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Bot/%04d.png");

					Compare->InJsonFilePath = TestDataDir + "/Tracking/HMCBotCurves.json";
					Compare->OutJsonFilePath = OutputDir + "/HyprsenseCompare/UnrealDiff_HMC.json";

					TestHelper->Pipeline.MakeConnection(Load, Track);
					TestHelper->Pipeline.MakeConnection(Track, Compare);
				}
				else if (Test == "Depth")
				{
					TSharedPtr<FDepthLoadNode> Load = TestHelper->Pipeline.MakeNode<FDepthLoadNode>("Load");
					TSharedPtr<FDepthToUEImageNode> Convert = TestHelper->Pipeline.MakeNode<FDepthToUEImageNode>("Convert");
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Depth/%04d.exr");

					Convert->Min = 20;
					Convert->Max = 30;

					Save->FilePath = OutputDir + "/Depth/%04d.png";

					TestHelper->Pipeline.MakeConnection(Load, Convert);
					TestHelper->Pipeline.MakeConnection(Convert, Save);
				}
				else if (Test == "DepthGenerate")
				{
					TSharedPtr<FUEImageLoadNode> Load0 = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load0");
					TSharedPtr<FUEImageLoadNode> Load1 = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load1");
					TSharedPtr<FDepthGenerateNode> GenerateDepth = TestHelper->Pipeline.MakeNode<FDepthGenerateNode>("GenerateDepth");
					GenerateDepth->DistanceRange = TRange<float>{ 10.0f, 25.0f }; // set a default depth range of 10-25cm
					TSharedPtr<FDepthSaveNode> Save = TestHelper->Pipeline.MakeNode<FDepthSaveNode>("Save");

					Load0->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Bot/%04d.png");
					Load1->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Top/%04d.png");

					GenerateDepth->Calibrations.SetNum(2);
					GenerateDepth->Calibrations[0] = TestHelper->BotCamera;
					GenerateDepth->Calibrations[1] = TestHelper->TopCamera;

					Save->FilePath = OutputDir + "/DepthGenerate/%04d.exr";

					TestHelper->Pipeline.MakeConnection(Load0, GenerateDepth, 0, 0);
					TestHelper->Pipeline.MakeConnection(Load1, GenerateDepth, 0, 1);
					TestHelper->Pipeline.MakeConnection(GenerateDepth, Save);
				}
				else if (Test == "FaceTrackerMonoPass1")
				{
					FString InputDir = FPaths::ProjectContentDir() + "/TestInput";

					UMetaHumanConfig *DeviceConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/iphone12.iphone12"));
					check(DeviceConfig);

					UMetaHumanConfig* PredictiveSolverConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/MetaHumanDepthProcessing/Solver/GenericPredictiveSolver.GenericPredictiveSolver"));
					check(PredictiveSolverConfig);

					TSharedPtr<FUEImageLoadNode> Color = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Image");
					TSharedPtr<FUEImageToUEGrayImageNode> Gray = TestHelper->Pipeline.MakeNode<FUEImageToUEGrayImageNode>("Gray");
					TSharedPtr<FHyprsenseManagedNode> Track = TestHelper->Pipeline.MakeNode<FHyprsenseManagedNode>("Track");
					TSharedPtr<FFlowNode> Flow = TestHelper->Pipeline.MakeNode<FFlowNode>("Flow");
					Flow->SolverConfigData = DeviceConfig->GetSolverConfigData();

					TSharedPtr<FDepthLoadNode> Depth = TestHelper->Pipeline.MakeNode<FDepthLoadNode>("Depth");
					TestHelper->NLS = TestHelper->Pipeline.MakeNode<FFaceTrackerIPhoneManagedNode>("NLS");
					TestHelper->NLS->SolverTemplateData = DeviceConfig->GetSolverTemplateData();
					TestHelper->NLS->SolverConfigData = Flow->SolverConfigData;
					TestHelper->NLS->SolverPCAFromDNAData = DeviceConfig->GetSolverPCAFromDNAData();
					TestHelper->NLS->PredictiveSolverGlobalTeethTrainingData = PredictiveSolverConfig->GetPredictiveGlobalTeethTrainingData();
					TestHelper->NLS->PredictiveSolverTrainingData = PredictiveSolverConfig->GetPredictiveTrainingData();	

					TestHelper->NLS->Calibrations.SetNum(2);

					Color->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Bot_Color/%04d.png");
					Depth->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Depth/%04d.exr");
					TestHelper->NLS->DNAFile = PluginDir / "IdentityTemplate/Face_Archetype.ardna";
					FFileHelper::LoadFileToArray(TestHelper->NLS->BrowJSONData, *(PluginDir / "IdentityTemplate/Face_Archetype_Brows.json"));

					TestHelper->NLS->Calibrations[0] = TestHelper->BotCamera;
					TestHelper->NLS->Calibrations[0].CameraId = "color";

					TestHelper->NLS->Calibrations[1] = TestHelper->NLS->Calibrations[0];
					TestHelper->NLS->Calibrations[1].CameraId = "depth";
					TestHelper->NLS->Calibrations[1].CameraType = FCameraCalibration::Depth;
					TestHelper->NLS->Calibrations[1].ImageSize = FVector2D(832.0, 488.0);
					TestHelper->NLS->Calibrations[1].PrincipalPoint = FVector2D(363.17810058593750, 247.99615478515625);
					TestHelper->NLS->Calibrations[1].FocalLength = FVector2D(1495.0434570312500, 1495.0434570312500);

					TestHelper->NLS->Camera = TestHelper->NLS->Calibrations[0].CameraId;

					TestHelper->NLS->NumberOfFrames = 10;
					TestHelper->NLS->bSkipPredictiveSolver = true;

					Flow->Calibrations = TestHelper->NLS->Calibrations;
					Flow->Camera = TestHelper->NLS->Camera;

					TestHelper->Pipeline.MakeConnection(Color, Gray);
					TestHelper->Pipeline.MakeConnection(Gray, Track);
					TestHelper->Pipeline.MakeConnection(Color, Flow);
					TestHelper->Pipeline.MakeConnection(Flow, TestHelper->NLS);
					TestHelper->Pipeline.MakeConnection(Track, TestHelper->NLS);
					TestHelper->Pipeline.MakeConnection(Depth, TestHelper->NLS);
				}
				else if (Test == "FaceTrackerMonoPass2")
				{
					UMetaHumanConfig* DeviceConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/iphone12.iphone12"));
					check(DeviceConfig);

					TSharedPtr<FFaceTrackerPostProcessingManagedNode> PostProcessing = TestHelper->Pipeline.MakeNode<FFaceTrackerPostProcessingManagedNode>("PostProcessing");

					PostProcessing->TemplateData = DeviceConfig->GetSolverTemplateData();
					PostProcessing->ConfigData = DeviceConfig->GetSolverConfigData();
					PostProcessing->DefinitionsData = DeviceConfig->GetSolverDefinitionsData();
					PostProcessing->HierarchicalDefinitionsData = DeviceConfig->GetSolverHierarchicalDefinitionsData();

					PostProcessing->PredictiveWithoutTeethSolver = PredictiveWithoutTeethSolver;
					PostProcessing->TrackingData = TrackingData;
					PostProcessing->FrameData = FrameData;

					PostProcessing->DNAFile = PluginDir / "IdentityTemplate/Face_Archetype.ardna";
					PostProcessing->Calibrations.SetNum(2);

					PostProcessing->Calibrations[0] = TestHelper->BotCamera;
					PostProcessing->Calibrations[0].CameraId = "color";

					PostProcessing->Calibrations[1] = PostProcessing->Calibrations[0];
					PostProcessing->Calibrations[1].CameraId = "depth";
					PostProcessing->Calibrations[1].CameraType = FCameraCalibration::Depth;
					PostProcessing->Calibrations[1].ImageSize = FVector2D(832.0, 488.0);
					PostProcessing->Calibrations[1].PrincipalPoint = FVector2D(363.17810058593750, 247.99615478515625);
					PostProcessing->Calibrations[1].FocalLength = FVector2D(1495.0434570312500, 1495.0434570312500);

					PostProcessing->Camera = PostProcessing->Calibrations[0].CameraId;

					TestHelper->StartFrame = 0;
					TestHelper->EndFrame = 10;
				}
				else if (Test == "FaceTrackerMonoPass3")
				{
					UMetaHumanConfig* DeviceConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/iphone12.iphone12"));
					check(DeviceConfig);

					TSharedPtr<FFaceTrackerPostProcessingFilterManagedNode> PostProcessingFilter = TestHelper->Pipeline.MakeNode<FFaceTrackerPostProcessingFilterManagedNode>("PostProcessingFilter");

					PostProcessingFilter->TemplateData = DeviceConfig->GetSolverTemplateData();
					PostProcessingFilter->ConfigData = DeviceConfig->GetSolverConfigData();
					PostProcessingFilter->DefinitionsData = DeviceConfig->GetSolverDefinitionsData();
					PostProcessingFilter->HierarchicalDefinitionsData = DeviceConfig->GetSolverHierarchicalDefinitionsData();

					PostProcessingFilter->FrameData = FrameData;

					PostProcessingFilter->DNAFile = PluginDir / "IdentityTemplate/Face_Archetype.ardna";

					TestHelper->StartFrame = 0;
					TestHelper->EndFrame = 10;
				}
				else if (Test == "Grayscale")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FUEImageToUEGrayImageNode> Color2Gray = TestHelper->Pipeline.MakeNode<FUEImageToUEGrayImageNode>("Color2Gray");
					TSharedPtr<FUEGrayImageToUEImageNode> Gray2Color = TestHelper->Pipeline.MakeNode<FUEGrayImageToUEImageNode>("Gray2Color");
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					Save->FilePath = OutputDir + "/Grayscale/%04d.png";

					TestHelper->Pipeline.MakeConnection(Load, Color2Gray);
					TestHelper->Pipeline.MakeConnection(Color2Gray, Gray2Color);
					TestHelper->Pipeline.MakeConnection(Gray2Color, Save);
				}
				else if (Test == "Crop")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FUEImageCropNode> Crop = TestHelper->Pipeline.MakeNode<FUEImageCropNode>("Crop");
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Bot/%04d.png");

					Crop->X = 100;
					Crop->Y = 200;
					Crop->Width = 300;
					Crop->Height = 400;

					Save->FilePath = OutputDir + "/Crop/%04d.png";

					TestHelper->Pipeline.MakeConnection(Load, Crop);
					TestHelper->Pipeline.MakeConnection(Crop, Save);
				}
				else if (Test == "Composite")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FUEImageCropNode> Crop = TestHelper->Pipeline.MakeNode<FUEImageCropNode>("Crop");
					TSharedPtr<FUEImageCompositeNode> Composite = TestHelper->Pipeline.MakeNode<FUEImageCompositeNode>("Composite");
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "HMC/Bot/%04d.png");

					Crop->X = 100;
					Crop->Y = 200;
					Crop->Width = 300;
					Crop->Height = 400;

					Save->FilePath = OutputDir + "/Composite/%04d.png";

					TestHelper->Pipeline.MakeConnection(Load, Crop);
					TestHelper->Pipeline.MakeConnection(Load, Composite, 0, 0);
					TestHelper->Pipeline.MakeConnection(Crop, Composite, 0, 1);
					TestHelper->Pipeline.MakeConnection(Composite, Save);
				}
				else if (Test == "Rotate")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FUEImageRotateNode> Rotate0_1 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate0_1");
					TSharedPtr<FUEImageRotateNode> Rotate90_1 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate90_1");
					TSharedPtr<FUEImageRotateNode> Rotate90_2 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate90_2");
					TSharedPtr<FUEImageRotateNode> Rotate90_3 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate90_3");
					TSharedPtr<FUEImageRotateNode> Rotate90_4 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate90_4");
					TSharedPtr<FUEImageRotateNode> Rotate180_1 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate180_1");
					TSharedPtr<FUEImageRotateNode> Rotate180_2 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate180_2");
					TSharedPtr<FUEImageRotateNode> Rotate270_1 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate270_1");
					TSharedPtr<FUEImageRotateNode> Rotate270_2 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate270_2");
					TSharedPtr<FUEImageRotateNode> Rotate270_3 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate270_3");
					TSharedPtr<FUEImageRotateNode> Rotate270_4 = TestHelper->Pipeline.MakeNode<FUEImageRotateNode>("Rotate270_4");
					TSharedPtr<FUEImageSaveNode> Save0 = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save0");
					TSharedPtr<FUEImageSaveNode> Save90 = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save90");
					TSharedPtr<FUEImageSaveNode> Save180 = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save180");
					TSharedPtr<FUEImageSaveNode> Save270 = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save270");

					constexpr int32 FrameNumberOffset = 99; // Only need a single frame
					FFrameNumberTransformer FrameNumberTransformer(FrameNumberOffset);
					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "/Color/%04d.png", MoveTemp(FrameNumberTransformer));

					Rotate0_1->SetAngle(0);

					Rotate90_1->SetAngle(90);
					Rotate90_2->SetAngle(90);
					Rotate90_3->SetAngle(90);
					Rotate90_4->SetAngle(90);

					Rotate180_1->SetAngle(180);
					Rotate180_2->SetAngle(180);

					Rotate270_1->SetAngle(270);
					Rotate270_2->SetAngle(270);
					Rotate270_3->SetAngle(270);
					Rotate270_4->SetAngle(270);

					Save0->FilePath = OutputDir + "/Rotate/0-%04d.png";
					Save90->FilePath = OutputDir + "/Rotate/90-%04d.png";
					Save180->FilePath = OutputDir + "/Rotate/180-%04d.png";
					Save270->FilePath = OutputDir + "/Rotate/270-%04d.png";

					TestHelper->Pipeline.MakeConnection(Load, Rotate0_1);

					TestHelper->Pipeline.MakeConnection(Load, Rotate90_1);
					TestHelper->Pipeline.MakeConnection(Rotate90_1, Rotate90_2);
					TestHelper->Pipeline.MakeConnection(Rotate90_2, Rotate90_3);
					TestHelper->Pipeline.MakeConnection(Rotate90_3, Rotate90_4);

					TestHelper->Pipeline.MakeConnection(Load, Rotate180_1);
					TestHelper->Pipeline.MakeConnection(Rotate180_1, Rotate180_2);

					TestHelper->Pipeline.MakeConnection(Load, Rotate270_1);
					TestHelper->Pipeline.MakeConnection(Rotate270_1, Rotate270_2);
					TestHelper->Pipeline.MakeConnection(Rotate270_2, Rotate270_3);
					TestHelper->Pipeline.MakeConnection(Rotate270_3, Rotate270_4);

					TestHelper->Pipeline.MakeConnection(Rotate0_1, Save0);
					TestHelper->Pipeline.MakeConnection(Rotate90_1, Save90);
					TestHelper->Pipeline.MakeConnection(Rotate180_1, Save180);
					TestHelper->Pipeline.MakeConnection(Rotate270_1, Save270);
				}
				else if (Test == "JsonTracker")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FJsonTrackerNode> Track = TestHelper->Pipeline.MakeNode<FJsonTrackerNode>("Track");
					TSharedPtr<FBurnContoursNode> Burn = TestHelper->Pipeline.MakeNode<FBurnContoursNode>("Burn");
					TSharedPtr<FUEImageSaveNode> Save = TestHelper->Pipeline.MakeNode<FUEImageSaveNode>("Save");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					Track->JsonFile = TestDataDir + "Tracking/ColorCurves.json";

					Save->FilePath = OutputDir + "/Json/%04d.png";

					TestHelper->Pipeline.MakeConnection(Load, Track);
					TestHelper->Pipeline.MakeConnection(Track, Burn);
					TestHelper->Pipeline.MakeConnection(Burn, Save);
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known test"), false);
				}

				if (bIsOK)
				{
					if (Method == "PushSync")
					{
						TestHelper->Run(EPipelineMode::PushSync);
					}
					else if (Method == "PushAsync")
					{
						TestHelper->Run(EPipelineMode::PushAsync);
					}
					else if (Method == "PushSyncNodes")
					{
						TestHelper->Run(EPipelineMode::PushSyncNodes);
					}
					else if (Method == "PushAsyncNodes")
					{
						TestHelper->Run(EPipelineMode::PushAsyncNodes);
					}
					else
					{
						bIsOK &= TestTrue(TEXT("Known method"), false);
					}
				}
			}
		}
		else if (Stage == "Stage2")
		{
			ADD_LATENT_AUTOMATION_COMMAND(FPipelineTestNodesComplete(Test));
		}
		else if (Stage == "Stage3")
		{
			bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				bIsOK &= TestEqual(TEXT("Process complete count"), TestHelper->ProcessCompleteCount, 1);
				bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::Ok);
				bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, -1);

				if (Test == "Hyprsense")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Load.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Load.UE Image Out").Width, 480));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Load.UE Image Out").Height, 640));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameTrackingContourData>("Track.Contours Out"));

						int32 TotalNumContours = 0;

						if (bIsOK)
						{
							TMap<FString, FTrackingContour> Contours = TestHelper->PipelineData[Frame]->GetData<FFrameTrackingContourData>("Track.Contours Out").TrackingContours;
							bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), Contours.Num() == 119));
							for (const TPair<FString, FTrackingContour>& Contour : Contours)
							{
								TotalNumContours += Contour.Value.DensePoints.Num();
							}
						}
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TotalNumContours == 857));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Burn.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Burn.UE Image Out").Width, 480));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Burn.UE Image Out").Height, 640));
					}
				}
				else if (Test == "HyprsenseSparse")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Load.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Load.UE Image Out").Width, 480));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Load.UE Image Out").Height, 640));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameTrackingContourData>("Track.Contours Out"));

						int32 TotalNumContours = 0;

						if (bIsOK)
						{
							TMap<FString, FTrackingContour> Contours = TestHelper->PipelineData[Frame]->GetData<FFrameTrackingContourData>("Track.Contours Out").TrackingContours;
							bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), Contours.Num() == 55)); 
							for (const TPair<FString, FTrackingContour>& Contour : Contours)
							{
								TotalNumContours += Contour.Value.DensePoints.Num();
							}
						}
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TotalNumContours == 209));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Burn.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Burn.UE Image Out").Width, 480));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Burn.UE Image Out").Height, 640));
					}
				}
				else if (Test == "DepthMapDiagnostics")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameTrackingContourData>("Track.Contours Out"));
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<TMap<FString, FDepthMapDiagnosticsResult>>("Diagnostics.DepthMap Diagnostics Out"));

						if (bIsOK)
						{
							TMap<FString, FDepthMapDiagnosticsResult> DiagnosticsResult = TestHelper->PipelineData[Frame]->GetData<TMap<FString, FDepthMapDiagnosticsResult>>("Diagnostics.DepthMap Diagnostics Out");
							bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), DiagnosticsResult.Num() == 1));
							// face must be at least 100 pixels across and at least 80% of depth map values within face convex hull must be valid
							bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), DiagnosticsResult["depth"].FaceWidthInPixels > 100.0f));
							float FractionFaceGoodDepth = static_cast<float>(DiagnosticsResult["depth"].NumFaceValidDepthMapPixels) / DiagnosticsResult["depth"].NumFacePixels;
							bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), FractionFaceGoodDepth > 0.8f));
						}

					}
				}
				else if (Test == "RealtimeMono")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("Realtime.Animation Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("Realtime.Animation Out").AnimationData.Num(), 251));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Realtime.Debug UE Image Out"));
						if (bIsOK)
						{
							int32 Width = TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Realtime.Debug UE Image Out").Width;
							int32 Height = TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Realtime.Debug UE Image Out").Height;
							int32 Num = TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Realtime.Debug UE Image Out").Data.Num();

							if (Params[0] == "None")
							{
								bIsOK &= TestEqual(TEXT("Expected value"), Width, -1);
								bIsOK &= TestEqual(TEXT("Expected value"), Height, -1);
								bIsOK &= TestEqual(TEXT("Expected value"), Num, 0);
							}
							else
							{
								bIsOK &= TestGreaterThan(TEXT("Expected value"), Width, 0);
								bIsOK &= TestGreaterThan(TEXT("Expected value"), Height, 0);
								bIsOK &= TestEqual(TEXT("Expected value"), Num, Width * Height * 4);
							}
						}
					}
				}
				else if (Test == "RealtimeMonoSmoothing")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 12);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("Smoothing.Animation Out"));

						if (bIsOK)
						{
							FFrameAnimationData AnimData = TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("Smoothing.Animation Out");
							bIsOK &= TestEqual(TEXT("Expected number of curves"), AnimData.AnimationData.Num(), 5);
							bIsOK &= TestTrue(TEXT("Expected Control1 curve"), AnimData.AnimationData.Contains("Control1"));
							bIsOK &= TestTrue(TEXT("Expected Control2 curve"), AnimData.AnimationData.Contains("Control2"));
							bIsOK &= TestTrue(TEXT("Expected Control3 curve"), AnimData.AnimationData.Contains("Control3"));
							bIsOK &= TestTrue(TEXT("Expected Control4 curve"), AnimData.AnimationData.Contains("Control4"));
							bIsOK &= TestTrue(TEXT("Expected Control5 curve"), AnimData.AnimationData.Contains("Control5"));

							if (bIsOK)
							{
								// Expected values
								// H = half, O = one-third, T = two-thirds
								// Frame:    0, 1, 2, 3, 4, 5, 6, 7, 8, 9
								// Input:    1, 1, 0, 0, 1, 1, 0, 0, 1, 1
								// V1:       1, 1, 0, 0, 1, 1, 0, 0, 1, 1
								// V2:       1, 1, H, 0, H, 1, H, 0, H, 1
								// V3:       1, 1, T, O, O, T, T, O, O, T
								// V4:       1, 1, T, H, H, H, H, H, H, H
								// V5:       1, 1, 0, 0, 1, 1, 0, 0, 1, 1

								float V1 = AnimData.AnimationData["Control1"];
								float V2 = AnimData.AnimationData["Control2"];
								float V3 = AnimData.AnimationData["Control3"];
								float V4 = AnimData.AnimationData["Control4"];
								float V5 = AnimData.AnimationData["Control5"];

								if (Frame < 2)
								{
									bIsOK &= TestEqual("Expected Control1 value", V1, 1.0f);
									bIsOK &= TestEqual("Expected Control2 value", V2, 1.0f);
									bIsOK &= TestEqual("Expected Control3 value", V3, 1.0f);
									bIsOK &= TestEqual("Expected Control4 value", V4, 1.0f);
									bIsOK &= TestEqual("Expected Control5 value", V5, 1.0f);
								}
								else
								{
									bIsOK &= TestEqual("Expected Control1 value", V1, ((Frame / 2) % 2) == 0 ? 1.0f : 0.0f);

									if ((Frame % 2) == 0)
									{
										bIsOK &= TestEqual("Expected Control2 value", V2, 0.5f);
									}
									else
									{
										bIsOK &= TestEqual("Expected Control2 value", V2, V1);
									}

									if (Frame == 3 || Frame == 4 || Frame == 7 || Frame == 8 || Frame == 11)
									{
										bIsOK &= TestEqual("Expected Control3 value", V3, 1.0f / 3.0f);
									}
									else
									{
										bIsOK &= TestEqual("Expected Control3 value", V3, 2.0f / 3.0f);
									}

									if (Frame == 2)
									{
										bIsOK &= TestEqual("Expected Control4 value", V4, 2.0f / 3.0f);
									}
									else
									{
										bIsOK &= TestEqual("Expected Control4 value", V4, 0.5f);
									}

									bIsOK &= TestEqual("Expected Control5 value", V5, V1);
								}
							}
						}
					}
				}
				else if (Test == "Audio")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 197);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FAudioDataType>("Load.Audio Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FAudioDataType>("Load.Audio Out").NumChannels, 2));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FAudioDataType>("Load.Audio Out").SampleRate, 16000));
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FAudioDataType>("Convert.Audio Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FAudioDataType>("Convert.Audio Out").NumChannels, 1));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FAudioDataType>("Convert.Audio Out").SampleRate, 22050));

						if (bIsOK)
						{
							int32 NumSamplesLoad = TestHelper->PipelineData[Frame]->GetData<FAudioDataType>("Load.Audio Out").NumSamples;
							int32 ExpectedNumSamplesLoad = -1;

							// Because the 16k sample rate is not divisible by the 30fps playback rate, we dont get an equal number of samples every frame.
							// Its 16000/30 = 533.33 so 2 frames of 533 samples followed by one of 534. Last frame is not full since audio does not end on a frame boundary.

							if (Frame == TestHelper->FrameCompleteCount - 1)
							{
								ExpectedNumSamplesLoad = 218;
							}
							else if (Frame % 3 == 2)
							{
								ExpectedNumSamplesLoad = 534;
							}
							else
							{
								ExpectedNumSamplesLoad = 533;
							}

							bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), NumSamplesLoad, ExpectedNumSamplesLoad));

							int32 NumSamplesConvert = TestHelper->PipelineData[Frame]->GetData<FAudioDataType>("Convert.Audio Out").NumSamples;
							bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), NumSamplesConvert, ExpectedNumSamplesLoad * 22050/16000.0, 1));
						}
					}
				}
				else if (Test == "RealtimeAudio")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 197);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("Realtime.Animation Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("Realtime.Animation Out").AnimationData.Num(), Frame >= 30 ? 251 : 0)); // No anim will be produced in first 1.0s (30 frames)
					}
				}
				else if (Test == "HyprsenseCompareColor")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<float>("Compare.Avg Diff Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<float>("Compare.Avg Diff Out"), 0.5f, 0.5f));
					}
				}
				else if (Test == "HyprsenseCompareHMC")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<float>("Compare.Avg Diff Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<float>("Compare.Avg Diff Out"), 0.5f, 1.0f));
					}
				}
				else if (Test == "Depth")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FDepthDataType>("Load.Depth Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FDepthDataType>("Load.Depth Out").Width, 832));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FDepthDataType>("Load.Depth Out").Height, 488));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FDepthDataType>("Load.Depth Out").Data.Num(), 832 * 488));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Convert.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Convert.UE Image Out").Width, 832));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Convert.UE Image Out").Height, 488));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Convert.UE Image Out").Data.Num(), 832 * 488 * 4));
					}
				}
				else if (Test == "DepthGenerate")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);
				}
				else if (Test == "FaceTrackerMonoPass1")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					TrackingData.Reset();
					FrameData.Reset();
					PredictiveWithoutTeethSolver.Reset();

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameTrackingContourData>("Track.Contours Out"));
						if (bIsOK)
						{
							TrackingData.Add(TestHelper->PipelineData[Frame]->GetData<FFrameTrackingContourData>("Track.Contours Out"));
						}

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("NLS.Animation Out"));
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").Pose.IsValid()));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").RawPoseData.Num(), 16));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").AnimationData.Num(), 251));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").RawAnimationData.Num(), 251));
						bIsOK &= (bIsOK && TestFalse(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").MeshData.FaceMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestFalse(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").MeshData.TeethMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestFalse(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").MeshData.LeftEyeMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestFalse(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").MeshData.RightEyeMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out").AnimationQuality, EFrameAnimationQuality::Preview));
						if (bIsOK)
						{
							FrameData.Add(TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("NLS.Animation Out"));
						}
					}

					PredictiveWithoutTeethSolver = TestHelper->NLS->PredictiveWithoutTeethSolver;
					TestHelper->NLS = nullptr;
				}
				else if (Test == "FaceTrackerMonoPass2")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("PostProcessing.Animation Out"));
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").Pose.IsValid()));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").RawPoseData.Num(), 16));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").AnimationData.Num(), 251));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").RawAnimationData.Num(), 251));
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").MeshData.FaceMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").MeshData.TeethMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").MeshData.LeftEyeMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestTrue(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").MeshData.RightEyeMeshVertData.IsEmpty()));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessing.Animation Out").AnimationQuality, EFrameAnimationQuality::Final));
					}
				}
				else if (Test == "FaceTrackerMonoPass3")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("PostProcessingFilter.Animation Out"));
						// no checks on pose and mesh data as the content of this is not touched by the filtering
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessingFilter.Animation Out").AnimationData.Num(), 251));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessingFilter.Animation Out").RawAnimationData.Num(), 251));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("PostProcessingFilter.Animation Out").AnimationQuality, EFrameAnimationQuality::PostFiltered));
					}
				}
				else if (Test == "Grayscale")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEGrayImageDataType>("Color2Gray.UE Gray Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEGrayImageDataType>("Color2Gray.UE Gray Image Out").Width, 480));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEGrayImageDataType>("Color2Gray.UE Gray Image Out").Height, 640));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEGrayImageDataType>("Color2Gray.UE Gray Image Out").Data.Num(), 480 * 640));

						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Gray2Color.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Gray2Color.UE Image Out").Width, 480));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Gray2Color.UE Image Out").Height, 640));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Gray2Color.UE Image Out").Data.Num(), 480 * 640 * 4));
					}
				}
				else if (Test == "Crop")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Crop.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Crop.UE Image Out").Width, 300));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Crop.UE Image Out").Height, 400));
					}
				}
				else if (Test == "Composite")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 10);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FUEImageDataType>("Composite.UE Image Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Composite.UE Image Out").Width, 480 + 300));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FUEImageDataType>("Composite.UE Image Out").Height, 640));
					}
				}
				else if (Test == "Rotate")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 1);

					if (bIsOK)
					{
						TSharedPtr<const FPipelineData> Data = TestHelper->PipelineData[0];

						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Load.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate0_1.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate90_1.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate90_2.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate90_3.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate90_4.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate180_1.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate180_2.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate270_1.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate270_2.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate270_3.UE Image Out"));
						bIsOK &= TestTrue(TEXT("Data present"), Data->HasData<FUEImageDataType>("Rotate270_4.UE Image Out"));

						// Check four lots of 90 degree rotate gets you back to original image; 2 lots of 180 does the same;
						// a 270 degree rotate equals 3 lots of 90 etc
						bIsOK &= (bIsOK && TestTrue(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate0_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_4.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate180_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_4.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_3.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestTrue(TEXT("270 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_3.UE Image Out"))));

						// Check other images are not the same as would be the case if the rotate was a no-op
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_3.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("0 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Load.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_3.UE Image Out"))));

						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_3.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_4.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate180_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("90 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate90_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_4.UE Image Out"))));

						bIsOK &= (bIsOK && TestFalse(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_3.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate90_4.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate180_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_3.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("180 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate180_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_4.UE Image Out"))));

						bIsOK &= (bIsOK && TestFalse(TEXT("270 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_2.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("270 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_3.UE Image Out"))));
						bIsOK &= (bIsOK && TestFalse(TEXT("270 rot"), UEImagesAreEqual(Data->GetData<FUEImageDataType>("Rotate270_1.UE Image Out"), Data->GetData<FUEImageDataType>("Rotate270_4.UE Image Out"))));
					}
				}
				else if (Test == "JsonTracker")
				{
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameTrackingContourData>("Track.Contours Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameTrackingContourData>("Track.Contours Out").TrackingContours.Num(), 119));
					}
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known test"), false);
				}
			}

			TestHelper = nullptr;
		}
		else
		{
			bIsOK &= TestTrue(TEXT("Known stage"), false);
		}
	}

	return bIsOK;
}

bool FPipelineTestBenchmarks::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	TArray<FString> Tokens;
	InTestCommand.ParseIntoArray(Tokens, TEXT(" "), true);
	bIsOK &= TestEqual<int32>(TEXT("Well formed Parameters"), Tokens.Num(), 3);

	if (bIsOK)
	{
		TArray<FString> Params;
		Tokens[0].ParseIntoArray(Params, TEXT("-"), true);
		FString Test = Params[0];
		Params.RemoveAt(0);

		FString Method = Tokens[1];
		FString Stage = Tokens[2];

		if (Stage == "Stage1")
		{
			bIsOK &= TestInvalid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				TestHelper = MakeShared<FPipelineTestHelper>();
				FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
				FString TestDataDir = PluginDir + "/TestData/";

				FString OutputDir = FPaths::ProjectIntermediateDir() + "/TestOutput";

				if (Test == "RealtimeMono")
				{
					TSharedPtr<FUEImageLoadNode> Load = TestHelper->Pipeline.MakeNode<FUEImageLoadNode>("Load");
					TSharedPtr<FNeutralFrameNode> NeutralFrame = TestHelper->Pipeline.MakeNode<FNeutralFrameNode>("Neutral Frame");
					TSharedPtr<FHyprsenseRealtimeNode> Realtime = TestHelper->Pipeline.MakeNode<FHyprsenseRealtimeNode>("Realtime");

					Load->FramePathResolver = MakeUnique<FFramePathResolver>(TestDataDir + "Color/%04d.png");

					Realtime->SetDebugImage(EHyprsenseRealtimeNodeDebugImage::None);
					Realtime->LoadModels();

					TestHelper->Pipeline.MakeConnection(Load, NeutralFrame);
					TestHelper->Pipeline.MakeConnection(NeutralFrame, Realtime);
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known test"), false);
				}

				if (bIsOK)
				{
					if (Method == "PushSync")
					{
						TestHelper->Run(EPipelineMode::PushSync);
					}
					else if (Method == "PushAsync")
					{
						TestHelper->Run(EPipelineMode::PushAsync);
					}
					else if (Method == "PushSyncNodes")
					{
						TestHelper->Run(EPipelineMode::PushSyncNodes);
					}
					else if (Method == "PushAsyncNodes")
					{
						TestHelper->Run(EPipelineMode::PushAsyncNodes);
					}
					else
					{
						bIsOK &= TestTrue(TEXT("Known method"), false);
					}
				}
			}
		}
		else if (Stage == "Stage2")
		{
			ADD_LATENT_AUTOMATION_COMMAND(FPipelineTestNodesComplete(Test));
		}
		else if (Stage == "Stage3")
		{
			bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);

			if (bIsOK)
			{
				bIsOK &= TestEqual(TEXT("Process complete count"), TestHelper->ProcessCompleteCount, 1);
				bIsOK &= TestEqual(TEXT("Exit status"), TestHelper->ExitStatus, EPipelineExitStatus::Ok);
				bIsOK &= TestEqual(TEXT("Error node code"), TestHelper->ErrorNodeCode, -1);

				if (Test == "RealtimeMono")
				{
					bIsOK &= TestValid(TEXT("Test helper set"), TestHelper);
					bIsOK &= TestEqual(TEXT("Frame completed count"), TestHelper->FrameCompleteCount, 100);

					TArray<double> Times;
					double TotalTime = 0;

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						bIsOK &= TestTrue(TEXT("Data present"), TestHelper->PipelineData[Frame]->HasData<FFrameAnimationData>("Realtime.Animation Out"));
						bIsOK &= (bIsOK && TestEqual(TEXT("Expected value"), TestHelper->PipelineData[Frame]->GetData<FFrameAnimationData>("Realtime.Animation Out").AnimationData.Num(), 251));

						double Time = (TestHelper->PipelineData[Frame]->GetMarkerEndTime("Realtime") - TestHelper->PipelineData[Frame]->GetMarkerStartTime("Realtime")) * 1000; // in ms
						TotalTime += Time;
						Times.Add(Time);
					}

					double AvgTime = TotalTime / TestHelper->FrameCompleteCount;
					double StdDev = 0;

					for (int32 Frame = 0; Frame < TestHelper->FrameCompleteCount; ++Frame)
					{
						StdDev += ((Times[Frame] - AvgTime) * (Times[Frame] - AvgTime));
					}

					StdDev /= (TestHelper->FrameCompleteCount - 1);
					StdDev = sqrt(StdDev);

					if (bIsOK)
					{
						UE_LOG(LogMHABenchmark, Display, TEXT("%s: Average = %.2fms, SD = %.2fms"), *Test, AvgTime, StdDev);
					}
					else
					{
						UE_LOG(LogMHABenchmark, Warning, TEXT("Failed"));
					}
				}
				else
				{
					bIsOK &= TestTrue(TEXT("Known test"), false);
				}
			}

			TestHelper = nullptr;
		}
		else
		{
			bIsOK &= TestTrue(TEXT("Known stage"), false);
		}
	}

	return bIsOK;
}

#endif

}
