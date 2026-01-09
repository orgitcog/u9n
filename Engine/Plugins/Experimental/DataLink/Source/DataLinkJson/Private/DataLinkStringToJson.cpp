// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkStringToJson.h"
#include "DataLinkCoreTypes.h"
#include "DataLinkExecutor.h"
#include "DataLinkJsonNames.h"
#include "DataLinkNames.h"
#include "DataLinkNodeInstance.h"
#include "DataLinkPinBuilder.h"
#include "JsonObjectWrapper.h"
#include "StructUtils/StructView.h"

#define LOCTEXT_NAMESPACE "DataLinkStringToJson"

void UDataLinkStringToJson::OnBuildPins(FDataLinkPinBuilder& Inputs, FDataLinkPinBuilder& Outputs) const
{
	Super::OnBuildPins(Inputs, Outputs);

	Inputs.Add(UE::DataLinkJson::InputString)
		.SetDisplayName(LOCTEXT("InputString", "String"))
		.SetStruct<FDataLinkString>();

	Outputs.Add(UE::DataLink::OutputDefault)
		.SetDisplayName(LOCTEXT("OutputDisplay", "Json"))
		.SetStruct<FJsonObjectWrapper>();
}

EDataLinkExecutionReply UDataLinkStringToJson::OnExecute(FDataLinkExecutor& InExecutor) const
{
	const FDataLinkNodeInstance& NodeInstance = InExecutor.GetNodeInstance(this);

	const FDataLinkInputDataViewer& InputDataViewer = NodeInstance.GetInputDataViewer();
	const FDataLinkOutputDataViewer& OutputDataViewer = NodeInstance.GetOutputDataViewer();

	const FDataLinkString& InputData = InputDataViewer.Get<FDataLinkString>(UE::DataLinkJson::InputString);

	FJsonObjectWrapper& OutputData = OutputDataViewer.Get<FJsonObjectWrapper>(UE::DataLink::OutputDefault);

	// Return early if the output data's json string already matches the input string
	if (OutputData.JsonString == InputData.Value)
	{
		InExecutor.SucceedNode(this, FConstStructView::Make(OutputData));
		return EDataLinkExecutionReply::Handled;
	}

	if (!OutputData.JsonObjectFromString(InputData.Value))
	{
		return EDataLinkExecutionReply::Unhandled;
	}

	// Save the Json String for future reference if the parsed object can be re-used for matching strings
	OutputData.JsonString = InputData.Value;
	InExecutor.SucceedNode(this, FConstStructView::Make(OutputData));
	return EDataLinkExecutionReply::Handled;
}

#undef LOCTEXT_NAMESPACE
