// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Logger.h>
#include <pma/MemoryResource.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
* @Brief Integration structure wraps objects that implements core functionalities from the external client.
*/
struct Integration
{
	Logger logger{};
	pma::MemoryResource* memoryResource{};
};

/**
* @Brief Gets/Sets Integration struct to be used in the core tech lib
*
* @returns Integraton parameters - singleton instance.
*/
EPIC_CARBON_API Integration&  GetIntegrationParams();

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

//Singletone instances - recommended usage is providing object through DI
#define LOGGER TITAN_NAMESPACE::GetIntegrationParams().logger
#define MEM_RESOURCE TITAN_NAMESPACE::GetIntegrationParams().memoryResource
