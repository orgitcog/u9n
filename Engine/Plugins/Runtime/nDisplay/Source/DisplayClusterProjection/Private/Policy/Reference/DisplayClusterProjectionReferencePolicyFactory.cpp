// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Reference/DisplayClusterProjectionReferencePolicyFactory.h"
#include "Policy/Reference/DisplayClusterProjectionReferencePolicy.h"
#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterProjectionLog.h"

TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> FDisplayClusterProjectionReferencePolicyFactory::Create(
	const FString& ProjectionPolicyId,
	const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	check(InConfigurationProjectionPolicy != nullptr);

	UE_LOG(LogDisplayClusterProjectionReference, Verbose,
		TEXT("Instantiating projection policy <%s> id='%s'"), *InConfigurationProjectionPolicy->Type, *ProjectionPolicyId);

	return  MakeShared<FDisplayClusterProjectionReferencePolicy, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy);
}
