// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkNode.h"
#include "DataLinkEnums.h"
#include "DataLinkExecutor.h"
#include "DataLinkLog.h"
#include "DataLinkNodeMetadata.h"
#include "DataLinkPinBuilder.h"

void UDataLinkNode::Execute(FDataLinkExecutor& InExecutor) const
{
	const EDataLinkExecutionReply Reply = OnExecute(InExecutor);

	if (Reply == EDataLinkExecutionReply::Unhandled)
	{
		UE_LOG(LogDataLink, Log, TEXT("[%s] Node '%s' did not handle execution."), InExecutor.GetContextName().GetData(), *GetName())
		InExecutor.FailNode(this);
	}
}

#if WITH_EDITOR
void UDataLinkNode::BuildMetadata(FDataLinkNodeMetadata& OutMetadata) const
{
	OutMetadata
		.SetDisplayName(GetClass()->GetDisplayNameText())
		.SetTooltipText(GetClass()->GetToolTipText());

	OnBuildMetadata(OutMetadata);
}
#endif

void UDataLinkNode::BuildPins(TArray<FDataLinkPin>& OutInputPins, TArray<FDataLinkPin>& OutOutputPins) const
{
	FDataLinkPinBuilder InputBuilder(OutInputPins);
	FDataLinkPinBuilder OutputBuilder(OutOutputPins);
	OnBuildPins(InputBuilder, OutputBuilder);
}
