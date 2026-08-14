// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_AsyncDataLinkRequest.h"
#include "DataLinkRequestProxy.h"

UK2Node_AsyncDataLinkRequest::UK2Node_AsyncDataLinkRequest()
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UDataLinkRequestProxy, CreateRequestProxy);
	ProxyFactoryClass = UDataLinkRequestProxy::StaticClass();
	ProxyClass = UDataLinkRequestProxy::StaticClass();
}

UEdGraphPin* UK2Node_AsyncDataLinkRequest::GetDataLinkInstancePin() const
{
	return FindPinChecked(TEXT("InDataLinkInstance"), EGPD_Input);
}

UEdGraphPin* UK2Node_AsyncDataLinkRequest::GetExecutionContextPin() const
{
	return FindPinChecked(TEXT("InExecutionContext"), EGPD_Input);
}

UEdGraphPin* UK2Node_AsyncDataLinkRequest::GetDataLinkSinkProviderPin() const
{
	return FindPinChecked(TEXT("InDataLinkSinkProvider"), EGPD_Input);
}

UEdGraphPin* UK2Node_AsyncDataLinkRequest::GetOutputDataPin() const
{
	return FindPinChecked(TEXT("OutputData"), EGPD_Output);
}

UEdGraphPin* UK2Node_AsyncDataLinkRequest::GetExecutionResultPin() const
{
	return FindPinChecked(TEXT("ExecutionResult"), EGPD_Output);
}

UEdGraphPin* UK2Node_AsyncDataLinkRequest::GetRequestCompletePin() const
{
	return FindPinChecked(TEXT("OnRequestComplete"), EGPD_Output);
}

void UK2Node_AsyncDataLinkRequest::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	// Hide this node from menu actions
}
