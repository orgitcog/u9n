// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "CameraCalibration.h"
#include "UObject/ObjectPtr.h"


namespace UE
{
	namespace Wrappers
	{
		class FMetaHumanDepthMapDiagnostics;
	}
}

class IDepthMapDiagnosticsInterface;

namespace UE::MetaHuman::Pipeline
{

	class FUEImageDataType;

	class METAHUMANPIPELINE_API FDepthMapDiagnosticsNode : public FNode
	{
	public:

		TArray<FCameraCalibration> Calibrations;
		FString Camera;

		enum ErrorCode
		{
			FailedToInitialize = 0,
			FailedToFindCalibration
		};

		FDepthMapDiagnosticsNode(const FString& InName);

		virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
		virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
		virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	private:

		TSharedPtr<IDepthMapDiagnosticsInterface> Diagnostics = nullptr;
	};


}
