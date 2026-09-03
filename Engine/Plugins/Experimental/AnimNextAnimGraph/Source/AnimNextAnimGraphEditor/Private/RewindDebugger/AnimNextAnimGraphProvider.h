// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Model/PointTimeline.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Model/IntervalTimeline.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices { class IAnalysisSession; }

struct FEvaluationProgramData
{
	FEvaluationProgramData(TraceServices::IAnalysisSession& Session)
		: GraphInstanceId(0),
		EvaluationProgramTimeline(Session.GetLinearAllocator( ))
			
	{
	}

	uint64 GraphInstanceId;
	TraceServices::TPointTimeline<TArray<uint8>> EvaluationProgramTimeline;
};

class FAnimNextAnimGraphProvider : public TraceServices::IProvider
{
public:
	static FName ProviderName;

	FAnimNextAnimGraphProvider(TraceServices::IAnalysisSession& InSession);

	const FEvaluationProgramData* GetEvaluationProgramData(uint64 GraphInstanceId) const;
	void AppendEvaluationProgram(double ProfileTime, double RecordingTime, uint64 ObjectInstanceId, uint64 GraphInstanceId, const TArrayView<const uint8>& ProgramData);
	
	void EnumerateEvaluationGraphs(uint64 OuterObjectId, TFunctionRef<void(uint64 GraphInstanceId)> Callback) const;
	
private:
	TraceServices::IAnalysisSession& Session;

	TMap<uint64, TSharedRef<FEvaluationProgramData>> EvaluationProgramData;

	// map from object id to a list of graph instance ids that have evaluation data
	TMultiMap<uint64, uint64> EvaluationGraphs;
};
