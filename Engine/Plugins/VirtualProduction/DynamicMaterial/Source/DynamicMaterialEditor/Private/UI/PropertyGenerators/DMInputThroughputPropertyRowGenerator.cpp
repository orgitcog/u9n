// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PropertyGenerators/DMInputThroughputPropertyRowGenerator.h"

#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "IDynamicMaterialEditorModule.h"

const TSharedRef<FDMInputThroughputPropertyRowGenerator>& FDMInputThroughputPropertyRowGenerator::Get()
{
	static TSharedRef<FDMInputThroughputPropertyRowGenerator> Generator = MakeShared<FDMInputThroughputPropertyRowGenerator>();
	return Generator;
}

void FDMInputThroughputPropertyRowGenerator::AddComponentProperties(FDMComponentPropertyRowGeneratorParams& InParams)
{
	if (!IsValid(InParams.Object))
	{
		return;
	}

	if (InParams.ProcessedObjects.Contains(InParams.Object))
	{
		return;
	}

	UDMMaterialStageInputThroughput* InputThroughput = Cast<UDMMaterialStageInputThroughput>(InParams.Object);

	if (!InputThroughput)
	{
		return;
	}

	InParams.ProcessedObjects.Add(InputThroughput);

	if (UDMMaterialSubStage* SubStage = InputThroughput->GetSubStage())
	{
		InParams.ProcessedObjects.Add(SubStage);
	}

	UDMMaterialStageThroughput* Throughput = InputThroughput->GetMaterialStageThroughput();

	if (!Throughput)
	{
		return;
	}

	FDMComponentPropertyRowGeneratorParams ThroughputParams = InParams;
	ThroughputParams.Object = Throughput;

	FDMThroughputPropertyRowGenerator::AddComponentProperties(ThroughputParams);
}
