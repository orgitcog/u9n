// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Pipeline.h"
#include "Pipeline/DataTree.h"

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FPipelineData : public FDataTree
{
public:

	FPipelineData();

	void SetFrameNumber(int32 InFrameNumber);
	int32 GetFrameNumber() const;

	void SetExitStatus(EPipelineExitStatus InExitStatus);
	EPipelineExitStatus GetExitStatus() const;

	void SetErrorMessage(const FString& InErrorMessage);
	const FString& GetErrorMessage() const;

	void SetErrorNodeName(const FString& InErrorNodeName);
	const FString& GetErrorNodeName() const;

	void SetErrorNodeCode(int32 InErrorNodeCode);
	int32 GetErrorNodeCode() const;

	void SetErrorNodeMessage(const FString& InErrorNodeMessage);
	const FString& GetErrorNodeMessage() const;

	void SetEndFrameMarker(bool bInEndFrameMarker);
	bool GetEndFrameMarker() const;

	void SetDropFrame(bool bInDropFrame);
	bool GetDropFrame() const;

	void SetMarkerStartTime(const FString& InMarkerName);
	double GetMarkerStartTime(const FString& InMarkerName) const;

	void SetMarkerEndTime(const FString& InMarkerName);
	double GetMarkerEndTime(const FString& InMarkerName) const;

	void SetUseGPU(const FString& InUseGPU);
	FString GetUseGPU() const;

private:

	void SetMarkerTime(const FString& InMarkerName);
	double GetMarkerTime(const FString& InMarkerName) const;
};

}
