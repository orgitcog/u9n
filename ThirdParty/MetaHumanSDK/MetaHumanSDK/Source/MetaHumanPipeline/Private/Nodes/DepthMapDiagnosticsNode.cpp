// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/DepthMapDiagnosticsNode.h"
#include "Pipeline/PipelineData.h"
#include "DepthMapDiagnosticsResult.h"
#include "Pipeline/Log.h"

#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"

namespace UE::MetaHuman::Pipeline
{
FDepthMapDiagnosticsNode::FDepthMapDiagnosticsNode(const FString& InName) : FNode("DepthMapDiagnostics", InName)
{
	Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
	Pins.Add(FPin("Contours In", EPinDirection::Input, EPinType::Contours));
	Pins.Add(FPin("Depth In", EPinDirection::Input, EPinType::Depth));
	Pins.Add(FPin("DepthMap Diagnostics Out", EPinDirection::Output, EPinType::DepthMapDiagnostics));
}

bool FDepthMapDiagnosticsNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		IFaceTrackerNodeImplFactory& DepthMapDiagnosticsImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
		Diagnostics = DepthMapDiagnosticsImplFactory.CreateDepthMapImplementor();
	}

	if (!Diagnostics.IsValid())
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Depth Processing plugin is not enabled");
		return false;
	}

	if (Calibrations.Num() != 2 && Calibrations.Num() != 3)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Must have 2 or 3 cameras");
		return false;
	}

	if (!Diagnostics->Init(Calibrations))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to initialize depthmap diagnostics");
		return false;
	}

	return true;
}

bool FDepthMapDiagnosticsNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FCameraCalibration* DepthCalibrationPtr = Calibrations.FindByPredicate([](const FCameraCalibration& InCalibration)
	{
		return InCalibration.CameraType == FCameraCalibration::Depth;
	});

	if (!DepthCalibrationPtr)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToFindCalibration);
		InPipelineData->SetErrorNodeMessage("Failed to find the calibration for the depth camera");
		return false;
	}

	const FCameraCalibration& DepthCalibration = *DepthCalibrationPtr;

	const FUEImageDataType& Image = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
	const FFrameTrackingContourData& Contours = InPipelineData->GetData<FFrameTrackingContourData>(Pins[1]);
	const FDepthDataType& Depth = InPipelineData->GetData<FDepthDataType>(Pins[2]);

	TMap<FString, const unsigned char*> ImageDataMap;
	TMap<FString, const FFrameTrackingContourData*> LandmarkMap;
	TMap<FString, const float*> DepthDataMap;

	ImageDataMap.Add(Camera, Image.Data.GetData());
	LandmarkMap.Add(Camera, &Contours);	LandmarkMap.Add(Camera, &Contours);

	DepthDataMap.Add(DepthCalibration.CameraId, Depth.Data.GetData());

	TMap<FString, FDepthMapDiagnosticsResult> OutputDiagnostics;

	if (!Diagnostics->CalcDiagnostics(ImageDataMap, LandmarkMap, DepthDataMap, OutputDiagnostics))
	{
		// we don't want the pipeline to fail completely due to a failure in calculating diagnostics, so in this case just create
		// a default diagnostics result which indicates a failure for that frame and log this as a warning
		int32 FrameNumber = InPipelineData->GetFrameNumber();
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to calculate depthmap diagnostics for frame %d"), FrameNumber);
		OutputDiagnostics.Add(DepthCalibration.CameraId, {});
	}

	InPipelineData->SetData<TMap<FString, FDepthMapDiagnosticsResult>>(Pins[3], MoveTemp(OutputDiagnostics));

	return true;
}

bool FDepthMapDiagnosticsNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Diagnostics = nullptr;

	return true;
}

}