// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/common/Integration.h>
#include <pma/resources/AlignedMemoryResource.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

Integration& GetIntegrationParams()
{
	static pma::AlignedMemoryResource mem;
	static Integration params = {Logger(), &mem};
	return params;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
