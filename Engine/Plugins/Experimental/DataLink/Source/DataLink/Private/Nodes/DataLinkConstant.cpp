// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/DataLinkConstant.h"
#include "DataLinkExecutor.h"
#include "DataLinkNames.h"
#include "DataLinkNodeInstance.h"
#include "DataLinkNodeMetadata.h"
#include "DataLinkPinBuilder.h"

void UDataLinkConstant::SetStruct(const UScriptStruct* InStructType)
{
	Instance.InitializeAs(InStructType);
}

#if WITH_EDITOR
void UDataLinkConstant::OnBuildMetadata(FDataLinkNodeMetadata& Metadata) const
{
	Super::OnBuildMetadata(Metadata);

	if (const UScriptStruct* Struct = Instance.GetScriptStruct())
	{
		Metadata
			.SetDisplayName(Struct->GetDisplayNameText())
			.SetTooltipText(Struct->GetToolTipText());
	}

	if (!DisplayName.IsEmpty())
	{
		Metadata.SetDisplayName(DisplayName);
	}
}
#endif

void UDataLinkConstant::OnBuildPins(FDataLinkPinBuilder& Inputs, FDataLinkPinBuilder& Outputs) const
{
	Super::OnBuildPins(Inputs, Outputs);

	if (const UScriptStruct* Struct = Instance.GetScriptStruct())
	{
		Outputs.Add(UE::DataLink::OutputDefault)
			.SetStruct(Struct);
	}
}

EDataLinkExecutionReply UDataLinkConstant::OnExecute(FDataLinkExecutor& InExecutor) const
{
	InExecutor.SucceedNode(this, FConstStructView(Instance.GetScriptStruct(), Instance.GetMemory()));
	return EDataLinkExecutionReply::Handled;
}
