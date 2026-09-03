// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryClient.h"

#include "Cluster/DisplayClusterClusterEvent.h"
#include "Cluster/IPDisplayClusterClusterManager.h"

#include "Network/Configuration/DisplayClusterNetworkConfiguration.h"
#include "Network/Conversion/DisplayClusterNetworkDataConversion.h"
#include "Network/Listener/DisplayClusterHelloMessageStrings.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"
#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryStrings.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterEventsBinaryClient::FDisplayClusterClusterEventsBinaryClient(const FName& InName, bool bIsInternal)
	: FDisplayClusterClient(InName.ToString(), 1)
	, bIsInternalClient(bIsInternal)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClient
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterEventsBinaryClient::Connect(const FString& Address, const uint16 Port, const uint32 ConnectRetriesAmount, const uint32 ConnectRetryDelay)
{
	// First, allow base class to perform connection
	if (!FDisplayClusterClient::Connect(Address, Port, ConnectRetriesAmount, ConnectRetryDelay))
	{
		return false;
	}

	// Only internal clients greet the server on connection
	if (bIsInternalClient)
	{
		// Prepare 'hello' message
		TSharedPtr<FDisplayClusterPacketInternal> HelloMsg = MakeShared<FDisplayClusterPacketInternal>(
			DisplayClusterHelloMessageStrings::Hello::Name,
			DisplayClusterHelloMessageStrings::Hello::TypeRequest,
			DisplayClusterClusterEventsBinaryStrings::ProtocolName
		);

		// Fill in the message with data
		const FString NodeId = GDisplayCluster->GetPrivateClusterMgr()->GetNodeId();
		HelloMsg->SetTextArg(DisplayClusterHelloMessageStrings::ArgumentsDefaultCategory, DisplayClusterHelloMessageStrings::Hello::ArgNodeId, NodeId);

		// Send message (no response awaiting)
		return SendPacket(HelloMsg);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsBinary
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterEventsBinaryClient::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event)
{
	// Convert internal binary event type to binary net packet
	TSharedPtr<FDisplayClusterPacketBinary> Request = UE::nDisplay::DisplayClusterNetworkDataConversion::BinaryEventToBinaryPacket(const_cast<FDisplayClusterClusterEventBinary&>(Event));
	if (!Request)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Couldn't convert binary cluster event data to net packet"));
		return EDisplayClusterCommResult::WrongRequestData;
	}

	bool bResult = false;

	{
		// Send event
		TRACE_CPUPROFILER_EVENT_SCOPE(CLN_CEB::EmitClusterEventBinary);
		bResult = SendPacket(Request);
	}

	if (!bResult)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Couldn't send binary cluster event"));
		return EDisplayClusterCommResult::NetworkError;
	}

	return EDisplayClusterCommResult::Ok;
}
