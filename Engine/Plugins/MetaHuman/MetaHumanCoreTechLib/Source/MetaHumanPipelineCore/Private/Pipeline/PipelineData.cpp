// Copyright Epic Games, Inc.All Rights Reserved.

#include "Pipeline/PipelineData.h"

namespace UE::MetaHuman::Pipeline
{

static FString FrameNumberID = "Reserved.FrameNumber";
static FString ExitStatusID = "Reserved.ExitStatus";
static FString ErrorMessageID = "Reserved.ErrorMessage";
static FString ErrorNodeNameID = "Reserved.ErrorNodeName";
static FString ErrorNodeCodeID = "Reserved.ErrorNodeCode";
static FString ErrorNodeMessageID = "Reserved.ErrorNodeMessage";
static FString EndFrameMarkerID = "Reserved.EndFrameMarker";
static FString DropFrameID = "Reserved.DropFrame";
static FString MarkerTimeID = "Reserved.MarkerTime";
static FString UseGPUID = "Reserved.UseGPU";

FPipelineData::FPipelineData()
{
	SetFrameNumber(-1);
	SetExitStatus(EPipelineExitStatus::Unknown);
	SetErrorMessage("");
	SetErrorNodeName("");
	SetErrorNodeCode(-1);
	SetErrorNodeMessage("");
	SetEndFrameMarker(false);
	SetDropFrame(false);
	SetUseGPU("");
}

void FPipelineData::SetFrameNumber(int32 InFrameNumber)
{
	SetData<int32>(FrameNumberID, InFrameNumber);
}

int32 FPipelineData::GetFrameNumber() const
{
	return GetData<int32>(FrameNumberID);
}

void FPipelineData::SetExitStatus(EPipelineExitStatus InExitStatus)
{
	SetData<EPipelineExitStatus>(ExitStatusID, InExitStatus);
}

EPipelineExitStatus FPipelineData::GetExitStatus() const
{
	return GetData<EPipelineExitStatus>(ExitStatusID);
}

void FPipelineData::SetErrorMessage(const FString& InErrorMessage)
{
	SetData<FString>(ErrorMessageID, InErrorMessage);
}

const FString& FPipelineData::GetErrorMessage() const
{
	return GetData<FString>(ErrorMessageID);
}

void FPipelineData::SetErrorNodeName(const FString& InErrorNodeName)
{
	SetData<FString>(ErrorNodeNameID, InErrorNodeName);
}

const FString& FPipelineData::GetErrorNodeName() const
{
	return GetData<FString>(ErrorNodeNameID);
}

void FPipelineData::SetErrorNodeCode(int32 InErrorNodeCode)
{
	SetData<int32>(ErrorNodeCodeID, InErrorNodeCode);
}

int32 FPipelineData::GetErrorNodeCode() const
{
	return GetData<int32>(ErrorNodeCodeID);
}

void FPipelineData::SetErrorNodeMessage(const FString& InErrorNodeMessage)
{
	SetData<FString>(ErrorNodeMessageID, InErrorNodeMessage);
}

const FString& FPipelineData::GetErrorNodeMessage() const
{
	return GetData<FString>(ErrorNodeMessageID);
}

void FPipelineData::SetEndFrameMarker(bool bInEndFrameMarker)
{
	SetData<bool>(EndFrameMarkerID, bInEndFrameMarker);
}

bool FPipelineData::GetEndFrameMarker() const
{
	return GetData<bool>(EndFrameMarkerID);
}

void FPipelineData::SetDropFrame(bool bInDropFrame)
{
	SetData<bool>(DropFrameID, bInDropFrame);
}

bool FPipelineData::GetDropFrame() const
{
	return GetData<bool>(DropFrameID);
}

void FPipelineData::SetMarkerTime(const FString& InMarkerName)
{
	SetData<double>(MarkerTimeID + "." + InMarkerName, FDateTime::Now().GetTimeOfDay().GetTotalSeconds());
}

double FPipelineData::GetMarkerTime(const FString& InMarkerName) const
{
	return GetData<double>(MarkerTimeID + "." + InMarkerName);
}

void FPipelineData::SetMarkerStartTime(const FString& InMarkerName)
{
	SetMarkerTime(InMarkerName + "-Start");
}

double FPipelineData::GetMarkerStartTime(const FString& InMarkerName) const
{
	return GetMarkerTime(InMarkerName + "-Start");
}

void FPipelineData::SetMarkerEndTime(const FString& InMarkerName)
{
	SetMarkerTime(InMarkerName + "-End");
}

double FPipelineData::GetMarkerEndTime(const FString& InMarkerName) const
{
	return GetMarkerTime(InMarkerName + "-End");
}

void FPipelineData::SetUseGPU(const FString& InUseGPU)
{
	SetData<FString>(UseGPUID, InUseGPU);
}

FString FPipelineData::GetUseGPU() const
{
	return GetData<FString>(UseGPUID);
}

}
